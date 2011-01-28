/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Alexander Shalamov <alexander.shalamov@nokia.com>
**
** This library is free software; you can redistribute it and/or modify it
** under the terms of the GNU Lesser General Public License version 2.1 as
** published by the Free Software Foundation.
**
** This library is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
** or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
** License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this library; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
**
******************************************************************************/

// Qt includes
#include <QCoreApplication>
#include <QDBusReply>
#include <QDebug>
#include <QTimer>
#include <QDir>

// MeegoTouch includes
#include <MLocale>
#include <MRemoteAction>
#include <MNotification>
#include <MNotificationGroup>

// contacts
#include <QContactManager>
#include <QContactFetchRequest>
#include <QContactOnlineAccount>
#include <QContactDetailFilter>
#include <QContactLocalIdFilter>
#include <QContactPhoneNumber>
#include <QContactId>
#include <QContactName>
#include <QContactDisplayLabel>

// ContextKit includes
#include <contextproperty.h>
#include <contextpropertyinfo.h>

// CommHistory includes
#include <CommHistory/commonutils.h>
#include <CommHistory/GroupModel>
#include <CommHistory/Group>

// Telepathy includes
#include <TelepathyQt4/Constants>

// Our includes
#include "notificationmanager.h"
#include "locstrings.h"
#include "constants.h"
#include "mwilistener.h"
#include "channellistener.h"

using namespace RTComLogger;
using namespace CommHistory;

NotificationManager* NotificationManager::m_pInstance = 0;

namespace {

QContactFilter createContactFilter(const QString &localId, const QString &remoteId)
{
    if (localId == RING_ACCOUNT_PATH || localId == MMS_ACCOUNT_PATH) {
        return QContactPhoneNumber::match(remoteId);
    } else {
        QContactDetailFilter filterLocal;
        filterLocal.setDetailDefinitionName(QContactOnlineAccount::DefinitionName,
                                            QLatin1String("AccountPath"));
        filterLocal.setValue(localId);

        QContactDetailFilter filterRemote;
        filterRemote.setDetailDefinitionName(QContactOnlineAccount::DefinitionName,
                                             QContactOnlineAccount::FieldAccountUri);
        filterRemote.setValue(remoteId);

        // for phone calls over SIP/Skype check remote id as a phone number
        QString number = CommHistory::normalizePhoneNumber(remoteId);
        if (number.isEmpty()) {
            return filterLocal & filterRemote;
        } else {
            return ((filterLocal & filterRemote)
                    | QContactPhoneNumber::match(remoteId));
        }
    }
}

QContactFilter addContactFilter(const QContactFilter &existingFilter,
                                const QContactFilter &newFilter)
{
    if (existingFilter == QContactFilter())
        return newFilter;

    return existingFilter | newFilter;
}

bool matchContact(const QList<QContactOnlineAccount> &accounts,
                  const QList<QContactPhoneNumber> &phones,
                  const QString &localId,
                  const QString &remoteId)
{
    foreach (QContactOnlineAccount account, accounts) {
        if (localId == account.value("AccountPath")
            && CommHistory::remoteAddressMatch(remoteId, account.value(QContactOnlineAccount::FieldAccountUri)))
            return true;
    }

    foreach (QContactPhoneNumber phone, phones) {
        if (CommHistory::remoteAddressMatch(remoteId, phone.number()))
            return true;
    }
    return false;
}

}

// constructor
//
NotificationManager::NotificationManager(QObject* parent)
        : QObject(parent)
        , m_ObservedConversation(new ContextProperty(OBSERVED_CONVERSATION_KEY, this))
        , m_ObservedInbox(new ContextProperty(INBOX_KEY, this))
        , m_ObservedCallHistory(new ContextProperty(CALL_HISTORY_KEY, this))
        , m_Storage(QDir::homePath() + COMMHISTORYD_NOTIFICATIONSSTORAGE)
        , m_Initialised(false)
        , m_pContactManager(0)
        , m_GroupModel(0)
{
    qRegisterMetaType<RTComLogger::NotificationGroup>("RTComLogger::NotificationGroup");
    qRegisterMetaTypeStreamOperators<RTComLogger::NotificationGroup>("RTComLogger::NotificationGroup");
    qRegisterMetaType<RTComLogger::PersonalNotification>("RTComLogger::PersonalNotification");
    qRegisterMetaTypeStreamOperators<RTComLogger::PersonalNotification>("RTComLogger::PersonalNotification");
}

NotificationManager::~NotificationManager()
{
    removeNotificationGroup(CommHistory::Event::VoicemailEvent);

    foreach (MNotificationGroup *group, m_MgtGroups) {
        delete group;
    }
    m_MgtGroups.clear();

    foreach(QContactFetchRequest *request, m_requests.keys())
        delete request;
    m_requests.clear();

    // TODO: remove when https://projects.maemo.org/bugzilla/show_bug.cgi?id=199411 fixed
    m_pContactManager->deleteLater();
}

