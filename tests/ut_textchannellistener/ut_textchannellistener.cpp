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
#define IM_TARGET_HANDLE 1

using namespace RTComLogger;

namespace {
    bool waitSignal(QSignalSpy &spy, int msec)
    {
        QTime timer;
        timer.start();
        while (timer.elapsed() < msec && spy.isEmpty())
            QCoreApplication::processEvents();

        return !spy.isEmpty();
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

    QSignalSpy modelReady(&model, SIGNAL(modelReady()));
    model.getGroups(localUid, remoteUid);

    if (!waitSignal(modelReady, 5000))
        goto end;

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
    QSignalSpy modelReady(&model, SIGNAL(modelReady()));

    model.getEventByUri(CommHistory::Event::idToUrl(eventId));
    if(!waitSignal(modelReady, 5000))
        goto end;

    if(model.rowCount())
        return model.event(model.index(0, 0));

end:
    qWarning() << Q_FUNC_INFO << "Failed to fetch event" << eventId;
    return CommHistory::Event();
}

void Ut_TextChannelListener::testImSending()
{
    QString message = QString(SENT_MESSAGE) + QString(" : ") + QTime::currentTime().toString(Qt::ISODate);

    NotificationManager *nm = NotificationManager::instance();
    QVERIFY(nm);
    nm->postedNotifications.clear();

    Tp::Account acc(IM_ACCOUNT_PATH);
    Tp::Channel ch(IM_CHANNEL_PATH);
    ch.ut_setIsRequested(true);
    //ch.immutableProperties(); // add targetID or persitent
    //ch. add pending sent message;
    Tp::MethodInvocationContext<> ctx; // used to check that finished() was called on it
    TextChannelListener tcl(Tp::AccountPtr(&acc),
                            Tp::ChannelPtr(&ch),
                            Tp::MethodInvocationContextPtr<>(&ctx));

    //tcl wait for conversation event added signal and than commited signal

    CommHistory::Group g = fetchGroup(IM_ACCOUNT_PATH, IM_USERNAME, true);

    QVERIFY(g.isValid());
    QCOMPARE(g.localUid(), QLatin1String(IM_ACCOUNT_PATH));
    QCOMPARE(g.remoteUids().first(), QLatin1String(IM_USERNAME));
    QCOMPARE(g.lastMessageText(), message);
    QCOMPARE(g.lastEventType(), CommHistory::Event::IMEvent);

    CommHistory::Event e = fetchEvent(g.lastEventId());
    QCOMPARE(e.direction(), CommHistory::Event::Outbound);
    QCOMPARE(e.freeText(), message);

    QCOMPARE(nm->postedNotifications.size(), 0);
}

void Ut_TextChannelListener::testImReceiving()
{
    QString message = QString(RECEIVED_MESSAGE) + QString(" : ") + QTime::currentTime().toString(Qt::ISODate);

    NotificationManager *nm = NotificationManager::instance();
    QVERIFY(nm);
    nm->postedNotifications.clear();

    //setup account
    Tp::AccountPtr acc(new Tp::Account(IM_ACCOUNT_PATH));

    // setup connection
    Tp::ContactManager cm;
    Tp::ConnectionPtr conn(new Tp::Connection());
    conn->ut_setContactManager(&cm);
    conn->ut_setIsReady(true);

            //setup channel
    Tp::ChannelPtr ch(new Tp::TextChannel(IM_CHANNEL_PATH));
    ch->ut_setIsRequested(false);
    ch->ut_setTargetHandleType(Tp::HandleTypeContact);
    ch->ut_setTargetHandle(IM_TARGET_HANDLE);
    QVariantMap immProp;
    immProp.insert(TELEPATHY_INTERFACE_CHANNEL ".TargetID", IM_USERNAME);
    ch->ut_setImmutableProperties(immProp);
    ch->ut_setConnection(conn);
    Tp::MethodInvocationContextPtr<> ctx(new Tp::MethodInvocationContext<>()); // TODO: used to check that finished() was called on it

    TextChannelListener tcl(acc, ch, ctx);
    QSignalSpy readySpy(ch->ut_pendingReady(), SIGNAL(finished(Tp::PendingOperation*)));
    QVERIFY(waitSignal(readySpy, 5000));

    // send received message
    Tp::ReceivedMessage msg(Tp::MessagePartList() << Tp::MessagePart() << Tp::MessagePart());

    uint timestamp = QDateTime::currentDateTime().toTime_t();
    addMsgHeader(msg, 0, "received", timestamp);
    addMsgHeader(msg, 0, "message-type", (uint)Tp::ChannelTextMessageTypeNormal);
    QString token = QUuid::createUuid().toString();
    addMsgHeader(msg, 0, "message-token", token);

    addMsgHeader(msg, 1,"content-type", "text/plain");
    addMsgHeader(msg, 1,"content", message);
    // set sender contact
    Tp::ContactPtr sender(new Tp::Contact());
    sender->ut_setHandle(22);
    sender->ut_setId(IM_USERNAME);
    msg.ut_setSender(sender);

    Tp::TextChannelPtr::dynamicCast(ch)->ut_receiveMessage(msg);

    QSignalSpy eventCommitted(&tcl.eventModel(), SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));
    QVERIFY(waitSignal(eventCommitted, 5000));

