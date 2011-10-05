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

// INCLUDES
#include "ut_textchannellistener.h"

// Qt includes
#include <QDebug>
#include <QTest>
#include <QTime>
#include <QSignalSpy>
#include <QUuid>

#include "TelepathyQt4/types.h"
#include "TelepathyQt4/account.h"
#include "TelepathyQt4/text-channel.h"
#include "TelepathyQt4/message.h"
#include "TelepathyQt4/connection.h"
#include "TelepathyQt4/contact-manager.h"

#include "TpExtensions/cli-connection.h" // stored messages if

#include <CommHistory/GroupModel>
#include <CommHistory/SingleEventModel>

#include "textchannellistener.h"
#include "notificationmanager.h"

// constants
#define IM_USERNAME QLatin1String("dut@localhost")
#define SENT_MESSAGE QLatin1String("Hello, how is life mon?")
#define RECEIVED_MESSAGE QLatin1String("Good, good")
#define IM_ACCOUNT_PATH QLatin1String("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0")
#define SMS_ACCOUNT_PATH QLatin1String("/org/freedesktop/Telepathy/Account/ring/tel/ring")
#define SMS_NUMBER QLatin1String("+358987654321")
#define IM_CHANNEL_PATH QLatin1String("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0")
#define SMS_CHANNEL_PATH QLatin1String("/org/freedesktop/Telepathy/Connection/ring/tel/ring/text0")
#define TARGET_HANDLE 1
#define SELF_HANDLE 0
#define IM_REMOTE_ID QLatin1String("td@jabber.org")

#define VCARD_CONTENT QLatin1String("BEGIN:VCARD\n" \
                                    "VERSION:2.1\n" \
                                    "N;CHARSET=ISO-8859-1;ENCODING=QUOTED-PRINTABLE:ABcd 123\n" \
                                    "TEL;PREF:12345678\n" \
                                    "END:VCARD")
#define VCARD_NAME QLatin1String("ABcd 123")

using namespace RTComLogger;

namespace {
    static uint pendingMessageId = 0;

    bool waitSignal(QSignalSpy &spy, int msec)
    {
        QTime timer;
        timer.start();
        while (timer.elapsed() < msec && spy.isEmpty())
            QCoreApplication::processEvents();

        return !spy.isEmpty();
    }

    void waitInvocationContext(Tp::MethodInvocationContextPtr<> &ctx, int msec)
    {
        QTime timer;
        timer.start();
        while (timer.elapsed() < msec && !ctx->isFinished())
            QCoreApplication::processEvents();
    }

    template<typename T>
    void addMsgHeader(Tp::Message &msg, int index, const char *key, T value) {
        msg.ut_part(index).insert(QLatin1String(key),
                                  QDBusVariant(value));
    }

    template<const char*>
    void addMsgHeader(Tp::Message &msg, int index, const char *key, const char* value) {
        msg.ut_part(index).insert(QLatin1String(key),
                                  QDBusVariant(QLatin1String(value)));
    }
}

Ut_TextChannelListener::Ut_TextChannelListener()
{
}

Ut_TextChannelListener::~Ut_TextChannelListener()
{
}

/*!
 * This function will be called before the first testfunction is executed.
 */
void Ut_TextChannelListener::initTestCase()
{
    qRegisterMetaType<Tp::PendingOperation*>("Tp::PendingOperation*");
}

/*!
 * This function will be called after the last testfunction was executed.
 */
void Ut_TextChannelListener::cleanupTestCase()
{
}

/*!
 * This function will be called before each testfunction is executed.
 */
void Ut_TextChannelListener::init()
{
}

/*!
 * This unction will be called after every testfunction.
 */
void Ut_TextChannelListener::cleanup()
{
}

CommHistory::Group Ut_TextChannelListener::fetchGroup(const QString &localUid,
                                                      const QString &remoteUid,
                                                      bool wait)
{
    CommHistory::GroupModel model;

    QSignalSpy modelReady(&model, SIGNAL(modelReady(bool)));
    model.getGroups(localUid, remoteUid);

    if (!waitSignal(modelReady, 5000))
        goto end;

    qRegisterMetaType<QModelIndex>("QModelIndex");
    if (!model.rowCount() && wait) {
        QSignalSpy rowInsert(&model, SIGNAL(rowsInserted(const QModelIndex &, int, int)));
        if (!waitSignal(rowInsert, 5000))
            goto end;
    }

    if (model.rowCount() == 1)
        return model.group(model.index(0, 0));

end:
    qWarning() << Q_FUNC_INFO << "Failed to fetch group" << localUid << remoteUid;
    return CommHistory::Group();
}

CommHistory::Event Ut_TextChannelListener::fetchEvent(int eventId)
{
    CommHistory::SingleEventModel model;
    QSignalSpy modelReady(&model, SIGNAL(modelReady(bool)));

    model.getEventByUri(CommHistory::Event::idToUrl(eventId));
    if(!waitSignal(modelReady, 5000))
        goto end;

    if(model.rowCount())
        return model.event(model.index(0, 0));

end:
    qWarning() << Q_FUNC_INFO << "Failed to fetch event" << eventId;
    return CommHistory::Event();
}