void NotificationManager::init()
{
    if (m_Initialised) {
        return;
    }
    // creates data directory
    createDataDir();
    // Loads old state
    loadState();
    // create mnotification restored groups

    QList<MNotificationGroup *> mgtGroups = MNotificationGroup::notificationGroups();
    foreach(NotificationGroup group, m_Notifications.uniqueKeys()) {
        foreach(MNotificationGroup *mgtGroup, mgtGroups) {
            if (mgtGroup->eventType() == eventType(group.type())) {
                m_MgtGroups.insert(group.type(), mgtGroup);
                mgtGroups.removeOne(mgtGroup);
                break;
            }
        }
        if (!m_MgtGroups.contains(group.type())) {
            qWarning() << "Group incorrectly saved " << eventType(group.type());
            addGroup(group.type());
        }
    }

    if (mgtGroups.size() > 0) {
        qWarning() << "Mismatch between meegoo groups and our groups:";
        foreach(MNotificationGroup *mgtGroup, mgtGroups) {
            qWarning() << mgtGroup->eventType();
            mgtGroup->remove();
            delete mgtGroup;
        }
    }

    m_ObservedConversation->subscribe();
    m_ObservedConversation->waitForSubscription();
    connect(m_ObservedConversation,
            SIGNAL(valueChanged()),
            SLOT(slotObservedConversationChanged()));

    m_ObservedInbox->subscribe();
    m_ObservedInbox->waitForSubscription();
    connect(m_ObservedInbox,
            SIGNAL(valueChanged()),
            SLOT(slotObservedInboxChanged()));

    m_ObservedCallHistory->subscribe();
    m_ObservedCallHistory->waitForSubscription();
    connect(m_ObservedCallHistory,
            SIGNAL(valueChanged()),
            SLOT(slotObservedCallHistoryChanged()));

    m_NotificationTimer.setSingleShot(true);
    m_NotificationTimer.setInterval(NOTIFICATION_THRESHOLD);
    connect(&m_NotificationTimer, SIGNAL(timeout()), this, SLOT(fireNotifications()));

    m_ContactsTimer.setSingleShot(true);
    m_ContactsTimer.setInterval(CONTACT_REQUEST_THRESHOLD);
    connect(&m_ContactsTimer, SIGNAL(timeout()), this, SLOT(fireUnknownContactsRequest()));

    // start contact tracking
    contactManager();

    if (hasMessageNotification())
        groupModel();

    m_pMWIListener = new MWIListener(this);
    connect(m_pMWIListener,
            SIGNAL(MWICountChanged(int)),
            this,
            SLOT(slotMWICountChanged(int)));

    m_Initialised = true;
}

NotificationManager* NotificationManager::instance()
{
    if (!m_pInstance) {
        m_pInstance = new NotificationManager(QCoreApplication::instance());
        m_pInstance->init();
    }

    return m_pInstance;
}

void NotificationManager::showNotification(ChannelListener * channelListener,
                                           const CommHistory::Event& event,
                                           const QString& channelTargetId,
                                           CommHistory::Group::ChatType chatType)
{
    qDebug() << Q_FUNC_INFO << event.id() << channelTargetId << chatType;

    bool observed = isCurrentlyObservedByUI(event, channelTargetId, chatType);
    if (!event.isRead() && !observed) {
        PersonalNotification notification(event.remoteUid(),
                                          event.localUid(),
                                          event.type(),
                                          channelTargetId,
                                          chatType);
        notification.setNotificationText(notificationText(event));

        m_unresolvedEvents.enqueue(notification);

        TpContactUid cuid(event.localUid(),
                          event.remoteUid());
        if (!event.remoteUid().isEmpty() // private number
            && !m_contacts.contains(cuid))
            requestContact(cuid, channelListener);
        else
            resolveEvents();
    } else if (observed
               && event.direction() == Event::Inbound) {

    }
}

bool NotificationManager::isCurrentlyObservedByUI(const CommHistory::Event& event,
                                                  const QString &channelTargetId,
                                                  CommHistory::Group::ChatType chatType)
{
    if (m_ObservedChannelLocalId.isNull()
        || m_ObservedChannelRemoteId.isNull())
        return false;

    // Return false if it's not message event (IM or SMS/MMS)
    CommHistory::Event::EventType eventType = event.type();
    if (eventType != CommHistory::Event::IMEvent
        && eventType != CommHistory::Event::SMSEvent
        && eventType != CommHistory::Event::MMSEvent)
    {
        return false;
    }

    bool remoteIdMatch = false;
    bool localIdMatch = false;
    bool chatTypeMatch = false;

    // check contextkit property status, if not ready, we assume
    // ui is not observed
    if(m_ObservedConversation) {
        const ContextPropertyInfo* info = m_ObservedConversation->info();
        if (info && info->exists() && info->provided()) {
            QString remoteMatch;
            if (chatType == CommHistory::Group::ChatTypeP2P)
                remoteMatch = event.remoteUid();
            else
                remoteMatch = channelTargetId;

            remoteIdMatch = (CommHistory::remoteAddressMatch(remoteMatch,
                                                             m_ObservedChannelRemoteId));

            localIdMatch = MAP_MMS_TO_RING(event.localUid()) == m_ObservedChannelLocalId;

            chatTypeMatch = chatType == m_ObservedChannelChatType;
        }
    }

    return localIdMatch && remoteIdMatch && chatTypeMatch;
}

