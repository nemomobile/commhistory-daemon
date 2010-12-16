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

#ifndef UT_TEXTCHANNELLISTENER_H
#define UT_TEXTCHANNELLISTENER_H

// INCLUDES
#include <QObject>
#include <QEventLoop>

#include <CommHistory/GroupModel>
#include <CommHistory/ConversationModel>

namespace RTComLogger {

class Ut_TextChannelListener : public QObject
{
    Q_OBJECT
public:
    Ut_TextChannelListener();
    ~Ut_TextChannelListener();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

// Test functions
private Q_SLOTS:
    void testImSending();
    void testImReceiving();

protected Q_SLOTS:
    void slotOnGroupInserted(const QModelIndex &index, int start, int end);
    void slotRowsInserted(const QModelIndex &, int, int);

private:
    int startEventLoop();
    void stopEventLoop(int returnCode);
    void sendMessage(const QString& message);
    void receiveMessage(const QString& message);
    bool hasMessage(const QString& message);
    void verifyMessage(const QString& message);

private:
    CommHistory::ConversationModel* model;
    CommHistory::GroupModel* groupModel;
    int conversationUid;
    QEventLoop eventLoop;
};

}
#endif // UT_TEXTCHANNELLISTENER_H
