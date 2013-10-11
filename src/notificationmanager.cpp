/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2013 Jolla Ltd.
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: John Brooks <john.brooks@jolla.com>
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
#include <QTimer>
#include <QDir>

// CommHistory includes
#include <CommHistory/commonutils.h>
#include <CommHistory/GroupModel>
#include <CommHistory/Group>

// Telepathy includes
#include <TelepathyQt/Constants>

// NGF-Qt includes
#include <NgfClient>

// nemo notifications
#include <notification.h>

// mce
#include <mce/dbus-names.h>

// Our includes
#include "notificationmanager.h"
#include "locstrings.h"
#include "constants.h"
#include "mwilistener.h"
#include "commhistoryservice.h"
#include "debug.h"

using namespace RTComLogger;
using namespace CommHistory;

NotificationManager* NotificationManager::m_pInstance = 0;

// constructor
//
NotificationManager::NotificationManager(QObject* parent)
        : QObject(parent)
        , m_Initialised(false)
        , m_GroupModel(0)
        , m_ngfClient(0)
        , m_ngfEvent(0)
{
}

NotificationManager::~NotificationManager()
{
    qDeleteAll(m_Groups);
    qDeleteAll(m_unresolvedEvents);
}

void NotificationManager::init()
{
    if (m_Initialised) {
        return;
    }

    m_contactListener = ContactListener::instance();
    connect(m_contactListener.data(), SIGNAL(contactUpdated(quint32,QString,QList<ContactAddress>)),
            SLOT(slotContactUpdated(quint32,QString,QList<ContactAddress>)));
    connect(m_contactListener.data(), SIGNAL(contactRemoved(quint32)),
            SLOT(slotContactRemoved(quint32)));
    connect(m_contactListener.data(), SIGNAL(contactUnknown(QPair<QString,QString>)),
            SLOT(slotContactUnknown(QPair<QString,QString>)));

    m_ngfClient = new Ngf::Client(this);
    connect(m_ngfClient, SIGNAL(eventFailed(quint32)), SLOT(slotNgfEventFinished(quint32)));
    connect(m_ngfClient, SIGNAL(eventCompleted(quint32)), SLOT(slotNgfEventFinished(quint32)));

    // Loads old state
    syncNotifications();

    CommHistoryService *service = CommHistoryService::instance();
    connect(service, SIGNAL(inboxObservedChanged(bool,QString)), SLOT(slotInboxObservedChanged()));
    connect(service, SIGNAL(callHistoryObservedChanged(bool)), SLOT(slotCallHistoryObservedChanged(bool)));
    connect(service, SIGNAL(observedConversationsChanged(QVariantList)),
                     SLOT(slotObservedConversationsChanged(QVariantList)));

    // For notifications fired when inbox is observed, clear them after NOTIFICATION_THRESHOLD
    m_NotificationTimer.setSingleShot(true);
    m_NotificationTimer.setInterval(NOTIFICATION_THRESHOLD);
    connect(&m_NotificationTimer, SIGNAL(timeout()), this, SLOT(slotInboxObservedChanged()));

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
    QList<PersonalNotification*> pnList;
    QMap<int,int> typeCounts;
    QList<QObject*> notifications = Notification::notifications();

    foreach (QObject *o, notifications) {
        Notification *n = static_cast<Notification*>(o);

        if (n->previewBody().isEmpty() && !n->body().isEmpty() && n->hintValue("x-commhistoryd-data").isNull()) {
            NotificationGroup *group = new NotificationGroup(n, this);
            if (m_Groups.contains(group->type())) {
                group->removeGroup();
                delete group;
                continue;
            }

            connect(group, SIGNAL(changed()), SLOT(slotNotificationGroupChanged()));
            m_Groups.insert(group->type(), group);
        } else {
            PersonalNotification *pn = new PersonalNotification(this);
            if (!pn->restore(n)) {
                delete pn;
                n->close();
                delete n;
                continue;
            }

            typeCounts[pn->eventType()]++;
            pnList.append(pn);
        }
    }

    foreach (PersonalNotification *pn, pnList)
        resolveNotification(pn);

    // Remove groups with no events or unresolved events
    for (QMap<int,NotificationGroup*>::iterator it = m_Groups.begin(); it != m_Groups.end(); ) {
        NotificationGroup *group = *it;
        if (typeCounts[group->type()] < 1) {
            group->removeGroup();
            delete group;
            it = m_Groups.erase(it);
        } else
            it++;
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

    foreach (PersonalNotification *notification, m_unresolvedEvents) {
        if (notification->eventToken() == event.messageToken()) {
            notification->setNotificationText(notificationText(event));
            return true;
        }
    }

    NotificationGroup *eventGroup = m_Groups.value(event.type());
    if (!eventGroup)
        return false;

    foreach (PersonalNotification *pn, eventGroup->notifications()) {
        if (pn->eventToken() == event.messageToken()) {
            pn->setNotificationText(notificationText(event));
            return true;
        }
    }

    return false;
}

void NotificationManager::showNotification(const CommHistory::Event& event,
                                           const QString& channelTargetId,
                                           CommHistory::Group::ChatType chatType)
{
    DEBUG() << Q_FUNC_INFO << event.id() << channelTargetId << chatType;

    if (event.isRead() || isCurrentlyObservedByUI(event, channelTargetId, chatType)) {
        if (!m_ngfClient->isConnected())
            m_ngfClient->connect();

        if (!m_ngfEvent) {
            if (event.type() == CommHistory::Event::SMSEvent || event.type() == CommHistory::Event::MMSEvent)
                m_ngfEvent = m_ngfClient->play(QLatin1Literal("sms_fg"));
            else
                m_ngfEvent = m_ngfClient->play(QLatin1Literal("chat_fg"));
        }

        return;
    }

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
                DEBUG() << Q_FUNC_INFO << "Using chatName:" << chatName;
                break;
            }
        }
    }

    PersonalNotification *notification = new PersonalNotification(event.remoteUid(),
            event.localUid(), event.type(), channelTargetId, chatType);
    notification->setNotificationText(notificationText(event));
    notification->setSmsReplaceNumber(event.headers().value(REPLACE_TYPE));

    if (!chatName.isEmpty())
        notification->setChatName(chatName);

    notification->setEventToken(event.messageToken());

    resolveNotification(notification);

    if (event.type() == CommHistory::Event::SMSEvent ||
        event.type() == CommHistory::Event::MMSEvent) {
        // ask mce to undim the screen
        QString mceMethod = QString::fromLatin1(MCE_DISPLAY_ON_REQ);
        QDBusMessage msg = QDBusMessage::createMethodCall(MCE_SERVICE, MCE_REQUEST_PATH, MCE_REQUEST_IF, mceMethod);
        QDBusConnection::systemBus().call(msg, QDBus::NoBlock);
    }
}