void NotificationManager::removeConversationNotifications(const QString &localId,
                                                          const QString &remoteId,
                                                          CommHistory::Group::ChatType chatType)
{
    NotificationGroup updatedGroup;

    // remove matched notifications and udpate group
    QMutableHashIterator<NotificationGroup,PersonalNotification> i(m_Notifications);
    while (i.hasNext()) {
        i.next();
        if (i.key().isValid()) {
            int eventType = i.key().type();
            if ((eventType == CommHistory::Event::IMEvent
                 || eventType == CommHistory::Event::SMSEvent
                 || eventType == CommHistory::Event::MMSEvent)
                && i.value().account() == localId
                && CommHistory::remoteAddressMatch(i.value().remoteUid(),
                                                   remoteId)
                && (CommHistory::Group::ChatType)(i.value().chatType()) == chatType) {
                if (!updatedGroup.isValid())
                    updatedGroup = i.key();

                i.remove();
            }
        }
    }

    if (updatedGroup.isValid()) {
        updateNotificationGroup(updatedGroup);
        saveState();
    }
}

void NotificationManager::slotObservedConversationChanged()
{
    if (m_ObservedConversation) {
        QVariant value = m_ObservedConversation->value(QVariant());
        if (!value.isNull()) {
            QVariantList values = value.toList();
            if (values.count() > 1) {
                m_ObservedChannelLocalId = values.takeFirst().toString();
                m_ObservedChannelRemoteId = values.takeFirst().toString();
                m_ObservedChannelChatType = (CommHistory::Group::ChatType)(values.takeFirst().toUInt());

                removeConversationNotifications(m_ObservedChannelLocalId,
                                                m_ObservedChannelRemoteId,
                                                m_ObservedChannelChatType);
            }
        } else {
            m_ObservedChannelLocalId = QString();
            m_ObservedChannelRemoteId = QString();
            m_ObservedChannelChatType = CommHistory::Group::ChatTypeP2P;
        }
    }
}

void NotificationManager::slotObservedInboxChanged()
{
    if (m_ObservedInbox) {
        QVariant value = m_ObservedInbox->value(QVariant());
        if (!value.isNull()) {
            bool inbox = value.toBool();
            qDebug() << Q_FUNC_INFO << "inbox? " << inbox;
            if (inbox) {
                // remove sms and im notification groups and save state
                // remove meegotouch groups
                removeNotificationGroup(CommHistory::Event::IMEvent);
                removeNotificationGroup(CommHistory::Event::SMSEvent);
                removeNotificationGroup(CommHistory::Event::MMSEvent);
                saveState();
            }
        }
    }
}

void NotificationManager::slotObservedCallHistoryChanged()
{
    if (m_ObservedCallHistory) {
        QVariant value = m_ObservedCallHistory->value(QVariant());
        if (!value.isNull()) {
            bool inbox = value.toBool();
            qDebug() << Q_FUNC_INFO << " call history inbox? " << inbox;
            if (inbox) {
                removeNotificationGroup(CommHistory::Event::CallEvent);
                saveState();
            }
        }
    }
}

bool NotificationManager::removeNotificationGroup(int type)
{
    NotificationGroup group(type);
    removeGroup(type);
    saveState();
    return m_Notifications.remove(group) > 0;
}

NotificationGroup NotificationManager::notificationGroup(int type)
{
    NotificationGroup notificationgroup(type);
    if( !m_Notifications.contains( notificationgroup )) {
        // create group
        addGroup(type);
    } else {
        foreach(NotificationGroup group, m_Notifications.keys()) {
            if(group.type() == type) {
                notificationgroup = group;
                break;
            }
        }
    }
    return notificationgroup;
}


void NotificationManager::addNotification(PersonalNotification notification)
{
    NotificationGroup notificationgroup = notificationGroup(notification.eventType());

    if(!notificationgroup.isValid()) {
        qWarning() << "Wrong notification group";
        return;
    }

    notification.setHasPendingEvents();

    m_Notifications.insertMulti(notificationgroup, notification);

    saveState();
}

void NotificationManager::resolveEvents()
{
    while(!m_unresolvedEvents.isEmpty()) {
        PersonalNotification notification = m_unresolvedEvents.head();

        TpContactUid cuid(notification.account(), notification.remoteUid());

        if (notification.remoteUid().isEmpty()) {
            addNotification(m_unresolvedEvents.dequeue());
        } else if (m_contacts.contains(cuid)) {
            QContact contact = m_contacts.value(cuid);

            //set contact id for usage by notification key
            if (!contact.isEmpty())
                notification.setContactId(contact.localId());
            else if (m_requests.key(cuid) != NULL) // if contact is empty
                break;                             // but there is pending request for it
                                                   // wait

            addNotification(m_unresolvedEvents.dequeue());
        } else
            break;
    }

    fireNotifications();
}

