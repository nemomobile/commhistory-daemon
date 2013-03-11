/*
 * Copyright (C) 2013 Jolla Mobile <matthew.vogt@jollamobile.com>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

#ifndef ACCOUNTPRESENCESERVICE_H
#define ACCOUNTPRESENCESERVICE_H

#include <TelepathyQt/AccountManager>

#include <QObject>
#include <QMap>
#include <QPair>

namespace Tp { class PendingOperation; }

class AccountPresenceService : public QObject
{
    Q_OBJECT

public:
    AccountPresenceService(Tp::AccountManagerPtr manager, QObject* parent = 0);
    ~AccountPresenceService();

public Q_SLOTS:
    void setGlobalPresence(int state);
    void setGlobalPresenceWithMessage(int state, const QString &message);
    void setAccountPresence(const QString &accountUri, int state);
    void setAccountPresenceWithMessage(const QString &accountUri, int state, const QString &message);

    void accountManagerReady(Tp::PendingOperation *po);
    bool isRegistered();

    void pendingOperationCompleted(Tp::PendingOperation *po);

private:
    void globalPresenceUpdate(int state, const QString &message);
    void accountPresenceUpdate(const QString &accountUri, int state, const QString &message);

    bool presenceUpdate(Tp::AccountPtr account, const Tp::Presence &presence);
    bool presenceUpdate(Tp::AccountPtr account, const Tp::Presence &presence, bool current);

    void operationCompleted(Tp::PendingOperation *po, const QString &description);

    bool m_IsRegistered;
    Tp::AccountManagerPtr m_accountManager;
    QMap<Tp::PendingOperation*, QString> m_operations;

    typedef QPair<int, QString> UpdateDetails;

    UpdateDetails m_globalUpdate;
    bool m_globalUpdatePresent;

    QMap<QString, UpdateDetails> m_accountUpdate;
};

#endif // ACCOUNTPRESENCESERVICE_H