void Ut_TextChannelListener::imSending()
{
    QString message = QString(SENT_MESSAGE) + QString(" : ") + QTime::currentTime().toString(Qt::ISODate);

    NotificationManager *nm = NotificationManager::instance();
    QVERIFY(nm);
    nm->postedNotifications.clear();

    // setup connection
    Tp::ConnectionPtr conn(new Tp::Connection());
    conn->ut_setIsReady(true);

    Tp::AccountPtr acc(new Tp::Account(conn, IM_ACCOUNT_PATH));

    //setup channel
    Tp::ChannelPtr ch(new Tp::TextChannel(IM_CHANNEL_PATH));
    ch->ut_setIsRequested(true);
    ch->ut_setTargetHandleType(Tp::HandleTypeContact);
    ch->ut_setTargetHandle(TARGET_HANDLE);
    QVariantMap immProp;
    immProp.insert(TELEPATHY_INTERFACE_CHANNEL ".TargetID", IM_USERNAME);
    ch->ut_setImmutableProperties(immProp);
    ch->ut_setConnection(conn);

    Tp::MethodInvocationContextPtr<> ctx(new Tp::MethodInvocationContext<>());

    TextChannelListener tcl(acc, ch, ctx);
    waitInvocationContext(ctx, 5000);

    QVERIFY(ctx->isFinished());
    QVERIFY(!ctx->isError());

    // send sent message
    uint timestamp = QDateTime::currentDateTime().toTime_t();
    Tp::Message msg(timestamp, (uint)Tp::ChannelTextMessageTypeNormal, message);
    QString token = QUuid::createUuid().toString();
    Tp::TextChannelPtr::dynamicCast(ch)->ut_sendMessage(msg, Tp::MessageSendingFlagReportDelivery, token);

    QSignalSpy eventCommitted(&tcl.eventModel(), SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));
    QVERIFY(waitSignal(eventCommitted, 5000));

    CommHistory::Group g = fetchGroup(IM_ACCOUNT_PATH, IM_USERNAME, true);

    QVERIFY(g.isValid());
    QCOMPARE(g.localUid(), IM_ACCOUNT_PATH);
    QCOMPARE(g.remoteUids().first(), IM_USERNAME);
    QCOMPARE(g.lastMessageText(), message);
    QCOMPARE(g.lastEventType(), CommHistory::Event::IMEvent);

    CommHistory::Event e = fetchEvent(g.lastEventId());
    QCOMPARE(e.direction(), CommHistory::Event::Outbound);
    QCOMPARE(e.freeText(), message);
    QCOMPARE(e.startTime().toTime_t(), timestamp);
    QCOMPARE(e.endTime().toTime_t(), timestamp);
    QCOMPARE(e.messageToken(), token);

    QCOMPARE(nm->postedNotifications.size(), 0);
}

void Ut_TextChannelListener::receiving_data()
{
    QTest::addColumn<QString>("accountPath");
    QTest::addColumn<QString>("username");
    QTest::addColumn<QString>("messageBase");
    QTest::addColumn<QString>("channelPath");
    QTest::addColumn<bool>("cellular");

    QTest::newRow("IM") << QString(IM_ACCOUNT_PATH)
            << QString(IM_USERNAME)
            << QString(RECEIVED_MESSAGE)
            << QString(IM_CHANNEL_PATH)
            << false;
    QTest::newRow("SMS") << QString(SMS_ACCOUNT_PATH)
            << QString(SMS_NUMBER)
            << QString(RECEIVED_MESSAGE)
            << QString(SMS_CHANNEL_PATH)
            << true;
}

void Ut_TextChannelListener::receiving()
{
    QFETCH(QString, accountPath);
    QFETCH(QString, username);
    QFETCH(QString, messageBase);
    QFETCH(QString, channelPath);
    QFETCH(bool, cellular);

    QString message = messageBase + QString(" : ") + QLatin1String(cellular?" cell ":" notcell ")
                      + QTime::currentTime().toString(Qt::ISODate);

    NotificationManager *nm = NotificationManager::instance();
    QVERIFY(nm);
    nm->postedNotifications.clear();

    // setup connection
    Tp::ConnectionPtr conn(new Tp::Connection());
    conn->ut_setIsReady(true);
    if (cellular)
        conn->ut_setInterfaces(QStringList() << CommHistoryTp::Client::ConnectionInterfaceStoredMessagesInterface::staticInterfaceName());

    //setup account
    Tp::AccountPtr acc(new Tp::Account(conn, accountPath));
    if (cellular)
        acc->ut_setProtocolName("tel");

    //setup channel
    Tp::ChannelPtr ch(new Tp::TextChannel(channelPath));
    ch->ut_setIsRequested(false);
    ch->ut_setTargetHandleType(Tp::HandleTypeContact);
    ch->ut_setTargetHandle(TARGET_HANDLE);
    QVariantMap immProp;
    immProp.insert(TELEPATHY_INTERFACE_CHANNEL ".TargetID", username);
    ch->ut_setImmutableProperties(immProp);
    ch->ut_setConnection(conn);

    Tp::MethodInvocationContextPtr<> ctx(new Tp::MethodInvocationContext<>());

    TextChannelListener tcl(acc, ch, ctx);
    waitInvocationContext(ctx, 5000);

    QVERIFY(ctx->isFinished());
    QVERIFY(!ctx->isError());

    // send received message
    Tp::ReceivedMessage msg(Tp::MessagePartList() << Tp::MessagePart() << Tp::MessagePart());

    uint timestamp = QDateTime::currentDateTime().toTime_t();
    addMsgHeader(msg, 0, "pending-message-id", pendingMessageId++);
    addMsgHeader(msg, 0, "received", timestamp);
    addMsgHeader(msg, 0, "message-type", (uint)Tp::ChannelTextMessageTypeNormal);
    QString token = QUuid::createUuid().toString();
    addMsgHeader(msg, 0, "message-token", token);

    addMsgHeader(msg, 1,"content-type", "text/plain");
    addMsgHeader(msg, 1,"content", message);
    // set sender contact
    Tp::ContactPtr sender(new Tp::Contact());
    sender->ut_setHandle(22);
    sender->ut_setId(username);
    msg.ut_setSender(sender);

    Tp::TextChannelPtr::dynamicCast(ch)->ut_receiveMessage(msg);

    QSignalSpy eventCommitted(&tcl.eventModel(), SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));
    QVERIFY(waitSignal(eventCommitted, 5000));

    CommHistory::Group g = fetchGroup(accountPath, username, true);

    QVERIFY(g.isValid());
    QCOMPARE(g.localUid(), accountPath);
    QCOMPARE(g.remoteUids().first(), username);
    QCOMPARE(g.lastMessageText(), message);
    if (cellular)
        QCOMPARE(g.lastEventType(), CommHistory::Event::SMSEvent);
    else
        QCOMPARE(g.lastEventType(), CommHistory::Event::IMEvent);

    CommHistory::Event e = fetchEvent(g.lastEventId());
    QCOMPARE(e.id(), g.lastEventId());
    QCOMPARE(e.direction(), CommHistory::Event::Inbound);
    QCOMPARE(e.freeText(), message);
    QCOMPARE(e.startTime().toTime_t(), timestamp);
    QCOMPARE(e.endTime().toTime_t(), timestamp);
    QCOMPARE(e.messageToken(), token);

    QCOMPARE(nm->postedNotifications.size(), 1);
    QCOMPARE(nm->postedNotifications.first().event.freeText(), message);
    QCOMPARE(nm->postedNotifications.first().channelTargetId, username);
    QCOMPARE(nm->postedNotifications.first().chatType, CommHistory::Group::ChatTypeP2P);

    if (cellular) {
        CommHistoryTp::Client::ConnectionInterfaceStoredMessagesInterface* storedMessages =
                conn->interface<CommHistoryTp::Client::ConnectionInterfaceStoredMessagesInterface>();
        QVERIFY(storedMessages);
        QStringList sm = storedMessages->ut_getExpungedMessages();
        QVERIFY(sm.contains(token));
    }
}

