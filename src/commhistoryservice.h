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
#include <QVariantList>

class CommHistoryService : public QObject
{
    Q_OBJECT

public:
    ~CommHistoryService();

    static CommHistoryService *instance();

    bool isRegistered();

    bool callHistoryObserved() const { return m_callHistoryObserved; }
    bool inboxObserved() const { return m_inboxObserved; }
    QString inboxFilterAccount() const { return m_inboxFilterAccount; }
    QVariantList observedConversations() const { return m_observedConversations; }

public Q_SLOTS:
    /*! \brief emits signal that authorisation dialog should be shown for contact */
    void activateAuthorization(const QString& contactId, const QString& accountPath,
                               const QString& filename, const QString& message,
                               const QString& transactionId,
                               const QString& accountUniqueIdentifier);
    void setCallHistoryObserved(bool observed);
    void setInboxObserved(bool observed, const QString &filterAccount = QString());
    void setObservedConversations(const QVariantList &conversations);

Q_SIGNALS:
    void showAuthorizationDialog(const QString& contactId,
                                 const QString& accountPath,
                                 const QString& filename,
                                 const QString& message,
                                 const QString& transactionId,
                                 const QString& accountUniqueIdentifier);
    void callHistoryObservedChanged(bool observed);
    void inboxObservedChanged(bool observed, const QString &filterAccount);
    void observedConversationsChanged(const QVariantList &conversations);

private:
    bool m_IsRegistered;
    bool m_callHistoryObserved;
    bool m_inboxObserved;
    QString m_inboxFilterAccount;
    QVariantList m_observedConversations;

    CommHistoryService( QObject* parent = 0 );
};

#endif // COMMHISTORYSERVICE_H
