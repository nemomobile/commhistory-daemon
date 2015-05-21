/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2013-2015 Jolla Ltd.
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

    groupModel();

    m_Initialised = true;
}

void NotificationManager::syncNotifications()
{
    QList<PersonalNotification*> pnList;
    QMap<int,int> typeCounts;
    QList<QObject*> notifications = Notification::notifications();

    foreach (QObject *o, notifications) {
        Notification *n = static_cast<Notification*>(o);

        if (n->hintValue("x-commhistoryd-data").isNull()) {
            // This was a group notification, which will be recreated if required
            n->close();
            delete n;
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
}

NotificationManager* NotificationManager::instance()
{
    if (!m_pInstance) {
        m_pInstance = new NotificationManager(QCoreApplication::instance());
        m_pInstance->init();
    }

    return m_pInstance;
}

bool NotificationManager::updateEditedEvent(const CommHistory::Event& event, const QString &text)
{
    if (event.messageToken().isEmpty())
        return false;

    foreach (PersonalNotification *notification, m_unresolvedEvents) {
        if (notification->eventToken() == event.messageToken()) {
            notification->setNotificationText(text);
            return true;
        }
    }

    EventGroupProperties groupProperties(eventGroup(PersonalNotification::collection(event.type()), event.localUid(), event.remoteUid()));
    NotificationGroup *eventGroup = m_Groups.value(groupProperties);
    if (!eventGroup)
        return false;

    foreach (PersonalNotification *pn, eventGroup->notifications()) {
        if (pn->eventToken() == event.messageToken()) {
            pn->setNotificationText(text);
            return true;
        }
    }

    return false;
}

void NotificationManager::showNotification(const CommHistory::Event& event,
                                           const QString& channelTargetId,
                                           CommHistory::Group::ChatType chatType,
                                           const QString &details)
{
    DEBUG() << Q_FUNC_INFO << event.id() << channelTargetId << chatType;

    if (event.type() == CommHistory::Event::SMSEvent
        || event.type() == CommHistory::Event::MMSEvent
        || event.type() == CommHistory::Event::IMEvent)
    {
        bool inboxObserved = CommHistoryService::instance()->inboxObserved();
        if (inboxObserved || isCurrentlyObservedByUI(event, channelTargetId, chatType)) {
            if (!m_ngfClient->isConnected())
                m_ngfClient->connect();

            if (!m_ngfEvent) {
                if (event.type() == CommHistory::Event::SMSEvent || event.type() == CommHistory::Event::MMSEvent) {
                    m_ngfEvent = m_ngfClient->play(QLatin1Literal(inboxObserved ? "sms" : "sms_fg"));
                } else {
                    m_ngfEvent = m_ngfClient->play(QLatin1Literal(inboxObserved ? "chat" : "chat_fg"));
                }
            }

            return;
        }
    }

    // try to update notifications for existing event
    QString text(notificationText(event, details));
    if (event.isValid() && updateEditedEvent(event, text))
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
    notification->setNotificationText(text);
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

void NotificationManager::requestClass0Notification(const CommHistory::Event &event)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(QLatin1String("org.nemomobile.ClassZeroSmsNotification"),
                                                      QLatin1String("/org/nemomobile/ClassZeroSmsNotification"),
                                                      QLatin1String("org.nemomobile.ClassZeroSmsNotification"),
                                                      QLatin1String("showNotification"));
    QList<QVariant> arguments;
    arguments << event.freeText();
    msg.setArguments(arguments);
    if (!QDBusConnection::sessionBus().callWithCallback(msg, this, 0, SLOT(slotClassZeroError(QDBusError)))) {
        qWarning() << "Unable to create class 0 SMS notification request";
    }
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

        if (event.localUid() != values[0].toString())
            continue;

        if (!CommHistory::remoteAddressMatch(event.localUid(), remoteMatch, values[1].toString()))
            continue;

        if (chatType != (CommHistory::Group::ChatType)values[2].toUInt())
            continue;

        return true;
    }

    return false;
}

void NotificationManager::removeNotifications(const QString &accountPath, const QList<int> &removeTypes)
{
    DEBUG() << Q_FUNC_INFO << "Removing notifications of account " << accountPath;

    QSet<NotificationGroup> updatedGroups;

    // remove matched notifications and update group
    foreach (NotificationGroup *group, m_Groups) {
        if (group->localUid() != accountPath) {
            continue;
        }

        foreach (PersonalNotification *notification, group->notifications()) {
            if (removeTypes.isEmpty() || removeTypes.contains(notification->eventType())) {
                DEBUG() << Q_FUNC_INFO << "Removing notification: accountPath: " << notification->account() << " remoteUid: " << notification->remoteUid();
                group->removeNotification(notification);
            }
        }
    }

    for (QList<PersonalNotification*>::iterator it = m_unresolvedEvents.begin();
            it != m_unresolvedEvents.end(); ) {
        if ((*it)->account() == accountPath) {
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
    QHash<EventGroupProperties, NotificationGroup *>::const_iterator it = m_Groups.constBegin(), end = m_Groups.constEnd();
    for ( ; it != end; ++it) {
        NotificationGroup *group(it.value());
        if (group->localUid() != localUid)
            continue;

        foreach (PersonalNotification *notification, group->notifications()) {
            if (notification->collection() != PersonalNotification::Messaging)
                continue;

            QString notificationRemoteUidStr;
            // For p-to-p chat we use remote uid for comparison and for MUC we use target (channel) id:
            if (chatType == CommHistory::Group::ChatTypeP2P)
                notificationRemoteUidStr = notification->remoteUid();
            else
                notificationRemoteUidStr = notification->targetId();

            if (notification->account() == localUid
                    && CommHistory::remoteAddressMatch(localUid, notificationRemoteUidStr, remoteUid)
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
        QList<int> removeTypes;
        removeTypes << CommHistory::Event::IMEvent << CommHistory::Event::SMSEvent << CommHistory::Event::MMSEvent << VOICEMAIL_SMS_EVENT_TYPE;

        if (!isFilteredInbox()) {
            // remove sms, mms and im notifications
            removeNotificationTypes(removeTypes);
        } else {
            // Filtering is in use, remove only notifications of that account whose threads are visible in inbox:
            QString filteredAccountPath = filteredInboxAccountPath();
            DEBUG() << Q_FUNC_INFO << "Removing only notifications belonging to account " << filteredAccountPath;
            if (!filteredAccountPath.isEmpty())
                removeNotifications(filteredAccountPath, removeTypes);
        }
    }
}

void NotificationManager::slotCallHistoryObservedChanged(bool observed)
{
    if (observed) {
        removeNotificationTypes(QList<int>() << CommHistory::Event::CallEvent);
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

void NotificationManager::removeNotificationTypes(const QList<int> &types)
{
    DEBUG() << Q_FUNC_INFO << types;

    foreach (NotificationGroup *group, m_Groups) {
        foreach (PersonalNotification *notification, group->notifications()) {
            if (types.contains(notification->eventType())) {
                group->removeNotification(notification);
            }
        }
    }
}

void NotificationManager::addNotification(PersonalNotification *notification)
{
    EventGroupProperties groupProperties(eventGroup(notification->collection(), notification->account(), notification->remoteUid()));
    NotificationGroup *group = m_Groups.value(groupProperties);
    if (!group) {
        group = new NotificationGroup(groupProperties.collection, groupProperties.localUid, groupProperties.remoteUid, this);
        m_Groups.insert(groupProperties, group);
    }

    group->addNotification(notification);
}

int NotificationManager::pendingEventCount()
{
    return m_unresolvedEvents.size();
}

QString NotificationManager::notificationText(const CommHistory::Event& event, const QString &details)
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
            if (event.status() == Event::ManualNotificationStatus) {
                text = txt_qtn_mms_notification_manual_download;
            } else if (event.status() >= Event::TemporarilyFailedStatus) {
                QString trimmedDetails(details.trimmed());
                if (trimmedDetails.isEmpty()) {
                    if (event.direction() == Event::Inbound)
                        text = txt_qtn_mms_notification_download_failed;
                    else
                        text = txt_qtn_mms_notification_send_failed;
                } else {
                    text = trimmedDetails;
                }
            } else {
                if (!event.subject().isEmpty())
                    text = event.subject();
                else
                    text = event.freeText();

                int attachmentCount = 0;
                foreach (const MessagePart &part, event.messageParts()) {
                    if (!part.contentType().startsWith("text/plain") &&
                        !part.contentType().startsWith("application/smil"))
                    {
                        attachmentCount++;
                    }
                }

                if (attachmentCount > 0) {
                    if (!text.isEmpty())
                        text = txt_qtn_mms_notification_with_text(attachmentCount, text);
                    else
                        text = txt_qtn_mms_notification_attachment(attachmentCount);
                }
            }
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

static QVariantMap dbusAction(const QString &name, const QString &service, const QString &path, const QString &iface,
                              const QString &method, const QVariantList &arguments = QVariantList())
{
    QVariantMap action;
    action.insert(QStringLiteral("name"), name);
    action.insert(QStringLiteral("service"), service);
    action.insert(QStringLiteral("path"), path);
    action.insert(QStringLiteral("iface"), iface);
    action.insert(QStringLiteral("method"), method);
    action.insert(QStringLiteral("arguments"), arguments);
    return action;
}

void NotificationManager::setNotificationProperties(Notification *notification, PersonalNotification *pn, bool grouped)
{
    QString appName;
    QVariantList remoteActions;

    switch (pn->collection()) {
        case PersonalNotification::Messaging:

            appName = txt_qtn_msg_notifications_group;

            if (pn->eventType() != VOICEMAIL_SMS_EVENT_TYPE && grouped) {
                remoteActions.append(dbusAction("default",
                                                MESSAGING_SERVICE_NAME,
                                                OBJECT_PATH,
                                                MESSAGING_INTERFACE,
                                                SHOW_INBOX_METHOD));
            } else {
                remoteActions.append(dbusAction("default",
                                                MESSAGING_SERVICE_NAME,
                                                OBJECT_PATH,
                                                MESSAGING_INTERFACE,
                                                START_CONVERSATION_METHOD,
                                                QVariantList() << pn->account()
                                                               << pn->targetId()
                                                               << static_cast<uint>(pn->chatType())));
            }

            remoteActions.append(dbusAction("app",
                                            MESSAGING_SERVICE_NAME,
                                            OBJECT_PATH,
                                            MESSAGING_INTERFACE,
                                            SHOW_INBOX_METHOD));
            break;

        case PersonalNotification::Voice:

            appName = txt_qtn_msg_missed_calls_group;

            remoteActions.append(dbusAction("default",
                                            CALL_HISTORY_SERVICE_NAME,
                                            CALL_HISTORY_OBJECT_PATH,
                                            CALL_HISTORY_INTERFACE,
                                            CALL_HISTORY_METHOD,
                                            QVariantList() << CALL_HISTORY_PARAMETER));
            remoteActions.append(dbusAction("app",
                                            CALL_HISTORY_SERVICE_NAME,
                                            CALL_HISTORY_OBJECT_PATH,
                                            CALL_HISTORY_INTERFACE,
                                            CALL_HISTORY_METHOD,
                                            QVariantList() << CALL_HISTORY_PARAMETER));
            break;

        case PersonalNotification::Voicemail:

            appName = txt_qtn_msg_voicemail_group;

            remoteActions.append(dbusAction("default",
                                            CALL_HISTORY_SERVICE_NAME,
                                            VOICEMAIL_OBJECT_PATH,
                                            VOICEMAIL_INTERFACE,
                                            VOICEMAIL_METHOD));
            remoteActions.append(dbusAction("app",
                                            CALL_HISTORY_SERVICE_NAME,
                                            VOICEMAIL_OBJECT_PATH,
                                            VOICEMAIL_INTERFACE,
                                            VOICEMAIL_METHOD));
            break;
    }

    notification->setAppName(appName);
    notification->setRemoteActions(remoteActions);
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
                CommHistory::remoteAddressMatch(address.first, notification->remoteUid(), address.second)) {
            DEBUG() << "Unknown contact for notification" << notification->account() << notification->remoteUid();
            addNotification(notification);
            it = m_unresolvedEvents.erase(it);
        } else
            it++;
    }
}

void NotificationManager::slotClassZeroError(const QDBusError &error)
{
    qWarning() << "Class 0 SMS notification failed:" << error.message();
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
    Q_UNUSED(count)
    qWarning() << Q_FUNC_INFO << "Stub";
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
            const QString remoteUid = group.remoteUids().first();
            const QString localUid = group.localUid();

            foreach (NotificationGroup *g, m_Groups) {
                if (g->localUid() != localUid) {
                    continue;
                }

                foreach (PersonalNotification *pn, g->notifications()) {
                    // If notification is for MUC and matches to changed group...
                    if (!pn->chatName().isEmpty() && pn->account() == localUid &&
                            CommHistory::remoteAddressMatch(localUid, pn->targetId(), remoteUid))
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