void NotificationManager::resolveNotification(PersonalNotification *pn)
{
    if (pn->remoteUid() == QLatin1String("<hidden>") || !pn->chatName().isEmpty()) {
        // Add notification immediately
        addNotification(pn);
    } else {
        DEBUG() << Q_FUNC_INFO << "Trying to resolve contact for" << pn->account() << pn->remoteUid();
        m_unresolvedEvents.append(pn);
        m_contactListener->resolveContact(pn->account(), pn->remoteUid());
    }
}

void NotificationManager::playClass0SMSAlert()
{
    if (!m_ngfClient->isConnected())
        m_ngfClient->connect();

    m_ngfEvent = m_ngfClient->play(QLatin1Literal("sms"));

    // ask mce to undim the screen
    QString mceMethod = QString::fromLatin1(MCE_DISPLAY_ON_REQ);
    QDBusMessage msg = QDBusMessage::createMethodCall(MCE_SERVICE, MCE_REQUEST_PATH, MCE_REQUEST_IF, mceMethod);
    QDBusConnection::systemBus().call(msg, QDBus::NoBlock);
}

bool NotificationManager::isCurrentlyObservedByUI(const CommHistory::Event& event,
                                                  const QString &channelTargetId,
                                                  CommHistory::Group::ChatType chatType)
{
    // Return false if it's not message event (IM or SMS/MMS)
    CommHistory::Event::EventType eventType = event.type();
    if (eventType != CommHistory::Event::IMEvent
        && eventType != CommHistory::Event::SMSEvent
        && eventType != CommHistory::Event::MMSEvent)
    {
        return false;
    }

    QString remoteMatch;
    if (chatType == CommHistory::Group::ChatTypeP2P)
        remoteMatch = event.remoteUid();
    else
        remoteMatch = channelTargetId;

    QVariantList conversations = CommHistoryService::instance()->observedConversations();
    foreach (const QVariant &conversation, conversations) {
        QVariantList values = conversation.toList();
        if (values.size() != 3)
            continue;

        if (MAP_MMS_TO_RING(event.localUid()) != values[0].toString())
            continue;

        if (!CommHistory::remoteAddressMatch(remoteMatch, values[1].toString()))
            continue;

        if (chatType != (CommHistory::Group::ChatType)values[2].toUInt())
            continue;

        return true;
    }

    return false;
}

