/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Reto Zingg <reto.zingg@nokia.com>
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
#include "voicemailhandler.h"

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
        , m_ObservedInbox(new ContextProperty(OBSERVED_INBOX_KEY, this))
        , m_FilteredInbox(new ContextProperty(FILTERED_INBOX_KEY, this))
        , m_ObservedCallHistory(new ContextProperty(CALL_HISTORY_KEY, this))
        , m_Storage(QDir::homePath() + COMMHISTORYD_NOTIFICATIONSSTORAGE)
        , m_Initialised(false)
        , m_pContactManager(0)
        , m_GroupModel(0)
        , m_pDisplayState(0)
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

    m_FilteredInbox->subscribe();
    connect(m_FilteredInbox,
            SIGNAL(valueChanged()),
            SLOT(slotObservedInboxChanged()));

    m_ObservedCallHistory->subscribe();
    connect(m_ObservedCallHistory,
            SIGNAL(valueChanged()),
            SLOT(slotObservedCallHistoryChanged()));

    m_NotificationTimer.setSingleShot(true);
    m_NotificationTimer.setInterval(NOTIFICATION_THRESHOLD);
    connect(&m_NotificationTimer, SIGNAL(timeout()), this, SLOT(fireNotifications()));
    connect(&m_NotificationTimer, SIGNAL(timeout()), this, SLOT(slotObservedInboxChanged()));

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

    m_pDisplayState = new MeeGo::QmDisplayState(this);

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

bool NotificationManager::updateEditedEvent(const CommHistory::Event& event)
{
    if (event.messageToken().isEmpty())
        return false;

    QMutableListIterator<PersonalNotification> i(m_unresolvedEvents);
    while (i.hasNext()) {
        if (i.value().eventToken() == event.messageToken()) {
            PersonalNotification pn = i.value();
            i.remove();

            pn.setNotificationText(notificationText(event));
            pn.setHasPendingEvents(true);
            m_unresolvedEvents.enqueue(pn);

            return true;
        }
    }

    NotificationGroup eventGroup = notificationGroup(event.type());
    QHash<NotificationGroup, PersonalNotification>::iterator j = m_Notifications.find(eventGroup);
    while (j != m_Notifications.end() &&  j.key() == eventGroup) {
        if (j.value().eventToken() == event.messageToken()) {
            PersonalNotification pn = j.value();
            m_Notifications.erase(j);

            pn.setNotificationText(notificationText(event));
            pn.setHasPendingEvents(true);

            m_Notifications.insertMulti(eventGroup, pn);

            fireNotifications();

            return true;
        }
        ++j;
    }

    return false;
}