void NotificationManager::fireNotifications()
{
    // if we cant show notification
    // return
    if (!canShowNotification()) {
        // when new event is received and timer is active
        // restart timer
        startNotificationTimer();
        return;
    }

    // find notifications that we need to show
    QList<NotificationGroup> keys = m_Notifications.uniqueKeys();
    foreach(NotificationGroup ng ,keys) {
        if (ng.isValid()) {
            PersonalNotification pn = m_Notifications.value(ng);
            if (pn.hasPendingEvents()) {
                showLatestNotification(ng, pn);
            }
        } else {
            // show group-less notification in received order
            QList<PersonalNotification> ungrouped = m_Notifications.values(ng);
            QMutableListIterator<PersonalNotification> i(ungrouped);
            i.toBack();
            while (i.hasPrevious()) {
                showLatestNotification(ng, i.previous());
            }
        }
    }
}

void NotificationManager::startNotificationTimer()
{
    m_NotificationTimer.start();
}

bool NotificationManager::canShowNotification()
{
    return !m_NotificationTimer.isActive();
}

QString NotificationManager::eventType(int type)
{
    QString event;
    for(int i = 0; i < _eventTypesCount; i++) {
        if(_eventTypes[i].type == type) {
            event = QLatin1String(_eventTypes[i].event);
            break;
        }
    }
    return event;
}

void NotificationManager::showLatestNotification(const NotificationGroup &group,
                                                 PersonalNotification &notification)
{
    // show personal notification
    int type = group.type(); // event type
    QString name;

    // voicemail notification shouldn't have contact name
    if (type != CommHistory::Event::VoicemailEvent) {
        name = contactName(notification.account(),
                           notification.remoteUid());
    }

    QString activateAction = action(group, notification, false);
    activateAction = activateNotificationRemoteAction(type, activateAction);

    QString event = eventType(type);

    MNotification mnote(event,
                        name,
                        notification.notificationText());
    if (group.isValid() && m_MgtGroups.contains(type))
        mnote.setGroup(*m_MgtGroups.value(type));

    mnote.setAction(MRemoteAction(activateAction, this));
    mnote.publish();

    if (group.isValid()) {
        notification.setHasPendingEvents(false);
        m_Notifications.replace(group, notification);

        updateNotificationGroup(group);
    } else {
        m_Notifications.remove(group, notification);
    }

    // event was shown to the user
    // start notification threshold timer
    startNotificationTimer();
}

void NotificationManager::updateNotificationGroup(const NotificationGroup &group)
{
    if (m_Notifications.contains(group)) {
        const PersonalNotification notification = m_Notifications.value(group);

        QString name;

        // update group notification
        bool grouped = countContacts(group) > 1;
        // get group message
        QString message = notificationGroupText(group, notification);
        // get group action
        QString groupAction = action(group, notification, grouped);
        groupAction = activateNotificationRemoteAction(group.type(),
                                                       groupAction);

        // update group
        // if there are more than 1 contact, name is empty
        // if event is voicemail, name is already empty
        if (!grouped && group.type() != CommHistory::Event::VoicemailEvent) {
            name = contactName(notification.account(),
                               notification.remoteUid());
        }
        updateGroup(group.type(), name, message, groupAction);
    } else {
        // m_Notifications doesnt have any personal notification of the given group
        // remove the group type completly
        removeNotificationGroup(group.type());
    }
}

QString NotificationManager::createActionInbox()
{
    return MRemoteAction(MESSAGING_SERVICE_NAME,
                         OBJECT_PATH,
                         MESSAGING_INTERFACE,
                         SHOW_INBOX_METHOD).toString();
}

QString NotificationManager::createActionConversation(const QString& accountPath,
                                                      const QString& remoteUid,
                                                      CommHistory::Group::ChatType chatType)
{
    QList<QVariant> args;
    args.append(QVariant(accountPath));
    args.append(QVariant(remoteUid));
    args.append(QVariant((uint)chatType));
    return MRemoteAction(MESSAGING_SERVICE_NAME,
                         OBJECT_PATH,
                         MESSAGING_INTERFACE,
                         START_CONVERSATION_METHOD,
                         args).toString();
}

QString NotificationManager::activateNotificationRemoteAction(int type,
                                                              const QString& action)
{
    // tapping on notification will invoke D-Bus method com.nokia.CommHistoryIf.activateNotification
    // CommHistoryService::activateNotification(uint groupId, const QString& remoteActionString)
    // The subsequent action (e.g. show mui inbox or conversation view) is passed as string parameter
    // to activateNotification(), which will convert the string back to MRemoteAction and trigger it
    QList<QVariant> args;
    args.append(QVariant(type));
    args.append(QVariant(action));
    return MRemoteAction(COMM_HISTORY_SERVICE_NAME,
                         COMM_HISTORY_OBJECT_PATH,
                         COMM_HISTORY_INTERFACE,
                         ACTIVATE_NOTIFICATION_METHOD,
                         args).toString();
}

