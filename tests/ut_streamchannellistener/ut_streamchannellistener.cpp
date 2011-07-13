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
#include "ut_streamchannellistener.h"

// Qt includes
#include <QDebug>
#include <QTest>
#include <QTime>
#include <QSignalSpy>

#include "TelepathyQt4/types.h"
#include "TelepathyQt4/account.h"
#include "TelepathyQt4/channel.h"
#include "TelepathyQt4/streamed-media-channel.h"
#include "TelepathyQt4/message.h"
#include "TelepathyQt4/connection.h"
#include "TelepathyQt4/contact-manager.h"

#include "streamchannellistener.h"
#include "notificationmanager.h"

// constants
#define ACCOUNT_PATH QLatin1String("/org/freedesktop/Telepathy/Account/ring/tel/ring")
#define NUMBER QLatin1String("+358987654321")
#define CHANNEL_PATH QLatin1String("/org/freedesktop/Telepathy/Connection/ring/tel/ring/call0")
#define TARGET_HANDLE 1

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

    void justWait(int msec)
    {
        QTime timer;
        timer.start();
        while (timer.elapsed() < msec)
            QCoreApplication::processEvents();
    }
}

Ut_StreamChannelListener::Ut_StreamChannelListener()
{
}

Ut_StreamChannelListener::~Ut_StreamChannelListener()
{
}

/*!
 * This function will be called before the first testfunction is executed.
 */
void Ut_StreamChannelListener::initTestCase()
{
    qRegisterMetaType<ChannelListener*>("ChannelListener*");
    //qRegisterMetaType<Tp::PendingOperation*>("Tp::PendingOperation*");
}

/*!
 * This function will be called after the last testfunction was executed.
 */
void Ut_StreamChannelListener::cleanupTestCase()
{
}

/*!
 * This function will be called before each testfunction is executed.
 */
void Ut_StreamChannelListener::init()
{
}

/*!
 * This unction will be called after every testfunction.
 */
void Ut_StreamChannelListener::cleanup()
{
}

void Ut_StreamChannelListener::invalidated_data()
{
    QTest::addColumn<bool>("waitReady");
    QTest::addColumn<bool>("startCall");
    QTest::newRow("waitReady") << true << false;
    QTest::newRow("invalidateBeforeReady") << false << false;
    QTest::newRow("invalidateAfterCallStart") << true << true;
}

void Ut_StreamChannelListener::invalidated()
{
    QFETCH(bool, waitReady);
    QFETCH(bool, startCall);

    NotificationManager *nm = NotificationManager::instance();
    QVERIFY(nm);
    nm->postedNotifications.clear();

    // setup connection
    Tp::ConnectionPtr conn(new Tp::Connection());
    conn->ut_setIsReady(true);

    Tp::AccountPtr acc(new Tp::Account(conn, ACCOUNT_PATH));

    //setup channel
    QDateTime startTime = QDateTime::currentDateTime();
    QVariantMap immProp;
    immProp.insert(TELEPATHY_INTERFACE_CHANNEL ".TargetID", NUMBER);
    Tp::ChannelPtr ch(new Tp::StreamedMediaChannel(CHANNEL_PATH, immProp));
    ch->ut_setIsRequested(false);
    ch->ut_setTargetHandleType(Tp::HandleTypeContact);
    ch->ut_setTargetHandle(TARGET_HANDLE);
    ch->ut_setConnection(conn);

    Tp::MethodInvocationContextPtr<> ctx(new Tp::MethodInvocationContext<>());

    StreamChannelListener scl(acc, ch, ctx);

    QVERIFY(ctx->isFinished());
    QVERIFY(!ctx->isError());

    if (waitReady)
        QCoreApplication::processEvents();

    if (startCall) {
        // add streams
        Tp::StreamedMediaChannelPtr::dynamicCast(ch)->ut_addStream();
        Tp::StreamedMediaChannelPtr::dynamicCast(ch)->ut_streams().first()->ut_setLocalPendingState(Tp::StreamedMediaStream::SendingStateSending);

        // add contact
        // add contact
        Tp::ContactPtr self(new Tp::Contact());
        self->ut_setHandle(1);
        Tp::ContactPtr target(new Tp::Contact());
        target->ut_setHandle(TARGET_HANDLE);
        target->ut_setId(NUMBER);

        ch->ut_setGroupSelfContact(self);
        ch->ut_setGroupContacts(Tp::Contacts() << target << self);

        Tp::Channel::GroupMemberChangeDetails details(target,
                                                      Tp::ChannelGroupChangeReasonNone);

        ch->ut_emitGroupMembersChanged(Tp::Contacts() << target,
                                       Tp::Contacts(),
                                       Tp::Contacts(),
                                       Tp::Contacts(),
                                       details);

        justWait(2000);
    }

    QSignalSpy closed(&scl, SIGNAL(channelClosed(ChannelListener*)));
    ch->ut_invalidate(TELEPATHY_ERROR_TERMINATED, QString());

    QVERIFY(waitSignal(closed, 5000));

    CommHistory::CallModel model;

    QSignalSpy modelReady(&model, SIGNAL(modelReady(bool)));
    model.getEvents();
    QVERIFY(waitSignal(modelReady, 5000));
    QVERIFY(model.rowCount() > 0);

    CommHistory::Event e = model.event(model.index(0,0));
    QCOMPARE(e.localUid(), ACCOUNT_PATH);
    QCOMPARE(e.remoteUid(), NUMBER);

    if (startCall) {
        QVERIFY(startTime.toTime_t() < e.endTime().toTime_t());
    } else {
        QVERIFY(e.startTime().toTime_t() - startTime.toTime_t() <= 1);
    }
    if (startCall)
        QVERIFY(!e.isMissedCall());
    else {
        QVERIFY(e.isMissedCall());
        QCOMPARE(nm->postedNotifications.size(), 1);
    }
    QCOMPARE(e.direction(), CommHistory::Event::Inbound);
}

