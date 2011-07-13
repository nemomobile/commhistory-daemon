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

#include <QDebug>
#include <QDBusConnection>
#include <QDBusPendingReply>
#include <QCoreApplication>

#include "mwilistener.h"
#include "constants.h"

using namespace RTComLogger;

MWIListener::MWIListener(QObject *parent) :
    QObject(parent),
    m_countsLoaded(false),
    m_savePending(false),
    m_simSettings(CSD_SIM_SERVICE_NAME,
                  CSD_SIM_OBJECT_PATH,
                  CSD_SIM_SETTINGS_INTERFACE,
                  QDBusConnection::systemBus())
{
    QDBusConnection dbus = QDBusConnection::systemBus();
    dbus.connect(CSD_SIM_SERVICE_NAME,
                 CSD_SIM_OBJECT_PATH,
                 CSD_SIM_INTERFACE,
                 CSD_SIM_STATUS_SIGNAL,
                 this,
                 SLOT(onSIMStatusChanged(const QString&)));

    QDBusInterface sim(CSD_SIM_SERVICE_NAME,
                       CSD_SIM_OBJECT_PATH,
                       CSD_SIM_INTERFACE,
                       dbus);

    QDBusPendingCall pcall = sim.asyncCall(CSD_SIM_GET_SIM_STATUS);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pcall, this);
    QObject::connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
                     this, SLOT(onGetSIMStatus(QDBusPendingCallWatcher*)));
}

void MWIListener::onGetSIMStatus(QDBusPendingCallWatcher *call)
{
    QDBusPendingReply<QString> reply = *call;

    if (reply.isError()) {
        qWarning() << "Failed to get SIM status" << reply.error();
    } else {
        QString simStatus = reply.value();

        qDebug() << Q_FUNC_INFO << simStatus;

        if (simStatus == CSD_SIM_STATUS_OK)
            loadMWI();
    }

    call->deleteLater();
}

void MWIListener::onSIMStatusChanged(const QString &simStatus)
{
    qDebug() << Q_FUNC_INFO << simStatus;

    if (simStatus == CSD_SIM_STATUS_REMOVED
        || simStatus == CSD_SIM_STATUS_NO_SIM)
        updateCount(0);
    else if (simStatus == CSD_SIM_STATUS_OK)
        loadMWI();
}

void MWIListener::saveMWI(int count)
{
    qDebug() << Q_FUNC_INFO << count;

    updateCount(count);

    if (m_countsLoaded)
        doSaveMWI();
    else
        m_savePending = true;
}

void MWIListener::doSaveMWI()
{
    QDBusPendingCall pcall = m_simSettings.asyncCall(CSD_SIM_SET_MWI_METHOD,
                                                     m_counts.voicemail,
                                                     m_counts.fax,
                                                     m_counts.email,
                                                     m_counts.other);

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pcall, this);
    QObject::connect(watcher,
                     SIGNAL(finished(QDBusPendingCallWatcher*)),
                     this,
                     SLOT(onSetMWICount(QDBusPendingCallWatcher*)));

    m_savePending = false;
}

void MWIListener::onSetMWICount(QDBusPendingCallWatcher *call)
{
    QDBusPendingReply<> reply = *call;

    if (reply.isError()) {
        qDebug() << "Failed to set MWI counts" << reply.error();
    }

    call->deleteLater();
}

void MWIListener::loadMWI()
{
    QDBusPendingCall pcall = m_simSettings.asyncCall(CSD_SIM_GET_MWI_METHOD);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pcall, this);
    QObject::connect(watcher,
                     SIGNAL(finished(QDBusPendingCallWatcher*)),
                     this,
                     SLOT(onGetMWICount(QDBusPendingCallWatcher*)));
}

void MWIListener::onGetMWICount(QDBusPendingCallWatcher *call)
{
    QDBusPendingReply<int,int,int,int> reply = *call;

    if (reply.isError()) {
        qDebug() << "Failed to get MWI counts" << reply.error();
        updateCount(0);
    } else {
        updateCount(reply.argumentAt<0>());
        // keep other counts for correct SIM update
        m_counts.fax = reply.argumentAt<1>();
        m_counts.email = reply.argumentAt<2>();
        m_counts.other = reply.argumentAt<3>();

        m_countsLoaded = true;

        if (m_savePending)
            doSaveMWI();
    }

    call->deleteLater();
}

int MWIListener::MWICount() const
{
    qDebug() << Q_FUNC_INFO << m_counts.voicemail;

    return m_counts.voicemail;
}

void MWIListener::updateCount(int count)
{
    if (m_counts.voicemail != count) {
        m_counts.voicemail = count;
        emit MWICountChanged(MWICount());
    }
}
