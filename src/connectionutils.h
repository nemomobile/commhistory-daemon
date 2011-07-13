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

#ifndef CONNECTIONUTILS_H
#define CONNECTIONUTILS_H

#include <QObject>

// Tp includes
#include <TelepathyQt4/Connection>
#include <TelepathyQt4/AccountManager>
#include <TelepathyQt4/Account>
#include <TelepathyQt4/PendingOperation>

namespace RTComLogger
{

class ConnectionUtils : public QObject
{
    Q_OBJECT

public:
    ConnectionUtils(QObject* parent = 0);

    Tp::AccountManagerPtr accountManager() const;

Q_SIGNALS:
    void connectionReady(const Tp::ConnectionPtr& connection);

private Q_SLOTS:
        void slotAccountManagerReady(Tp::PendingOperation *);
        void slotAccountReady(Tp::PendingOperation *);
        void slotAddAccount(const Tp::AccountPtr &account);
        void slotAccountValidityChanged(bool valid);
        void slotConnectionChanged(const Tp::ConnectionPtr &connection);
        void slotConnectionReady(Tp::PendingOperation* operation);

private:
        void prepareAccounts();
        void prepareAccount(const Tp::AccountPtr &account);
        void prepareConnection(const Tp::AccountPtr &account);

private:
        Tp::AccountManagerPtr m_AccountManager;
};

} // namespace RTComLogger

#endif // CONNECTIONUTILS_H
