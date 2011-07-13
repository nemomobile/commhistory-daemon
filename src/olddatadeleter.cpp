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

#include "accountspecificcallmodel.h"
#include "olddatadeleter.h"
#include "constants.h"

/* Qt */
#include <QTimer>
#include <QDateTime>
#include <QModelIndex>
#include <QDebug>

using namespace RTComLogger;

OldDataDeleter::OldDataDeleter(QObject *parent)
    : QObject(parent)
    , m_pCallModel(0)
{
    qDebug() << Q_FUNC_INFO << "START";

    m_pCleanUpTimer = new QTimer(this);
    // Let's run first scan BOOT_CLEANUP_MS ms after boot:
    m_pCleanUpTimer->setInterval(BOOT_CLEANUP_MS);
    connect(m_pCleanUpTimer,
            SIGNAL(timeout()),
            this,
            SLOT(slotCleanUp()));
    m_pCleanUpTimer->start();

    qDebug() << Q_FUNC_INFO << "END";
}

OldDataDeleter::~OldDataDeleter()
{
    qDebug() << Q_FUNC_INFO;
}

void OldDataDeleter::slotCleanUp()
{
    qDebug() << Q_FUNC_INFO << "START";

    QDateTime currentDate = QDateTime::currentDateTime();
    QDateTime removalDate = currentDate.addDays(REMOVAL_TARGET_DAYS);

    int cleanUpInterval = DAYS_TO_MS(CLEANUP_PERIOD_DAYS);

    qDebug() << Q_FUNC_INFO << "Cleanup interval: " << cleanUpInterval;

    m_pCleanUpTimer->setInterval(cleanUpInterval);

    if (!m_pCallModel) {
        m_pCallModel = new AccountSpecificCallModel(this);
        m_pCallModel->setPropertyMask(CommHistory::Event::PropertySet()
                                      << CommHistory::Event::Type);

        connect(m_pCallModel,
                SIGNAL(modelReady(bool)),
                this,
                SLOT(slotDeleteCalls()),
                Qt::UniqueConnection);

        connect(m_pCallModel,
                SIGNAL(rowsRemoved(const QModelIndex&,int,int)),
                this,
                SLOT(slotRowsRemoved(const QModelIndex&,int,int)),
                Qt::UniqueConnection);
    }

    m_pCallModel->getEvents(removalDate);

    qDebug() << Q_FUNC_INFO << "END";
}

void OldDataDeleter::slotDeleteCalls()
{
    qDebug() << Q_FUNC_INFO << "START";

    if (!m_pCallModel) {
        qWarning() << "Null AccountSpecificCallModel!";
        return;
    }

    QList<CommHistory::Event> callsToBeDeleted;

    for (int i=0;i<m_pCallModel->rowCount();i++) {
        QModelIndex index = m_pCallModel->index(i,0);
        if (index.isValid()) {
            CommHistory::Event callEvent = m_pCallModel->event(index);
            qDebug() << Q_FUNC_INFO << "Call " << callEvent.id() << " to be deleted";
            callsToBeDeleted.append(callEvent);
        }
     }

    foreach(CommHistory::Event callEvent, callsToBeDeleted) {
        if (!m_pCallModel->deleteEvent(callEvent)) {
            qWarning() << "Error while deleting call " << callEvent.id();
        }
     }

    if (callsToBeDeleted.isEmpty()) {
        qDebug() << Q_FUNC_INFO << "No old calls to be deleted. We can delete the AccountSpecificCallModel.";
        m_pCallModel->deleteLater();
        m_pCallModel = 0;
    }

    qDebug() << Q_FUNC_INFO << "END";
}

void OldDataDeleter::slotRowsRemoved(const QModelIndex& index, int start, int end)
{
    Q_UNUSED(index)

    qDebug() << Q_FUNC_INFO << "start: " << start << " end: " << end;

    if (!m_pCallModel) {
        qWarning() << "Null AccountSpecificCallModel!";
        return;
    }

    if (m_pCallModel->rowCount() == 0) {
        qDebug() << Q_FUNC_INFO << "Last event in AccountSpecificCallModel deleted. We can delete the model.";
        m_pCallModel->deleteLater();
        m_pCallModel = 0;
    }

    qDebug() << Q_FUNC_INFO << "END";
}
