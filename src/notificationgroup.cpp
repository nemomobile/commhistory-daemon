/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2013 Jolla Ltd.
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: John Brooks <john.brooks@jolla.com>
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

#include "notificationgroup.h"
#include "personalnotification.h"
#include "notificationmanager.h"
#include "locstrings.h"
#include "constants.h"
#include "debug.h"

#include <notification.h>

#include <MLocale>

#include <CommHistory/commonutils.h>
#include <CommHistory/Event>

using namespace RTComLogger;

ML10N::MLocale mLocale;

NotificationGroup::NotificationGroup(PersonalNotification::EventCollection collection, const QString &localUid, const QString &remoteUid, QObject *parent)
    : QObject(parent), m_collection(collection), m_localUid(localUid), m_remoteUid(remoteUid), mGroup(0)
{
    updateTimer.setInterval(0);
    updateTimer.setSingleShot(true);
    connect(&updateTimer, SIGNAL(timeout()), SLOT(updateGroup()));

    connect(this, SIGNAL(changed()), SLOT(updateGroupLater()));
}

NotificationGroup::~NotificationGroup()
{
    qDeleteAll(mNotifications);
    delete mGroup;
}

QString NotificationGroup::groupType(int eventType)
{
    for (int i = 0; i < _eventTypesCount; i++) {
        if (_eventTypes[i].type == eventType)
            return QLatin1String(_eventTypes[i].event);
    }
    return QString();
}

int NotificationGroup::eventType(const QString &groupType)
{
    for (int i = 0; i < _eventTypesCount; i++) {
        if (_eventTypes[i].event == groupType)
            return _eventTypes[i].type;
    }
    return -1;
}

PersonalNotification::EventCollection NotificationGroup::collection() const
{
    return m_collection;
}

const QString &NotificationGroup::localUid() const
{
    return m_localUid;
}

const QString &NotificationGroup::remoteUid() const
{
    return m_remoteUid;
}

Notification *NotificationGroup::notification()
{
    if (!mGroup && !mNotifications.isEmpty())
        updateGroup();
    return mGroup;
}

QList<PersonalNotification*> NotificationGroup::notifications() const
{
    return mNotifications;
}

void NotificationGroup::updateGroup()
{
    if (mNotifications.isEmpty()) {
        removeGroup();
        return;
    }

    // Publish group notification, not including preview banners/sounds.
    if (!mGroup)
        mGroup = new Notification(this);

    mGroup->setAppName(txt_qtn_msg_notifications_group);
    mGroup->setCategory("x-nemo.messaging.group");
    mGroup->setBody(notificationGroupText());
    mGroup->setItemCount(mNotifications.size());
    mGroup->setHintValue("x-nemo-hidden", mNotifications.size() < 2);
    NotificationManager::instance()->setNotificationProperties(mGroup, mNotifications[0],
            countConversations() > 1);

    mGroup->setSummary(mLocale.joinStringList(contactNames()));
    mGroup->publish();

    foreach (PersonalNotification *pn, mNotifications) {
        if (pn->hasPendingEvents())
            pn->publishNotification();
    }
}

void NotificationGroup::updateGroupLater()
{
    updateTimer.start();
}

QStringList NotificationGroup::contactNames()
{
    QStringList names;
    foreach (PersonalNotification *pn, mNotifications) {
        // events are ordered from most recent to oldest
        QString name = pn->notificationName();
        if (!names.contains(name)) {
            names.prepend(name);
        } else if (names.size() > 1) {
            names.removeOne(name);
            names.prepend(name);
        }
    }
    return names;
}

int NotificationGroup::countConversations()
{
    QSet<QPair<QString, QString> > seen;
    foreach (PersonalNotification *pn, mNotifications)
        seen.insert(qMakePair(pn->account(), pn->remoteUid()));
    return seen.count();
}

QString NotificationGroup::notificationGroupText()
{
    QString message;
    int notifications = mNotifications.size();
    if (!notifications)
        return QString();

    switch (m_collection)
    {
        case PersonalNotification::Messaging:
        {
            if (notifications > 1)
                message = txt_qtn_msg_notification_new_message(notifications);
            else
                message = mNotifications[0]->notificationText();
            break;
        }
        case PersonalNotification::Voice:
        {
            message = txt_qtn_call_missed(notifications);
            break;
        }
        case PersonalNotification::Voicemail:
        {
            // The amount of new / not listened voicemails
            message = mNotifications[0]->notificationText();
            break;
        }
        default:
            break;
    }

    return message;
}

void NotificationGroup::removeGroup()
{
    if (mGroup) {
        mGroup->close();
        delete mGroup;
        mGroup = 0;
    }

    while (!mNotifications.isEmpty())
        removeNotification(mNotifications.first());
}

void NotificationGroup::addNotification(PersonalNotification *notification)
{
    if (mNotifications.contains(notification))
        return;

    // If notification->hasPendingEvents, the updateGroup slot will also publish the notification
    connect(notification, SIGNAL(hasPendingEventsChanged(bool)), SLOT(onNotificationChanged()));
    mNotifications.append(notification);

    if (mNotifications.count() > 1) {
        // Hide the member notification
        notification->setHidden(true);

        // Also hide the first member, which would not have been hidden on addition
        mNotifications.first()->setHidden(true);
    }

    emit changed();
}

bool NotificationGroup::removeNotification(PersonalNotification *&notification)
{
    if (mNotifications.removeOne(notification)) {
        notification->removeNotification();
        delete notification;
        notification = 0;

        if (mNotifications.count() == 1) {
            // Hide the member notification
            mNotifications.first()->setHidden(false);
        }

        emit changed();
        return true;
    }

    return false;
}

void NotificationGroup::onNotificationChanged()
{
    PersonalNotification *pn = qobject_cast<PersonalNotification*>(sender());
    if (!pn || !mNotifications.contains(pn))
        return;

    if (pn->hasPendingEvents())
        emit changed();
}

