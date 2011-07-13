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

#ifndef COMMHISTORYSERVICE_H
#define COMMHISTORYSERVICE_H

#include <QObject>

class CommHistoryService : public QObject
{
    Q_OBJECT

public:
    CommHistoryService( QObject* parent = 0 );
    ~CommHistoryService();

public Q_SLOTS:
    /*! \brief emits signal that authorisation dialog should be shown for contact */
    void activateAuthorization(const QString& contactId, const QString& accountPath,
                               const QString& filename, const QString& message,
                               const QString& transactionId,
                               const QString& accountUniqueIdentifier);

    bool isRegistered();

Q_SIGNALS:
    void showAuthorizationDialog(const QString& contactId,
                                 const QString& accountPath,
                                 const QString& filename,
                                 const QString& message,
                                 const QString& transactionId,
                                 const QString& accountUniqueIdentifier);

private:
    bool m_IsRegistered;
};

#endif // COMMHISTORYSERVICE_H