void NotificationManager::removeNotifications(const QString &accountPath, bool messagesOnly)
{
    DEBUG() << Q_FUNC_INFO << "Removing notifications of account " << accountPath;

    QSet<NotificationGroup> updatedGroups;

    // remove matched notifications and update group
    foreach (NotificationGroup *group, m_Groups) {
        int eventType = group->type();

        // If removal should be done based on Inbox being observed then remove only those notifications
        // that belong to messaging-ui area:
        if (messagesOnly && (eventType != CommHistory::Event::IMEvent && eventType != CommHistory::Event::SMSEvent
             && eventType != CommHistory::Event::MMSEvent && eventType != VOICEMAIL_SMS_EVENT_TYPE)) {
            DEBUG() << Q_FUNC_INFO << "Skipping " << eventType << " type of notification";
            continue;
        }

        foreach (PersonalNotification *notification, group->notifications()) {
            // Remove only a notification matching to the account:
            if (MAP_MMS_TO_RING(notification->account()) == accountPath) {
                DEBUG() << Q_FUNC_INFO << "Removing notification: accountPath: " << notification->account() << " remoteUid: " << notification->remoteUid();
                group->removeNotification(notification);
            }
        }
    }

    for (QList<PersonalNotification*>::iterator it = m_unresolvedEvents.begin();
            it != m_unresolvedEvents.end(); ) {
        if (MAP_MMS_TO_RING((*it)->account()) == accountPath) {
            delete *it;
            it = m_unresolvedEvents.erase(it);
        } else
            it++;
    }
}

void NotificationManager::removeConversationNotifications(const QString &localUid,
                                                          const QString &remoteUid,
                                                          CommHistory::Group::ChatType chatType)
{
    foreach (NotificationGroup *group, m_Groups) {
        int eventType = group->type();
        if (eventType != CommHistory::Event::IMEvent
             && eventType != CommHistory::Event::SMSEvent
             && eventType != CommHistory::Event::MMSEvent
             && eventType != VOICEMAIL_SMS_EVENT_TYPE)
            continue;

        foreach (PersonalNotification *notification, group->notifications()) {
            QString notificationRemoteUidStr;
            // For p-to-p chat we use remote uid for comparison and for MUC we use target (channel) id:
            if (chatType == CommHistory::Group::ChatTypeP2P)
                notificationRemoteUidStr = notification->remoteUid();
            else
                notificationRemoteUidStr = notification->targetId();

            if (MAP_MMS_TO_RING(notification->account()) == localUid
                    && CommHistory::remoteAddressMatch(notificationRemoteUidStr, remoteUid)
                    && (CommHistory::Group::ChatType)(notification->chatType()) == chatType) {
                group->removeNotification(notification);
            }
        }
    }
}

void NotificationManager::slotObservedConversationsChanged(const QVariantList &conversations)
{
    foreach (const QVariant &conversation, conversations) {
        QVariantList values = conversation.toList();
        if (values.size() != 3)
            continue;

        removeConversationNotifications(values[0].toString(), values[1].toString(),
                                        (CommHistory::Group::ChatType)values[2].toUInt());
    }
}

void NotificationManager::slotInboxObservedChanged()
{
    DEBUG() << Q_FUNC_INFO;

    // Cannot be passed as a parameter, because this slot is also used for m_notificationTimer
    bool observed = CommHistoryService::instance()->inboxObserved();
    if (observed) {
        if (!isFilteredInbox()) {
            // remove sms, mms and im notification groups
            // remove meegotouch groups
            removeNotificationGroup(CommHistory::Event::IMEvent);
            removeNotificationGroup(CommHistory::Event::SMSEvent);
            removeNotificationGroup(CommHistory::Event::MMSEvent);
            removeNotificationGroup(VOICEMAIL_SMS_EVENT_TYPE);
        } else {
            // Filtering is in use, remove only notifications of that account whose threads are visible in inbox:
            QString filteredAccountPath = filteredInboxAccountPath();
            DEBUG() << Q_FUNC_INFO << "Removing only notifications belonging to account " << filteredAccountPath;
            if (!filteredAccountPath.isEmpty())
                removeNotifications(filteredAccountPath, true);
        }
    }
}

