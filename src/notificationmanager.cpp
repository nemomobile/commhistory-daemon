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
#include <QTimer>
#include <QDir>

// MeegoTouch includes
#include <MLocale>
#include <MRemoteAction>
#include <MNotification>
#include <MNotificationGroup>

// CommHistory includes
#include <CommHistory/commonutils.h>
#include <CommHistory/GroupModel>
#include <CommHistory/Group>

// Telepathy includes
#include <TelepathyQt/Constants>

// NGF-Qt includes
#include <NgfClient>

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
        , m_Storage(QDir::homePath() + COMMHISTORYD_NOTIFICATIONSSTORAGE)
        , m_Initialised(false)
        , m_GroupModel(0)
        , m_ngfClient(0)
        , m_ngfEvent(0)
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

    // creates data directory
    createDataDir();
    // Loads old state
    loadState();
    syncNotifications();

    CommHistoryService *service = CommHistoryService::instance();
    connect(service, SIGNAL(inboxObservedChanged(bool,QString)), SLOT(slotInboxObservedChanged()));
    connect(service, SIGNAL(callHistoryObservedChanged(bool)), SLOT(slotCallHistoryObservedChanged(bool)));
    connect(service, SIGNAL(observedConversationsChanged(QVariantList)),
                     SLOT(slotObservedConversationsChanged(QVariantList)));

    m_NotificationTimer.setSingleShot(true);
    m_NotificationTimer.setInterval(NOTIFICATION_THRESHOLD);
    connect(&m_NotificationTimer, SIGNAL(timeout()), this, SLOT(fireNotifications()));
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
        qWarning() << "Mismatch between meego groups and our groups:";
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
    DEBUG() << Q_FUNC_INFO << event.id() << channelTargetId << chatType;

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
                    DEBUG() << Q_FUNC_INFO << "Using chatName:" << chatName;
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

        TpContactUid cuid(event.localUid(),
                          event.remoteUid());
        if (event.remoteUid() == QLatin1String("<hidden>") // private number
                || !chatName.isEmpty()) {
            // Add notification immediately
            addNotification(notification);
        } else {
            DEBUG() << Q_FUNC_INFO << "Trying to resolve contact for" << event.localUid() << event.remoteUid();
            m_unresolvedEvents.enqueue(notification);
            m_contactListener->resolveContact(event.localUid(), event.remoteUid());
        }

        // XXX Shouldn't this be done when the notification is fired..?
        if (event.type() == CommHistory::Event::SMSEvent ||
            event.type() == CommHistory::Event::MMSEvent) {
            // ask mce to undim the screen
            QString mceMethod = QString::fromLatin1(MCE_DISPLAY_ON_REQ);
            QDBusMessage msg = QDBusMessage::createMethodCall(MCE_SERVICE, MCE_REQUEST_PATH, MCE_REQUEST_IF, mceMethod);
            QDBusConnection::systemBus().call(msg, QDBus::NoBlock);
        }
    } else {
        if (!m_ngfClient->isConnected())
            m_ngfClient->connect();

        if (!m_ngfEvent) {
            if (event.type() == CommHistory::Event::SMSEvent || event.type() == CommHistory::Event::MMSEvent)
                m_ngfEvent = m_ngfClient->play(QLatin1Literal("sms_fg"));
            else
                m_ngfEvent = m_ngfClient->play(QLatin1Literal("chat_fg"));
        }
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
    QMutableHashIterator<NotificationGroup, PersonalNotification> i(m_Notifications);
    while (i.hasNext()) {
        i.next();
        int eventType = i.value().eventType();

        // If removal should be done based on Inbox being observed then remove only those notifications
        // that belong to messaging-ui area:
        if (messagesOnly && (eventType != CommHistory::Event::IMEvent && eventType != CommHistory::Event::SMSEvent
             && eventType != CommHistory::Event::MMSEvent && eventType != VOICEMAIL_SMS_EVENT_TYPE)) {
            DEBUG() << Q_FUNC_INFO << "Skipping " << eventType << " type of notification";
            continue;
        }

        // Remove only a notification matching to the account:
        if (i.key().isValid() && MAP_MMS_TO_RING(i.value().account()) == accountPath) {
            DEBUG() << Q_FUNC_INFO << "Removing notification: accountPath: " << i.value().account() << " remoteUid: " << i.value().targetId();
            // record notification group id
            updatedGroups.insert(i.key());
            // no need to resolve events anymore -> remove personal notification from queue
            m_unresolvedEvents.removeAll(i.value());
            // at last, delete _i_
            i.remove();
        }
    }

    if (!updatedGroups.isEmpty()) {
        foreach(NotificationGroup group, updatedGroups)
            updateNotificationGroup(group);

        saveState();
    }
}