void Ut_TextChannelListener::smsSending_data()
{
    QTest::addColumn<bool>("finalStatus");

    QTest::newRow("Delivered") << true;
    QTest::newRow("Failed") << false;
}

void Ut_TextChannelListener::smsSending()
{
    QFETCH(bool, finalStatus);

    QString message = QString(SENT_MESSAGE) + QString(" : ") + QLatin1String(finalStatus ? " delivered ":" failed ")
                      + QTime::currentTime().toString(Qt::ISODate);

    NotificationManager *nm = NotificationManager::instance();
    QVERIFY(nm);
    nm->postedNotifications.clear();

    // setup connection
    Tp::ConnectionPtr conn(new Tp::Connection());
    conn->ut_setIsReady(true);
    conn->ut_setInterfaces(QStringList() << CommHistoryTp::Client::ConnectionInterfaceStoredMessagesInterface::staticInterfaceName());

    Tp::AccountPtr acc(new Tp::Account(conn, SMS_ACCOUNT_PATH));
    acc->ut_setProtocolName("tel");

    //setup channel
    Tp::ChannelPtr ch(new Tp::TextChannel(SMS_CHANNEL_PATH));
    ch->ut_setIsRequested(true);
    ch->ut_setTargetHandleType(Tp::HandleTypeContact);
    ch->ut_setTargetHandle(TARGET_HANDLE);
    QVariantMap immProp;
    immProp.insert(TELEPATHY_INTERFACE_CHANNEL ".TargetID", SMS_NUMBER);
    ch->ut_setImmutableProperties(immProp);
    ch->ut_setConnection(conn);

    Tp::MethodInvocationContextPtr<> ctx(new Tp::MethodInvocationContext<>());

    TextChannelListener tcl(acc, ch, ctx);
    waitInvocationContext(ctx, 5000);

    QVERIFY(ctx->isFinished());
    QVERIFY(!ctx->isError());

    // send sent message
    uint timestamp = QDateTime::currentDateTime().toTime_t();
    Tp::Message msg(timestamp, (uint)Tp::ChannelTextMessageTypeNormal, message);
    QString token = QUuid::createUuid().toString();
    Tp::TextChannelPtr::dynamicCast(ch)->ut_sendMessage(msg, Tp::MessageSendingFlagReportDelivery, token);

    QSignalSpy eventCommitted(&tcl.eventModel(), SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));
    QVERIFY(waitSignal(eventCommitted, 5000));

    CommHistory::Group g = fetchGroup(SMS_ACCOUNT_PATH, SMS_NUMBER, true);

    QVERIFY(g.isValid());
    QCOMPARE(g.localUid(), SMS_ACCOUNT_PATH);
    QCOMPARE(g.remoteUids().first(), SMS_NUMBER);
    QCOMPARE(g.lastMessageText(), message);
    QCOMPARE(g.lastEventType(), CommHistory::Event::SMSEvent);
    QVERIFY(g.lastEventStatus() == CommHistory::Event::SendingStatus
            || g.lastEventStatus() == CommHistory::Event::UnknownStatus);

    CommHistory::Event e = fetchEvent(g.lastEventId());
    QCOMPARE(e.direction(), CommHistory::Event::Outbound);
    QCOMPARE(e.freeText(), message);
    QCOMPARE(e.startTime().toTime_t(), timestamp);
    QCOMPARE(e.endTime().toTime_t(), timestamp);
    QCOMPARE(e.messageToken(), token);
    QVERIFY(e.status() == CommHistory::Event::SendingStatus
            || e.status() == CommHistory::Event::UnknownStatus);

    QCOMPARE(nm->postedNotifications.size(), 0);

    // test delivery report handling
    // accepted
    Tp::ReceivedMessage accepted(Tp::MessagePartList() << Tp::MessagePart());

    uint timestampAccepted = QDateTime::currentDateTime().toTime_t();
    addMsgHeader(accepted, 0, "pending-message-id", pendingMessageId++);
    addMsgHeader(accepted, 0, "received", timestampAccepted);
    addMsgHeader(accepted, 0, "message-sent", timestampAccepted);
    addMsgHeader(accepted, 0, "message-type", (uint)Tp::ChannelTextMessageTypeDeliveryReport);
    addMsgHeader(accepted, 0, "delivery-token", token);
    addMsgHeader(accepted, 0, "delivery-status", (uint)Tp::DeliveryStatusAccepted);
    QString acceptedToken = QUuid::createUuid().toString();
    addMsgHeader(accepted, 0, "message-token", acceptedToken);

    eventCommitted.clear();
    Tp::TextChannelPtr::dynamicCast(ch)->ut_receiveMessage(accepted);
    QVERIFY(waitSignal(eventCommitted, 5000));

    g = fetchGroup(SMS_ACCOUNT_PATH, SMS_NUMBER, true);

    QVERIFY(g.isValid());
    QCOMPARE(g.lastMessageText(), message);
    QCOMPARE(g.lastEventType(), CommHistory::Event::SMSEvent);
    QCOMPARE(g.lastEventStatus(), CommHistory::Event::SentStatus);

    // delivered
    Tp::ReceivedMessage delivered(Tp::MessagePartList() << Tp::MessagePart());

    uint timestampDelivered = QDateTime::currentDateTime().toTime_t();
    addMsgHeader(delivered, 0, "pending-message-id", pendingMessageId++);
    addMsgHeader(delivered, 0, "received", timestampDelivered);
    addMsgHeader(delivered, 0, "message-sent", timestampDelivered);
    addMsgHeader(delivered, 0, "message-type", (uint)Tp::ChannelTextMessageTypeDeliveryReport);
    addMsgHeader(delivered, 0, "delivery-token", token);
    QString deliveredToken = QUuid::createUuid().toString();
    addMsgHeader(delivered, 0, "message-token", deliveredToken);


    if (finalStatus)
        addMsgHeader(delivered, 0, "delivery-status", (uint)Tp::DeliveryStatusDelivered);
    else
        addMsgHeader(delivered, 0, "delivery-status", (uint)Tp::DeliveryStatusPermanentlyFailed);

    eventCommitted.clear();
    Tp::TextChannelPtr::dynamicCast(ch)->ut_receiveMessage(delivered);
    QVERIFY(waitSignal(eventCommitted, 5000));

    g = fetchGroup(SMS_ACCOUNT_PATH, SMS_NUMBER, true);

    QVERIFY(g.isValid());
    QCOMPARE(g.lastMessageText(), message);
    QCOMPARE(g.lastEventType(), CommHistory::Event::SMSEvent);

    if (finalStatus)
        QCOMPARE(g.lastEventStatus(), CommHistory::Event::DeliveredStatus);
    else
        QCOMPARE(g.lastEventStatus(), CommHistory::Event::PermanentlyFailedStatus);

    e = fetchEvent(g.lastEventId());
    QCOMPARE(e.startTime().toTime_t(), timestampDelivered);

    CommHistoryTp::Client::ConnectionInterfaceStoredMessagesInterface* storedMessages =
            conn->interface<CommHistoryTp::Client::ConnectionInterfaceStoredMessagesInterface>();
    QVERIFY(storedMessages);
    QStringList sm = storedMessages->ut_getExpungedMessages();
    QVERIFY(sm.contains(acceptedToken));
    QVERIFY(sm.contains(deliveredToken));
    QVERIFY(!sm.contains(token));
}