void NotificationManager::slotCallHistoryObservedChanged(bool observed)
{
    if (observed) {
        removeNotificationGroup(CommHistory::Event::CallEvent);
    }
}

bool NotificationManager::isFilteredInbox()
{
    return !CommHistoryService::instance()->inboxFilterAccount().isEmpty();
}

QString NotificationManager::filteredInboxAccountPath()
{
    return CommHistoryService::instance()->inboxFilterAccount();
}

void NotificationManager::slotNotificationGroupChanged()
{
    NotificationGroup *group = qobject_cast<NotificationGroup*>(sender());
    if (!group)
        return;

    if (CommHistoryService::instance()->inboxObserved())
        m_NotificationTimer.start();
}

bool NotificationManager::removeNotificationGroup(int type)
{
    DEBUG() << Q_FUNC_INFO << type;

    QMap<int,NotificationGroup*>::iterator it = m_Groups.find(type);
    if (it == m_Groups.end())
        return false;

    (*it)->removeGroup();
    delete *it;
    m_Groups.erase(it);

    return true;
}

void NotificationManager::addNotification(PersonalNotification *notification)
{
    uint eventType = notification->eventType();

    NotificationGroup *group = m_Groups.value(eventType);
    if (!group) {
        group = new NotificationGroup(eventType, this);
        connect(group, SIGNAL(changed()), SLOT(slotNotificationGroupChanged()));
        m_Groups.insert(eventType, group);
    }

    group->addNotification(notification);
}