QString NotificationManager::createActionCallHistory()
{
    QList<QVariant> args;
    args.append(QVariant(QStringList() << CALL_HISTORY_PARAMETER));

    return MRemoteAction(CALL_HISTORY_SERVICE_NAME,
                         CALL_HISTORY_OBJECT_PATH,
                         CALL_HISTORY_INTERFACE,
                         CALL_HISTORY_METHOD,
                         args).toString();
}

QString NotificationManager::createActionVoicemail()
{
    // todo: implement when sapi for voicemail calling is ready
    return MRemoteAction(CALL_SERVICE_NAME,
                         CALL_OBJECT_PATH,
                         CALL_INTERFACE,
                         VOICEMAIL_METHOD).toString();
}

QString NotificationManager::action(const NotificationGroup& group,
                                    const PersonalNotification& notification,
                                    bool grouped)
{
    QString action;

    switch (group.type())
    {
        case CommHistory::Event::IMEvent:
        case CommHistory::Event::SMSEvent:
        case CommHistory::Event::MMSEvent:
        {
            if (grouped)
                action = createActionInbox();
            else
                action = createActionConversation(notification.account(),
                                                  notification.targetId(),
                                                  (CommHistory::Group::ChatType)notification.chatType());
            break;
        }
        case CommHistory::Event::CallEvent:
        {
            action = createActionCallHistory();
            break;
        }
        case CommHistory::Event::VoicemailEvent:
        {
            action = createActionVoicemail();
            break;
        }
        default:
            break;
    }

    return action;
}

QString NotificationManager::notificationText(const CommHistory::Event& event)
{
    QString text;
    switch(event.type())
    {
        case CommHistory::Event::IMEvent:
        case CommHistory::Event::SMSEvent:
        {
            text = event.fromVCardLabel().isEmpty()
                   ? event.freeText()
                   : txt_qtn_msg_notification_new_vcard(event.fromVCardLabel());
            break;
        }
        case CommHistory::Event::MMSEvent:
        {
            // TODO : not specified what to show in notification in case of single MMS
            text = event.subject();
            break;
        }

        case CommHistory::Event::CallEvent:
        {
            text = txt_qtn_call_missed(1);
            break;
        }
        case CommHistory::Event::VoicemailEvent:
        {
            // freeText() returns the amount of new / not listened voicemails
            // e.g. 3 Voicemails
            text = event.freeText();
            break;
        }
        default:
            break;
    }

    return text;
}

QString NotificationManager::notificationGroupText(const NotificationGroup& group,
                                          const PersonalNotification& notification)
{
    QString groupmessage;
    int notifications = countNotifications(group);
    switch (group.type())
    {
        case CommHistory::Event::IMEvent:
        case CommHistory::Event::SMSEvent:
        case CommHistory::Event::MMSEvent:
        {
            if(notifications > 1)
                groupmessage = txt_qtn_msg_notification_new_message(notifications);
            else
                groupmessage = notification.notificationText();
            break;
        }
        case CommHistory::Event::CallEvent:
        {
            groupmessage = txt_qtn_call_missed(notifications);
            break;
        }
        case CommHistory::Event::VoicemailEvent:
        {
            // The amount of new / not listened voicemails
            groupmessage = notification.notificationText();
            break;
        }
        default:
            break;
    }

    return groupmessage;
}

int NotificationManager::countContacts(const NotificationGroup& group)
{
    QList<PersonalNotification> contacts;
    foreach(PersonalNotification pn, m_Notifications.values(group)) {
        if(!contacts.contains(pn)) {
            contacts.append(pn);
        }
    }
    return contacts.count();
}

int NotificationManager::countNotifications(const NotificationGroup& group)
{
    return m_Notifications.count(group);
}

void NotificationManager::requestContact(TpContactUid cuid,
                                         ChannelListener *channelListener)
{
    QContactFilter filter = createContactFilter(cuid.first,
                                                cuid.second);
    QContactFetchRequest* request = startContactRequest(filter,
                                                        SLOT(slotResultsAvailable()));

    qDebug() << Q_FUNC_INFO << cuid.first << cuid.second;

    m_requests.insert(request, cuid);
    QWeakPointer<ChannelListener> channelListenerPtr(channelListener);
    if (!channelListenerPtr.isNull())
        m_pendingChannelListeners.insert(request, channelListenerPtr);
}

QContactFetchRequest* NotificationManager::startContactRequest(QContactFilter &filter,
                                                               const char *resultSlot)
{
    Q_ASSERT(resultSlot != NULL);

    QContactFetchRequest* request = new QContactFetchRequest();
    request->setManager(contactManager());
    request->setParent(this);

    // connect ready signal to message item so that it can refresh itself when fetching is done
    connect(request,
            SIGNAL(resultsAvailable()),
            resultSlot);

    request->setFilter(filter);

    QStringList details;
    details << QContactName::DefinitionName
            << QContactOnlineAccount::DefinitionName
            << QContactDisplayLabel::DefinitionName;

    QContactFetchHint hint;
    hint.setDetailDefinitionsHint(details);
    request->setFetchHint(hint);

    request->start();

    return request;
}

