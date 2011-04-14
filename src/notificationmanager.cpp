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
    if (removeNotificationGroup(CommHistory::Event::VoicemailEvent))
        saveState();

    foreach (MNotificationGroup *group, m_MgtGroups) {
        delete group;
    }
    m_MgtGroups.clear();

    foreach(QContactFetchRequest *request, m_requests.keys())
        delete request;
    m_requests.clear();
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
    syncNotifications();

    m_ObservedConversation->subscribe();
    connect(m_ObservedConversation,
            SIGNAL(valueChanged()),
            SLOT(slotObservedConversationChanged()));

    m_ObservedInbox->subscribe();
    connect(m_ObservedInbox,
            SIGNAL(valueChanged()),
            SLOT(slotObservedInboxChanged()));

    m_ObservedCallHistory->subscribe();
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

void NotificationManager::syncNotifications()
{
    QList<MNotificationGroup*> mgtGroups = MNotificationGroup::notificationGroups();
    foreach (NotificationGroup group, m_Notifications.uniqueKeys()) {
        // for a chd group find matched meego touch group
        foreach(MNotificationGroup *mgtGroup, mgtGroups) {
            if (mgtGroup->eventType() == eventType(group.type())) {
                m_MgtGroups.insert(group.type(), mgtGroup);
                mgtGroups.removeOne(mgtGroup);
                break;
            }
        }

        if (!m_MgtGroups.contains(group.type()))
            addGroup(group.type());
    }

    if (mgtGroups.size() > 0) {
        qWarning() << "Mismatch between meegoo groups and our groups:";
        foreach(MNotificationGroup *mgtGroup, mgtGroups) {
            qWarning() << mgtGroup->eventType();
            mgtGroup->remove();
            delete mgtGroup;
        }
    }
}

NotificationManager* NotificationManager::instance()
{
    if (!m_pInstance) {
        m_pInstance = new NotificationManager(QCoreApplication::instance());
        m_pInstance->init();
    }

    return m_pInstance;
}

void NotificationManager::showNotification(const CommHistory::Event& event,
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
            requestContact(cuid);
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
    if (m_ObservedConversation) {
        if (!m_ObservedConversation->value().isNull()) {
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
    QSet<NotificationGroup> updatedGroups;

    // remove matched notifications and udpate group
    QMutableHashIterator<NotificationGroup,PersonalNotification> i(m_Notifications);
    while (i.hasNext()) {
        i.next();
        if (i.key().isValid()) {
            int eventType = i.key().type();

            QString notificationRemoteUidStr;
            // For p-to-p chat we use remote uid for comparison and for MUC we use target (channel) id:
            if (chatType == CommHistory::Group::ChatTypeP2P)
                notificationRemoteUidStr = i.value().remoteUid();
            else
                notificationRemoteUidStr = i.value().targetId();

            if ((eventType == CommHistory::Event::IMEvent
                 || eventType == CommHistory::Event::SMSEvent
                 || eventType == CommHistory::Event::MMSEvent)
                && MAP_MMS_TO_RING(i.value().account()) == localId
                && CommHistory::remoteAddressMatch(notificationRemoteUidStr,
                                                   remoteId)
                && (CommHistory::Group::ChatType)(i.value().chatType()) == chatType) {
                updatedGroups.insert(i.key());
                i.remove();
            }
        }
    }

    if (!updatedGroups.isEmpty()) {
        clearContactsCache();
        foreach(NotificationGroup group, updatedGroups)
            updateNotificationGroup(group);
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
                // remove sms, mms and im notification groups and save state
                // remove meegotouch groups
                bool save = false;
                save = removeNotificationGroup(CommHistory::Event::IMEvent) || save;
                save = removeNotificationGroup(CommHistory::Event::SMSEvent) || save;
                save = removeNotificationGroup(CommHistory::Event::MMSEvent) || save;
                if (save)
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
                if (removeNotificationGroup(CommHistory::Event::CallEvent))
                    saveState();
            }
        }
    }
}

void NotificationManager::clearContactsCache()
{
    QList<TpContactUid> tpContactUids;
    QList<NotificationGroup> keys = m_Notifications.uniqueKeys();

    foreach(NotificationGroup ng,keys) {
        QList<PersonalNotification> notifications = m_Notifications.values(ng);
        foreach(PersonalNotification pn,notifications) {
            TpContactUid cuid(pn.account(), pn.remoteUid());
            if (!tpContactUids.contains(cuid))
                tpContactUids.append(cuid);
        }
    }

    QMutableHashIterator<TpContactUid, QContact> contactIt(m_contacts);
    while (contactIt.hasNext()) {
        contactIt.next();
        if (!tpContactUids.contains(contactIt.key())) {
            qDebug() << Q_FUNC_INFO << "Removing contact " << contactIt.key().second;
            contactIt.remove();
        }
    }
}

bool NotificationManager::removeNotificationGroup(int type)
{
    NotificationGroup group(type);
    removeGroup(type);
    bool success = m_Notifications.remove(group) > 0;

    /* We need to iterate here through the still existing notification groups and if our contact cache contains
       a contact not belonging to any of those existing notification groups anymore then we can delete that contact
       from the contact cache. */
    if (success)
        clearContactsCache();

    return success;
}

NotificationGroup NotificationManager::notificationGroup(int type)
{
    NotificationGroup group(type);

    if (!m_Notifications.contains(group))
        addGroup(type);

    return group;
}


