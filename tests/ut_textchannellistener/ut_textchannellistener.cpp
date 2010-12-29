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

#include "TelepathyQt4/types.h"
#include "TelepathyQt4/account.h"
#include "TelepathyQt4/channel.h"

#include <CommHistory/SingleEventModel>

#include "textchannellistener.h"

// constants
#define IM_USERNAME "dut@localhost"
#define SENT_MESSAGE "Hello, how is life mon?"
#define RECEIVED_MESSAGE "Good, good"
#define IM_ACCOUNT_PATH "/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0"
#define SMS_ACCOUNT_PATH "/org/freedesktop/Telepathy/Account/ring/tel/ring"
#define SMS_NUMBER "+358987654321"

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
    //TODO: clean conversations and events
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

    Tp::Account acc;
    //acc.setObjectPath(IM_ACCOUNT_PATH); //set uid
    //acc.setProtocol(); //tel mms
    Tp::Channel ch;
    //ch.setObjectPath(CH_OBJ_PATH);
    //ch.setIsRequested();
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
}

void Ut_TextChannelListener::testImReceiving()
{
    QString message = QString(RECEIVED_MESSAGE) + QString(" : ") + QTime::currentTime().toString(Qt::ISODate);
    Tp::Account acc;
    //acc.setObjectPath(IM_ACCOUNT_PATH); //set uid
    //acc.setProtocol(); //tel mms
    Tp::Channel ch;
    //ch.setObjectPath(CH_OBJ_PATH);
    //ch.setIsRequested();
    //ch.immutableProperties(); // add targetID or persitent
    //ch. add pending received message;
    Tp::MethodInvocationContext<> ctx; // used to check that finished() was called on it
    TextChannelListener tcl(Tp::AccountPtr(&acc),
                            Tp::ChannelPtr(&ch),
                            Tp::MethodInvocationContextPtr<>(&ctx));

    // catch group
}

void Ut_TextChannelListener::testSmsSending()
{
    QString message = QString(SENT_MESSAGE) + QString(" : ") + QTime::currentTime().toString(Qt::ISODate);

    Tp::Account acc;
    //acc.setObjectPath(ACCOUNT_OBJ_PATH); //set uid
    //acc.setProtocol(); //tel mms
    Tp::Channel ch;
    //ch.setObjectPath(CH_OBJ_PATH);
    //ch.setIsRequested();
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
    Tp::Account acc;
    //acc.setObjectPath(SMS_ACCOUNT_PATH); //set uid
    //acc.setProtocol(); //tel mms
    Tp::Channel ch;
    //ch.setObjectPath(CH_OBJ_PATH);
    //ch.setIsRequested();
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