void NotificationManager::slotResultsAvailable()
{
    QContactFetchRequest *request = qobject_cast<QContactFetchRequest *>(sender());

    if(!request || !request->isFinished()) {
        return;
    }

    qDebug() << Q_FUNC_INFO << request->contacts().size() << "contacts";

    TpContactUid cuid = m_requests.value(request);
    QContact contact;  // insert emtpy contact to indicate
                       // there is no contact for the remote id

    if (request->contacts().size() > 1) {
        // in case there are several contacts, probably in cellular over voip case
        // prefer contacts from account
        foreach (QContact c, request->contacts()) {
            QContactOnlineAccount account = c.detail(QContactOnlineAccount::DefinitionName);
            if (account.value("AccountPath") == cuid.first) {
                contact = c;
                break;
            }
        }
    }

    if (!request->contacts().empty() && contact.isEmpty())
        contact = request->contacts().first();

    qDebug() << Q_FUNC_INFO << "Using" << contact.displayLabel();

    m_contacts.insert(cuid, contact);
    // Here we need to keep track of what instances of ChannelListener are associated with a
    // certain QContact. We might not have any, and this means we're adding the first notification
    // for this contact. We might have some, and we need to update the list.
    QWeakPointer<ChannelListener> channelListener = m_pendingChannelListeners.value(request);
    if (!channelListener.isNull()) {
        QList<QWeakPointer<ChannelListener> > *listeners =
            m_channelsPerContact.value(contact);
        if (listeners == 0) {
            listeners = new QList<QWeakPointer<ChannelListener> >;
            m_channelsPerContact.insert(contact, listeners);
        }
        if (!listeners->contains(channelListener)) {
            listeners->append(channelListener);
            connect(channelListener.data(), SIGNAL(channelClosed(ChannelListener *)),
                    this, SLOT(slotChannelClosed(ChannelListener *)));
        }

        m_pendingChannelListeners.remove(request);
    }

    m_requests.remove(request);
    request->deleteLater();

    resolveEvents();
}

void NotificationManager::slotChannelClosed(ChannelListener *channelListener)
{
    disconnect(channelListener,
               SIGNAL(channelClosed(ChannelListener *)),
               this,
               SLOT (slotChannelClosed(ChannelListener *)));

    QMutableHashIterator<QContact, QList<QWeakPointer<ChannelListener> >*> i
        (m_channelsPerContact);
    while (i.hasNext()) {
        QContact contact = i.next().key();
        QList<QWeakPointer<ChannelListener> > *listeners = i.value();
        if (listeners != 0) {
            listeners->removeOne(QWeakPointer<ChannelListener>(channelListener));
            if (listeners->isEmpty()) {
                // Alright: this is the thing we were looking for: it appeas that the last channel
                // this contact was on got just closed. We can get rid of the entry from the
                // contact cache.

                // First let's delete the list of listeners, or it will leak.
                delete listeners;

                // Then we can remove the entry from m_channelsPerContact altogether, as we will
                // be creating it again if there will be another notification involving this
                // contact.
                i.remove();

                // Now we can finally remove the contact from the cache! Again some iteration,
                // though :(
                QMutableHashIterator<TpContactUid, QContact> j(m_contacts);
                while (j.hasNext()) {
                    QContact foundContact = j.next().value();
                    if (foundContact == contact) {
                        // BAM!
                        j.remove();
                        break; // out of the inner while.
                    }
                }
            }

            // We found the contact whose list of listeners contains the listener given as an
            // argument to this method. No need to keep looking.
            break;
        }
    }
}

QString NotificationManager::contactName(const QString &localUid,
                                         const QString &remoteUid)
{
    QString result;

    if (remoteUid.isEmpty()) {
        result = txt_qtn_call_type_private;
    } else {
        QContact contact = m_contacts.value(TpContactUid(localUid, remoteUid));

        if (!contact.isEmpty()) {
            result = contact.displayLabel();
        }

        if (result.isEmpty())
            result = remoteUid;
    }

    qDebug() << Q_FUNC_INFO << localUid << remoteUid << result;
    return result;
}

void NotificationManager::addGroup(int type)
{
    MNotificationGroup *group = new MNotificationGroup(eventType(type));
    m_MgtGroups.insert(type, group);
    group->publish(); //init group id
}

void NotificationManager::removeGroup(int type)
{
    if (m_MgtGroups.contains(type)) {
        MNotificationGroup *group = m_MgtGroups.take(type);
        if (group)
            group->remove();
        delete group;
    }
}

void NotificationManager::updateGroup(int eventType,
                                      const QString& contactName,
                                      const QString& message,
                                      const QString& action)
{
    if (m_MgtGroups.contains(eventType)) {
        MNotificationGroup *group = m_MgtGroups.value(eventType);
        if (group) {
            group->setSummary(contactName);
            group->setBody(message);
            group->setAction(MRemoteAction(action));
            group->publish();
        } else
            qWarning() << "NULL group for event type" << eventType;
    } else
        qWarning() << "No group for event type" << eventType;
}


