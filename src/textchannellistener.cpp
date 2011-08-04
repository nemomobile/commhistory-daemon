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

// QT
#include <QDebug>
#include <QtDBus/QtDBus>

// MeegoTouch
#include <MNotification>

// libcommhistory
#include <CommHistory/EventModel>
#include <CommHistory/GroupModel>
#include <CommHistory/Event>
#include <CommHistory/Group>
#include <CommHistory/commonutils.h>
#include <CommHistory/ClassZeroSMSModel>
#include <CommHistory/SingleEventModel>
#include <CommHistory/TrackerIO>

// Telepathy
#include <TelepathyQt4/TextChannel>
#include <TelepathyQt4/Message>
#include <TelepathyQt4/Contact>
#include <TelepathyQt4/ContactManager>
#include <TelepathyQt4/PendingContacts>
#include <TelepathyQt4/Connection>
#include <TelepathyQt4/Properties>
#include <TelepathyQt4/Presence>
#include <TelepathyQt4/types.h>

#include <TpExtensions/Connection> // stored messages if
#include <TpExtensions/Constants> // Flash sms

// Contacts
#include <qmobilityglobal.h>
#include <qtcontacts.h>
#include <QVersitReader>
#include <QVersitContactImporter>

#include "textchannellistener.h"
#include "notificationmanager.h"
#include "locstrings.h"
#include "constants.h"

// LOCAL DEFINITIONS
// delivery report
#define DELIVERY_STATUS       QLatin1String("delivery-status")
#define DELIVERY_TOKEN        QLatin1String("delivery-token")
#define DELIVERY_TO           QLatin1String("delivery-to")
#define DELIVERY_DBUSERROR    QLatin1String("delivery-dbus-error")
#define DELIVERY_ERRORMESSAGE QLatin1String("delivery-error-message")
#define DELIVERY_ECHO         QLatin1String("delivery-echo")

// errors
#define MODEM_ERROR_SMSC_ADDRESS_NOT_AVAILABLE         "com.nokia.Modem.SMS.Errors.SMSCAddressNotAvailable"
#define MODEM_ERROR_DESTINATION_ADDRESS_FDN_RESTRICTED "com.nokia.Modem.SMS.Errors.DestinationAddressFDNRestricted"
#define MODEM_ERROR_SMS_ADDRESS_FDN_RESTRICTED         "com.nokia.Modem.SMS.Errors.SMSCAddressFDNRestricted"

// voicemail
#define TXT_VOICE            QLatin1String("voice")
#define MAILBOX_NOTIFICATION QLatin1String("x-nokia-mailbox-notification")
#define VOICEMAIL_TYPE       QLatin1String("x-nokia-voicemail-type")
#define MAILBOX_UNREAD_COUNT QLatin1String("x-nokia-mailbox-unread-count")
#define MAILBOX_HAS_UNREAD   QLatin1String("x-nokia-mailbox-has-unread")

// content
#define PART_CONTENT       QLatin1String("content")
#define PART_CONTENT_TYPE  QLatin1String("content-type")
#define TXT_CONTENT_TYPE   QLatin1String("text/plain")
#define VCARD_CONTENT_TYPE QLatin1String("text/x-vcard")

#define VCARD_EXTENSION   QLatin1String("vcf")

// MMS
#define MMS_MESSAGE_SUBJECT   QLatin1String("message-subject")
#define MMS_CONTENT_LOCATION  QLatin1String("x-mms-content-location")
#define MMS_MESSAGE_SIZE      QLatin1String("x-mms-message-size")
#define MMS_PART_TYPE         PART_CONTENT_TYPE
#define MMS_PART_CONTENT      PART_CONTENT
#define MMS_PART_LOCATION     QLatin1String("content-location")
#define MMS_PART_ID           QLatin1String("identifier")
#define MMS_MESSAGE_CC        QLatin1String("message-cc")
#define MMS_MESSAGE_BCC       QLatin1String("message-bcc")
#define MMS_MESSAGE_TO        QLatin1String("message-to")
#define MMS_ADDRESS_SEPARATOR QChar(';')
#define MMS_DEFAULT_FROM      QLatin1String("MMS")

// mms read report
#define MMS_MESSAGE_ID      QLatin1String("x-mms-message-id")
#define MMS_READ_REPORT     QLatin1String("x-mms-read-report")
#define MMS_READ_STATUS     QLatin1String("read-status")
#define MMS_READ_BY         QLatin1String("read-by")

#define PROTOCOL_TEL QLatin1String("tel")
#define PROTOCOL_MMS QLatin1String("mms")

// tp properties
#define CHANNEL_PROPERTY_NAME QLatin1String("name")
#define CHANNEL_PROPERTY_SUBJECT QLatin1String("subject")
#define CHANNEL_PROPERTY_SUBJECT_CONTACT QLatin1String("subject-contact")

#define MAX_SAVE_ATTEMPTS 3
#define RESAVE_INTERVAL 5000 //ms

using namespace RTComLogger;
QTM_USE_NAMESPACE;

namespace {

static CommHistory::Event::PropertySet deliveryHandlingProperties = CommHistory::Event::PropertySet()
                                                 << CommHistory::Event::Id
                                                 << CommHistory::Event::Type
                                                 << CommHistory::Event::StartTime
                                                 << CommHistory::Event::EndTime
                                                 << CommHistory::Event::Direction
                                                 << CommHistory::Event::IsRead
                                                 << CommHistory::Event::Status
                                                 << CommHistory::Event::LocalUid
                                                 << CommHistory::Event::RemoteUid
                                                 << CommHistory::Event::GroupId
                                                 << CommHistory::Event::MessageToken
                                                 << CommHistory::Event::Subject
                                                 << CommHistory::Event::FreeText
                                                 << CommHistory::Event::FromVCardFileName
                                                 << CommHistory::Event::FromVCardLabel
                                                 << CommHistory::Event::ContentLocation
                                                 << CommHistory::Event::MessageParts
                                                 << CommHistory::Event::ReportDelivery
                                                 << CommHistory::Event::ReadStatus
                                                 << CommHistory::Event::ReportReadRequested
                                                 << CommHistory::Event::MmsId
                                                 << CommHistory::Event::ReportRead
                                                 << CommHistory::Event::ParentId;


bool isVoicemail(const Tp::MessagePart &header)
{
    if (header.contains(MAILBOX_NOTIFICATION)) {
        QVariant mailboxVar = header.value(MAILBOX_NOTIFICATION).variant();
        if (mailboxVar.isValid())
            return (mailboxVar.value<QString>() == TXT_VOICE);
    }

    return false;
}

QString voicemailType(const Tp::MessagePart &header)
{
    if (header.contains(VOICEMAIL_TYPE)) {
        // check whether it's skype or sms voicemail, aka
        // "x-nokia-voicemail-type" is "skype" or "tel"
        // Both SMS and Skype voicemail notifications (x-nokia-mailbox-notification
        // with value "voice") MUST have this header
        QVariant typeVar = header.value(VOICEMAIL_TYPE).variant();
        if (typeVar.isValid())
            return typeVar.value<QString>();
    }

    return QString();
}

bool mailboxHasUnread(const Tp::MessagePart &header)
{
    if (header.contains(MAILBOX_HAS_UNREAD)) {
        QVariant typeVar = header.value(MAILBOX_HAS_UNREAD).variant();
        if (typeVar.isValid())
            return typeVar.value<bool>();
    }

    return false;
}

uint mailboxUnreadCount(const Tp::MessagePart &header)
{
    if (header.contains(MAILBOX_UNREAD_COUNT)) {
        QVariant countVar = header.value(MAILBOX_UNREAD_COUNT).variant();
        if (countVar.isValid())
            return countVar.value<uint>();
    }

    return 0;
}

} // anonymous namespace

TextChannelListener::TextChannelListener(const Tp::AccountPtr &account,
                                         const Tp::ChannelPtr &channel,
                                         const Tp::MethodInvocationContextPtr<> &context,
                                         QObject *parent)
    : ChannelListener(account, channel, context, parent),
      m_GroupModel(0),
      m_GroupRequested(false),
      m_ShowOfflineChatError(true),
      m_isClassZeroSMS(false),
      m_pClassZeroSMSModel(0),
      m_PropertiesIf(0),
      m_IsGroupChat(false),
      m_channelClosed(false),
      m_FailedSaveCount(0)
{
    qDebug() << __PRETTY_FUNCTION__;
    makeChannelReady(Tp::TextChannel::FeatureMessageQueue
                     | Tp::TextChannel::FeatureMessageSentSignal);
}