namespace {

QString sendVoicemail(Tp::ChannelPtr ch,
                   const QString &notificationType,
                   const QString &voicemailType,
                   bool includeHas,
                   bool hasUnread,
                   bool includeCount,
                   int unreadCount) {
    Tp::ReceivedMessage msg(Tp::MessagePartList() << Tp::MessagePart() << Tp::MessagePart());

    addMsgHeader(msg, 0, "pending-message-id", pendingMessageId++);
    addMsgHeader(msg, 0, "received", QDateTime::currentDateTime().toTime_t());
    addMsgHeader(msg, 0, "message-type", (uint)Tp::ChannelTextMessageTypeNotice);
    QString token = QUuid::createUuid().toString();
    addMsgHeader(msg, 0, "message-token", token);
    addMsgHeader(msg, 0, "x-nokia-mailbox-notification", notificationType);
    addMsgHeader(msg, 0, "x-nokia-voicemail-type", voicemailType);
    if (includeHas)
        addMsgHeader(msg, 0, "x-nokia-mailbox-has-unread", hasUnread);
    if (includeCount)
        addMsgHeader(msg, 0, "x-nokia-mailbox-unread-count", unreadCount);

    addMsgHeader(msg, 1,"content-type", "text/plain");
    addMsgHeader(msg, 1,"content", QString());
    // set sender contact
    Tp::ContactPtr sender(new Tp::Contact());
    sender->ut_setHandle(22);
    sender->ut_setId(SMS_NUMBER);
    msg.ut_setSender(sender);

    Tp::TextChannelPtr::dynamicCast(ch)->ut_receiveMessage(msg);

    return token;
}

}
void Ut_TextChannelListener::voicemail()
{
    NotificationManager *nm = NotificationManager::instance();
    QVERIFY(nm);
    nm->postedNotifications.clear();

    // setup connection
    Tp::ConnectionPtr conn(new Tp::Connection());
    conn->ut_setIsReady(true);
    conn->ut_setInterfaces(QStringList() << CommHistoryTp::Client::ConnectionInterfaceStoredMessagesInterface::staticInterfaceName());

    //setup account
    Tp::AccountPtr acc(new Tp::Account(conn, SMS_ACCOUNT_PATH));
    acc->ut_setProtocolName("tel");

    //setup channel
    Tp::ChannelPtr ch(new Tp::TextChannel(SMS_CHANNEL_PATH));
    ch->ut_setIsRequested(false);
    ch->ut_setTargetHandleType(Tp::HandleTypeContact);
    ch->ut_setTargetHandle(TARGET_HANDLE);
    QVariantMap immProp;
    immProp.insert(TELEPATHY_INTERFACE_CHANNEL ".TargetID", SMS_NUMBER);
    ch->ut_setImmutableProperties(immProp);
    ch->ut_setConnection(conn);

    Tp::MethodInvocationContextPtr<> ctx(new Tp::MethodInvocationContext<>());

    TextChannelListener tcl(acc, ch, ctx);
    waitInvocationContext(ctx, 5000);

    QVERIFY(ctx->isFinished());
    QVERIFY(!ctx->isError());

    // show voicemail
    nm->voicemailNotifications = 0xDEAD;
    QString token = sendVoicemail(ch, "voice", "tel",
                                  true, true,
                                  true, 2);

    QCoreApplication::processEvents(); // let expunging run off the loop

    QCOMPARE(nm->postedNotifications.size(), 0);
    QCOMPARE(nm->voicemailNotifications, 2);

    CommHistoryTp::Client::ConnectionInterfaceStoredMessagesInterface* storedMessages =
            conn->interface<CommHistoryTp::Client::ConnectionInterfaceStoredMessagesInterface>();
    QVERIFY(storedMessages);
    QVERIFY(storedMessages->ut_getExpungedMessages().contains(token));

    // hide voicemail
    nm->voicemailNotifications = 0xBEEF;
    token = sendVoicemail(ch, "voice", "tel",
                          true, false,
                          true, 0);

    QCoreApplication::processEvents(); // let expunging run off the loop

    QCOMPARE(nm->postedNotifications.size(), 0);
    QCOMPARE(nm->voicemailNotifications, 0);

    QVERIFY(storedMessages->ut_getExpungedMessages().contains(token));

    // hide voicemail
    nm->voicemailNotifications = 0xFA11;
    token = sendVoicemail(ch, "voice", "tel",
                          true, false,
                          false, 0);

    QCoreApplication::processEvents(); // let expunging run off the loop

    QCOMPARE(nm->postedNotifications.size(), 0);
    QCOMPARE(nm->voicemailNotifications, 0);

    QVERIFY(storedMessages->ut_getExpungedMessages().contains(token));

    // hide voicemail
    nm->voicemailNotifications = 0xBEEB;
    token = sendVoicemail(ch, "voice", "tel",
                          false, false,
                          false, 0);

    QCoreApplication::processEvents(); // let expunging run off the loop

    QCOMPARE(nm->postedNotifications.size(), 0);
    QCOMPARE(nm->voicemailNotifications, 0);

    QVERIFY(storedMessages->ut_getExpungedMessages().contains(token));

    // skype voicemail should be ignored
    nm->voicemailNotifications = 0xACE;
    token = sendVoicemail(ch, "voice", "skype",
                          true, true,
                          true, 1);

    QCoreApplication::processEvents(); // let expunging run off the loop

    QCOMPARE(nm->postedNotifications.size(), 0);
    QCOMPARE(nm->voicemailNotifications, 0xACE);

    QVERIFY(storedMessages->ut_getExpungedMessages().contains(token));

    // non voice notifications should be ignored
    token = sendVoicemail(ch, "text", "jumps",
                          true, true,
                          true, 1);

    QCoreApplication::processEvents(); // let expunging run off the loop

    QCOMPARE(nm->postedNotifications.size(), 0);
    QCOMPARE(nm->voicemailNotifications, 0xACE);

    QVERIFY(storedMessages->ut_getExpungedMessages().contains(token));

    // show unknown number
    nm->voicemailNotifications = 0xCAB;
    token = sendVoicemail(ch, "voice", "tel",
                          true, true,
                          true, 0);

    QCoreApplication::processEvents(); // let expunging run off the loop

    QCOMPARE(nm->postedNotifications.size(), 0);
    QCOMPARE(nm->voicemailNotifications, -1); // -1 == unknown number

    QVERIFY(storedMessages->ut_getExpungedMessages().contains(token));

    // show unknown number
    nm->voicemailNotifications = 0xCAB;
    token = sendVoicemail(ch, "voice", "tel",
                          true, true,
                          false, 0);

    QCoreApplication::processEvents(); // let expunging run off the loop

    QCOMPARE(nm->postedNotifications.size(), 0);
    QCOMPARE(nm->voicemailNotifications, -1); // -1 == unknown number

    QVERIFY(storedMessages->ut_getExpungedMessages().contains(token));

}

