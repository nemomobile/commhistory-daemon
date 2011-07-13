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

#ifndef UT_STREAMCHANNELLISTENER_H
#define UT_STREAMCHANNELLISTENER_H

// INCLUDES
#include <QObject>
#include <QEventLoop>

#include <CommHistory/CallModel>

namespace RTComLogger {

class Ut_StreamChannelListener : public QObject
{
    Q_OBJECT
public:
    Ut_StreamChannelListener();
    ~Ut_StreamChannelListener();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

// Test functions
private Q_SLOTS:
    void invalidated_data();
    void invalidated();

    void normalCall_data();
    void normalCall();
    void emergency_data();
    void emergency();
};

}
#endif // UT_STREAMCHANNELLISTENER_H