void TextChannelListener::channelListenerReady()
{
    qDebug() << __PRETTY_FUNCTION__;

    if (m_IsGroupChat)
        handleTpProperties();

    Tp::TextChannelPtr textChannel = Tp::TextChannelPtr::dynamicCast(m_Channel);

    if(!textChannel.isNull()) {
        connect( textChannel.data(),
                 SIGNAL( messageReceived(const Tp::ReceivedMessage&) ),
                 SLOT( slotMessageReceived(const Tp::ReceivedMessage&) ),
                 Qt::UniqueConnection);

        connect( textChannel.data(),
                 SIGNAL( messageSent(const Tp::Message&, Tp::MessageSendingFlags, const QString&) ),
                 SLOT( slotMessageSent(const Tp::Message&, Tp::MessageSendingFlags, const QString&) ),
                 Qt::UniqueConnection );

        // check if channel is meant to be used for class 0 sms messages
        QVariantMap properties = textChannel->immutableProperties();

        QVariant property = properties.value(QLatin1String(COMM_HISTORY_TP_INTERFACE_CHANNEL_INTERFACE_SMS ".Flash"), QVariant());
        if(property.isValid() && property.value<bool>() == true) {
            qDebug() << __FUNCTION__ << "Channel contains class 0 property";
            m_isClassZeroSMS = true;
            connect(classZeroSMSModel(),
                    SIGNAL(rowsAboutToBeRemoved(const QModelIndex&, int, int)),
                    SLOT(slotClassZeroSMSRemoved(const QModelIndex&, int, int)),
                    Qt::UniqueConnection);
        }

        handleMessages();
    } else {
        qCritical() << Q_FUNC_INFO << "Wrong channel - Null";
    }
    invocationContextFinished();
}

bool TextChannelListener::checkStoredMessagesIf()
{
    bool result = false;

    if (m_Connection.isNull() || !m_Connection->isReady()) {
        qCritical() << Q_FUNC_INFO << "Connection is not ready when it should be";
        return result;
    }

    if (m_Connection->hasInterface(CommHistoryTp::Client::ConnectionInterfaceStoredMessagesInterface::staticInterfaceName())) {
        eventModel().setSyncMode(true);
        result = true;
    }
    return result;
}

void TextChannelListener::requestConversationId()
{
    if (!m_GroupRequested) {
        m_GroupModel = NotificationManager::instance()->groupModel();
        if (m_GroupModel) {
            m_GroupRequested = true;

            connect(m_GroupModel, SIGNAL(rowsAboutToBeRemoved(const QModelIndex&,int,int)),
                    SLOT(slotGroupRemoved(const QModelIndex&,int,int)));
            connect(m_GroupModel, SIGNAL(dataChanged(const QModelIndex&,const QModelIndex&)),
                    SLOT(slotGroupDataChanged(const QModelIndex&,const QModelIndex&)));
            connect(m_GroupModel, SIGNAL(rowsInserted(const QModelIndex &, int, int)),
                    this, SLOT(slotGroupInserted(const QModelIndex &, int, int)));

            if (m_GroupModel->isReady()) {
                slotOnModelReady(true);
            } else {
                connect(m_GroupModel, SIGNAL(modelReady(bool)), SLOT(slotOnModelReady(bool)));
            }
        } else {
            qCritical() << "Failed to create group model";
        }
    }
}

TextChannelListener::~TextChannelListener()
{
}

void TextChannelListener::slotGroupDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    if (!m_Group.isValid())
        return;

    if (!topLeft.isValid() || !bottomRight.isValid()) {
        qWarning() << Q_FUNC_INFO << "Invalid indexes";
        return;
    }

    bool pendingGroupsHandled = false;

    for (int i = topLeft.row(); i <= bottomRight.row(); i++) {
        QModelIndex row = m_GroupModel->index(i, 0);
        CommHistory::Group group = m_GroupModel->group(row);
        if (group.isValid()) {
            if (m_pendingGroups.contains(group.id())) {
                pendingGroupsHandled = true;
                m_pendingGroups.removeAll(group.id());
            }

            if (m_Group.id() == group.id())
                m_Group = group;
        }
    }

    if (pendingGroupsHandled)
        handleMessages();

    tryToClose();
}

void TextChannelListener::slotGroupInserted(const QModelIndex &index, int start, int end)
{
    qDebug() << Q_FUNC_INFO << "Account path handled by this listener: " << MAP_MMS_TO_RING(m_Account->objectPath());
    qDebug() << Q_FUNC_INFO << "Target handled by this listener: " << targetId();

    for (int i = start; i <= end; i++) {
        QModelIndex row = m_GroupModel->index(i, 0, index);
        CommHistory::Group group = m_GroupModel->group(row);

        qDebug() << Q_FUNC_INFO << "Inserted group's account: " << group.localUid();
        qDebug() << Q_FUNC_INFO << "Inserted group's target: " << group.remoteUids().first();

        if (group.isValid()
            && group.localUid() == MAP_MMS_TO_RING(m_Account->objectPath())
            && CommHistory::remoteAddressMatch(group.remoteUids().first(),
                                               targetId())) {
            qDebug() << Q_FUNC_INFO << "found listener for group" << group.id();
            m_Group = group;
            break;
        }
    }
}

void TextChannelListener::slotGroupRemoved(const QModelIndex &index, int start, int end)
{
    qDebug() << Q_FUNC_INFO << "Account path handled by this listener: " << MAP_MMS_TO_RING(m_Account->objectPath());
    qDebug() << Q_FUNC_INFO << "Target handled by this listener: " << targetId();

    if (!m_Group.isValid())
    {
        qDebug() << Q_FUNC_INFO << "Group is not valid!";
        return;
    }

    for (int i = start; i <= end; i++) {
        QModelIndex row = m_GroupModel->index(i, 0, index);
        CommHistory::Group group = m_GroupModel->group(row);
        if (group == m_Group) {
            qDebug() << Q_FUNC_INFO << "Removed group belongs to this listener!";
            m_Group.setId(-1); // Invalidate the current group in this listener.
            break;
        }
    }
}

int TextChannelListener::groupIdForRecipient(const QString &remoteUid)
{
    int groupId = -1;
    // if group exist, read group id right away
    if (m_GroupModel->isReady()
        && m_GroupModel->rowCount() > 0
        && m_Account) {
        for (int row = 0; row < m_GroupModel->rowCount(); row++) {
            QModelIndex index = m_GroupModel->index(row, 0);
            CommHistory::Group group = m_GroupModel->group(index);
            if (group.isValid()
                && group.localUid() == MAP_MMS_TO_RING(m_Account->objectPath())
                && CommHistory::remoteAddressMatch(group.remoteUids().first(), remoteUid)) {
                groupId = group.id();
                qDebug() << Q_FUNC_INFO << "found existing group:" << groupId;
                break;
            }
        }

        if (groupId == -1) {
            // add a new group
            CommHistory::Group group;
            group.setLocalUid(MAP_MMS_TO_RING(m_Account->objectPath()));

            group.setRemoteUids(QStringList() << remoteUid);
            if (!m_GroupModel->addGroup(group)) {
                qCritical() << Q_FUNC_INFO << "error adding group";
            }
            else {
                groupId = group.id();
                qDebug() << Q_FUNC_INFO << "added new group:" << groupId;
            }
        }
    }
    return groupId;
}

int TextChannelListener::groupId()
{
    qDebug() << Q_FUNC_INFO;
    if (!m_Group.isValid()) {
        qDebug() << Q_FUNC_INFO << "Group is not valid!";

        if (m_GroupModel->isReady()
            && m_Account) { // m_Account not need to be ready

            // If account path if mms then we need to change it to ring because all groups are presented
            // with ring account info in db:
            CommHistory::Group group;
            group.setLocalUid(MAP_MMS_TO_RING(m_Account->objectPath()));

            QStringList remoteUids;
            qDebug() << Q_FUNC_INFO << targetId();
            remoteUids << targetId();
            group.setRemoteUids(remoteUids);

            if (m_IsGroupChat) {
                CommHistory::Group::ChatType chatType = CommHistory::Group::ChatTypeP2P;
                if (m_GroupHandleType == Tp::HandleTypeNone) {
                    chatType = CommHistory::Group::ChatTypeUnnamed;
                } else if (m_GroupHandleType == Tp::HandleTypeRoom) {
                    chatType = CommHistory::Group::ChatTypeRoom;
                }
                group.setChatType(chatType);

                if (!m_GroupChatName.isEmpty())
                    group.setChatName(m_GroupChatName);
            }

            if (!m_GroupModel->addGroup(group)) {

                qCritical() << Q_FUNC_INFO << "error adding group";
            }
            else {

                m_Group = group;
                qDebug() << Q_FUNC_INFO << "added new group:" << m_Group.id();
            }
        }
        else {

            qWarning() << "Group id is requested in invalid state";
        }
    }

    return m_Group.id();
}

void TextChannelListener::handleTpProperties()
{
    m_PropertiesIf = new Tp::Client::PropertiesInterfaceInterface(
        *(m_Channel->interface<Tp::Client::ChannelInterface>()));
    connect(m_PropertiesIf, SIGNAL(PropertiesChanged(const Tp::PropertyValueList &)),
            this, SLOT(slotPropertiesChanged(const Tp::PropertyValueList &)));
    QDBusPendingCall propertyCall = m_PropertiesIf->ListProperties();
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(propertyCall, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher *)),
            this, SLOT(slotListPropertiesFinished(QDBusPendingCallWatcher *)));
}