void NotificationManager::addNotification(PersonalNotification notification)
{
    NotificationGroup notificationgroup = notificationGroup(notification.eventType());

    if(!notificationgroup.isValid()) {
        qWarning() << Q_FUNC_INFO << "Wrong notification group";
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

void NotificationManager::clearPendingEvents(const NotificationGroup &group)
{
    QList<PersonalNotification> notifications = m_Notifications.values(group);
    m_Notifications.remove(group);
    foreach (PersonalNotification p, notifications) {
        p.setHasPendingEvents(false);
        m_Notifications.insertMulti(group, p);
    }
}

void NotificationManager::removeNotPendingEvents(const NotificationGroup &group)
{
    QList<PersonalNotification> notifications = m_Notifications.values(group);
    m_Notifications.remove(group);
    foreach (PersonalNotification p, notifications) {
        if (p.hasPendingEvents()) {
            m_Notifications.insertMulti(group, p);
        }
    }

    clearContactsCache();
}

void NotificationManager::showLatestNotification(const NotificationGroup &group,
                                                 PersonalNotification &notification)
{
    MNotificationGroup *mgroup = m_MgtGroups.value(group.type());
    if (mgroup) {
        // reset counters if the group has been cleared
        if (mgroup->notificationCount() == 0)
            removeNotPendingEvents(group);
    } else {
        qWarning() << Q_FUNC_INFO << "NULL group for " << group.type();
    }

    // show personal notification
    int type = group.type(); // event type
    QString name;

    // voicemail notification shouldn't have contact name
    if (type != CommHistory::Event::VoicemailEvent) {
        name = contactName(notification.account(),
                           notification.remoteUid());
    }

    QString activateAction = action(group, notification, false);

    QString event = eventType(type);

    MNotification mnote(event,
                        name,
                        notification.notificationText());
    if (group.isValid() && m_MgtGroups.contains(type))
        mnote.setGroup(*m_MgtGroups.value(type));

    mnote.setAction(MRemoteAction(activateAction, this));
    mnote.publish();

    if (group.isValid()) {
        clearPendingEvents(group);
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

        // update group
        // if there are more than 1 contact, name is empty
        // if event is voicemail, name is already empty
        if (!grouped && group.type() != CommHistory::Event::VoicemailEvent) {
            name = contactName(notification.account(),
                               notification.remoteUid());
        } else if (grouped && group.type() != CommHistory::Event::VoicemailEvent) {
            name = contactNames(group).join(CONTACT_SEPARATOR_IN_NOTIFICATION_GROUP);
        }
        updateGroup(group.type(), countNotifications(group), name, message, groupAction);
    } else {
        // m_Notifications doesnt have any personal notification of the given group
        // remove the group type completly
        if (removeNotificationGroup(group.type()))
            saveState();
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

void NotificationManager::requestContact(TpContactUid cuid)
{
    QContactFilter filter = createContactFilter(cuid.first,
                                                cuid.second);
    QContactFetchRequest* request = startContactRequest(filter,
                                                        SLOT(slotResultsAvailable()));

    qDebug() << Q_FUNC_INFO << cuid.first << cuid.second;

    m_requests.insert(request, cuid);
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
    QContact contact;  // insert empty contact to indicate
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

    m_requests.remove(request);
    request->deleteLater();

    resolveEvents();
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

QStringList NotificationManager::contactNames(const NotificationGroup& group)
{
    QStringList names;
    QList<PersonalNotification> contacts;

    foreach(PersonalNotification pn, m_Notifications.values(group)) {
        if(!contacts.contains(pn)) {
            contacts.append(pn);
        }
    }

    for (int i=0; i<contacts.size();i++)
        names.append(contactName(contacts[i].account(), contacts[i].remoteUid()));

    return names;
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
                                      int notificationCount,
                                      const QString& contactName,
                                      const QString& message,
                                      const QString& action)
{
    MNotificationGroup *group = m_MgtGroups.value(eventType);
    if (group) {
        group->setSummary(contactName);
        group->setBody(message);
        group->setAction(MRemoteAction(action));
        group->setCount(notificationCount);
        group->publish();
    } else {
        qWarning() << Q_FUNC_INFO << "No group for event type" << eventType;
    }
}

void NotificationManager::saveState()
{
    qDebug() << Q_FUNC_INFO;

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
        m_pContactManager->setParent(this);
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
        m_GroupModel->enableContactChanges(true);
        connect(m_GroupModel,
                SIGNAL(rowsAboutToBeRemoved(const QModelIndex&, int, int)),
                this,
                SLOT(slotGroupRemoved(const QModelIndex&, int, int)));

        if (!m_GroupModel->getGroups()) {
            qCritical() << "Failed to request group ";
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

void NotificationManager::slotOnModelReady(bool status)
{
    disconnect(m_GroupModel, SIGNAL(modelReady(bool)),
               this, SLOT(slotOnModelReady(bool)));
    if (status)
        fireUnknownContactsRequest();
    else
        qCritical() << "Group model failed to load";
}

void NotificationManager::fireUnknownContactsRequest()
{
    if (!groupModel()->isReady()) {
        connect(m_GroupModel, SIGNAL(modelReady(bool)), SLOT(slotOnModelReady(bool)));
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
        if (removeNotificationGroup(CommHistory::Event::VoicemailEvent))
            saveState();
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