void NotificationManager::saveState()
{
    if( !openStorageFile(QIODevice::WriteOnly) ) {
        qDebug() << "Cant open storage for saving";
        return;
    }

    m_Storage.reset();

    QDataStream out(&m_Storage);
    if( out.status() ) {
        qDebug() << "Corrupted data file";
        return;
    }
    out.setVersion(QDataStream::Qt_4_6);
    out << m_Notifications;
    m_Storage.flush();
    m_Storage.close();
}

void NotificationManager::loadState()
{
    if( !openStorageFile(QIODevice::ReadOnly) ) {
        qDebug() << "Cant open storage for reading";
        return;
    }

    QDataStream in(&m_Storage);
    if( in.status() ) {
        qDebug() << "Corrupted data file";
        return;
    }
    in.setVersion(QDataStream::Qt_4_6);
    in >> m_Notifications;
    m_Storage.close();
}

bool NotificationManager::openStorageFile(QIODevice::OpenModeFlag flag)
{
    if(!m_Storage.isOpen() || m_Storage.openMode() != flag) {
        m_Storage.close();
        if(!m_Storage.open(flag)) {
            qDebug() << "Cannot open storage file";
            return false;
        }
    }

    return true;
}

void NotificationManager::createDataDir()
{
    QString dirName = QDir::homePath() + COMMHISTORYD_NOTIFICATIONSDIR;
    QDir dir(dirName);

    if( !dir.exists() ) {
        if( !dir.mkpath(dirName) ) {
            qWarning() << "Cannot create directory for notifications data file";
        }
    }
}

QContactManager* NotificationManager::contactManager()
{
    if(!m_pContactManager){
        m_pContactManager = new QContactManager(CONTACT_STORAGE_TYPE);
        // TODO: uncomment when https://projects.maemo.org/bugzilla/show_bug.cgi?id=199411 fixed
        //m_pContactManager->setParent(this);
        connect(m_pContactManager,
                SIGNAL(contactsAdded(const QList<QContactLocalId>&)),
                SLOT(slotContactsAdded(const QList<QContactLocalId>&)));
        connect(m_pContactManager,
                SIGNAL(contactsRemoved(const QList<QContactLocalId>&)),
                SLOT(slotContactsRemoved(const QList<QContactLocalId>&)));
        connect(m_pContactManager,
                SIGNAL(contactsChanged(const QList<QContactLocalId>&)),
                SLOT(slotContactsChanged(const QList<QContactLocalId>&)));
    }
    return m_pContactManager;
}

void NotificationManager::slotContactsAdded(const QList<QContactLocalId> &contactIds)
{
    if (contactIds.isEmpty())
        return;

    qDebug() << Q_FUNC_INFO;

    // we can't match contactIds with local unknown contacts
    // therefore request all unknown contacts from notification contacts
    QMutableHashIterator<TpContactUid, QContact> i(m_contacts);
    while (i.hasNext()) {
        i.next();
        if (i.value().isEmpty()) {
            TpContactUid cuid = i.key();
            QContactFilter filter = createContactFilter(cuid.first, cuid.second);
            m_ContactFilter = addContactFilter(m_ContactFilter, filter);
        }
    }

    startContactsTimer();
}

void NotificationManager::slotContactsRemoved(const QList<QContactLocalId> &contactIds)
{
    if (contactIds.isEmpty())
        return;

    qDebug() << Q_FUNC_INFO;

    // update contact cache for notifications
    QList<QContactLocalId> updatedContactIds;
    QMutableHashIterator<TpContactUid, QContact> i(m_contacts);
    while (i.hasNext()) {
        QContact c = i.next().value();
        if (!c.isEmpty() && contactIds.contains(c.localId())) {
            i.setValue(QContact());
            updatedContactIds << c.localId();
        }
    }

    updateNotifcationContacts(updatedContactIds);
}

void NotificationManager::slotContactsChanged(const QList<QContactLocalId> &contactIds)
{
    if (contactIds.isEmpty())
        return;

    qDebug() << Q_FUNC_INFO;

    if (!m_contacts.isEmpty() || !m_Notifications.isEmpty()) {
        QContactLocalIdFilter filter;
        filter.setIds(contactIds);
        m_ContactFilter = addContactFilter(m_ContactFilter, filter);

        startContactsTimer();
    }
}

CommHistory::GroupModel* NotificationManager::groupModel()
{
    if (!m_GroupModel) {
        m_GroupModel = new CommHistory::GroupModel(this);
        m_GroupModel->enableContactChanges(false);
        connect(m_GroupModel,
                SIGNAL(rowsAboutToBeRemoved(const QModelIndex&, int, int)),
                this,
                SLOT(slotGroupRemoved(const QModelIndex&, int, int)));

        if (!m_GroupModel->getGroups()) {
            qCritical() << "Failed to request group "
                        << m_GroupModel->lastError().text();
            delete m_GroupModel;
            m_GroupModel = 0;
        }
    }

    return m_GroupModel;
}