void Ut_TextChannelListener::receiveVCard()
{
    NotificationManager *nm = NotificationManager::instance();
    QVERIFY(nm);
    nm->postedNotifications.clear();

    // setup connection
    Tp::ConnectionPtr conn(new Tp::Connection());
    conn->ut_setIsReady(true);
    conn->ut_setInterfaces(QStringList() << CommHistoryTp::Client::ConnectionInterfaceStoredMessagesInterface::staticInterfaceName());

    //setup account
    Tp::AccountPtr acc(new Tp::Account(conn, SMS_ACCOUNT_PATH));
    acc->ut_setProtocolName("tel");

    //setup channel
    Tp::ChannelPtr ch(new Tp::TextChannel(SMS_CHANNEL_PATH));
    ch->ut_setIsRequested(false);
    ch->ut_setTargetHandleType(Tp::HandleTypeContact);
    ch->ut_setTargetHandle(TARGET_HANDLE);
    QVariantMap immProp;
    immProp.insert(TELEPATHY_INTERFACE_CHANNEL ".TargetID", SMS_NUMBER);
    ch->ut_setImmutableProperties(immProp);
    ch->ut_setConnection(conn);

    Tp::MethodInvocationContextPtr<> ctx(new Tp::MethodInvocationContext<>());

    TextChannelListener tcl(acc, ch, ctx);
    waitInvocationContext(ctx, 5000);

    QVERIFY(ctx->isFinished());
    QVERIFY(!ctx->isError());

    // send received message
    Tp::ReceivedMessage msg(Tp::MessagePartList() << Tp::MessagePart() << Tp::MessagePart());

    uint timestamp = QDateTime::currentDateTime().toTime_t();
    addMsgHeader(msg, 0, "pending-message-id", pendingMessageId++);
    addMsgHeader(msg, 0, "received", timestamp);
    addMsgHeader(msg, 0, "message-type", (uint)Tp::ChannelTextMessageTypeNormal);
    QString token = QUuid::createUuid().toString();
    addMsgHeader(msg, 0, "message-token", token);

    addMsgHeader(msg, 1,"content-type", "text/x-vcard");
    addMsgHeader(msg, 1,"content", VCARD_CONTENT);
    // set sender contact
    Tp::ContactPtr sender(new Tp::Contact());
    sender->ut_setHandle(22);
    sender->ut_setId(SMS_NUMBER);
    msg.ut_setSender(sender);

    Tp::TextChannelPtr::dynamicCast(ch)->ut_receiveMessage(msg);

    QSignalSpy eventCommitted(&tcl.eventModel(), SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));
    QVERIFY(waitSignal(eventCommitted, 5000));

    CommHistory::Group g = fetchGroup(SMS_ACCOUNT_PATH, SMS_NUMBER, true);

    QVERIFY(g.isValid());
    QCOMPARE(g.localUid(), SMS_ACCOUNT_PATH);
    QCOMPARE(g.remoteUids().first(), SMS_NUMBER);
    QVERIFY(g.lastMessageText().isEmpty());
    QVERIFY(!g.lastVCardFileName().isEmpty());
    QCOMPARE(g.lastVCardLabel(), VCARD_NAME);
    QCOMPARE(g.lastEventType(), CommHistory::Event::SMSEvent);

    CommHistory::Event e = fetchEvent(g.lastEventId());
    QCOMPARE(e.id(), g.lastEventId());
    QCOMPARE(e.direction(), CommHistory::Event::Inbound);
    QVERIFY(e.freeText().isEmpty());
    QCOMPARE(e.startTime().toTime_t(), timestamp);
    QCOMPARE(e.endTime().toTime_t(), timestamp);
    QCOMPARE(e.messageToken(), token);
    QCOMPARE(e.fromVCardLabel(), VCARD_NAME);
    QVERIFY(!e.fromVCardFileName().isEmpty());

    QCOMPARE(nm->postedNotifications.size(), 1);
    QCOMPARE(nm->postedNotifications.first().channelTargetId, SMS_NUMBER);
    QCOMPARE(nm->postedNotifications.first().chatType, CommHistory::Group::ChatTypeP2P);

    QFile vcardFile(e.fromVCardFileName());
    QVERIFY(vcardFile.exists());
    QVERIFY(vcardFile.size() > 0);

    CommHistoryTp::Client::ConnectionInterfaceStoredMessagesInterface* storedMessages =
            conn->interface<CommHistoryTp::Client::ConnectionInterfaceStoredMessagesInterface>();
    QVERIFY(storedMessages);
    QVERIFY(storedMessages->ut_getExpungedMessages().contains(token));
}