void Ut_StreamChannelListener::normalCall_data()
{
    QTest::addColumn<bool>("incoming");
    QTest::addColumn<bool>("accept");
    QTest::addColumn<bool>("endLocally");
    QTest::addColumn<bool>("missLocally");
    QTest::addColumn<QString>("invalidateError");

    QTest::newRow("incoming acceptAndEnd") << true << true << true << false << QString();
    QTest::newRow("incoming acceptAndEnded") << true << true << false << false << QString();
    QTest::newRow("incoming rejectedLocally") << true << false << true << false << QString();
    QTest::newRow("incoming rejectedRemotley") << true << false << false << false << QString();
    QTest::newRow("incoming missLocally") << true << false << true << true << QString();

    QTest::newRow("outgoing acceptAndEnd") << false << true << true << false << QString();
    QTest::newRow("outgoing acceptAndEnded") << false << true << false << false << QString();
    QTest::newRow("outgoing rejectedLocally") << false << false << true << false << QString();
    QTest::newRow("outgoing rejectedRemotley") << false << false << false << false << QString();
    QTest::newRow("outgoing missLocally") << false << false << true << true << QString();

    QTest::newRow("error before start") << true << false << true << false << QString(TP_QT4_ERROR_NOT_AVAILABLE);
    QTest::newRow("error after end") << true << true << true << false << QString(TP_QT4_ERROR_NOT_AVAILABLE);
}