void TextChannelListener::slotListPropertiesFinished(QDBusPendingCallWatcher *watcher)
{
    qDebug() << Q_FUNC_INFO;

    QDBusPendingReply<Tp::PropertySpecList> reply = *watcher;
    if (!reply.isValid()) {
        qWarning() << Q_FUNC_INFO << "ListProperties failed:" << reply.error();
        watcher->deleteLater();
        return;
    }

    foreach (Tp::PropertySpec spec, reply.value())
        m_Properties.insert(spec.name, spec);

    Tp::UIntList propIds;
    if (m_Properties.contains(CHANNEL_PROPERTY_NAME)
        && (m_Properties.value(CHANNEL_PROPERTY_NAME).flags & Tp::PropertyFlagRead))
        propIds << m_Properties.value(CHANNEL_PROPERTY_NAME).propertyID;

    if (m_Properties.contains(CHANNEL_PROPERTY_SUBJECT)
        && (m_Properties.value(CHANNEL_PROPERTY_SUBJECT).flags & Tp::PropertyFlagRead))
        propIds << m_Properties.value(CHANNEL_PROPERTY_SUBJECT).propertyID;

    if (m_Properties.contains(CHANNEL_PROPERTY_SUBJECT_CONTACT)
        && (m_Properties.value(CHANNEL_PROPERTY_SUBJECT_CONTACT).flags & Tp::PropertyFlagRead))
        propIds << m_Properties.value(CHANNEL_PROPERTY_SUBJECT_CONTACT).propertyID;

    qDebug() << Q_FUNC_INFO << propIds;

    if (!propIds.isEmpty()) {
        QDBusPendingCall getPropertyCall = m_PropertiesIf->GetProperties(propIds);
        QDBusPendingCallWatcher *getWatcher = new QDBusPendingCallWatcher(getPropertyCall, this);
        connect(getWatcher, SIGNAL(finished(QDBusPendingCallWatcher *)),
                this, SLOT(slotGetPropertiesFinished(QDBusPendingCallWatcher *)));
    }

    watcher->deleteLater();
}

void TextChannelListener::slotGetPropertiesFinished(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<Tp::PropertyValueList> reply = *watcher;
    if (!reply.isValid()) {
        qDebug() << Q_FUNC_INFO << "GetProperties failed:" << reply.error();
        watcher->deleteLater();
        return;
    }
    slotPropertiesChanged(reply.value());
    watcher->deleteLater();
}

void TextChannelListener::slotOnModelReady(bool status)
{
    qDebug() << __PRETTY_FUNCTION__ << m_Account->objectPath() << targetId();

    disconnect(m_GroupModel, SIGNAL(modelReady(bool)),
               this, SLOT(slotOnModelReady(bool)));

    if (!status) {
        qCritical() << "Group model failed to load";
        return;
    }

    // if group exist, read group id right away
    // otherwise add a new group only when a new message(received/sent) comes
    if (m_GroupModel->rowCount() > 0
        && m_Account) {
        for (int row = 0; row < m_GroupModel->rowCount(); row++) {
            QModelIndex index = m_GroupModel->index(row, 0);
            CommHistory::Group group = m_GroupModel->group(index);
            if (group.isValid()
                && group.localUid() == MAP_MMS_TO_RING(m_Account->objectPath())
                && CommHistory::remoteAddressMatch(group.remoteUids().first(),
                                                   targetId())) {
                m_Group = group;

                qDebug() << Q_FUNC_INFO << "found existing group:" << m_Group.id();
                break;
            }
        }
    }

    channelListenerReady();
}

void TextChannelListener::slotMessageReceived(const Tp::ReceivedMessage &message)
{
    Q_UNUSED(message);
    handleMessages();
}

void TextChannelListener::handleMessages()
{
    QList<CommHistory::Event> scrollbackEvents;
    QList<CommHistory::Event> addEvents;
    QHash<int, QList<CommHistory::Event> > modifyEvents; // separate list for each group
    QList<Tp::ReceivedMessage> processedMessages;
    QList<Tp::ReceivedMessage> addMessages;
    QHash<int, QList<Tp::ReceivedMessage> > modifyMessages;
    // expunge tokens for committing events
    QHash<int, QMultiHash<int, QString> > modifyTokens;

    NotificationManager* nManager = NotificationManager::instance();

    Tp::TextChannelPtr textChannel = Tp::TextChannelPtr::dynamicCast(m_Channel);
    if (!textChannel) {
        qCritical() << "TextChannelListener has non text channel";
        return;
    }

    m_messageQueue << textChannel->messageQueue();
    Tp::TextChannelPtr textChannelPtr = Tp::TextChannelPtr::dynamicCast(m_Channel);
    if(!textChannelPtr.isNull()) {
        textChannelPtr->forget(textChannel->messageQueue());
    }

    foreach(Tp::ReceivedMessage message, m_messageQueue) {
        CommHistory::Event event;
        Tp::ChannelTextMessageType type = message.messageType();
        bool wait = false;

        switch (type) {
        case Tp::ChannelTextMessageTypeDeliveryReport: {
            DeliveryHandlingStatus status = handleDeliveryReport(message, event);
            switch (status) {
            case DeliveryHandlingResolved:
                if (m_pendingGroups.contains(event.groupId())) {
                    wait = true;
                    break;
                }

                if (event.isValid()) {
                    int groupId = event.groupId();
                    modifyEvents[groupId] << event;
                    modifyMessages[groupId] << message;

                    QString token = message.messageToken();
                    if (token.isEmpty())
                        token = event.messageToken();

                    modifyTokens[groupId].insertMulti(event.id(), token);

                } else {
                    // recovered message should be added
                    addEvents << event;
                    addMessages << message;
                    // expunging of added message will fail, but it's harmless
                    // and we don't need to expung this delivery report cause
                    // tp-ring do it automatically if the status is accepted
                }

                break;
            case DeliveryHandlingFailed:
                expungeMessage(message.messageToken());
                processedMessages << message;
                break;
            case DeliveryHandlingPending:
                wait = true;
                break;
            default:
                qCritical() << "Unknown DeliveryHandlingStatus" << status;
            }
            break;
        }
        case Tp::ChannelTextMessageTypeNormal: {
           CommHistory::Event mmsNotification;

           if (m_Account->protocolName() == PROTOCOL_MMS) {
               if (getEventForToken(message.messageToken(),
                                    QString(), // mmsId
                                    -1, // groupId could differ for mms notifications
                                    mmsNotification) == DeliveryHandlingPending) {
                   wait = true;
                   break;
               }
           }

            // fills event properties
            handleReceivedMessage(message, event);

            if (mmsNotification.isValid()) {
                qDebug() << __FUNCTION__ << "found MMS notification, overwriting";
                // overwrite MMS notification with downloaded message
                event.setId(mmsNotification.id());
                //notification may have "failed" status, so overwrite it with default value
                event.setStatus(CommHistory::Event::UnknownStatus);
                if (event.type() == CommHistory::Event::MMSEvent && event.remoteUid() == MMS_DEFAULT_FROM) {
                    // MMS has no message-from so keep notification's From and group
                    qDebug() << __FUNCTION__ << "MMS has no From. Copy From and groupId from notification";
                    event.setRemoteUid(mmsNotification.remoteUid()); // keep
                    event.setGroupId(mmsNotification.groupId());
                } else if (mmsNotification.groupId()!=event.groupId()) {
                    //MMS notification and real MMS are from different groups, so we need to update From and move it
                    mmsNotification.setRemoteUid(event.remoteUid());
                    if (!eventModel().moveEvent(mmsNotification, event.groupId())) {
                        qWarning() << "Move events failed" << event.id();
                    }
                }

                int groupId = event.groupId();
                modifyEvents[groupId] << event;
                modifyMessages[groupId] << message;

                QString token = message.messageToken();
                if (token.isEmpty())
                    token = event.messageToken();

                modifyTokens[groupId].insertMulti(event.id(), token);

            // class 0 sms
            } else if(m_isClassZeroSMS){
                // just ack message, expunge would be called
                // as soon as user reads message
                qDebug() << __FUNCTION__ << "Adding class 0 sms";
                processedMessages << message;
                classZeroSMSModel()->addEvent(event,true);
              // Normal sms
            } else {
                if (message.isScrollback()) {
                    scrollbackEvents << event;
                } else {
                    addEvents << event;
                }
                addMessages << message;

                if (event.direction() != CommHistory::Event::Outbound) {
                    nManager->showNotification(event, targetId(), m_Group.chatType());
                }
            }
            break;
        }
        case Tp::ChannelTextMessageTypeNotice: {
            // Telepathy can add a new type for mailbox notification in future
            Tp::MessagePart header = message.header();
            if (isVoicemail(header)) {
                QString type = voicemailType(header);

                if (type == PROTOCOL_TEL) {
                    int unread = 0;

                    if (mailboxHasUnread(header)) {
                        unread = mailboxUnreadCount(message.header());
                        if (unread == 0)
                            unread = -1; // set count to -1 if unread flag is set
                                         // but count is not available
                                         // as requiried by USIM MWI notation
                    }

                    nManager->showVoicemailNotification(unread);
                }
                // TODO skype voicemail support
            }
            expungeMessage(message.messageToken());
            processedMessages << message;
            break;
        }
        default:
            qDebug() << "onMessageReceived: type " << type << " not supported";
            break;
        }

        if (wait)
            break;
    }

    if (!scrollbackEvents.isEmpty()) {
        if (eventModel().addEvents(scrollbackEvents, true)) {
            processedMessages << addMessages;
        } else {
            qWarning() << "Adding events failed";
        }
    }

    if (!addEvents.isEmpty()) {
        if (eventModel().addEvents(addEvents)) {
            processedMessages << addMessages;
            foreach (CommHistory::Event e, addEvents)
                m_EventTokens.insertMulti(e.id(), e.messageToken());
        } else {
            qWarning() << "Adding events failed";
        }
    }

    if (!modifyEvents.isEmpty()) {
        QHash<int, QList<CommHistory::Event> >::iterator i;
        for (i = modifyEvents.begin(); i != modifyEvents.end(); ++i) {
            CommHistory::Group group = getGroupById(i.key());
            if (group.isValid() && eventModel().modifyEventsInGroup(i.value(), group)) {
                processedMessages << modifyMessages[i.key()];
                m_EventTokens += modifyTokens[i.key()];
            } else {
                qWarning() << "Modify events failed for group" << i.key();
            }
        }
    }

    foreach (Tp::ReceivedMessage message, processedMessages) {
        m_messageQueue.removeOne(message);
    }
}