void Ut_TextChannelListener::groups()
{
    {
        CommHistory::GroupModel gm;
        gm.getGroups(SMS_ACCOUNT_PATH, SMS_NUMBER);
        QSignalSpy ready(&gm, SIGNAL(modelReady(bool)));
        QVERIFY(waitSignal(ready, 5000));
        QSignalSpy groupsCommitted(&gm, SIGNAL(groupsCommitted(const QList<int>&, bool)));
        gm.deleteAll();
        QVERIFY(waitSignal(groupsCommitted, 5000));
    }

    NotificationManager *nm = NotificationManager::instance();
    QVERIFY(nm);
    nm->postedNotifications.clear();

    CommHistory::Group g = fetchGroup(SMS_ACCOUNT_PATH, SMS_NUMBER, true);
    QVERIFY(!g.isValid());

    // setup connection
    Tp::ConnectionPtr conn(new Tp::Connection());
    conn->ut_setIsReady(true);

    //setup account
    Tp::AccountPtr acc(new Tp::Account(conn, SMS_ACCOUNT_PATH));
    acc->ut_setProtocolName("tel");

    //setup channel
    Tp::ChannelPtr ch(new Tp::TextChannel(SMS_CHANNEL_PATH));
    ch->ut_setIsRequested(false);
    ch->ut_setTargetHandleType(Tp::HandleTypeContact);
    ch->ut_setTargetHandle(TARGET_HANDLE);
    QVariantMap immProp;
    immProp.insert(TELEPATHY_INTERFACE_CHANNEL ".TargetID", SMS_NUMBER);
    ch->ut_setImmutableProperties(immProp);
    ch->ut_setConnection(conn);

    Tp::MethodInvocationContextPtr<> ctx(new Tp::MethodInvocationContext<>());

    TextChannelListener tcl(acc, ch, ctx);
    waitInvocationContext(ctx, 5000);

    QVERIFY(ctx->isFinished());
    QVERIFY(!ctx->isError());

    QVERIFY(!tcl.m_Group.isValid());

    {
        // send received message
        Tp::ReceivedMessage msg(Tp::MessagePartList() << Tp::MessagePart() << Tp::MessagePart());

        uint timestamp = QDateTime::currentDateTime().toTime_t();
        addMsgHeader(msg, 0, "pending-message-id", pendingMessageId++);
        addMsgHeader(msg, 0, "received", timestamp);
        addMsgHeader(msg, 0, "message-type", (uint)Tp::ChannelTextMessageTypeNormal);

        QString message = QString(RECEIVED_MESSAGE) + QString(" : ") + QTime::currentTime().toString(Qt::ISODate);
        addMsgHeader(msg, 1,"content-type", "text/plain");
        addMsgHeader(msg, 1,"content", message);
        // set sender contact
        Tp::ContactPtr sender(new Tp::Contact());
        sender->ut_setHandle(22);
        sender->ut_setId(SMS_NUMBER);
        msg.ut_setSender(sender);

        Tp::TextChannelPtr::dynamicCast(ch)->ut_receiveMessage(msg);

        QSignalSpy eventCommitted(&tcl.eventModel(), SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));
        QVERIFY(waitSignal(eventCommitted, 5000));

        QTest::qWait(100); //let group model in tcl to handle eventsAdded

        QVERIFY(tcl.m_Group.isValid());
        QCOMPARE(tcl.m_Group.localUid(), SMS_ACCOUNT_PATH);
        QCOMPARE(tcl.m_Group.remoteUids().first(), SMS_NUMBER);
        QCOMPARE(tcl.m_Group.lastMessageText(), message);
        QCOMPARE(tcl.m_Group.lastEventType(), CommHistory::Event::SMSEvent);

        QCOMPARE(nm->postedNotifications.size(), 1);
        QCOMPARE(nm->postedNotifications.first().event.freeText(), message);
    }

    int firstGroup = tcl.m_Group.id();

    // delete group
    QSignalSpy groupsCommitted(nm->m_GroupModel, SIGNAL(groupsCommitted(const QList<int>&, bool)));
    nm->m_GroupModel->deleteAll();
    QVERIFY(waitSignal(groupsCommitted, 5000));

    g = fetchGroup(SMS_ACCOUNT_PATH, SMS_NUMBER, true);

    QVERIFY(!g.isValid());

    QVERIFY(!tcl.m_Group.isValid());
    {
        // send received message
        Tp::ReceivedMessage msg(Tp::MessagePartList() << Tp::MessagePart() << Tp::MessagePart());

        uint timestamp = QDateTime::currentDateTime().toTime_t();
        addMsgHeader(msg, 0, "pending-message-id", pendingMessageId++);
        addMsgHeader(msg, 0, "received", timestamp);
        addMsgHeader(msg, 0, "message-type", (uint)Tp::ChannelTextMessageTypeNormal);

        QString message = QString(RECEIVED_MESSAGE) + QString(" : ") + QTime::currentTime().toString(Qt::ISODate);
        addMsgHeader(msg, 1,"content-type", "text/plain");
        addMsgHeader(msg, 1,"content", message);
        // set sender contact
        Tp::ContactPtr sender(new Tp::Contact());
        sender->ut_setHandle(22);
        sender->ut_setId(SMS_NUMBER);
        msg.ut_setSender(sender);

        Tp::TextChannelPtr::dynamicCast(ch)->ut_receiveMessage(msg);

        QSignalSpy eventCommitted(&tcl.eventModel(), SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));
        QVERIFY(waitSignal(eventCommitted, 5000));

        QTest::qWait(100); //let group model in tcl to handle eventsAdded

        QVERIFY(tcl.m_Group.isValid());
        QCOMPARE(tcl.m_Group.localUid(), SMS_ACCOUNT_PATH);
        QCOMPARE(tcl.m_Group.remoteUids().first(), SMS_NUMBER);
        QCOMPARE(tcl.m_Group.lastMessageText(), message);
        QCOMPARE(tcl.m_Group.lastEventType(), CommHistory::Event::SMSEvent);

        QCOMPARE(nm->postedNotifications.size(), 2);
        QCOMPARE(nm->postedNotifications.at(1).event.freeText(), message);
    }

    QVERIFY(firstGroup != tcl.m_Group.id());
}

