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

#ifndef OLDDATADELETER_H
#define OLDDATADELETER_H

#include <QObject>

#define DAYS_TO_MS(days) days*24*60*60*1000

class QTimer;
class QModelIndex;

namespace RTComLogger
{
    class AccountSpecificCallModel;
}

namespace RTComLogger
{

class OldDataDeleter : public QObject
{
    Q_OBJECT

public:
    OldDataDeleter(QObject *parent = 0);
    ~OldDataDeleter();

private Q_SLOTS:
    void slotCleanUp();
    void slotDeleteCalls();
    void slotRowsRemoved(const QModelIndex& index, int start, int end);

private:
    QTimer *m_pCleanUpTimer;
    RTComLogger::AccountSpecificCallModel *m_pCallModel;
};

} // namespace RTComLogger

#endif
