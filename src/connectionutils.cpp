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

#include "connectionutils.h"

// Tp
#include <TelepathyQt4/PendingReady>
#include <TelepathyQt4/AccountSet>

using namespace RTComLogger;

ConnectionUtils::ConnectionUtils(QObject* parent) : QObject(parent)
{
    qDebug() << Q_FUNC_INFO;

    Tp::registerTypes();

    m_AccountManager = Tp::AccountManager::create();

    if (!m_AccountManager.isNull()
        && !m_AccountManager->isReady()) {
        connect(m_AccountManager->becomeReady(),
                SIGNAL(finished(Tp::PendingOperation *)),
                SLOT(slotAccountManagerReady(Tp::PendingOperation *)));
    } else {
        prepareAccounts();
    }
}

Tp::AccountManagerPtr ConnectionUtils::accountManager() const
{
    return m_AccountManager;
}

void ConnectionUtils::prepareAccounts()
{
    qDebug() << Q_FUNC_INFO;

    if(!m_AccountManager.isNull() && m_AccountManager->isReady()){
        // connect to new account signals
        connect(m_AccountManager.data(),
                SIGNAL(newAccount(const Tp::AccountPtr &)),
                SLOT(slotAddAccount(const Tp::AccountPtr &)),
                Qt::UniqueConnection);

        // initialise valid accounts
        foreach(const Tp::AccountPtr &account, m_AccountManager->validAccounts()->accounts()) {
            prepareAccount(account);
        }
    }
}

void ConnectionUtils::slotAccountManagerReady(Tp::PendingOperation *op)
{
    qDebug() << Q_FUNC_INFO;

    if (!op->isError() && m_AccountManager->isReady()) {
        prepareAccounts();
    } else {
        qWarning() << "Account manager not ready";
    }
}

void ConnectionUtils::prepareAccount(const Tp::AccountPtr &account)
{
    if(!account.isNull() && account->isReady()) {
        prepareConnection(account);
    } else if(!account.isNull() && !account->isReady()) {
        Tp::PendingOperation* po = account->becomeReady();
        connect(po,
                SIGNAL(finished(Tp::PendingOperation *)),
                SLOT(slotAccountReady(Tp::PendingOperation *)));
    }
}

void ConnectionUtils::slotAccountReady( Tp::PendingOperation *op )
{
    if (op && !op->isError()) {
        Tp::PendingReady *pr = qobject_cast<Tp::PendingReady *>( op );
        if ( !pr ) {
            return;
        }

        Tp::AccountPtr account =
                Tp::AccountPtr::qObjectCast(pr->proxy());
        if ( !account.isNull() && account->isReady() ) {
            prepareConnection(account);
        }
    }
}

void ConnectionUtils::slotAccountValidityChanged(bool valid)
{
    if (valid) {
        Tp::Account *account = qobject_cast<Tp::Account *>(sender());
        QString path = account->objectPath();
        Tp::AccountPtr accountPtr = m_AccountManager->accountForPath(path);
        slotAddAccount(accountPtr);
    }
}

void ConnectionUtils::slotAddAccount(const Tp::AccountPtr &account)
{
    if(!m_AccountManager.isNull()) {
        prepareAccount(account);
    }
}

void ConnectionUtils::prepareConnection(const Tp::AccountPtr &account)
{
    connect(account.data(),
            SIGNAL(validityChanged(bool)),
            SLOT(slotAccountValidityChanged(bool)),
            Qt::UniqueConnection);

    Tp::ConnectionPtr connection = account->connection();

    if(!connection.isNull()) {
        qDebug() << Q_FUNC_INFO << "Connection object exists";
        connect(connection->becomeReady(Tp::Connection::FeatureSimplePresence),
                SIGNAL(finished(Tp::PendingOperation*)),
                SLOT(slotConnectionReady(Tp::PendingOperation*)));
    } else {
        connect(account.data(),
                SIGNAL(connectionChanged(const Tp::ConnectionPtr &)),
                SLOT(slotConnectionChanged(const Tp::ConnectionPtr &)));
    }
}

void ConnectionUtils::slotConnectionChanged(const Tp::ConnectionPtr &connection)
{
    qDebug() << Q_FUNC_INFO;

    Tp::Account *account = qobject_cast<Tp::Account *>(sender());

    if(account != 0 && account->isValid()) {
        if(!connection.isNull()) {
            qDebug() << Q_FUNC_INFO << "Connection object exists";
            connect(connection->becomeReady(Tp::Connection::FeatureSimplePresence),
                    SIGNAL(finished(Tp::PendingOperation*)),
                    SLOT(slotConnectionReady(Tp::PendingOperation*)));
        }
    }
}

void ConnectionUtils::slotConnectionReady(Tp::PendingOperation* operation)
{
    qDebug() << Q_FUNC_INFO;

    if(operation && !operation->isError()) {
        Tp::PendingReady *pr = qobject_cast<Tp::PendingReady*>(operation);
        if(pr){
            Tp::ConnectionPtr connection = Tp::ConnectionPtr::qObjectCast(pr->proxy());

            if (!connection.isNull() && connection->isValid()) {
                emit connectionReady(connection);
            }
        }
    }
}