/*
 * Test receiving IM message from remote device that is using same account as us.
 */
void Ut_TextChannelListener::receivingFromSelf()
{
    QString message = RECEIVED_MESSAGE + QString(" : ") + QTime::currentTime().toString(Qt::ISODate);

    NotificationManager *nm = NotificationManager::instance();
    QVERIFY(nm);
    nm->postedNotifications.clear();

    // setup connection
    Tp::ConnectionPtr conn(new Tp::Connection());
    conn->ut_setIsReady(true);
    conn->ut_setSelfHandle(SELF_HANDLE);

    //setup account
    Tp::AccountPtr acc(new Tp::Account(conn, IM_ACCOUNT_PATH));

    //setup channel
    Tp::ChannelPtr ch(new Tp::TextChannel(IM_CHANNEL_PATH));
    ch->ut_setIsRequested(false);
    ch->ut_setTargetHandleType(Tp::HandleTypeContact);
    ch->ut_setTargetHandle(TARGET_HANDLE);
    QVariantMap immProp;
    immProp.insert(TELEPATHY_INTERFACE_CHANNEL ".TargetID", IM_REMOTE_ID);
    ch->ut_setImmutableProperties(immProp);
    ch->ut_setConnection(conn);

    Tp::MethodInvocationContextPtr<> ctx(new Tp::MethodInvocationContext<>());

    TextChannelListener tcl(acc, ch, ctx);
    waitInvocationContext(ctx, 5000);

    QVERIFY(ctx->isFinished());
    QVERIFY(!ctx->isError());

    // send received message
    Tp::ReceivedMessage msg(Tp::MessagePartList() << Tp::MessagePart() << Tp::MessagePart());

    uint timestamp = QDateTime::currentDateTime().toTime_t();
    addMsgHeader(msg, 0, "pending-message-id", pendingMessageId++);
    addMsgHeader(msg, 0, "received", timestamp);
    addMsgHeader(msg, 0, "message-type", (uint)Tp::ChannelTextMessageTypeNormal);
    QString token = QUuid::createUuid().toString();
    addMsgHeader(msg, 0, "message-token", token);

    addMsgHeader(msg, 1,"content-type", "text/plain");
    addMsgHeader(msg, 1,"content", message);
    // set sender contact
    Tp::ContactPtr sender(new Tp::Contact());
    sender->ut_setHandle(SELF_HANDLE);
    sender->ut_setId(IM_USERNAME);
    msg.ut_setSender(sender);

    Tp::TextChannelPtr::dynamicCast(ch)->ut_receiveMessage(msg);

    QSignalSpy eventCommitted(&tcl.eventModel(), SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));
    QVERIFY(waitSignal(eventCommitted, 5000));

    CommHistory::Group g = fetchGroup(IM_ACCOUNT_PATH, IM_REMOTE_ID, true);

    QVERIFY(g.isValid());
    QCOMPARE(g.localUid(), IM_ACCOUNT_PATH);
    QCOMPARE(g.remoteUids().first(), IM_REMOTE_ID);
    QCOMPARE(g.lastMessageText(), message);
    QCOMPARE(g.lastEventType(), CommHistory::Event::IMEvent);

    CommHistory::Event e = fetchEvent(g.lastEventId());
    QCOMPARE(e.id(), g.lastEventId());
    QCOMPARE(e.direction(), CommHistory::Event::Outbound);
    QCOMPARE(e.isRead(), true);
    QCOMPARE(e.freeText(), message);
    QCOMPARE(e.startTime().toTime_t(), timestamp);
    QCOMPARE(e.endTime().toTime_t(), timestamp);
    QCOMPARE(e.messageToken(), token);

    QCOMPARE(nm->postedNotifications.size(), 0);
}

