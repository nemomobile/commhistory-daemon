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

#ifndef NOTIFICATIONMANAGER_H
#define NOTIFICATIONMANAGER_H

// QT includes
#include <QObject>
#include <QHash>
#include <QPair>
#include <QDBusInterface>
#include <QFile>
#include <QQueue>
#include <QTimer>
#include <QMultiHash>
#include <QModelIndex>

#include <QContact>
#include <QContactManager>
#include <QContactFetchRequest>
#include <QContactFilter>

#include <CommHistory/Event>
#include <CommHistory/Group>

// our includes
#include "notificationgroup.h"
#include "personalnotification.h"

QTCONTACTS_USE_NAMESPACE

class MNotificationGroup;

namespace CommHistory {
    class GroupModel;
}

namespace Ngf {
    class Client;
}

namespace RTComLogger {

class MWIListener;

typedef QPair<QString,QString> TpContactUid;

/*!
 * \class NotificationManager
 * \brief class responsible for showing notifications on desktop
 */
class NotificationManager : public QObject
{
    Q_OBJECT

public:
    /*!
     *  \param QObject parent object
     *  \returns Notification manager singleton
     */
    static NotificationManager* instance();

    /*!
     * \brief shows notification
     * \param event to be shown
     */
    void showNotification(const CommHistory::Event& event,
                          const QString &channelTargetId = QString(),
                          CommHistory::Group::ChatType chatType = CommHistory::Group::ChatTypeP2P);

    /*!
     * \brief removes notification
     * \param event type of the group to be removed
     * \returns whether NotificationGroup with event type existed
     */
    bool removeNotificationGroup(int type);

    /*!
     * \brief return group model with all conversations
     * \returns group model pointer
     */
    CommHistory::GroupModel* groupModel();

    /*!
     * \brief Show voicemail notification or removes it if count is 0
     * \param count number of voicemails if it's known,
     *              a negative number if the number is unknown
     */
    void showVoicemailNotification(int count);

    /*!
     * \brief Play class 0 SMS alert
     */
    void playClass0SMSAlert();

    /*!
     * \brief Get the QContactManager.
     */
    QContactManager* contactManager();

    int pendingRequestCount() const;

Q_SIGNALS:
    void pendingRequestCountChanged();

public Q_SLOTS:
    /*!
     * \brief Removes notifications belonging to a particular account having optionally certain remote uids.
     * \param accountPath Notifications of this account are to be removed.
     * \param messagesOnly Remove only messaging related notifications.
     */
    void removeNotifications(const QString &accountPath, bool messagesOnly = false);

private Q_SLOTS:
    /*!
     * Initialises notification manager instance
     */
    void init();
    void slotObservedConversationsChanged(const QVariantList &conversations);
    void slotInboxObservedChanged();
    void slotCallHistoryObservedChanged(bool observed);
    void slotResultsAvailable();
    void slotResultsAvailableForUnknown();
    void fireNotifications();
    void slotContactsAdded(const QList<QContactId> &contactIds);
    void slotContactsRemoved(const QList<QContactId> &contactIds);
    void slotContactsChanged(const QList<QContactId> &contactIds);
    void fireUnknownContactsRequest();
    void slotOnModelReady(bool status);
    void slotGroupRemoved(const QModelIndex &index, int start, int end);
    void slotMWICountChanged(int count);
    void slotContactRequestTimeout();
    void slotGroupDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void slotNgfEventFinished(quint32 id);

private:

    NotificationManager( QObject* parent = 0);
    ~NotificationManager();
    bool isCurrentlyObservedByUI(const CommHistory::Event& event,
                                 const QString &channelTargetId,
                                 CommHistory::Group::ChatType chatType);
    void addNotification(PersonalNotification notification);
    NotificationGroup notificationGroup(int type);

    void showLatestNotification(const NotificationGroup& group,
                                PersonalNotification& notification);
    int countContacts(const NotificationGroup& group);
    int countNotifications(const NotificationGroup& group);

    QString action(const NotificationGroup& group,
                   const PersonalNotification& notification,
                   bool grouped);
    QString notificationText(const CommHistory::Event& event);
    QString notificationGroupText(const NotificationGroup& group,
                                  const PersonalNotification& notification);
    static QString eventType(int type);
    void updateNotificationGroup(const NotificationGroup& group);

    /* actions */
    QString createActionInbox();
    QString createActionCallHistory();
    QString createActionConversation(const QString& accountPath,
                                     const QString& remoteUid,
                                     CommHistory::Group::ChatType chatType);
    QString createActionVoicemail();

    /* persistent notification support */
    void createDataDir();
    bool openStorageFile(QIODevice::OpenModeFlag flag);
    void saveState();
    void loadState();

    /* contacts fetching */
    void requestContact(TpContactUid contactUid);
    void resolveEvents();
    QString contactName(const QString &localUid, const QString &remoteUid);
    QStringList contactNames(const NotificationGroup& group);

    /* uses MeeGoTouch notification framework */
    void addGroup(int type);
    void updateGroup(int eventType,
                     int notificationCount,
                     const QString& contactName,
                     const QString& message,
                     const QString& action);
    void removeGroup(int type);

    void startNotificationTimer();
    void startContactsTimer();
    bool canShowNotification();

    void removeConversationNotifications(const QString &localId,
                                         const QString &remoteId,
                                         CommHistory::Group::ChatType chatType);

    QContactFetchRequest* startContactRequest(QContactFilter &filter,
                                              const char *resultSlot);
    void updateNotificationContacts(const QList<QContactId> &contactIds);
    bool hasMessageNotification() const;

    void syncNotifications();
    void clearPendingEvents(const NotificationGroup &group);
    void removeNotPendingEvents(const NotificationGroup &group);

    void clearContactsCache();
    QString notificationName(const PersonalNotification &notification);
    bool isFilteredInbox();
    QString filteredInboxAccountPath();
    bool updateEditedEvent(const CommHistory::Event& event);

private:
    static NotificationManager* m_pInstance;
    QMultiHash<NotificationGroup,PersonalNotification> m_Notifications;
    QHash<int, MNotificationGroup*> m_MgtGroups;
    QFile m_Storage;
    bool m_Initialised;

    QContactManager *m_pContactManager;
    QQueue<PersonalNotification> m_unresolvedEvents;
    QHash<TpContactUid, QContact> m_contacts;
    QHash<QContactFetchRequest*, TpContactUid> m_requests;

    // Delayed notifications
    QTimer m_NotificationTimer;

    CommHistory::GroupModel *m_GroupModel;
    // contact request for unknown/modified group contact
    QContactFilter m_ContactFilter;
    QTimer m_ContactsTimer;

    MWIListener *m_pMWIListener;
    Ngf::Client *m_ngfClient;
    quint32 m_ngfEvent;

#ifdef UNIT_TEST
    friend class Ut_NotificationManager;
#endif
};

} // namespace RTComLogger

#endif // NOTIFICATIONMANAGER_H