void NotificationManager::startContactsTimer()
{
    m_ContactsTimer.start();
}

void NotificationManager::slotOnModelReady()
{
    disconnect(m_GroupModel, SIGNAL(modelReady()),
               this, SLOT(slotOnModelReady()));
    fireUnknownContactsRequest();
}

void NotificationManager::fireUnknownContactsRequest()
{
    if (!groupModel()->isReady()) {
        connect(m_GroupModel, SIGNAL(modelReady()), SLOT(slotOnModelReady()));
        return;
    }

    qDebug() << Q_FUNC_INFO;

    if (m_ContactFilter != QContactFilter()) {
        startContactRequest(m_ContactFilter,
                            SLOT(slotResultsAvailableForUnknown()));
        m_ContactFilter = QContactFilter();
    }
}

void NotificationManager::slotResultsAvailableForUnknown()
{
    QContactFetchRequest *request = qobject_cast<QContactFetchRequest *>(sender());

    if(!request || !request->isFinished()) {
        return;
    }

    qDebug() << Q_FUNC_INFO << request->contacts().size() << "contacts";

    QList<QContactLocalId> updatedContactIds;

    foreach (QContact contact, request->contacts()) {
        QList<QContactOnlineAccount> accounts = contact.details<QContactOnlineAccount>();
        QList<QContactPhoneNumber> phones = contact.details<QContactPhoneNumber>();

        // check notifications contact
        QMutableHashIterator<TpContactUid, QContact> i(m_contacts);
        while (i.hasNext()) {
            i.next();
            TpContactUid cuid = i.key();
            QContact cacheContact = i.value();
            if (matchContact(accounts, phones, cuid.first, cuid.second)) {
                i.setValue(contact);
                updatedContactIds << contact.localId();
            } else if (cacheContact.localId() == contact.localId()) {
                i.setValue(QContact());
                updatedContactIds << contact.localId();
            }
        }
    }

    updateNotifcationContacts(updatedContactIds);

    request->deleteLater();
}

void NotificationManager::slotGroupRemoved(const QModelIndex &index, int start, int end)
{
    qDebug() << Q_FUNC_INFO;
    for (int i = start; i <= end; i++) {
        QModelIndex row = m_GroupModel->index(i, 0, index);
        Group group = m_GroupModel->group(row);
        if (group.isValid()
            && !group.remoteUids().isEmpty()) {
            removeConversationNotifications(group.localUid(),
                                            group.remoteUids().first(),
                                            group.chatType());
        }
    }
}

void NotificationManager::updateNotifcationContacts(const QList<QContactLocalId> &contactIds)
{
    QMutableHashIterator<NotificationGroup,PersonalNotification> i(m_Notifications);

    QSet<NotificationGroup> updatedGroups;
    bool changed = false;

    while (i.hasNext()) {
        i.next();
        if (i.key().isValid()) {
            PersonalNotification pn = i.value();
            TpContactUid cuid(pn.account(),
                              pn.remoteUid());

            QContact c = m_contacts.value(cuid);
            int localId = 0;

            if (!c.isEmpty())
                localId = c.localId();

            if (localId != pn.contactId()) {
                pn.setContactId(c.localId());
                i.setValue(pn);
                changed = true;
                updatedGroups << i.key();
            } else if (contactIds.contains(localId)) {
                updatedGroups << i.key();
            }
        }
    }

    if (changed)
        saveState();

    foreach (NotificationGroup group, updatedGroups.toList()) {
        if (countContacts(group) == 1)
            updateNotificationGroup(group);
    }
}

void NotificationManager::showVoicemailNotification(int count)
{
    m_pMWIListener->saveMWI(count);
}

void NotificationManager::slotMWICountChanged(int count)
{
    qDebug() << Q_FUNC_INFO << count;

    if (count < -1) {
        qCritical() << "Invalid voicemail count" << count;
        return;
    }

    if (count == 0) {
        removeNotificationGroup(CommHistory::Event::VoicemailEvent);
        return;
    }

    NotificationGroup notificationgroup = notificationGroup(CommHistory::Event::VoicemailEvent);

    if (!notificationgroup.isValid()) {
        qWarning() << "Wrong notification group";
        return;
    }

    PersonalNotification notification;
    notification.setHasPendingEvents();

    if (count > 0)
        notification.setNotificationText(txt_qtn_call_voicemail_notification(count));
    else // unknown number, use string for 1 notification
        notification.setNotificationText(txt_qtn_call_voicemail_notification(1));

    m_Notifications.insert(notificationgroup, notification);

    saveState();
    fireNotifications();
}

bool NotificationManager::hasMessageNotification() const
{
    foreach(NotificationGroup g, m_Notifications.keys()) {
        int eventType = g.type();
        if (eventType == CommHistory::Event::IMEvent
            || eventType == CommHistory::Event::SMSEvent
            || eventType == CommHistory::Event::MMSEvent)
            return true;
    }

    return false;
}