int NotificationManager::pendingEventCount()
{
    return m_unresolvedEvents.size();
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

void NotificationManager::setNotificationAction(Notification *notification, PersonalNotification *pn, bool grouped)
{
    switch (pn->eventType()) {
        case CommHistory::Event::IMEvent:
        case CommHistory::Event::SMSEvent:
        case CommHistory::Event::MMSEvent:
        case VOICEMAIL_SMS_EVENT_TYPE:
            if (pn->eventType() != VOICEMAIL_SMS_EVENT_TYPE && grouped) {
                notification->setRemoteDBusCallServiceName(MESSAGING_SERVICE_NAME);
                notification->setRemoteDBusCallObjectPath(OBJECT_PATH);
                notification->setRemoteDBusCallInterface(MESSAGING_INTERFACE);
                notification->setRemoteDBusCallMethodName(SHOW_INBOX_METHOD);
                notification->setRemoteDBusCallArguments(QVariantList());
            } else {
                notification->setRemoteDBusCallServiceName(MESSAGING_SERVICE_NAME);
                notification->setRemoteDBusCallObjectPath(OBJECT_PATH);
                notification->setRemoteDBusCallInterface(MESSAGING_INTERFACE);
                notification->setRemoteDBusCallMethodName(START_CONVERSATION_METHOD);

                QVariantList args;
                args << pn->account() << pn->targetId() << uint(pn->chatType());
                notification->setRemoteDBusCallArguments(args);
            }
            break;

        case CommHistory::Event::CallEvent:
            {
                notification->setRemoteDBusCallServiceName(CALL_HISTORY_SERVICE_NAME);
                notification->setRemoteDBusCallObjectPath(CALL_HISTORY_OBJECT_PATH);
                notification->setRemoteDBusCallInterface(CALL_HISTORY_INTERFACE);
                notification->setRemoteDBusCallMethodName(CALL_HISTORY_METHOD);

                QVariantList args;
                args << (QStringList() << CALL_HISTORY_PARAMETER);
                notification->setRemoteDBusCallArguments(args);
            }
            break;

        case CommHistory::Event::VoicemailEvent:
            notification->setRemoteDBusCallServiceName(CALL_HISTORY_SERVICE_NAME);
            notification->setRemoteDBusCallObjectPath(VOICEMAIL_OBJECT_PATH);
            notification->setRemoteDBusCallInterface(VOICEMAIL_INTERFACE);
            notification->setRemoteDBusCallMethodName(VOICEMAIL_METHOD);
            notification->setRemoteDBusCallArguments(QVariantList());
            break;
    }
}

void NotificationManager::slotContactUpdated(quint32 localId, const QString &contactName,
                                             const QList<ContactListener::ContactAddress> &addresses)
{
    DEBUG() << Q_FUNC_INFO << localId << contactName;

    // Check all existing notifications and update if necessary
    foreach (NotificationGroup *group, m_Groups) {
        foreach (PersonalNotification *notification, group->notifications()) {
            if (ContactListener::addressMatchesList(notification->account(),
                        notification->remoteUid(), addresses)) {
                DEBUG() << "Match existing notification" << notification->account() << notification->remoteUid();
                notification->setContactId(localId);
                notification->setContactName(contactName);
            }
        }
    }

    // Check all unresolved events for a match, and add notifications for any that do
    for (QList<PersonalNotification*>::iterator it = m_unresolvedEvents.begin(); it != m_unresolvedEvents.end(); ) {
        PersonalNotification *notification = *it;

        if (ContactListener::addressMatchesList(notification->account(),
                    notification->remoteUid(), addresses)) {
            notification->setContactId(localId);
            notification->setContactName(contactName);
            DEBUG() << "Resolved contact for notification" << notification->account() << notification->remoteUid();
            addNotification(notification);
            it = m_unresolvedEvents.erase(it);
        } else
            it++;
    }
}

void NotificationManager::slotContactRemoved(quint32 localId)
{
    DEBUG() << Q_FUNC_INFO << localId;

    // Check all existing notifications and update if necessary
    foreach (NotificationGroup *group, m_Groups) {
        foreach (PersonalNotification *notification, group->notifications()) {
            if (notification->contactId() == localId) {
                notification->setContactId(0);
                notification->setContactName(QString());
            }
        }
    }
}

void NotificationManager::slotContactUnknown(const QPair<QString,QString> &address)
{
    for (QList<PersonalNotification*>::iterator it = m_unresolvedEvents.begin(); it != m_unresolvedEvents.end(); ) {
        PersonalNotification *notification = *it;

        if (address.first == notification->account() &&
                CommHistory::remoteAddressMatch(notification->remoteUid(), address.second)) {
            DEBUG() << "Unknown contact for notification" << notification->account() << notification->remoteUid();
            addNotification(notification);
            it = m_unresolvedEvents.erase(it);
        } else
            it++;
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

void NotificationManager::slotGroupRemoved(const QModelIndex &index, int start, int end)
{
    DEBUG() << Q_FUNC_INFO;
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
void NotificationManager::showVoicemailNotification(int count)
{
    m_pMWIListener->saveMWI(count);
}

void NotificationManager::slotMWICountChanged(int count)
{
    DEBUG() << Q_FUNC_INFO << count;

    if (count < -1) {
        qCritical() << "Invalid voicemail count" << count;
        return;
    }

    if (count == 0) {
        removeNotificationGroup(CommHistory::Event::VoicemailEvent);
    }
}

bool NotificationManager::hasMessageNotification() const
{
    return m_Groups.contains(CommHistory::Event::IMEvent) ||
           m_Groups.contains(CommHistory::Event::SMSEvent) ||
           m_Groups.contains(CommHistory::Event::MMSEvent);
}

void NotificationManager::slotGroupDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    DEBUG() << Q_FUNC_INFO;

    QSet<NotificationGroup> updatedGroups;

    // Update MUC notifications if MUC topic has changed
    for (int i = topLeft.row(); i <= bottomRight.row(); i++) {
        QModelIndex row = m_GroupModel->index(i, 0);
        CommHistory::Group group = m_GroupModel->group(row);
        if (group.isValid()) {
            QString remoteUid = group.remoteUids().first();
            QString localUid = group.localUid();

            foreach (NotificationGroup *g, m_Groups) {
                foreach (PersonalNotification *pn, g->notifications()) {
                    // If notification is for MUC and matches to changed group...
                    if (!pn->chatName().isEmpty() && pn->account() == localUid &&
                            CommHistory::remoteAddressMatch(pn->targetId(), remoteUid))
                    {
                        QString newChatName;
                        if (group.chatName().isEmpty() && pn->chatName() != txt_qtn_msg_group_chat)
                            newChatName = txt_qtn_msg_group_chat;
                        else if (group.chatName() != pn->chatName())
                            newChatName = group.chatName();

                        if (!newChatName.isEmpty()) {
                            DEBUG() << Q_FUNC_INFO << "Changing chat name to" << newChatName;
                            pn->setChatName(newChatName);
                        }
                    }
                }
            }
        }
    }
}

void NotificationManager::slotNgfEventFinished(quint32 id)
{
    if (id == m_ngfEvent)
        m_ngfEvent = 0;
}

