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
#include <QProcess>

#include <QDBusConnection>
#include <QDBusMessage>

#include <TelepathyQt4/Types>

// constants
#define SERVER_DOMAIN "localhost"
#define TD_USERNAME "td"
#define DUT_USERNAME "dut"
#define DUT "dut@localhost"
#define TD "td@localhost"
#define SENT_MESSAGE "Hello, how is life mon?"
#define RECEIVED_MESSAGE "Good, good"
#define DUT_ACCOUNT_PATH "/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0"
#define PYTHON "/usr/bin/python"
#define SENDMESSAGE "/usr/share/commhistory-daemon-tests/sendmessage.py"

#define UNREAD_VOICEMAIL 5

using namespace RTComLogger;

Ut_TextChannelListener::Ut_TextChannelListener() : conversationUid(-1)
{
}

Ut_TextChannelListener::~Ut_TextChannelListener()
{
}

int Ut_TextChannelListener::startEventLoop()
{
    if(eventLoop.isRunning()) {
        return 1;
    } else {
        return eventLoop.exec();
    }
}

void Ut_TextChannelListener::stopEventLoop(int returnCode)
{
    if(eventLoop.isRunning())
        eventLoop.exit(returnCode);
}

/*!
 * This function will be called before the first testfunction is executed.
 */
void Ut_TextChannelListener::initTestCase()
{
    model = new CommHistory::ConversationModel();
    groupModel = new CommHistory::GroupModel();

    model->setQueryMode(CommHistory::EventModel::SyncQuery);
    groupModel->setQueryMode(CommHistory::EventModel::SyncQuery);

    if (!groupModel->getGroups(DUT_ACCOUNT_PATH, TD)) {
        connect(groupModel, SIGNAL(rowsInserted(const QModelIndex &, int, int)),
                this, SLOT(slotOnGroupInserted(const QModelIndex &, int, int)));
    } else {
        conversationUid = groupModel->group(groupModel->index(0, 0)).id();
        model->getEvents(conversationUid);
    }
}

void Ut_TextChannelListener::slotOnGroupInserted(const QModelIndex &index, int start, int end)
{
    Q_UNUSED(index);
    Q_UNUSED(end);

    CommHistory::Group group = groupModel->group(groupModel->index(start, 0));
    if (group.localUid() == DUT_ACCOUNT_PATH &&
        group.remoteUids().first() == TD) {
        conversationUid = group.id();
        model->getEvents(conversationUid);
        disconnect(groupModel, SIGNAL(rowsInserted(const QModelIndex &, int, int)),
                   this, SLOT(slotOnGroupInserted(const QModelIndex &, int, int)));
        stopEventLoop(0);
    } else {
        stopEventLoop(1);
    }
}


void Ut_TextChannelListener::slotRowsInserted(const QModelIndex &, int, int)
{
    qDebug() << "slotRowsInserted";
    stopEventLoop(0);
}

/*!
 * This function will be called after the last testfunction was executed.
 */
void Ut_TextChannelListener::cleanupTestCase()
{
    delete model;
    delete groupModel;
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

void Ut_TextChannelListener::sendMessage(const QString& message)
{
    QDBusConnection dbus_conn = QDBusConnection::sessionBus();
    QDBusMessage request = QDBusMessage::createMethodCall("com.nokia.Messaging",
                                                          "/",
                                                          "com.nokia.MessagingIf",
                                                          "sendMessage");
    request << DUT_ACCOUNT_PATH
            << TD
            << message;
    QDBusMessage reply = dbus_conn.call(request);
    QVERIFY(reply.type() == QDBusMessage::ReplyMessage);
}

void Ut_TextChannelListener::receiveMessage(const QString& message)
{
    // send using python script
    QString program = QString(PYTHON).append(" ").append(SENDMESSAGE).append(" ");
    program += QString(SERVER_DOMAIN).append(" ") +
               QString(TD_USERNAME).append(" ") +
               QString(DUT_USERNAME).append(" ") +
               QString("\"").append(message).append("\"");
    qDebug() << program;

    QVERIFY(QProcess::startDetached(program));
}

bool Ut_TextChannelListener::hasMessage(const QString& message)
{
    const int count = model->rowCount();
    for(int i = 0; i < count; i++) {
        QModelIndex index = model->index(i,0);
        if( index.isValid() ) {
            CommHistory::Event event = model->event(index);
            if( event.freeText() == message ) {
                return true;
            }
        }
    }
    return false;
}

void Ut_TextChannelListener::verifyMessage(const QString& message)
{
    // no messages were sent between td and dut
    if(conversationUid == (-1)) {
        // wait for group to be added
        QVERIFY(startEventLoop() == 0);
        // check that that message is the same that was sent
        QVERIFY(hasMessage(message));
    } else {
        // connect to rows added
        connect(model, SIGNAL(rowsInserted(const QModelIndex &, int, int)),
                this, SLOT(slotRowsInserted(const QModelIndex &, int, int)));
        // wait
        QVERIFY(startEventLoop() == 0);
        // check that added row is same message
        QVERIFY(hasMessage(message));
        disconnect(model, SIGNAL(rowsInserted(const QModelIndex &, int, int)),
        this, SLOT(slotRowsInserted(const QModelIndex &, int, int)));
    }
}

void Ut_TextChannelListener::testImSending()
{
    QString message = QString(SENT_MESSAGE) + QString(" : ") + QTime::currentTime().toString(Qt::ISODate);
    sendMessage(message);
    verifyMessage(message);
}

void Ut_TextChannelListener::testImReceiving()
{
    QString message = QString(RECEIVED_MESSAGE) + QString(" : ") + QTime::currentTime().toString(Qt::ISODate);
    receiveMessage(message);
    verifyMessage(message);
}

QTEST_MAIN(Ut_TextChannelListener)