void Ut_StreamChannelListener::normalCall()
{
    QFETCH(bool, incoming);
    QFETCH(bool, accept);
    QFETCH(bool, endLocally);
    QFETCH(bool, missLocally);
    QFETCH(QString, invalidateError);

    NotificationManager *nm = NotificationManager::instance();
    QVERIFY(nm);
    nm->postedNotifications.clear();

    // setup connection
    Tp::ConnectionPtr conn(new Tp::Connection());
    conn->ut_setIsReady(true);

    Tp::AccountPtr acc(new Tp::Account(conn, ACCOUNT_PATH));

    //setup channel
    QDateTime startTime = QDateTime::currentDateTime();
    QVariantMap immProp;
    immProp.insert(TELEPATHY_INTERFACE_CHANNEL ".TargetID", NUMBER);
    immProp.insert(TELEPATHY_INTERFACE_CHANNEL ".Requested", !incoming);
    Tp::ChannelPtr ch(new Tp::StreamedMediaChannel(CHANNEL_PATH, immProp));
    ch->ut_setIsRequested(!incoming);
    ch->ut_setTargetHandleType(Tp::HandleTypeContact);
    ch->ut_setTargetHandle(TARGET_HANDLE);
    ch->ut_setConnection(conn);

    Tp::MethodInvocationContextPtr<> ctx(new Tp::MethodInvocationContext<>());

    StreamChannelListener scl(acc, ch, ctx);

    QVERIFY(ctx->isFinished());
    QVERIFY(!ctx->isError());

    QCoreApplication::processEvents(); //allow channel to become ready

    // add contact
    Tp::ContactPtr self(new Tp::Contact());
    self->ut_setHandle(1);
    Tp::ContactPtr target(new Tp::Contact());
    target->ut_setHandle(TARGET_HANDLE);
    target->ut_setId(NUMBER);

    ch->ut_setGroupSelfContact(self);
    ch->ut_setGroupContacts(Tp::Contacts() << target << self);

    // add streams
    Tp::StreamedMediaChannelPtr::dynamicCast(ch)->ut_addStream();
    Tp::StreamedMediaChannelPtr::dynamicCast(ch)->ut_streams().first()->ut_setLocalPendingState(Tp::StreamedMediaStream::SendingStateSending);

    // emit membersChanged
    if (accept) {
        // start call
        if (incoming) {
            Tp::Channel::GroupMemberChangeDetails details(self,
                                                          Tp::ChannelGroupChangeReasonNone);

            ch->ut_emitGroupMembersChanged(Tp::Contacts() << self,
                                           Tp::Contacts(),
                                           Tp::Contacts(),
                                           Tp::Contacts(),
                                           details);
        } else {
            Tp::Channel::GroupMemberChangeDetails details(target,
                                                          Tp::ChannelGroupChangeReasonNone);

            ch->ut_emitGroupMembersChanged(Tp::Contacts() << target,
                                           Tp::Contacts(),
                                           Tp::Contacts(),
                                           Tp::Contacts(),
                                           details);
        }
    }

    justWait(2000);

    // end call
    if (endLocally) {
        Tp::Channel::GroupMemberChangeDetails details(self,
                                                      missLocally?
                                                      Tp::ChannelGroupChangeReasonNoAnswer
                                                          :Tp::ChannelGroupChangeReasonNone);

        ch->ut_setGroupContacts(Tp::Contacts() << target);
        ch->ut_emitGroupMembersChanged(Tp::Contacts(),
                                       Tp::Contacts(),
                                       Tp::Contacts(),
                                       Tp::Contacts() << self,
                                       details);
    } else {
        Tp::Channel::GroupMemberChangeDetails details(target,
                                                      Tp::ChannelGroupChangeReasonNone);

        ch->ut_setGroupContacts(Tp::Contacts() << self);
        ch->ut_emitGroupMembersChanged(Tp::Contacts(),
                                       Tp::Contacts(),
                                       Tp::Contacts(),
                                       Tp::Contacts() << target,
                                       details);
    }

    QSignalSpy closed(&scl, SIGNAL(channelClosed(ChannelListener*)));
    ch->ut_invalidate(invalidateError, QString());
    QVERIFY(waitSignal(closed, 5000));

    CommHistory::CallModel model;

    QSignalSpy modelReady(&model, SIGNAL(modelReady(bool)));
    model.getEvents();
    QVERIFY(waitSignal(modelReady, 5000));
    QVERIFY(model.rowCount() > 0);

    CommHistory::Event e = model.event(model.index(0,0));
    QCOMPARE(e.localUid(), ACCOUNT_PATH);
    QCOMPARE(e.remoteUid(), NUMBER);
    QVERIFY(startTime.toTime_t() <= e.startTime().toTime_t());
    justWait(1000); // account for rounding errors with clock_monotonic vs. QDateTime
    QVERIFY(QDateTime::currentDateTime().toTime_t() >= e.endTime().toTime_t());
    QCOMPARE(e.isEmergencyCall(), false);

    if (accept)
        QVERIFY(e.endTime().toTime_t() > e.startTime().toTime_t());
    else
        QCOMPARE(e.endTime().toTime_t(), e.startTime().toTime_t());

    if (incoming)
        QCOMPARE(e.direction(), CommHistory::Event::Inbound);
    else
        QCOMPARE(e.direction(), CommHistory::Event::Outbound);

    if (incoming && ((!accept && !endLocally) || missLocally || (!accept && !invalidateError.isEmpty()))) {
        QVERIFY(e.isMissedCall());
        QCOMPARE(nm->postedNotifications.size(), 1);
    } else {
        QVERIFY(!e.isMissedCall());
        QCOMPARE(nm->postedNotifications.size(), 0);
    }
}

void Ut_StreamChannelListener::emergency_data()
{
    QTest::addColumn<bool>("accept");

    QTest::newRow("accepted") << true;
    QTest::newRow("cancelled") << false;
}