bool TextChannelListener::recoverDeliveryEcho(const Tp::Message &message,
                                              CommHistory::Event &event)
{
    bool result = false;
    QVariant echoVar = message.header().value(DELIVERY_ECHO).variant();

    if (echoVar.isValid()) {
        Tp::MessagePartList parts = qdbus_cast<Tp::MessagePartList>(echoVar);
        if (parts.size() > 1) { // first one is header
            QString contentType = parts[1].value(PART_CONTENT_TYPE).variant().toString();
            QString content = parts[1].value(PART_CONTENT).variant().toString();

            if (contentType == TXT_CONTENT_TYPE) {
                event.setFreeText(content);
                result = true;
            } else if (contentType == VCARD_CONTENT_TYPE) {
                checkVCard(parts, event);
                result = true;
            }
        }
    }

    return result;
}

TextChannelListener::DeliveryHandlingStatus TextChannelListener::getEventForToken(const QString &token,
                                                                                  const QString &mmsId,
                                                                                  int groupId,
                                                                                  CommHistory::Event &event)
{
    DeliveryHandlingStatus result = DeliveryHandlingFailed;
    QString eventKey = token + "+" + mmsId;

    if (m_pendingEvents.contains(eventKey)) {
        CommHistory::SingleEventModel *model = m_pendingEvents.value(eventKey);

        if (!model) {
            m_pendingEvents.remove(eventKey);
        } else if (!model->isReady()) {
            result = DeliveryHandlingPending;
        } else {
            if (model->rowCount() > 0)
                event = model->event(model->index(0, 0));
            m_pendingEvents.remove(eventKey);
            model->deleteLater();
            result = DeliveryHandlingResolved;
        }
    } else {
        CommHistory::SingleEventModel *model = new CommHistory::SingleEventModel(this);
        model->setPropertyMask(deliveryHandlingProperties);
        if (model->getEventByTokens(token, mmsId, groupId)) {
            connect(model, SIGNAL(modelReady(bool)), SLOT(slotSingleModelReady(bool)));
            m_pendingEvents.insert(eventKey, model);
            result = DeliveryHandlingPending;
        } else {
            qWarning() << "Failed query single event model";
            delete model;
        }
    }

    return result;
}

TextChannelListener::DeliveryHandlingStatus TextChannelListener::handleDeliveryReport(const Tp::ReceivedMessage &message,
                                                                                      CommHistory::Event &event)
{
    DeliveryHandlingStatus result = DeliveryHandlingFailed;

    qDebug() << "[DELIVERY] Handling delivery report";
    if (message.messageToken().isNull()) {
        qWarning() << "[DELIVERY] Trying to handler delivery report, while message token is empty";
        return result;
    }

    // if we find message with the same token, update its status
    Tp::MessagePart header = message.header();
    QVariant tokenVar = header.value(DELIVERY_TOKEN).variant();

    QString deliveryToken;
    if (tokenVar.isValid()) {
        deliveryToken = tokenVar.value<QString>();
    } else {
        qWarning() << "[DELIVERY] Cannot fetch delivery token";
    }

    qDebug() << "[DELIVERY] Message token is: " << deliveryToken;

    QVariant mmsIdVar = header.value(MMS_MESSAGE_ID).variant();
    QString mmsId;
    if (mmsIdVar.isValid()) {
        mmsId = mmsIdVar.value<QString>();
        qDebug() << "[DELIVERY] Mms-id is: " << mmsId;
    }

    if (pendingCommit(deliveryToken)) {
        qDebug() << "[DELIVERY] Original message is not committed yet, wait for it";
        return DeliveryHandlingPending;
    }

    // get status
    QVariant status = header.value(DELIVERY_STATUS).variant();

    bool messageFound = false;
    if (!deliveryToken.isEmpty() || !mmsId.isEmpty()) {
        //use only delivery-token when status is "accepted"
        QString mmsIdForRequest = (status.isValid() && status.value<int>() == Tp::DeliveryStatusAccepted) ? QString() : mmsId;
        result = getEventForToken(deliveryToken, mmsIdForRequest, m_Group.id(), event);
        if (result != DeliveryHandlingResolved)
            return result;
        else
            messageFound = event.isValid();
    }

    // echo recovery
    if (!messageFound) {
        result = DeliveryHandlingFailed;
        messageFound = recoverDeliveryEcho(message, event);
        if (messageFound) {
            event.setMessageToken(deliveryToken);
            event.setType(eventType());
            event.setLocalUid(m_Account->objectPath());
            if (!m_isClassZeroSMS) {
                event.setGroupId(groupId());
            }
            event.setIsRead(true);
            event.setDirection(CommHistory::Event::Outbound);
            event.setRemoteUid(targetId());

            QDateTime sentTime = QDateTime::currentDateTime();
            event.setStartTime(sentTime);
            event.setEndTime(sentTime);

            event.setReportDelivery(true);

            qDebug() << "Message recovered from delivery-echo" << event.toString();
        }
    }


    if (!messageFound) {
        qWarning() << "[DELIVERY] Failed to fetch event by message token/id";
        return result;
    }

    qDebug() << "[DELIVERY] Event match: id:" << event.id()
             << "token:" << event.messageToken() << "mmsId:" << event.mmsId();

    // If variant is not valid, then we cant update status
    if (!status.isValid())
        return result;

    QDateTime deliveryTime;
    if (message.sent().isValid())
        deliveryTime = message.sent();
    else if (message.received().isValid())
        deliveryTime = message.received();
    else
        deliveryTime = QDateTime::currentDateTime();

    int deliveryStatus = status.value<int>();
    qDebug() << "[DELIVERY] Message delivery status: " << deliveryStatus;

    switch (deliveryStatus) {
    case Tp::DeliveryStatusDelivered: {
        event.setStatus(CommHistory::Event::DeliveredStatus);
        event.setStartTime(deliveryTime);

        break;
    }
    case Tp::DeliveryStatusAccepted: {
        event.setStatus(CommHistory::Event::SentStatus);
        event.setStartTime(deliveryTime);
        // set MMS message id
        if (!mmsId.isEmpty()) {
            event.setMmsId(mmsId);
        }

        break;
    }
    case Tp::DeliveryStatusTemporarilyFailed:
    case Tp::DeliveryStatusPermanentlyFailed: {
        // If sending fails, event is marked as temporarily failed
        // If delivery fails, event is marked as permanently failed
        event.setStartTime(deliveryTime);
        if (event.reportDelivery()
            && event.status() == CommHistory::Event::SentStatus) {
            event.setStatus(CommHistory::Event::PermanentlyFailedStatus);
        } else {
            event.setStatus(CommHistory::Event::TemporarilyFailedStatus);
        }
        handleMessageFailed(message, event);

        break;
    }
    case Tp::DeliveryStatusRead: {
        event.setStatus(CommHistory::Event::DeliveredStatus); // Message is read by recipient so it defenetelly delivered
        event.setStartTime(deliveryTime);
        event.setReadStatus(CommHistory::Event::ReadStatusRead);

        break;
    }
    case Tp::DeliveryStatusDeleted: {
        event.setStatus(CommHistory::Event::DeliveredStatus); // Message is read by recipient so it defenetelly delivered
        event.setStartTime(deliveryTime);
        event.setReadStatus(CommHistory::Event::ReadStatusDeleted);

        break;
    }
    default:
        // Unknown, cases are handled here.
        event.setStatus(CommHistory::Event::SendingStatus);

        break;
    }
    result = DeliveryHandlingResolved;

    return result;
}

