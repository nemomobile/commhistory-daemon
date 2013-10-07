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

#include "notificationgroup.h"
#include "personalnotification.h"
#include "notificationmanager.h"
#include "locstrings.h"
#include "constants.h"
#include "debug.h"

#include <MLocale>
#include <MRemoteAction>
#include <MNotification>
#include <MNotificationGroup>

#include <CommHistory/commonutils.h>
#include <CommHistory/Event>

using namespace RTComLogger;

NotificationGroup::NotificationGroup(int type, QObject *parent)
    : QObject(parent), m_type(type), mGroup(0)
{
    updateTimer.setInterval(0);
    updateTimer.setSingleShot(true);
    connect(&updateTimer, SIGNAL(timeout()), SLOT(updateGroup()));

    connect(this, SIGNAL(changed()), SLOT(updateGroupLater()));
}

NotificationGroup::NotificationGroup(MNotificationGroup *group, QObject *parent)
    : QObject(parent), m_type(eventType(group->eventType())), mGroup(group)
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

int NotificationGroup::type() const
{
    return m_type;
}

MNotificationGroup *NotificationGroup::notificationGroup()
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

    QString name;

    // update group notification
    bool grouped = mNotifications.size() > 1;
    // get group message
    QString message = notificationGroupText();
    // get group action
    QString groupAction = NotificationManager::instance()->action(this, mNotifications[0], grouped);

    ML10N::MLocale tempLocale;
    // update group
    if (type() != CommHistory::Event::VoicemailEvent)
        name = tempLocale.joinStringList(contactNames());

    if (!mGroup)
        mGroup = new MNotificationGroup(groupType(type()));

    if (mGroup->summary() == name && mGroup->body() == message)
    {
        DEBUG() << Q_FUNC_INFO << "Suppressing unnecessary notification update";
    } else {
        DEBUG() << "Publishing group:" << name << message << groupAction << mNotifications.size();

        mGroup->setSummary(name);
        mGroup->setBody(message);
        mGroup->setAction(MRemoteAction(groupAction));
        mGroup->setCount(mNotifications.size());
        // Publish to update lock screen item. Empty strings suppress any preview banner, which is sent with
        // individual notifications
        mGroup->publish(QString(), QString());
    }

    // Publish all notifications marked as pending
    // XXX does this show a banner for changes? maybe suppressed by existing id
    foreach (PersonalNotification *pn, mNotifications) {
        if (pn->hasPendingEvents()) {
            pn->publishNotification(this);
        }
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
        QString name = pn->notificationName();
        if (!names.contains(name))
            names.append(name);
    }

    return names;
}

QString NotificationGroup::notificationGroupText()
{
    QString message;
    int notifications = mNotifications.size();
    if (!notifications)
        return QString();

    switch (type())
    {
        case CommHistory::Event::IMEvent:
        case CommHistory::Event::SMSEvent:
        case CommHistory::Event::MMSEvent:
        {
            if (notifications > 1)
                message = txt_qtn_msg_notification_new_message(notifications);
            else
                message = mNotifications[0]->notificationText();
            break;
        }
        case CommHistory::Event::CallEvent:
        {
            message = txt_qtn_call_missed(notifications);
            break;
        }
        case CommHistory::Event::VoicemailEvent:
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
        mGroup->remove();
        delete mGroup;
        mGroup = 0;
    }

    foreach (PersonalNotification *n, mNotifications)
        removeNotification(n);
}

void NotificationGroup::addNotification(PersonalNotification *notification)
{
    if (mNotifications.contains(notification))
        return;

    // If notification->hasPendingEvents, the updateGroup slot will also publish the notification
    connect(notification, SIGNAL(hasPendingEventsChanged(bool)), SLOT(onNotificationChanged()));
    mNotifications.append(notification);
    emit changed();
}

bool NotificationGroup::removeNotification(PersonalNotification *notification)
{
    if (mNotifications.removeOne(notification)) {
        notification->removeNotification();
        delete notification;
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