void Ut_TextChannelListener::supersedes()
{
    QString originalText("Original message");
    QString editedText("Edited message");

    NotificationManager *nm = NotificationManager::instance();
    QVERIFY(nm);
    nm->postedNotifications.clear();

    // setup connection
    Tp::ConnectionPtr conn(new Tp::Connection());
    conn->ut_setIsReady(true);

    //setup account
    Tp::AccountPtr acc(new Tp::Account(conn, IM_ACCOUNT_PATH));

    //setup channel
    Tp::ChannelPtr ch(new Tp::TextChannel(IM_CHANNEL_PATH));
    ch->ut_setIsRequested(false);
    ch->ut_setTargetHandleType(Tp::HandleTypeContact);
    ch->ut_setTargetHandle(TARGET_HANDLE);
    QVariantMap immProp;
    immProp.insert(TELEPATHY_INTERFACE_CHANNEL ".TargetID", IM_USERNAME);
    ch->ut_setImmutableProperties(immProp);
    ch->ut_setConnection(conn);

    Tp::MethodInvocationContextPtr<> ctx(new Tp::MethodInvocationContext<>());

    TextChannelListener tcl(acc, ch, ctx);
    waitInvocationContext(ctx, 5000);

    QVERIFY(ctx->isFinished());
    QVERIFY(!ctx->isError());

    // send received message
    Tp::ReceivedMessage msg(Tp::MessagePartList() << Tp::MessagePart() << Tp::MessagePart());

    uint timestamp = QDateTime::currentDateTime().toTime_t();
    addMsgHeader(msg, 0, "pending-message-id", pendingMessageId++);
    addMsgHeader(msg, 0, "received", timestamp);
    addMsgHeader(msg, 0, "message-type", (uint)Tp::ChannelTextMessageTypeNormal);
    QString token = QUuid::createUuid().toString();
    addMsgHeader(msg, 0, "message-token", token);

    addMsgHeader(msg, 1,"content-type", "text/plain");
    addMsgHeader(msg, 1,"content", originalText);
    // set sender contact
    Tp::ContactPtr sender(new Tp::Contact());
    sender->ut_setHandle(22);
    sender->ut_setId(IM_USERNAME);
    msg.ut_setSender(sender);

    Tp::TextChannelPtr::dynamicCast(ch)->ut_receiveMessage(msg);

    QSignalSpy eventCommitted(&tcl.eventModel(), SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));
    QVERIFY(waitSignal(eventCommitted, 5000));

    CommHistory::Group g = fetchGroup(IM_ACCOUNT_PATH, IM_USERNAME, true);

    QVERIFY(g.isValid());
    QCOMPARE(g.localUid(), IM_ACCOUNT_PATH);
    QCOMPARE(g.remoteUids().first(), IM_USERNAME);
    QCOMPARE(g.lastMessageText(), originalText);
    QCOMPARE(g.lastEventType(), CommHistory::Event::IMEvent);

    CommHistory::Event e = fetchEvent(g.lastEventId());
    QCOMPARE(e.id(), g.lastEventId());
    QCOMPARE(e.direction(), CommHistory::Event::Inbound);
    QCOMPARE(e.freeText(), originalText);
    QCOMPARE(e.startTime().toTime_t(), timestamp);
    QCOMPARE(e.endTime().toTime_t(), timestamp);
    QCOMPARE(e.messageToken(), token);

    QCOMPARE(nm->postedNotifications.size(), 1);
    QCOMPARE(nm->postedNotifications.first().event.freeText(), originalText);
    QCOMPARE(nm->postedNotifications.first().channelTargetId, IM_USERNAME);
    QCOMPARE(nm->postedNotifications.first().chatType, CommHistory::Group::ChatTypeP2P);

    // send edited message

    Tp::ReceivedMessage msgEdited(Tp::MessagePartList() << Tp::MessagePart() << Tp::MessagePart());

    uint timestampEdited = QDateTime::currentDateTime().toTime_t();
    addMsgHeader(msgEdited, 0, "pending-message-id", pendingMessageId++);
    addMsgHeader(msgEdited, 0, "received", timestampEdited);
    addMsgHeader(msgEdited, 0, "message-type", (uint)Tp::ChannelTextMessageTypeNormal);
    QString tokenEdited = QUuid::createUuid().toString();
    addMsgHeader(msgEdited, 0, "message-token", tokenEdited);
    addMsgHeader(msgEdited, 0, "supersedes", token);

    addMsgHeader(msgEdited, 1,"content-type", "text/plain");
    addMsgHeader(msgEdited, 1,"content", editedText);
    // set sender contact
    msgEdited.ut_setSender(sender);

    eventCommitted.clear();
    Tp::TextChannelPtr::dynamicCast(ch)->ut_receiveMessage(msgEdited);

    QVERIFY(waitSignal(eventCommitted, 5000));

    g = fetchGroup(IM_ACCOUNT_PATH, IM_USERNAME, true);

    QVERIFY(g.isValid());
    QCOMPARE(g.localUid(), IM_ACCOUNT_PATH);
    QCOMPARE(g.remoteUids().first(), IM_USERNAME);
    QCOMPARE(g.lastMessageText(), editedText);
    QCOMPARE(g.lastEventType(), CommHistory::Event::IMEvent);

    CommHistory::Event ee = fetchEvent(g.lastEventId());
    QCOMPARE(ee.id(), g.lastEventId());
    QCOMPARE(ee.id(), e.id());
    QCOMPARE(ee.direction(), CommHistory::Event::Inbound);
    QCOMPARE(ee.freeText(), editedText);
    QCOMPARE(ee.startTime().toTime_t(), timestampEdited);
    QCOMPARE(ee.endTime().toTime_t(), e.endTime().toTime_t());
    QCOMPARE(ee.messageToken(), token);

    QCOMPARE(nm->postedNotifications.size(), 2);
    QCOMPARE(nm->postedNotifications.last().event.freeText(), editedText);
    QCOMPARE(nm->postedNotifications.last().channelTargetId, IM_USERNAME);
    QCOMPARE(nm->postedNotifications.last().chatType, CommHistory::Group::ChatTypeP2P);
}

QTEST_MAIN(Ut_TextChannelListener)