void TextChannelListener::slotSingleModelReady(bool status)
{
    Q_UNUSED(status);
    qDebug() << Q_FUNC_INFO;

    CommHistory::SingleEventModel *request= qobject_cast<CommHistory::SingleEventModel*>(sender());

    if (m_sendMms.contains(request)) {
        QMutableHashIterator<CommHistory::SingleEventModel*, CommHistory::Event> i(m_sendMms);
        while (i.hasNext()) {
            i.next();
            CommHistory::SingleEventModel *model = i.key();
            if (model->isReady()) {
                bool addMessage = true;
                CommHistory::Event event = i.value();

                if (model->rowCount() > 0) {
                    CommHistory::Event oldEvent = model->event(model->index(0,0));
                    if (oldEvent.isValid()) {
                        event.setId(oldEvent.id());
                        CommHistory::Group group = getGroupById(event.groupId());
                        if (group.isValid() && eventModel().modifyEventsInGroup(
                                QList<CommHistory::Event>() << event, group)) {
                            m_EventTokens.insertMulti(event.id(), event.messageToken());
                            addMessage = false;
                        } else {
                            qWarning() << "Failed modify mms event";
                        }
                    }
                }
                i.remove();
                model->deleteLater();
                if (addMessage)
                    saveNewMessage(event);
            } else {
                break;
            }
        }
    } else {
        handleMessages();
    }

    tryToClose();
}

bool TextChannelListener::areRemotePartiesOffline()
{
    bool offline = true;
    QHashIterator<QString,Presence> presenceIt(m_PresenceStatuses);
    while (presenceIt.hasNext()) {
        if (presenceIt.next().value().first != Tp::Presence::offline().status()) {
            offline = false;
            break;
        }
    }
    return offline;
}

void TextChannelListener::handleMessageFailed(const Tp::ReceivedMessage &message,
                                              const CommHistory::Event &event)
{
    qDebug() << __PRETTY_FUNCTION__ << "message type:" << message.messageType();

    // if the received message is a delivery report
    if (message.messageType() == Tp::ChannelTextMessageTypeDeliveryReport) {

        // see if the message is actually an error message
        Tp::MessagePart part = message.header();
        int status = part.value(DELIVERY_STATUS).variant().toInt();
        QString messageToken = part.value(DELIVERY_TOKEN).variant().toString();
        QString dbusError = part.value(DELIVERY_DBUSERROR).variant().toString();
        QString errorMessage = part.value(DELIVERY_ERRORMESSAGE).variant().toString();

        qDebug() << "status:"        << status
                 << "message token:" << messageToken
                 << "dbus error:"    << dbusError
                 << "error message:" << errorMessage;

        // dont show notes for perm. failed mms messages
        if(event.type() == CommHistory::Event::MMSEvent &&
           status == Tp::DeliveryStatusPermanentlyFailed) {
            return;
        }

        if (status == Tp::DeliveryStatusTemporarilyFailed ||
            status == Tp::DeliveryStatusPermanentlyFailed) {

            QString recipient = !event.contactName().isEmpty() ?
                                    event.contactName() : event.remoteUid();
            // general error
            QString errorMsgToUser = txt_qtn_msg_error_sms_sending_failed(recipient);

            // check for specific error:
            // missing smsc
            if (dbusError == MODEM_ERROR_SMSC_ADDRESS_NOT_AVAILABLE) {

                errorMsgToUser = txt_qtn_msg_error_missing_smsc;
            }
            // fdn error
            else if (dbusError == MODEM_ERROR_DESTINATION_ADDRESS_FDN_RESTRICTED ||
                     dbusError == MODEM_ERROR_SMS_ADDRESS_FDN_RESTRICTED) {

                errorMsgToUser = txt_qtn_msg_error_fixed_dialing_active(recipient);
            }
            // offline chatting
            else if (event.type() == CommHistory::Event::IMEvent
                     && areRemotePartiesOffline()) {
                errorMsgToUser = txt_qtn_msg_general_does_not_support_offline;
            }

            qDebug() << "error message shown to user:" << errorMsgToUser;
            showErrorNote(errorMsgToUser);
        }
    }
}

void TextChannelListener::showErrorNote(const QString &errorMsg)
{
    if (!errorMsg.isEmpty()) {
        MNotification notification(ErrorBanner);
        notification.setBody(errorMsg);
        notification.publish();
    }
}

void TextChannelListener::parseMMSHeaders(const Tp::Message &message,
                                          CommHistory::Event &event)
{
    QString subject = message.header().value(MMS_MESSAGE_SUBJECT).variant().toString();
    if (!subject.isEmpty())
        event.setSubject(subject);
    event.setReportReadRequested(message.header().value(MMS_READ_REPORT).variant().toBool());
    event.setMmsId(message.header().value(MMS_MESSAGE_ID).variant().toString());

    event.setContentLocation(message.header().value(MMS_CONTENT_LOCATION).variant().toString());
    event.setBytesReceived(message.header().value(MMS_MESSAGE_SIZE).variant().toUInt());
    event.setCcList(message.header().value(MMS_MESSAGE_CC).variant().toString()
                    .split(MMS_ADDRESS_SEPARATOR, QString::SkipEmptyParts));
    event.setBccList(message.header().value(MMS_MESSAGE_BCC).variant().toString()
                     .split(MMS_ADDRESS_SEPARATOR, QString::SkipEmptyParts));

    event.setToList(message.header().value(MMS_MESSAGE_TO).variant().toString()
                     .split(MMS_ADDRESS_SEPARATOR, QString::SkipEmptyParts));

    // skip part 0, it's the message header
    for (int i = 1; i < message.size(); i++) {

        Tp::MessagePart part = message.part(i);
        CommHistory::MessagePart messagePart;
        messagePart.setContentId(part.value(MMS_PART_ID).variant().toString());
        messagePart.setContentType(part.value(MMS_PART_TYPE).variant().toString());
        messagePart.setPlainTextContent(part.value(MMS_PART_CONTENT).variant().toString());
        messagePart.setContentLocation(part.value(MMS_PART_LOCATION).variant().toString());

        event.addMessagePart(messagePart);
    }
}

CommHistory::Event::EventType TextChannelListener::eventType() const
{
    CommHistory::Event::EventType type = CommHistory::Event::UnknownType;
    if (m_Account->protocolName() == PROTOCOL_TEL) {
        if (m_isClassZeroSMS) {
            type = CommHistory::Event::ClassZeroSMSEvent;
        } else {
            type = CommHistory::Event::SMSEvent;
        }
    } else if (m_Account->protocolName() == PROTOCOL_MMS) {
        type = CommHistory::Event::MMSEvent;
    } else {
        type = CommHistory::Event::IMEvent;
    }
    return type;
}

void TextChannelListener::checkVCard(const Tp::MessagePartList &parts,
                                     CommHistory::Event &event)
{
    QByteArray vcard = fetchVCardFromMessage(parts);
    if (!vcard.isEmpty()) {
        QString filename;
        if (storeVCard(vcard, filename)) {
            qDebug() << "Stored vcard to file: " << filename;

            QString label = fetchContactLabelFromVCard(vcard);
            qDebug () << "Setting vcard with label: " << label;
            event.setFromVCard(filename, label);
        } else {
            qWarning () << "Failed to store the vcard.";
        }
    }
}

void TextChannelListener::fillEventFromMessage(const Tp::Message &message,
                                               CommHistory::Event &event)
{
    event.setType(eventType());

    if (event.type() == CommHistory::Event::MMSEvent)
        parseMMSHeaders(message, event);

    checkVCard(message.parts(), event);

    event.setLocalUid(m_Account->objectPath());
    event.setFreeText(message.text());

    // do not set / create group id for class0 messages
    if (!m_isClassZeroSMS) {
        event.setGroupId(groupId());
    }
}

void TextChannelListener::handleReceivedMessage(const Tp::ReceivedMessage &message,
                                                CommHistory::Event &event)
{
    QString remoteId;
    QString messageText = message.text();

    uint handle = message.sender()->handle().first();

    bool fromSelf = false;
    if (m_Connection && m_Connection->isReady() && m_Connection->selfHandle() == handle) {
        fromSelf = true;
        remoteId = targetId();
    } else if (m_HandleOwnerNames.contains(handle))
        remoteId = m_HandleOwnerNames.value(handle);
    else if (message.sender())
        remoteId = message.sender()->id();
    else {
        qWarning() << "Message sender is unknown, use target id";
        remoteId = targetId();
    }

    qDebug() << "Handling received message: " << remoteId << (fromSelf ? "<-" : "->")
             << m_Account->objectPath() << messageText;

    fillEventFromMessage(message, event);
    event.setRemoteUid(remoteId);

    if (fromSelf) {
        event.setDirection(CommHistory::Event::Outbound);
        event.setIsRead(true);
    } else {
        event.setDirection(CommHistory::Event::Inbound);
    }

    QDateTime receivedTime;
    if (message.received().isValid())
        receivedTime = message.received();
    else
        receivedTime = QDateTime::currentDateTime();

    QDateTime sentTime;
    if (message.sent().isValid())
        sentTime = message.sent();
    else
        sentTime = receivedTime;

    event.setStartTime(sentTime);
    event.setEndTime(receivedTime);

    event.setMessageToken(message.messageToken());
    qDebug() << "Message token is: " << message.messageToken();
}

