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

#ifndef CONTACTAUTHORIZATIONLISTENER_H
#define CONTACTAUTHORIZATIONLISTENER_H

#include <QObject>
#include <QStringList>

// Tp
#include <TelepathyQt4/Connection>
#include <TelepathyQt4/AccountManager>

#include "contactauthorizer.h"

namespace RTComLogger
{
class ConnectionUtils;

class ContactAuthorizationListener : public QObject
{
    Q_OBJECT

public:
    explicit ContactAuthorizationListener(ConnectionUtils *connectionUtils,
                                          QObject *parent = 0);
    virtual ~ContactAuthorizationListener();

private Q_SLOTS:
    void slotConnectionReady(const Tp::ConnectionPtr& connection);
    void slotAccountStateChanged(bool isEnabled);
    void slotAccountRemoved();
    void slotShowUnableToAuthorizeDialog(const QString&, const QString&, const QString&,
                                         const QString&, const QString&, const QString&);
    void slotAccountConnectionStatusChanged(Tp::ConnectionStatus connectionStatus);
    void slotRemoveAuthorizer(QObject *removedAuthorizer);

private:
    ConnectionUtils *m_pConnectionUtils;
    QMap<QString, ContactAuthorizer*> m_authorizers;
    QMap<QString, Tp::AccountPtr> m_accounts;
};

} // namespace RTComLogger

#endif // CONTACTAUTHORIZATIONLISTENER_H