void NotificationManager::showNotification(const CommHistory::Event& event,
                                           const QString& channelTargetId,
                                           CommHistory::Group::ChatType chatType)
{
    qDebug() << Q_FUNC_INFO << event.id() << channelTargetId << chatType;

    bool observed = isCurrentlyObservedByUI(event, channelTargetId, chatType);
    if (!event.isRead() && !observed) {
        // try to update notifications for existing event
        if (event.isValid() && updateEditedEvent(event))
            return;

        // Get MUC topic from group
        QString chatName;
        if (m_GroupModel && (chatType == CommHistory::Group::ChatTypeUnnamed ||
            chatType == CommHistory::Group::ChatTypeRoom)) {
            for (int i = 0; i < m_GroupModel->rowCount(); i++) {
                QModelIndex row = m_GroupModel->index(i, 0);
                CommHistory::Group group = m_GroupModel->group(row);
                if (group.isValid() && group.id() == event.groupId()) {
                    chatName = group.chatName();
                    if (chatName.isEmpty())
                        chatName = txt_qtn_msg_group_chat;
                    qDebug() << Q_FUNC_INFO << "Using chatName:" << chatName;
                    break;
                }
            }
        }

        PersonalNotification notification(event.remoteUid(),
                                          event.localUid(),
                                          event.type(),
                                          channelTargetId,
                                          chatType);
        notification.setNotificationText(notificationText(event));
        notification.setSmsReplaceNumber(event.headers().value(REPLACE_TYPE));

        if (!chatName.isEmpty())
            notification.setChatName(chatName);

        notification.setEventToken(event.messageToken());

        m_unresolvedEvents.enqueue(notification);

        TpContactUid cuid(event.localUid(),
                          event.remoteUid());
        if (!event.remoteUid().isEmpty() // private number
            && !m_contacts.contains(cuid)
            && chatName.isEmpty())
            requestContact(cuid);
        else
            resolveEvents();

        if (event.type() == CommHistory::Event::SMSEvent ||
            event.type() == CommHistory::Event::MMSEvent) {
            undimScreen();
        }
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

void NotificationManager::removeNotifications(const QString &accountPath, bool messagesOnly)
{
    qDebug() << Q_FUNC_INFO << "Removing notifications of account " << accountPath;

    QSet<NotificationGroup> updatedGroups;

    // remove matched notifications and update group
    QMutableHashIterator<NotificationGroup, PersonalNotification> i(m_Notifications);
    while (i.hasNext()) {
        i.next();
        int eventType = i.value().eventType();

        // If removal should be done based on Inbox being observed then remove only those notifications
        // that belong to messaging-ui area:
        if (messagesOnly && (eventType != CommHistory::Event::IMEvent && eventType != CommHistory::Event::SMSEvent
             && eventType != CommHistory::Event::MMSEvent && eventType != VOICEMAIL_SMS_EVENT_TYPE)) {
            qDebug() << Q_FUNC_INFO << "Skipping " << eventType << " type of notification";
            continue;
        }

        // Remove only a notification matching to the account:
        if (i.key().isValid() && MAP_MMS_TO_RING(i.value().account()) == accountPath) {
            qDebug() << Q_FUNC_INFO << "Removing notification: accountPath: " << i.value().account() << " remoteUid: " << i.value().targetId();
            // record notification group id
            updatedGroups.insert(i.key());
            // no need to resolve events anymore -> remove personal notification from queue
            m_unresolvedEvents.removeAll(i.value());
            // at last, delete _i_
            i.remove();
        }
    }

    if (!updatedGroups.isEmpty()) {

        clearContactsCache();
        foreach(NotificationGroup group, updatedGroups)
            updateNotificationGroup(group);

        saveState();
    }
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
                 || eventType == CommHistory::Event::MMSEvent
                 || eventType == VOICEMAIL_SMS_EVENT_TYPE)
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
    qDebug() << Q_FUNC_INFO;

    if (m_ObservedInbox) {
        QVariant value = m_ObservedInbox->value(QVariant());
        if (!value.isNull()) {
            bool observed = value.toBool();
            qDebug() << Q_FUNC_INFO << "inbox observed? " << observed;
            if (observed) {
                // No filtering, we can remove all messaging related notifications:
                if (!isFilteredInbox()) {
                    // remove sms, mms and im notification groups and save state
                    // remove meegotouch groups
                    bool save = false;
                    save = removeNotificationGroup(CommHistory::Event::IMEvent) || save;
                    save = removeNotificationGroup(CommHistory::Event::SMSEvent) || save;
                    save = removeNotificationGroup(CommHistory::Event::MMSEvent) || save;
                    save = removeNotificationGroup(VOICEMAIL_SMS_EVENT_TYPE) || save;
                    if (save)
                        saveState();
                } else {
                    // Filtering is in use, remove only notifications of that account whose threads are visible in inbox:
                    QString filteredAccountPath = filteredInboxAccountPath();
                    qDebug() << Q_FUNC_INFO << "Removing only notifications belonging to account " << filteredAccountPath;
                    if (!filteredAccountPath.isEmpty())
                        removeNotifications(filteredAccountPath, true);
                }
            }
        } else {
            qDebug() << Q_FUNC_INFO << "Context property value for observing inbox is NULL!";
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

bool NotificationManager::isFilteredInbox()
{
    qDebug() << Q_FUNC_INFO;

    if (m_FilteredInbox)
        if (m_FilteredInbox->value(QVariant()).isNull()) return false;

    return true;
}

QString NotificationManager::filteredInboxAccountPath()
{
    qDebug() << Q_FUNC_INFO;

    QString filteredAccount;

    if (m_FilteredInbox) {
        QVariant value = m_FilteredInbox->value(QVariant());
        if (!value.isNull()) {
            filteredAccount = value.toString();
        } else {
            // We do not have filtering on.
            qDebug() << Q_FUNC_INFO << "Context property value for inbox filtering is not set.";
        }
    }

    return filteredAccount;
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
    qDebug() << Q_FUNC_INFO << type;

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
    qDebug() << Q_FUNC_INFO;

    uint eventType;

    if (VoiceMailHandler::instance()->isVoiceMailNumber(notification.remoteUid()))
        eventType = VOICEMAIL_SMS_EVENT_TYPE;
    else
        eventType = notification.eventType();

    NotificationGroup notificationgroup = notificationGroup(eventType);

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
        PersonalNotification &notification = m_unresolvedEvents.head();

        TpContactUid cuid(notification.account(), notification.remoteUid());

        if (notification.remoteUid().isEmpty() || !notification.chatName().isEmpty()) {
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
    qDebug() << Q_FUNC_INFO;

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

    // clear HasPendingEvents() for each notification
    // and keep the order of notifications intact
    while (!notifications.isEmpty()) {
        PersonalNotification p = notifications.takeLast();
        p.setHasPendingEvents(false);
        m_Notifications.insertMulti(group, p);
    }
}

void NotificationManager::removeNotPendingEvents(const NotificationGroup &group)
{
    QList<PersonalNotification> notifications = m_Notifications.values(group);
    m_Notifications.remove(group);

    while (!notifications.isEmpty()) {
        PersonalNotification p = notifications.takeLast();
        if (p.hasPendingEvents()) {
            m_Notifications.insertMulti(group, p);
        }
    }

    clearContactsCache();
}

void NotificationManager::showLatestNotification(const NotificationGroup &group,
                                                 PersonalNotification &notification)
{
    qDebug() << Q_FUNC_INFO;

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

    // voicemail notifications shouldn't have contact name
    if (type != CommHistory::Event::VoicemailEvent && type != VOICEMAIL_SMS_EVENT_TYPE) {
        name = notificationName(notification);
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
    qDebug() << Q_FUNC_INFO;

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
        if (group.type() != CommHistory::Event::VoicemailEvent && group.type() != VOICEMAIL_SMS_EVENT_TYPE)
            name = contactNames(group).join(CONTACT_SEPARATOR_IN_NOTIFICATION_GROUP);

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
    return MRemoteAction(CALL_HISTORY_SERVICE_NAME,
                         VOICEMAIL_OBJECT_PATH,
                         VOICEMAIL_INTERFACE,
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
        case VOICEMAIL_SMS_EVENT_TYPE:
        {
            action = createActionConversation(notification.account(),
                                              notification.targetId(),
                                              (CommHistory::Group::ChatType)notification.chatType());
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
        case VOICEMAIL_SMS_EVENT_TYPE:
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
    qDebug() << Q_FUNC_INFO;

    // If we have more than one new message from same sender and messages have
    // same SMS replace number, then we count all those messages as one.
    QList<PersonalNotification> pnsCounted;
    foreach (PersonalNotification pn, m_Notifications.values(group)) {
        bool match = false;
        if (!pn.smsReplaceNumber().isEmpty()) {
            foreach (PersonalNotification counted, pnsCounted) {
                if (pn.remoteUid() == counted.remoteUid() && pn.smsReplaceNumber() == counted.smsReplaceNumber()) {
                    match = true;
                }
            }
        }

        if (!match)
            pnsCounted.append(pn);
    }

    qDebug() << Q_FUNC_INFO << "Notification count with replace typed messages taken into account: " << pnsCounted.count();
    qDebug() << Q_FUNC_INFO << "Absolute notification count: " << m_Notifications.count(group);

    return pnsCounted.count();
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

    // setup timeout for request
    QTimer *timer = new QTimer(request);
    connect(timer, SIGNAL(timeout()),
            this, SLOT(slotContactRequestTimeout()));
    timer->start(CONTACT_REQUEST_TIMEROUT);

    return request;
}

void NotificationManager::slotContactRequestTimeout()
{
    Q_ASSERT(sender());

    QContactFetchRequest *request = qobject_cast<QContactFetchRequest*>(sender()->parent());

    Q_ASSERT(request);

    if (m_requests.contains(request)) {
        TpContactUid cuid = m_requests.take(request);
        if (!m_contacts.contains(cuid))
            m_contacts.insert(cuid, QContact());
        resolveEvents();
    }

    request->deleteLater();
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

    // show remote id in case of multiple contacts match
    if (request->contacts().size() == 1) {
        contact = request->contacts().first();
        qDebug() << Q_FUNC_INFO << "Using" << contact.displayLabel();
    }

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

        if (result.isEmpty()) {
            if (normalizePhoneNumber(remoteUid).isEmpty()) {
                result = remoteUid;
            } else {
                MLocale locale;
                result = locale.toLocalizedNumbers(remoteUid);
            }
        }
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
        names.append(notificationName(contacts[i]));

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
        if (group->summary() != contactName ||
            group->body() != message) {

            group->setSummary(contactName);
            group->setBody(message);
            group->setAction(MRemoteAction(action));
            group->setCount(notificationCount);
            group->publish();
        } else {
            qDebug() << Q_FUNC_INFO << "Suppressing unneccessary notification update";
        }
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
        QMap<QString,QString> params;
        params["contact-types"] = QLatin1String("contact");
        params["omit-presence-changes"] = QLatin1String(""); // value ignored
        m_pContactManager = new QContactManager(CONTACT_STORAGE_TYPE, params);
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

    if (contactIds.contains(VoiceMailHandler::instance()->voiceMailContactId())) {
        // If removed contact is a voice mail one, then clear its data from VoiceMailHandler:
        qDebug() << Q_FUNC_INFO << "Voice mail contact removed!";
        VoiceMailHandler::instance()->clear();
        // Start listening vmc file changes in order to be notified about new voice mail contact addings.
        VoiceMailHandler::instance()->startObservingVmcFile();
    }

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

    if (contactIds.contains(VoiceMailHandler::instance()->voiceMailContactId())) {
        // If changed contact is a voice mail one, then refresh its data in VoiceMailHandler:
        qDebug() << Q_FUNC_INFO << "Voice mail contact changed!";
        VoiceMailHandler::instance()->fetchVoiceMailContact();
    }

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
        connect(m_GroupModel,
                SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)),
                this,
                SLOT(slotGroupDataChanged(const QModelIndex&, const QModelIndex&)));
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

    if (!request || !request->isFinished()) {
        return;
    }

    qDebug() << Q_FUNC_INFO << request->contacts().size() << "contacts";

    QSet<QContactLocalId> updatedContactIds;

    // check notifications contact
    QMutableHashIterator<TpContactUid, QContact> i(m_contacts);
    while (i.hasNext()) {
        i.next();
        TpContactUid cuid = i.key();
        QContact cacheContact = i.value();
        QList<QContact> matchedContacts;
        bool matchedLocalId = false;

        foreach (QContact contact, request->contacts()) {
            QList<QContactOnlineAccount> accounts = contact.details<QContactOnlineAccount>();
            QList<QContactPhoneNumber> phones = contact.details<QContactPhoneNumber>();

            if (matchContact(accounts, phones, cuid.first, cuid.second))
                matchedContacts << contact;
            if (cacheContact.localId() == contact.localId())
                matchedLocalId = true;
        }

        if (matchedContacts.size() == 1)
            i.setValue(matchedContacts.first());

        if (matchedContacts.size() > 1
            || (matchedContacts.isEmpty() && matchedLocalId))
            i.setValue(QContact());

        foreach (QContact c, matchedContacts)
            updatedContactIds << c.localId();
    }

    updateNotifcationContacts(updatedContactIds.toList());

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

void NotificationManager::undimScreen()
{
    if (m_pDisplayState
       && m_pDisplayState->get() == MeeGo::QmDisplayState::Off) {
       m_pDisplayState->set(MeeGo::QmDisplayState::On);
    }
}

QString NotificationManager::notificationName(const PersonalNotification &notification)
{
    qDebug() << Q_FUNC_INFO;

    if (!notification.chatName().isEmpty())
        return notification.chatName();
    else
        return contactName(notification.account(), notification.remoteUid());
}

void NotificationManager::slotGroupDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    qDebug() << Q_FUNC_INFO;

    QSet<NotificationGroup> updatedGroups;

    // Update MUC notifications if MUC topic has changed
    for (int i = topLeft.row(); i <= bottomRight.row(); i++) {
        QModelIndex row = m_GroupModel->index(i, 0);
        CommHistory::Group group = m_GroupModel->group(row);
        if (group.isValid()) {
            QString remoteUid = group.remoteUids().first();
            bool changed = false;
            QMutableHashIterator<NotificationGroup,PersonalNotification> i(m_Notifications);

            while (i.hasNext()) {
                i.next();
                if (i.key().isValid()) {
                    PersonalNotification pn = i.value();

                    // If notification is for MUC and matches to changed group...
                    if (!pn.chatName().isEmpty() && pn.targetId() == remoteUid) {
                        QString newChatName;
                        if (group.chatName().isEmpty() && pn.chatName() != txt_qtn_msg_group_chat)
                            newChatName = txt_qtn_msg_group_chat;
                        else if (group.chatName() != pn.chatName())
                            newChatName = group.chatName();

                        if (!newChatName.isEmpty()) {
                            qDebug() << Q_FUNC_INFO << "Changing chat name to" << newChatName;
                            pn.setChatName(newChatName);
                            changed = true;
                            i.setValue(pn);
                            updatedGroups << i.key();
                        }
                    }
                }
            }

            if (changed)
                saveState();

            foreach (NotificationGroup ng, updatedGroups.toList())
                updateNotificationGroup(ng);
        }
    }
}