void Ut_StreamChannelListener::emergency()
{
    QFETCH(bool, accept);

    // setup connection
    Tp::ConnectionPtr conn(new Tp::Connection());
    conn->ut_setIsReady(true);

    Tp::AccountPtr acc(new Tp::Account(conn, ACCOUNT_PATH));

    //setup channel
    QDateTime startTime = QDateTime::currentDateTime();
    QVariantMap immProp;
    immProp.insert(TELEPATHY_INTERFACE_CHANNEL ".TargetID", "112");
    immProp.insert(TELEPATHY_INTERFACE_CHANNEL ".Requested", true);
    Tp::ServicePoint sp;
    sp.service = "urn:service:sos";
    sp.servicePointType = Tp::ServicePointTypeEmergency;
    immProp.insert(TELEPATHY_INTERFACE_CHANNEL_INTERFACE_SERVICE_POINT ".InitialServicePoint",
                   QVariant::fromValue(sp));
    Tp::ChannelPtr ch(new Tp::StreamedMediaChannel(CHANNEL_PATH, immProp));
    ch->ut_setIsRequested(true);
    ch->ut_setTargetHandleType(Tp::HandleTypeContact);
    ch->ut_setTargetHandle(42);
    ch->ut_setConnection(conn);
    //ch->ut_setInterfaces(QStringList() << Tp::Client::ChannelInterfaceServicePointInterface::staticInterfaceName());

    Tp::MethodInvocationContextPtr<> ctx(new Tp::MethodInvocationContext<>());

    StreamChannelListener scl(acc, ch, ctx);

    QVERIFY(ctx->isFinished());
    QVERIFY(!ctx->isError());

    QCoreApplication::processEvents(); //allow channel to become ready

    // add contact
    Tp::ContactPtr self(new Tp::Contact());
    self->ut_setHandle(1);
    Tp::ContactPtr target(new Tp::Contact());
    target->ut_setHandle(33);
    target->ut_setId("112");

    ch->ut_setGroupSelfContact(self);
    ch->ut_setGroupContacts(Tp::Contacts() << target << self);

    // add streams
    Tp::StreamedMediaChannelPtr::dynamicCast(ch)->ut_addStream();
    Tp::StreamedMediaChannelPtr::dynamicCast(ch)->ut_streams().first()->ut_setLocalPendingState(Tp::StreamedMediaStream::SendingStateSending);

    // emit membersChanged
    Tp::Channel::GroupMemberChangeDetails details(target,
                                                  Tp::ChannelGroupChangeReasonNone);

    if (accept)
        ch->ut_emitGroupMembersChanged(Tp::Contacts() << target,
                                       Tp::Contacts(),
                                       Tp::Contacts(),
                                       Tp::Contacts(),
                                       details);

    justWait(2000);

    // end call
    ch->ut_setGroupContacts(Tp::Contacts() << target);
    ch->ut_emitGroupMembersChanged(Tp::Contacts(),
                                   Tp::Contacts(),
                                   Tp::Contacts(),
                                   Tp::Contacts() << self,
                                   details);

    QSignalSpy closed(&scl, SIGNAL(channelClosed(ChannelListener*)));
    ch->ut_invalidate(QString(), QString());
    QVERIFY(waitSignal(closed, 5000));

    CommHistory::CallModel model;

    QSignalSpy modelReady(&model, SIGNAL(modelReady(bool)));
    model.getEvents();
    QVERIFY(waitSignal(modelReady, 5000));
    QVERIFY(model.rowCount() > 0);

    CommHistory::Event e = model.event(model.index(0,0));
    qDebug() << e.toString();
    QCOMPARE(e.localUid(), ACCOUNT_PATH);
    QCOMPARE(e.remoteUid(), QString("112"));
    QVERIFY(startTime.toTime_t() <= e.startTime().toTime_t());
    justWait(1000); // account for rounding errors with clock_monotonic vs. QDateTime
    QVERIFY(QDateTime::currentDateTime().toTime_t() >= e.endTime().toTime_t());

    if (accept)
        QVERIFY(e.endTime().toTime_t() > e.startTime().toTime_t());
    QCOMPARE(e.direction(), CommHistory::Event::Outbound);
    QCOMPARE(e.isEmergencyCall(), true);
}

QTEST_MAIN(Ut_StreamChannelListener)
