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
#include "ut_messagereviver.h"

// Qt includes
#include <QDebug>
#include <QTest>
#include <QTime>
#include <QSignalSpy>

#include <CommHistory/EventModel>

#include "TelepathyQt4/types.h"
#include "TelepathyQt4/account.h"
#include "TelepathyQt4/connection.h"
#include "TpExtensions/Connection"

#include "connectionutils.h"
#include "messagereviver.h"

// constants
#define ACCOUNT_PATH QLatin1String("/org/freedesktop/Telepathy/Account/ring/tel/ring")
#define NUMBER QLatin1String("+1111")

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
}

Ut_MessageReviver::Ut_MessageReviver()
{
}

Ut_MessageReviver::~Ut_MessageReviver()
{
}

/*!
 * This function will be called before the first testfunction is executed.
 */
void Ut_MessageReviver::initTestCase()
{
    groupModel.enableContactChanges(false);
    CommHistory::Group group;
    group.setLocalUid(ACCOUNT_PATH);
    group.setRemoteUids(QStringList() << NUMBER);
    groupModel.addGroup(group);

    CommHistory::EventModel model;

    QSignalSpy commit(&model,
                      SIGNAL(eventsCommitted(const QList<CommHistory::Event>&, bool)));
    CommHistory::Event event;
    event.setType(CommHistory::Event::SMSEvent);
    event.setDirection(CommHistory::Event::Inbound);
    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(QDateTime::currentDateTime());
    event.setLocalUid(ACCOUNT_PATH);
    event.setGroupId(group.id());
    event.setRemoteUid(NUMBER);
    event.setFreeText("blah");
    event.setMessageToken("mrtc1");
    model.addEvent(event, false);
    waitSignal(commit, 5000);
}

/*!
 * This function will be called after the last testfunction was executed.
 */
void Ut_MessageReviver::cleanupTestCase()
{
    groupModel.deleteAll();
}

/*!
 * This function will be called before each testfunction is executed.
 */
void Ut_MessageReviver::init()
{
}

/*!
 * This unction will be called after every testfunction.
 */
void Ut_MessageReviver::cleanup()
{
}

void Ut_MessageReviver::revive()
{
    ConnectionUtils utils;
    MessageReviver reviver(&utils);

    Tp::ConnectionPtr conn(new Tp::Connection());
    conn->ut_setIsReady(true);
    conn->ut_setIsValid(true);

    conn->ut_setInterfaces(QStringList()
                           << CommHistoryTp::Client::ConnectionInterfaceStoredMessagesInterface::staticInterfaceName());

    CommHistoryTp::Client::ConnectionInterfaceStoredMessagesInterface *sm = conn->optionalInterface<CommHistoryTp::Client::ConnectionInterfaceStoredMessagesInterface>();

    QVERIFY(sm->ut_getDeliveredMessages().isEmpty());

    QMetaObject::invokeMethod(&reviver,
                              "onConnectionReady",
                              Qt::DirectConnection,
                              Q_ARG(Tp::ConnectionPtr, conn));


    QStringList tokens;
    tokens << "mrtc1" << "mrtc2" << "mrtc3";
    reviver.updateTokens(tokens + QStringList() << "mrtc4", conn);
    QVERIFY(sm->ut_getDeliveredMessages().isEmpty());
    QVERIFY(sm->ut_getExpungedMessages().isEmpty());

    // fake check after timeout
    reviver.updateTokens(tokens, conn);
    // call seconde time with the same tokens to initate recovery
    reviver.updateTokens(tokens, conn);

    QStringList delivered = sm->ut_getDeliveredMessages();
    QCOMPARE(delivered.size(), 2);
    QVERIFY(delivered.contains("mrtc2"));
    QVERIFY(delivered.contains("mrtc3"));
    QCOMPARE(sm->ut_getExpungedMessages().size(), 1);
    QVERIFY(sm->ut_getExpungedMessages().contains("mrtc1"));
}

QTEST_MAIN(Ut_MessageReviver)