    // catch group
    CommHistory::Group g = fetchGroup(IM_ACCOUNT_PATH, IM_USERNAME, true);

    QVERIFY(g.isValid());
    QCOMPARE(g.localUid(), QLatin1String(IM_ACCOUNT_PATH));
    QCOMPARE(g.remoteUids().first(), QLatin1String(IM_USERNAME));
    QCOMPARE(g.lastMessageText(), message);
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
    QCOMPARE(nm->postedNotifications.first().channelTargetId, IM_USERNAME);
    QCOMPARE(nm->postedNotifications.first().chatType, CommHistory::Group::ChatTypeP2P);
}

void Ut_TextChannelListener::testSmsSending()
{
    QString message = QString(SENT_MESSAGE) + QString(" : ") + QTime::currentTime().toString(Qt::ISODate);

    Tp::Account acc(SMS_ACCOUNT_PATH);
    acc.ut_setProtocolName("tel");
    Tp::Channel ch(SMS_CHANNEL_PATH);
    ch.ut_setIsRequested(true);
    //ch.immutableProperties(); // add targetID or persitent
    //ch. add pending sent message;
    Tp::MethodInvocationContext<> ctx; // used to check that finished() was called on it
    TextChannelListener tcl(Tp::AccountPtr(&acc),
                            Tp::ChannelPtr(&ch),
                            Tp::MethodInvocationContextPtr<>(&ctx));

    //tcl wait for conversation event added signal and than commited signal
    //read message and verify it

    // send and verify delivery report with sent status
    // send and verify delivery report with delivered status
}

void Ut_TextChannelListener::testSmsReceiving()
{
    QString message = QString(RECEIVED_MESSAGE) + QString(" : ") + QTime::currentTime().toString(Qt::ISODate);
    Tp::Account acc(SMS_ACCOUNT_PATH);
    acc.ut_setProtocolName("tel");

    Tp::Channel ch(SMS_CHANNEL_PATH);
    ch.ut_setIsRequested(false);
    //ch.immutableProperties(); // add targetID or persitent
    //ch. add pending received message;
    Tp::MethodInvocationContext<> ctx; // used to check that finished() was called on it
    TextChannelListener tcl(Tp::AccountPtr(&acc),
                            Tp::ChannelPtr(&ch),
                            Tp::MethodInvocationContextPtr<>(&ctx));

    //tcl wait for conversation event added signal and than commited signal
    //read message and verify it

    // verify expung called
}

QTEST_MAIN(Ut_TextChannelListener)