void TextChannelListener::slotMessageSent(const Tp::Message &message,
                                        Tp::MessageSendingFlags flags,
                                        const QString &messageToken)
{
    QString messageText = message.text();
    QString remoteUid = targetId();

    if (remoteUid.isEmpty())
        qCritical() << "Empty target id";

    qDebug() << "Handling sent message: " << m_Account->objectPath() << "->" << remoteUid << messageText;

    CommHistory::Event event;
    fillEventFromMessage(message, event);
    event.setIsRead(true);
    event.setDirection(CommHistory::Event::Outbound);
    event.setRemoteUid(remoteUid);

    QDateTime sentTime;
    if (message.sent().isValid()) {
        sentTime = message.sent();
    } else {
        sentTime = QDateTime::currentDateTime();
    }
    // TODO: think about start/endtime...
    event.setStartTime(sentTime);
    event.setEndTime(sentTime);

    event.setMessageToken(messageToken);
    qDebug() << "Message token is: " << messageToken;

    // according to latest ui spec, sending status
    // should be set only for sms / mms messages
    if(m_Account->protocolName() == PROTOCOL_TEL
       || m_Account->protocolName() == PROTOCOL_MMS) {
        event.setStatus(CommHistory::Event::SendingStatus);
    }

    // check if Event is sms & Report_Delivery flag set
    if (flags.testFlag(Tp::MessageSendingFlagReportDelivery)) {
        event.setReportDelivery(true);
    }

    QStringList recipients;
    recipients <<  event.toList() << event.ccList() << event.bccList();
    if (recipients.isEmpty()) {
        recipients << event.remoteUid();
    }

    foreach (const QString& recipient, recipients) {
        bool addMessage = true;
        if (m_Account->protocolName() == PROTOCOL_MMS) {
            int groupId = groupIdForRecipient(recipient);

            event.setRemoteUid(recipient);
            event.setGroupId(groupId);

            CommHistory::SingleEventModel *request = new CommHistory::SingleEventModel(this);
            if (request->getEventByTokens(event.messageToken(),
                                          QString(), //mmsId
                                          groupId)) {
                m_sendMms.insert(request, event);
                connect(request, SIGNAL(modelReady(bool)), SLOT(slotSingleModelReady(bool)));
                addMessage = false;
            } else {
                qWarning() << "Failed query single event model";
                delete request;
            }
        }

        if (addMessage)
            saveNewMessage(event);
    }

    if (event.type() == CommHistory::Event::IMEvent
        && areRemotePartiesOffline()
        && m_ShowOfflineChatError) {
        if (!m_IsGroupChat) {
            showErrorNote(txt_qtn_msg_general_supports_offline);
        } else {
            showErrorNote(txt_qtn_msg_all_participants_offline);
        }
        m_ShowOfflineChatError = false;
    }
}

void TextChannelListener::saveNewMessage(CommHistory::Event &event)
{
    qDebug() << Q_FUNC_INFO << event.toString();

    if (eventModel().addEvent(event)) {
        m_pendingGroups.append(event.groupId());
        if (!event.messageToken().isEmpty())
            m_commitingEvents.insert(event.messageToken());
    } else {
        qWarning() << "failed to add event";
    }
}

void TextChannelListener::expungeMessage(const QString &token)
{
    if (eventModel().syncMode() && !token.isEmpty()) {
        if (m_expungeTokens.isEmpty()) {
            QTimer::singleShot(0, this, SLOT(slotExpungeMessages()));
        }
        m_expungeTokens.append(token);
    }
}

void TextChannelListener::updateGroupChatName(ChangedChannelProperty changedChannelProperty)
{
    qDebug() << Q_FUNC_INFO;

    if (changedChannelProperty == ChannelName)
        m_GroupChatName = m_ChannelName;

    else if (changedChannelProperty == ChannelSubject)
        m_GroupChatName = m_ChannelSubject;

    qDebug() << Q_FUNC_INFO << "New group chat name is" << m_GroupChatName;

    if (m_Group.isValid()) {

        qDebug() << Q_FUNC_INFO << "Current group chat name for this group is" << m_Group.chatName();

        if (m_GroupChatName != m_Group.chatName()) {

            qDebug() << Q_FUNC_INFO << "updating group chat name...";
            m_Group.setChatName(m_GroupChatName);

            if (m_GroupModel) {

                // Group is already in tracker
                CommHistory::Group modGroup;
                modGroup.setId(m_Group.id());
                modGroup.setChatName(m_Group.chatName());
                if (!m_GroupModel->modifyGroup(modGroup))
                    qCritical() << "failed to modify group in tracker";

                QString remoteId;
                // Here we need to figure out who changed the topic
                if (m_HandleOwnerNames.contains(m_ChannelSubjectContactHandle))
                    remoteId = m_HandleOwnerNames.value(m_ChannelSubjectContactHandle);

                else
                    foreach (Tp::ContactPtr contact, m_Channel->groupContacts())
                        if (contact != m_Channel->groupSelfContact()) {

                            // TODO: can contact have more than one handle in this case???
                            QList<uint> handles = contact->handle().toList();
                            if (handles.first() == m_ChannelSubjectContactHandle)
                                remoteId = contact->alias();
                        }


                qDebug() << Q_FUNC_INFO << "Chat room topic was changed by" << remoteId;

                // Create a temporary CommHistory::Event for showing chat room
                // topic change to the user in MUI conversation thread
                QString messageText;

                // if remoteId is empty, then YOU changed the topic
                if (remoteId.isEmpty()) {

                    // if new chat name is empty, then the topic was removed
                    if (m_GroupChatName.isEmpty())
                        messageText = txt_qtn_msg_group_chat_topic_cleared_user;

                    // otherwise a new topic was set
                    else
                        messageText = txt_qtn_msg_group_chat_topic_user_changed(m_GroupChatName);
                }
                // otherwise a remote party
                else {

                    if (m_GroupChatName.isEmpty())
                        messageText = txt_qtn_msg_group_chat_topic_cleared(remoteId);

                    else
                        messageText = txt_qtn_msg_group_chat_topic_changed(remoteId, m_GroupChatName);
                }

                sendGroupChatEvent(messageText);
            }
        }
    }
}

void TextChannelListener::sendGroupChatEvent(const QString &message)
{
    CommHistory::Event event;
    event.setType( CommHistory::Event::StatusMessageEvent );
    event.setDirection( CommHistory::Event::Inbound );
    event.setGroupId( m_Group.id() );
    event.setLocalUid( m_Account->objectPath() );
    event.setFreeText( message );
    event.setStartTime( QDateTime::currentDateTime() );
    event.setEndTime( QDateTime::currentDateTime() );
    event.setIsRead( true );

    if ( !eventModel().addEvent( event, true ) )
    {
        qDebug() << "*** Adding group chat event message to data model has been failed.";
     }
}

void TextChannelListener::slotEventsCommitted(QList<CommHistory::Event> events, bool status)
{
    qDebug() << Q_FUNC_INFO << status;

    bool removed = false;
    foreach (CommHistory::Event e, events) {
        if (m_EventTokens.contains(e.id())) {
            QString token = m_EventTokens.values(e.id()).last();
            if (status)
                expungeMessage(token);
            m_EventTokens.remove(e.id(), token);
        }
        if (m_commitingEvents.remove(e.messageToken()))
            removed = true;
    }

    if (!status) {
        qCritical() << "Failed to save message";
        // try to redeliver incoming messages
        if (!events.isEmpty()
            && events.first().direction() == CommHistory::Event::Inbound) {
            if (m_FailedSaveCount++ < MAX_SAVE_ATTEMPTS) {
                m_failedSaveEvents << events;
                QTimer::singleShot(m_FailedSaveCount*RESAVE_INTERVAL,
                                   this,
                                   SLOT(slotSaveFailedEvents()));
            } else {
               emit savingFailed(m_Connection);
           }
        }
    }

    // handle delivery reports pending for event commits
    if (removed)
        handleMessages();

    tryToClose();
}

void TextChannelListener::slotSaveFailedEvents()
{
    qDebug() << Q_FUNC_INFO;
    if (eventModel().addEvents(m_failedSaveEvents)) {
        foreach (CommHistory::Event e, m_failedSaveEvents)
            m_EventTokens.insertMulti(e.id(), e.messageToken());
    }
    m_failedSaveEvents.clear();
}

void TextChannelListener::slotExpungeMessages()
{
    if (m_expungeTokens.isEmpty()
        || (m_Connection && !m_Connection->isReady()))
        return;

    CommHistoryTp::Client::ConnectionInterfaceStoredMessagesInterface* storedMessages =
            m_Connection->interface<CommHistoryTp::Client::ConnectionInterfaceStoredMessagesInterface>();

    if (storedMessages) {
        qDebug() << Q_FUNC_INFO << m_expungeTokens;
        storedMessages->ExpungeMessages(m_expungeTokens);
        m_expungeTokens.clear();
    } else {
        qCritical() << Q_FUNC_INFO << "No stored messages interface present";
    }

    tryToClose();
}

QString TextChannelListener::fetchContactLabelFromVCard(const QByteArray &vcard)
{
    if (vcard.isEmpty())
        return QString();

    // Parse the input into QVersitDocument(s)
    QVersitReader reader(vcard);
    if (reader.startReading()) {
        reader.waitForFinished();

        // Import the QVersitDocument to a QContact
        QVersitContactImporter importer;
        if (importer.importDocuments(reader.results())) {
            QList<QContact> contacts = importer.contacts();

            if (!contacts.isEmpty()) {
                QContact contact = contacts.first();
                // Let's ensure the display label is set:
                QContactManager* manager = NotificationManager::instance()->contactManager();
                if (manager != 0) { // This should always be the case, but it's better to be cautious :)
                    manager->synthesizeContactDisplayLabel(&contact);
                }
                QString label = contact.displayLabel();
                if (label.isEmpty()) {
                    qWarning() << __PRETTY_FUNCTION__ << "The contact has an empty label, dispite our efforts.";
                }
                return label;
            }
        }
    }

    return QString();
}

