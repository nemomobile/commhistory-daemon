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
#include <TelepathyQt/PendingOperation>
#include <TelepathyQt/PendingReady>

#include <QtDBus>

AccountPresenceService::AccountPresenceService(Tp::AccountManagerPtr manager, QObject* parent)
    : QObject(parent),
      m_IsRegistered(false),
      m_accountManager(manager),
      m_globalUpdatePresent(false)
{
    if (!m_accountManager) {
        qCritical() << "ERROR: Cannot provide service without Account Manager!";
        return;
    }

    if (!QDBusConnection::sessionBus().isConnected()) {
        qCritical() << "ERROR: No DBus session bus found!";
        return;
    }

    if (!QDBusConnection::sessionBus().registerObject(ACCOUNT_PRESENCE_OBJECT_PATH, this)) {
        qWarning() << "Object registration failed!";
    } else {
        if(!QDBusConnection::sessionBus().registerService(ACCOUNT_PRESENCE_SERVICE_NAME)) {
            qWarning() << "Unable to register account presence service!"
                       << QDBusConnection::sessionBus().lastError();
        } else {
            m_IsRegistered = true;
        }
    }

    if (!m_accountManager->isReady()) {
        // Wait for the account manager to become ready
        qDebug() << "Waiting for account manager to become ready";
        Tp::PendingReady *pr = m_accountManager->becomeReady(Tp::AccountManager::FeatureCore);
        Q_ASSERT(pr);
        connect(pr, SIGNAL(finished(Tp::PendingOperation *)),
                this, SLOT(accountManagerReady(Tp::PendingOperation *)));
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
    if (m_accountManager->isReady()) {
        globalPresenceUpdate(state, message);
    } else {
        // Defer this update - any previous deferred updates are superseded
        m_globalUpdate = qMakePair(state, message);
        m_globalUpdatePresent = true;
        m_accountUpdate.clear();
    }
}

void AccountPresenceService::setAccountPresence(const QString &accountUri, int state)
{
    setAccountPresenceWithMessage(accountUri, state, QString());
}

void AccountPresenceService::setAccountPresenceWithMessage(const QString &accountUri, int state, const QString &message)
{
    if (m_accountManager->isReady()) {
        accountPresenceUpdate(accountUri, state, message);
    } else {
        // Defer this update - any previous deferred updates for this account are superseded
        m_accountUpdate.insert(accountUri, qMakePair(state, message));
    }
}

void AccountPresenceService::accountManagerReady(Tp::PendingOperation *)
{
    if (m_accountManager->isReady()) {
        qDebug() << "Account manager is now ready";

        // Process any updates that were previously deferred
        if (m_globalUpdatePresent) {
            m_globalUpdatePresent = false;
            globalPresenceUpdate(m_globalUpdate.first, m_globalUpdate.second);
        }

        QMap<QString, UpdateDetails>::const_iterator it = m_accountUpdate.constBegin(), end = m_accountUpdate.constEnd();
        for ( ; it != end; ++it) {
            const UpdateDetails &details(it.value());
            accountPresenceUpdate(it.key(), details.first, details.second);
        }
        m_accountUpdate.clear();
    } else {
        qCritical() << "ERROR: Cannot provide service without Account Manager readiness!";
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

void AccountPresenceService::globalPresenceUpdate(int state, const QString &message)
{
    Tp::Presence presence = presenceValue(state, message);
    if (presence.isValid()) {
        // Set all enabled accounts to have the same presence information
        foreach (Tp::AccountPtr account, m_accountManager->enabledAccounts()->accounts()) {
            if (!presenceUpdate(account, presence)) {
                qWarning() << "Unable to set global presence for account:" << account->displayName();
            }
        }
    } else {
        qWarning() << "Unable to set global presence to invalid state:" << state;
    }
}

void AccountPresenceService::accountPresenceUpdate(const QString &accountUri, int state, const QString &message)
{
    Tp::AccountPtr account = m_accountManager->accountForPath(accountUri);
    if (account && account->isValidAccount()) {
        Tp::Presence presence = presenceValue(state, message);
        if (presence.isValid()) {
            if (!presenceUpdate(account, presence)) {
                qWarning() << "Unable to set presence for account:" << account->displayName();
            }
        } else {
            qWarning() << "Unable to set account presence to invalid state:" << state << "for account:" << account->displayName();
        }
    } else {
        qWarning() << "Unable to identify account to set presence:" << accountUri;
    }
}

bool AccountPresenceService::presenceUpdate(Tp::AccountPtr account, const Tp::Presence &presence)
{
    if (account->isOnline()) {
        // Ignore any error from setting the automatic presence
        presenceUpdate(account, presence, false);
        return presenceUpdate(account, presence, true);
    } else {
        // Ignore any error from setting the current presence
        presenceUpdate(account, presence, true);
        return presenceUpdate(account, presence, false);
    }
}

bool AccountPresenceService::presenceUpdate(Tp::AccountPtr account, const Tp::Presence &presence, bool current)
{
    Tp::PendingOperation *po = 0;

    if (current) {
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

