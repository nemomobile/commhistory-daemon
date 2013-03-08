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

#include "accountpresenceservice.h"
#include "constants.h"

#include <TelepathyQt/Account>
#include <TelepathyQt/AccountSet>

#include <QtDBus>

AccountPresenceService::AccountPresenceService(Tp::AccountManagerPtr manager, QObject* parent)
    : QObject(parent),
      m_IsRegistered(false),
      m_accountManager(manager)
{
    if (!QDBusConnection::sessionBus().isConnected()) {
        qCritical() << "ERROR: No DBus session bus found!";
        return;
    }

    if (parent) {
        if(!QDBusConnection::sessionBus().registerObject(ACCOUNT_PRESENCE_OBJECT_PATH, this)) {
            qWarning() << "Object registration failed!";
        } else {
            if(!QDBusConnection::sessionBus().registerService(ACCOUNT_PRESENCE_SERVICE_NAME)) {
                qWarning() << "Unable to register account presence service!"
                           << QDBusConnection::sessionBus().lastError();
            } else {
                m_IsRegistered = true;
            }
        }
    }
}

AccountPresenceService::~AccountPresenceService()
{
    QDBusConnection::sessionBus().unregisterObject(ACCOUNT_PRESENCE_OBJECT_PATH);
    QDBusConnection::sessionBus().unregisterService(ACCOUNT_PRESENCE_SERVICE_NAME);
}

namespace {

Tp::Presence presenceValue(int state, const QString &message)
{
    switch (state) {
        // Values from QtMobility QContactPresence, as exported via SeasidePerson
        case 1: return Tp::Presence::available(message);
        case 2: return Tp::Presence::hidden(message);
        case 3: return Tp::Presence::busy(message);
        case 4: return Tp::Presence::away(message);
        case 5: return Tp::Presence::xa(message);
        case 6: return Tp::Presence::offline(message);
        default: break;
    }

    return Tp::Presence();
}

}

void AccountPresenceService::setGlobalPresence(int state)
{
    setGlobalPresenceWithMessage(state, QString());
}

void AccountPresenceService::setGlobalPresenceWithMessage(int state, const QString &message)
{
    Tp::Presence presence = presenceValue(state, message);
    if (presence.isValid()) {
        // Set all enabled accounts to have the same presence information
        foreach (Tp::AccountPtr account, m_accountManager->enabledAccounts()->accounts()) {
            if (!setAccountPresence(account, presence)) {
                qWarning() << "Unable to set global presence for account:" << account->displayName();
            }
        }
    } else {
        qWarning() << "Unable to set global presence to invalid state:" << state;
    }
}

void AccountPresenceService::setAccountPresence(const QString &accountUri, int state)
{
    setAccountPresenceWithMessage(accountUri, state, QString());
}

void AccountPresenceService::setAccountPresenceWithMessage(const QString &accountUri, int state, const QString &message)
{
    Tp::AccountPtr account = m_accountManager->accountForPath(accountUri);
    if (account && account->isValidAccount()) {
        Tp::Presence presence = presenceValue(state, message);
        if (presence.isValid()) {
            if (!setAccountPresence(account, presence)) {
                qWarning() << "Unable to set presence for account:" << account->displayName();
            }
        } else {
            qWarning() << "Unable to set account presence to invalid state:" << state << "for account:" << account->displayName();
        }
    } else {
        qWarning() << "Unable to identify account to set presence:" << accountUri;
    }
}

bool AccountPresenceService::isRegistered()
{
    return m_IsRegistered;
}

void AccountPresenceService::pendingOperationCompleted(Tp::PendingOperation *po)
{
    QMap<Tp::PendingOperation*, QString>::iterator it = m_operations.find(po);
    if (it != m_operations.end()) {
        operationCompleted(po, it.value());
        m_operations.erase(it);
    }
}

bool AccountPresenceService::setAccountPresence(Tp::AccountPtr account, const Tp::Presence &presence)
{
    Tp::PendingOperation *po = 0;

    if (account->isOnline()) {
        po = account->setRequestedPresence(presence);
    } else {
        po = account->setAutomaticPresence(presence);
    }

    if (!po)
        return false;

    QString description(QString("set presence for account: %1").arg(account->displayName()));

    if (po->isFinished()) {
        operationCompleted(po, description);
    } else {
        m_operations.insert(po, description);
        connect(po, SIGNAL(finished(Tp::PendingOperation*)),
                this, SLOT(pendingOperationCompleted(Tp::PendingOperation*)));
    }

    return !po->isError();
}

void AccountPresenceService::operationCompleted(Tp::PendingOperation *po, const QString &description)
{
    if (po->isError()) {
        qWarning() << "Unable to" << description << "error:" << QString("'%1' [%2]").arg(po->errorMessage(), po->errorName());
    }
}