QByteArray TextChannelListener::fetchVCardFromMessage(const Tp::MessagePartList &parts)
{
    for (int i = 0; i < parts.size (); ++i) {
        Tp::MessagePart part = parts.at(i);
        if (part.value(PART_CONTENT_TYPE).variant() == VCARD_CONTENT_TYPE) {
            qDebug() << Q_FUNC_INFO << "VCard from message:" << part.value(PART_CONTENT).variant().toByteArray();
            return part.value(PART_CONTENT).variant().toByteArray();
        }
    }

    return QByteArray();
}

bool TextChannelListener::storeVCard(const QByteArray &vcard, QString &name)
{
    QUuid uuid;
    QString dir_name = QDir::homePath() + "/"COMMHISTORYD_VCARDSDIR;
    QDir dir(dir_name);

    if (!dir.exists()) {
        if (!dir.mkpath(dir_name)) {
            qWarning() << "Could not create vcard directory.";
            return false;
        }
    }

    while (true) {
        uuid = QUuid::createUuid();

        name = QString("%1/%2.%3").arg(dir_name).arg(uuid).arg(VCARD_EXTENSION);

        QFileInfo info(name);
        if (!info.exists()) {
            break;
        } else {
            // We do this here only because we're reasonably sure that it won't
            // happen anyway.
            name.clear();
        }
    }

    QFile file(name);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Could not create vcard file:" << name;
        file.close();
        return false;
    }

    if (file.write(vcard) < 0) {

        qWarning() << "Could not write vcard data into file:" << name;
        file.close();
        return false;
    }
    file.close();

    return true;
}

void TextChannelListener::slotPresenceChanged(const Tp::Presence &presence)
{
    qDebug() << Q_FUNC_INFO;

    Tp::Contact *contact = qobject_cast<Tp::Contact *>(sender());
    if (!contact) {
        qWarning() << Q_FUNC_INFO << ": invalid contact";
        return;
    }

    Presence oldPresenceValues = m_PresenceStatuses.value(contact->id());
    m_PresenceStatuses.insert(contact->id(),Presence(presence.status(), presence.statusMessage()));

    /* If offline chat error has already been shown and presence status changed
       and we have at least one participant online after this status change
       then offline chat error is allowed to be shown next time: */
    if (!m_ShowOfflineChatError && (oldPresenceValues.first != presence.status()) && !areRemotePartiesOffline())
        m_ShowOfflineChatError = true;

    // Status message did not change, do not send status message event.
    if (oldPresenceValues.second == presence.statusMessage())
        return;

    QString remoteId;
    uint handle = contact->handle().first();
    if (m_HandleOwnerNames.contains(handle))
        remoteId = m_HandleOwnerNames.value(handle);
    else
        remoteId = contact->id();

    QString newStatusMessage = m_PresenceStatuses.value(contact->id()).second;

    if (!newStatusMessage.isEmpty() && groupId() != -1) {
        qDebug() << Q_FUNC_INFO << "Preparing status message event.";
        CommHistory::Event event;
        event.setType(CommHistory::Event::StatusMessageEvent);
        event.setDirection(CommHistory::Event::Inbound);
        event.setRemoteUid(remoteId);
        event.setGroupId(groupId());
        event.setLocalUid(m_Account->objectPath());
        event.setFreeText(newStatusMessage);
        event.setStartTime(QDateTime::currentDateTime());
        event.setEndTime(QDateTime::currentDateTime());
        event.setIsRead(true);

        if (!eventModel().addEvent(event, true)) {

            qDebug() << "*** Adding status message to data model has been failed.";
        }
    }
}

CommHistory::ClassZeroSMSModel* TextChannelListener::classZeroSMSModel()
{
    if (!m_pClassZeroSMSModel) {
        m_pClassZeroSMSModel = new CommHistory::ClassZeroSMSModel(this);
    }
    return m_pClassZeroSMSModel;
}

void TextChannelListener::slotClassZeroSMSRemoved(const QModelIndex& index,
                                                  int start,
                                                  int end)
{
    Q_UNUSED(index)

    qDebug() << "Class zero SMS removed from model, expunging";

    for (int row = start; row <= end; row++) {

        QModelIndex index = classZeroSMSModel()->index(row, 0);
        CommHistory::Event event = classZeroSMSModel()->event(index);
        if (event.isValid() && !event.messageToken().isEmpty()) {
            qDebug() << "Expunged message: " << event.messageToken();
            expungeMessage(event.messageToken());
        }
    }
}

void TextChannelListener::channelReady()
{
    qDebug() << Q_FUNC_INFO;

    if (m_Channel && m_Connection) {
        if (m_Channel->targetHandleType() == Tp::HandleTypeRoom) {
            qDebug() << Q_FUNC_INFO << "group chat: HandleTypeRoom";
            m_IsGroupChat = true;
            m_GroupHandleType = Tp::HandleTypeRoom;
        } else if (m_Channel->targetHandleType() == Tp::HandleTypeNone
                   && m_Channel->interfaces().contains(
                       TELEPATHY_INTERFACE_CHANNEL_INTERFACE_GROUP)) {
            m_IsGroupChat = true;
            m_GroupHandleType = Tp::HandleTypeNone;
            m_PersistentId = m_Channel->immutableProperties().value(
                TELEPATHY_CHANNEL_INTERFACE_PERSISTENT_ID).toString();
            qDebug() << Q_FUNC_INFO << "group chat: HandleTypeNone, PersistentId ="
                     << m_PersistentId;

            if (m_PersistentId.isEmpty()) {
                qCritical() << Q_FUNC_INFO << "No persistent id for Tp::HandleTypeNone groupchat";
                return;
            }
        }

        if (m_Channel->interfaces().contains(TELEPATHY_INTERFACE_CHANNEL_INTERFACE_GROUP))
            connect(m_Channel.data(),
                    SIGNAL(groupMembersChanged(const Tp::Contacts &,
                                               const Tp::Contacts &,
                                               const Tp::Contacts &,
                                               const Tp::Contacts &,
                                               const Tp::Channel::GroupMemberChangeDetails &)),
                    this,
                    SLOT(slotGroupMembersChanged(const Tp::Contacts &,
                                                 const Tp::Contacts &,
                                                 const Tp::Contacts &,
                                                 const Tp::Contacts &,
                                                 const Tp::Channel::GroupMemberChangeDetails &)));

        // request contact for target id to track presence
        Tp::ContactManagerPtr contactManager = m_Connection->contactManager();
        if (contactManager->supportedFeatures().contains(Tp::Contact::FeatureSimplePresence)) {
            Tp::UIntList handles;
            handles << m_Channel->targetHandle();

            Tp::Features features;
            features << Tp::Contact::FeatureSimplePresence;
            features << Tp::Contact::FeatureAlias;

            Tp::PendingContacts* pendingContacts = contactManager->contactsForHandles(handles,
                                                                                      features);
            connect(pendingContacts,
                    SIGNAL(finished(Tp::PendingOperation*)),
                    this,
                    SLOT(slotContactsReady(Tp::PendingOperation*)));

            if (m_IsGroupChat) {
                QList<Tp::ContactPtr> contactList;

                foreach (Tp::ContactPtr contact, m_Channel->groupContacts()) {
                    if (contact != m_Channel->groupSelfContact())
                        contactList << contact;
                }

                if (!contactList.isEmpty()) {
                    Tp::PendingContacts *pc = contactManager->upgradeContacts(contactList,
                                                                              features);
                    connect(pc, SIGNAL(finished(Tp::PendingOperation *)),
                            this, SLOT(slotContactsReady(Tp::PendingOperation *)));
                }

                if (m_Channel->groupAreHandleOwnersAvailable()) {
                    Tp::UIntList handleOwnerList;
                    foreach (Tp::ContactPtr contact, contactList) {
                        uint ownerHandle = m_Channel->groupHandleOwners().
                            value(contact->handle().first());
                        if (ownerHandle)
                            handleOwnerList << ownerHandle;
                    }

                    if (!handleOwnerList.isEmpty()) {
                        Tp::PendingContacts *owners =
                            contactManager->contactsForHandles(handleOwnerList);
                        connect(owners, SIGNAL(finished(Tp::PendingOperation *)),
                                this, SLOT(slotHandleOwnersFetched(Tp::PendingOperation *)));
                    }
                }

                connect(m_Channel.data(),
                        SIGNAL(groupHandleOwnersChanged(const Tp::HandleOwnerMap &,
                                                        const Tp::UIntList &,
                                                        const Tp::UIntList &)),
                        this,
                        SLOT(slotHandleOwnersChanged(const Tp::HandleOwnerMap &,
                                                     const Tp::UIntList &,
                                                     const Tp::UIntList &)));
            }
        }
        checkStoredMessagesIf();
        connect(&eventModel(), SIGNAL(eventsCommitted(QList<CommHistory::Event>,bool)),
                SLOT(slotEventsCommitted(QList<CommHistory::Event>,bool)),
                Qt::UniqueConnection);
        // call this last as it may start handle pending messages
        requestConversationId();
    } else {
        qCritical() << Q_FUNC_INFO << "Invalid channel: " << m_Channel.data()
                    << " or connection " << m_Connection.data();
    }
}

