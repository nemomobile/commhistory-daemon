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

#ifndef TEXT_CHANNEL_LISTENER_H
#define TEXT_CHANNEL_LISTENER_H

#include <QList>
#include <QMultiHash>

#include <CommHistory/Group>

#include "channellistener.h"

namespace CommHistory {
    class GroupModel;
    class Event;
    class ClassZeroSMSModel;
    class SingleEventModel;
}

namespace Tp {
    class Message;
    class ReceivedMessage;
    class PendingOperation;

    namespace Client {
        class PropertiesInterfaceInterface;
    }
}

namespace RTComLogger
{
/*!
 * \class TextChannelListener
 * \brief class responsible for listening and logging activity on a text channel
 * chats, sms
 */
class TextChannelListener : public ChannelListener
{
    Q_OBJECT

public:

    TextChannelListener(const Tp::AccountPtr &account,
                        const Tp::ChannelPtr &channel,
                        const Tp::MethodInvocationContextPtr<> &context,
                        QObject *parent = 0);

    virtual ~TextChannelListener();

private Q_SLOTS:
    void slotMessageReceived(const Tp::ReceivedMessage &message);
    void slotMessageSent(const Tp::Message &message,
                       Tp::MessageSendingFlags flags,
                       const QString &messageToken);
    void slotOnModelReady();
    void slotSimplePresenceChanged(const QString &, uint, const QString &);
    void slotGroupRemoved(const QModelIndex &index, int start, int end);
    void slotGroupInserted(const QModelIndex &index, int start, int end);
    void slotGroupDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void slotEventsCommitted(QList<CommHistory::Event> events, bool status);
    void slotClassZeroSMSRemoved(const QModelIndex&, int, int);
    void slotContactsReady(Tp::PendingOperation* operation);
    void slotPropertiesChanged(const Tp::PropertyValueList &props);
    void slotGroupMembersChanged(const Tp::Contacts &groupMembersAdded,
                                 const Tp::Contacts &groupLocalPendingMembersAdded,
                                 const Tp::Contacts &groupRemotePendingMembersAdded,
                                 const Tp::Contacts &groupMembersRemoved,
                                 const Tp::Channel::GroupMemberChangeDetails &details);
    void slotHandleOwnersFetched(Tp::PendingOperation *op);
    void slotHandleOwnersChanged(const Tp::HandleOwnerMap &handleOwnerMap,
                                 const Tp::UIntList &added,
                                 const Tp::UIntList &removed);
    void slotListPropertiesFinished(QDBusPendingCallWatcher *watcher);
    void slotGetPropertiesFinished(QDBusPendingCallWatcher *watcher);
    void slotExpungeMessages();
    void slotSingleModelReady();

private:

    enum ChangedChannelProperty {
        None,
        ChannelName,
        ChannelSubject
    };

    enum DeliveryHandlingStatus {
        DeliveryHandlingFailed,
        DeliveryHandlingResolved,
        DeliveryHandlingPending
    };

    void channelReady();
    void channelListenerReady();
    void requestConversationId();
    int groupId();
    int groupIdForRecipient(const QString& remoteUid);
    void handleTpProperties();

    // delivery report
    DeliveryHandlingStatus handleDeliveryReport(const Tp::ReceivedMessage &message,
                                                CommHistory::Event &event);
    // MMS
    void parseMMSHeaders(const Tp::Message &message, CommHistory::Event &event);
    // normal message
    void fillEventFromMessage(const Tp::Message &message, CommHistory::Event &event);
    void handleReceivedMessage(const Tp::ReceivedMessage &message,
                               CommHistory::Event &event);

    void handleMessages();
    QString fetchContactLabelFromVCard(const QString &vcard);
    QString fetchVCardFromMessage(const Tp::MessagePartList &parts);
    bool storeVCard (const QString &vcard, QString &name);
    bool checkStoredMessagesIf();
    void expungeMessage(const QString &token);
    void updateGroupChatName(ChangedChannelProperty changedChannelProperty);

    /*! returns model, if model doesnt exist, creates one */
    CommHistory::ClassZeroSMSModel* classZeroSMSModel();

    void fetchContacts();
    void handleMessageFailed(const Tp::ReceivedMessage &message,
                             const CommHistory::Event &event);
    void sendGroupChatEvent(const QString &message);
    void showErrorNote(const QString &errorMsg);

    // attempt to read original message from delivery report
    bool recoverDeliveryEcho(const Tp::Message &message, CommHistory::Event &event);

    CommHistory::Event::EventType eventType() const;
    void checkVCard(const Tp::MessagePartList &parts, CommHistory::Event &event);
    DeliveryHandlingStatus getEventForToken(const QString &token,
                                            const QString &mmsId,
                                            int groupId,
                                            CommHistory::Event &event);

    void saveNewMessage(CommHistory::Event &event);

private:

    // TODO: only for 1-1 chat, should be fixed later
    Tp::ContactPtr m_TargetContact;

    CommHistory::GroupModel *m_GroupModel;
    CommHistory::Group m_Group;
    bool m_GroupRequested;

    // tokens for expunging
    QList<QString> m_expungeTokens;
    // map event id to tokens that should be expunged,
    // Event does not have report delivery token, therefore it's stored here
    // until events are committed
    QMultiHash<int, QString> m_EventTokens;

    bool m_ShowOfflineChatError;
    QString m_TpContactStatusMessage;
    QString m_TpContactPresenceStatus;

    // indicates that channel contains class0 messages
    bool m_isClassZeroSMS;
    CommHistory::ClassZeroSMSModel *m_pClassZeroSMSModel;

    Tp::HandleIdentifierMap m_HandleOwnerNames;
    Tp::Client::PropertiesInterfaceInterface *m_PropertiesIf;
    QHash<QString, Tp::PropertySpec> m_Properties;

    bool m_IsGroupChat;
    uint m_GroupHandleType;
    QString m_GroupChatName;
    QString m_ChannelName;
    QString m_ChannelSubject;
    uint m_ChannelSubjectContactHandle;
    QString m_PersistentId;

    QHash<QString, CommHistory::SingleEventModel*> m_pendingEvents;
    QHash<CommHistory::SingleEventModel*, CommHistory::Event> m_sendMms; //serately handle mms saving as it needs event lookup before saving

#ifdef UNIT_TEST
    friend class Ut_TextChannelListener;
#endif
};

} // namespave RTComLogger

#endif // CHANNEL_LISTENER_H