void NotificationManager::removeConversationNotifications(const QString &localUid,
                                                          const QString &remoteUid,
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
                && MAP_MMS_TO_RING(i.value().account()) == localUid
                && CommHistory::remoteAddressMatch(notificationRemoteUidStr,
                                                   remoteUid)
                && (CommHistory::Group::ChatType)(i.value().chatType()) == chatType) {
                updatedGroups.insert(i.key());
                i.remove();
            }
        }
    }

    if (!updatedGroups.isEmpty()) {
        foreach(NotificationGroup group, updatedGroups)
            updateNotificationGroup(group);
        saveState();
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
            DEBUG() << Q_FUNC_INFO << "Removing only notifications belonging to account " << filteredAccountPath;
            if (!filteredAccountPath.isEmpty())
                removeNotifications(filteredAccountPath, true);
        }
    }
}

void NotificationManager::slotCallHistoryObservedChanged(bool observed)
{
    if (observed) {
        if (removeNotificationGroup(CommHistory::Event::CallEvent))
            saveState();
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

bool NotificationManager::removeNotificationGroup(int type)
{
    DEBUG() << Q_FUNC_INFO << type;

    NotificationGroup group(type);
    removeGroup(type);

    return m_Notifications.remove(group) > 0;
}

NotificationGroup NotificationManager::notificationGroup(int type)
{
    NotificationGroup group(type);

    if (!m_MgtGroups.contains(type))
        addGroup(type);

    return group;
}

void NotificationManager::addNotification(PersonalNotification notification)
{
    DEBUG() << Q_FUNC_INFO;

    uint eventType = notification.eventType();

    NotificationGroup notificationgroup = notificationGroup(eventType);

    if(!notificationgroup.isValid()) {
        qWarning() << Q_FUNC_INFO << "Wrong notification group";
        return;
    }

    notification.setHasPendingEvents();

    m_Notifications.insertMulti(notificationgroup, notification);

    saveState();
}

void NotificationManager::fireNotifications()
{
    DEBUG() << Q_FUNC_INFO;

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

int NotificationManager::pendingEventCount()
{
    return m_unresolvedEvents.size();
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
}

void NotificationManager::showLatestNotification(const NotificationGroup &group,
                                                 PersonalNotification &notification)
{
    DEBUG() << Q_FUNC_INFO;

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
    DEBUG() << Q_FUNC_INFO;

    if (m_Notifications.contains(group)) {
        const PersonalNotification notification = m_Notifications.value(group);

        QString name;

        // update group notification
        bool grouped = countContacts(group) > 1;
        // get group message
        QString message = notificationGroupText(group, notification);
        // get group action
        QString groupAction = action(group, notification, grouped);

        ML10N::MLocale tempLocale;
        // update group
        if (group.type() != CommHistory::Event::VoicemailEvent && group.type() != VOICEMAIL_SMS_EVENT_TYPE)
            name = tempLocale.joinStringList( contactNames(group) );

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
    DEBUG() << Q_FUNC_INFO;

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

    DEBUG() << Q_FUNC_INFO << "Notification count with replace typed messages taken into account: " << pnsCounted.count();
    DEBUG() << Q_FUNC_INFO << "Absolute notification count: " << m_Notifications.count(group);

    return pnsCounted.count();
}

void NotificationManager::slotContactUpdated(quint32 localId, const QString &contactName,
                                             const QList<ContactListener::ContactAddress> &addresses)
{
#if 0
    if (localId == VoiceMailHandler::instance()->voiceMailContactId()) {
        // If changed contact is a voice mail one, then refresh its data in VoiceMailHandler:
        DEBUG() << Q_FUNC_INFO << "Voice mail contact changed!";
        VoiceMailHandler::instance()->fetchVoiceMailContact();
    }
#endif

    DEBUG() << Q_FUNC_INFO << localId << contactName;

    // Check all existing notifications and update if necessary
    QSet<NotificationGroup> updatedGroups;
    bool changed = false;
    for (QHash<NotificationGroup,PersonalNotification>::iterator it = m_Notifications.begin();
             it != m_Notifications.end(); it++) {
        if (!it.key().isValid())
            continue;

        PersonalNotification &notification = *it;
        if (ContactListener::addressMatchesList(notification.account(), notification.remoteUid(), addresses)) {
            DEBUG() << "Match existing notification" << notification.account() << notification.remoteUid();
            if (localId != notification.contactId()) {
                notification.setContactId(localId);
                notification.setContactName(contactName);
                changed = true;
            }
            updatedGroups << it.key();
        }
    }

    // Check all unresolved events for a match, and add notifications for any that do
    for (QList<PersonalNotification>::iterator it = m_unresolvedEvents.begin(); it != m_unresolvedEvents.end(); ) {
        PersonalNotification &notification = *it;

        // XXX Is this relevant here?
        if (notification.remoteUid().isEmpty() || !notification.chatName().isEmpty()) {
            addNotification(notification);
            it = m_unresolvedEvents.erase(it);
        } else if (ContactListener::addressMatchesList(notification.account(),
                    notification.remoteUid(), addresses)) {
            //set contact id for usage by notification key
            notification.setContactId(localId);
            notification.setContactName(contactName);
            DEBUG() << "Resolved contact for notification" << notification.account() << notification.remoteUid();
            addNotification(notification);
            it = m_unresolvedEvents.erase(it);
        } else {
            it++;
            continue;
        }
    }

    if (changed)
        saveState();

    foreach (const NotificationGroup &group, updatedGroups)
        updateNotificationGroup(group);

    fireNotifications();
}

void NotificationManager::slotContactRemoved(quint32 localId)
{
    DEBUG() << Q_FUNC_INFO << localId;

#if 0
    if (localId == VoiceMailHandler::instance()->voiceMailContactId()) {
        // If removed contact is a voice mail one, then clear its data from VoiceMailHandler:
        DEBUG() << Q_FUNC_INFO << "Voice mail contact removed!";
        VoiceMailHandler::instance()->clear();
        // Start listening vmc file changes in order to be notified about new voice mail contact addings.
        VoiceMailHandler::instance()->startObservingVmcFile();
    }
#endif

    // Check all existing notifications and update if necessary
    QSet<NotificationGroup> updatedGroups;
    for (QHash<NotificationGroup,PersonalNotification>::iterator it = m_Notifications.begin();
             it != m_Notifications.end(); it++) {
        if (!it.key().isValid())
            continue;

        if (it->contactId() == localId) {
            it->setContactId(0);
            it->setContactName(QString());
            updatedGroups << it.key();
        }
    }

    if (!updatedGroups.isEmpty())
        saveState();

    foreach (const NotificationGroup &group, updatedGroups)
        updateNotificationGroup(group);
}

void NotificationManager::slotContactUnknown(const QPair<QString,QString> &address)
{
    for (QList<PersonalNotification>::iterator it = m_unresolvedEvents.begin(); it != m_unresolvedEvents.end(); ) {
        PersonalNotification &notification = *it;

        if (address.first == notification.account() &&
                CommHistory::remoteAddressMatch(notification.remoteUid(), address.second)) {
            DEBUG() << "Unknown contact for notification" << notification.account() << notification.remoteUid();
            addNotification(notification);
            it = m_unresolvedEvents.erase(it);
        } else
            it++;
    }

    fireNotifications();
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
            DEBUG() << Q_FUNC_INFO << "Suppressing unneccessary notification update";
        }
    } else {
        qWarning() << Q_FUNC_INFO << "No group for event type" << eventType;
    }
}

void NotificationManager::saveState()
{
    DEBUG() << Q_FUNC_INFO;

    if( !openStorageFile(QIODevice::WriteOnly) ) {
        DEBUG() << "Cant open storage for saving";
        return;
    }

    m_Storage.reset();

    QDataStream out(&m_Storage);
    if( out.status() ) {
        DEBUG() << "Corrupted data file";
        return;
    }
    out.setVersion(QDataStream::Qt_4_6);
    out << m_Notifications;
    m_Storage.close();
}

void NotificationManager::loadState()
{
    if( !openStorageFile(QIODevice::ReadOnly) ) {
        DEBUG() << "Cant open storage for reading";
        return;
    }

    QDataStream in(&m_Storage);
    if( in.status() ) {
        DEBUG() << "Corrupted data file";
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
            DEBUG() << "Cannot open storage file";
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

QString NotificationManager::notificationName(const PersonalNotification &notification)
{
    DEBUG() << Q_FUNC_INFO;
    QString remoteUid = notification.remoteUid();

    if (!notification.chatName().isEmpty())
        return notification.chatName();
    else if (!notification.contactName().isEmpty())
        return notification.contactName();
    else if (remoteUid == QLatin1String("<hidden>"))
        return txt_qtn_call_type_private;
    else if (normalizePhoneNumber(remoteUid).isEmpty())
        return remoteUid;

    ML10N::MLocale locale;
    return locale.toLocalizedNumbers(remoteUid);
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
                            DEBUG() << Q_FUNC_INFO << "Changing chat name to" << newChatName;
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

void NotificationManager::slotNgfEventFinished(quint32 id)
{
    if (id == m_ngfEvent)
        m_ngfEvent = 0;
}