void TextChannelListener::slotContactsReady(Tp::PendingOperation* operation)
{
    qDebug() << Q_FUNC_INFO << channel();

    if (operation && operation->isError()) {
        qWarning() << "No presence contacts" << operation->errorMessage();
        return;
    }

    Tp::PendingContacts* pendingContacts = static_cast<Tp::PendingContacts*>(operation);
    if (pendingContacts) {
        QList<Tp::ContactPtr> contacts(pendingContacts->contacts());

        for ( int i = 0; i < contacts.count(); i++ )
        {
            // initialise presence values:
            if(!contacts.value(i).isNull()) {
                m_PresenceStatuses.insert(contacts.value(i)->id(),Presence(contacts.value(i)->presence().status(),
                                          contacts.value(i)->presence().statusMessage()));
            }

            connect(contacts.value(i).data(),
                    SIGNAL(presenceChanged(const Tp::Presence &)),
                    SLOT(slotPresenceChanged(const Tp::Presence &)));
        }
    } else {
        qCritical() << Q_FUNC_INFO << "Invalid TpPendingOperation when requesting target contact";
    }
}

void TextChannelListener::slotPropertiesChanged(const Tp::PropertyValueList &props)
{
    qDebug() << Q_FUNC_INFO;
    ChangedChannelProperty changedProperty = None;

    foreach (Tp::PropertyValue value, props) {
        if (m_Properties.contains(CHANNEL_PROPERTY_NAME)) {
            if (value.identifier == m_Properties.value(CHANNEL_PROPERTY_NAME).propertyID) {
                m_ChannelName = value.value.variant().toString();
                changedProperty = ChannelName;
                qDebug() << Q_FUNC_INFO << "name changed:" << m_ChannelName;
            }
        }
        if (m_Properties.contains(CHANNEL_PROPERTY_SUBJECT)) {
            if (value.identifier == m_Properties.value(CHANNEL_PROPERTY_SUBJECT).propertyID) {
                m_ChannelSubject = value.value.variant().toString();
                changedProperty = ChannelSubject;
                qDebug() << Q_FUNC_INFO << "subject changed:" << m_ChannelSubject;
            }
        }
        if (m_Properties.contains(CHANNEL_PROPERTY_SUBJECT_CONTACT)) {
            if (value.identifier == m_Properties.value(CHANNEL_PROPERTY_SUBJECT_CONTACT).propertyID) {
                m_ChannelSubjectContactHandle = value.value.variant().toUInt();
                qDebug() << Q_FUNC_INFO << "handle of the contact who changed the subject:" << m_ChannelSubjectContactHandle;
            }
        }
    }

    if (changedProperty != None)
        updateGroupChatName(changedProperty);
}

void TextChannelListener::slotGroupMembersChanged(
        const Tp::Contacts &groupMembersAdded,
        const Tp::Contacts &groupLocalPendingMembersAdded,
        const Tp::Contacts &groupRemotePendingMembersAdded,
        const Tp::Contacts &groupMembersRemoved,
        const Tp::Channel::GroupMemberChangeDetails &details)
{
    qDebug() << Q_FUNC_INFO;

    Q_UNUSED(groupLocalPendingMembersAdded);
    Q_UNUSED(groupRemotePendingMembersAdded);

    if (!groupMembersRemoved.isEmpty()) {

        foreach (Tp::ContactPtr contact, groupMembersRemoved) {

            // if there is valid originatior for the change
            // but the originator is not the same as the person removed
            // and the reason is kick/ban
            if (/*details.isValid() &&*/
                !details.actor().isNull() &&
                details.actor() != contact &&
                (details.reason() == Tp::ChannelGroupChangeReasonKicked ||
                 details.reason() == Tp::ChannelGroupChangeReasonBanned)) {

                if (contact == m_Channel->groupSelfContact()) {

                    qDebug() << "YOU've been banned/kicked by" << details.actor()->alias();
                    sendGroupChatEvent(txt_qtn_msg_group_chat_you_removed(details.actor()->alias()));
                }
                else {

                    qDebug() << contact->alias() << "has been banned/kicked by" << details.actor()->alias();
                    sendGroupChatEvent(txt_qtn_msg_group_chat_person_removed(details.actor()->alias(), contact->alias()));
                    m_PresenceStatuses.remove(contact->id());
                }
            }

            // otherwise fall back to normal _leave_
            else {

                qDebug() << contact->alias() << "has left the channel";
                sendGroupChatEvent(txt_qtn_msg_group_chat_remote_left(contact->alias()));
                m_PresenceStatuses.remove(contact->id());
            }
        }
    }

    if (!groupMembersAdded.isEmpty()) {

        Tp::Features features;
        features << Tp::Contact::FeatureSimplePresence;
        features << Tp::Contact::FeatureAlias;

        Tp::PendingContacts *pc = m_Channel->connection()->contactManager()->upgradeContacts(
            QList<Tp::ContactPtr>::fromSet(groupMembersAdded), features);

        connect(pc,
                SIGNAL(finished(Tp::PendingOperation *)),
                this,
                SLOT(slotContactsReady(Tp::PendingOperation *)));

        foreach (Tp::ContactPtr contact, groupMembersAdded) {

            // Ignore self contact. In that case "You have joined..." message
            // should be shown instead (by messaging-ui)
            if (contact != m_Channel->groupSelfContact()) {

                qDebug() << contact->alias() << "joined";
                sendGroupChatEvent(txt_qtn_msg_group_chat_remote_joined(contact->alias()));
            }
        }
    }
}

void TextChannelListener::slotHandleOwnersFetched(Tp::PendingOperation *op)
{
    Tp::PendingContacts *pc = qobject_cast<Tp::PendingContacts *>(op);

    if (pc && !pc->isError()) {
        foreach (Tp::ContactPtr handleOwner, pc->contacts()) {
            QMapIterator<uint, uint> i(m_Channel->groupHandleOwners());
            while (i.hasNext()) {
                i.next();
                if (i.value() == handleOwner->handle().first()) {
                    m_HandleOwnerNames.insert(i.key(), handleOwner->id());
                    qDebug() << Q_FUNC_INFO << "added handle owner:"
                             << i.key() << handleOwner->id();
                    break;
                }
            }
        }
    }
}

void TextChannelListener::slotHandleOwnersChanged(const Tp::HandleOwnerMap &handleOwnerMap,
                                                  const Tp::UIntList &added,
                                                  const Tp::UIntList &removed)
{
    if (!added.isEmpty()) {
        Tp::UIntList addedOwners;
        foreach (uint handle, added)
            addedOwners << handleOwnerMap.value(handle);
        Tp::PendingContacts *owners =
            m_Channel->connection()->contactManager()->contactsForHandles(addedOwners);
        connect(owners, SIGNAL(finished(Tp::PendingOperation *)),
                this, SLOT(slotHandleOwnersFetched(Tp::PendingOperation *)));
    }

    foreach (uint handle, removed)
        m_HandleOwnerNames.remove(handle);
}

bool TextChannelListener::hasPendingOperations() const
{
    return !(m_sendMms.isEmpty()
             && m_pendingEvents.isEmpty()
             && m_expungeTokens.isEmpty()
             && m_EventTokens.isEmpty()
             && m_pendingGroups.isEmpty()
             && m_failedSaveEvents.isEmpty());
}

void TextChannelListener::tryToClose()
{
    if (m_channelClosed && !hasPendingOperations()) {
        closed();
    }
}

void TextChannelListener::finishedWithError(const QString& errorName,
                                            const QString& errorMessage)
{
    qDebug() << Q_FUNC_INFO;
    if (!m_InvocationContext.isNull())
        m_InvocationContext->setFinishedWithError(errorName, errorMessage);

    m_channelClosed = true;

    tryToClose();
}

CommHistory::Group TextChannelListener::getGroupById(int groupId) const
{
    // Check most common case first
    if (m_Group.isValid() && m_Group.id() == groupId)
        return m_Group;

    if (!m_GroupModel || !m_GroupModel->isReady()) {
        qWarning() << Q_FUNC_INFO << "Can't read group model";
        return CommHistory::Group();
    }

    for (int row = 0; row < m_GroupModel->rowCount(); row++) {
        QModelIndex index = m_GroupModel->index(row, 0);
        CommHistory::Group group = m_GroupModel->group(index);
        if (group.isValid() && group.id() == groupId)
            return group;
    }

    qWarning() << Q_FUNC_INFO << "Didn't find matching group";
    return CommHistory::Group();
}

bool TextChannelListener::pendingCommit(const QString &messageToken)
{
    foreach (QString token, m_commitingEvents) {
        if (token == messageToken)
            return true;
    }

    return false;
}
