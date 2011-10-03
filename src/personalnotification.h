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

#ifndef PERSONALNOTIFICATION_H
#define PERSONALNOTIFICATION_H

#include <QObject>
#include <QString>
#include <QMetaType>

#include "serialisable.h"

// commhistory
#include <CommHistory/Group>

namespace RTComLogger {

class PersonalNotification : public QObject, public Serialisable
{
    Q_OBJECT

    Q_PROPERTY(QString remoteuid READ remoteUid WRITE setRemoteUid)
    Q_PROPERTY(QString account READ account WRITE setAccount)

    // can't use CommHistory::Event::EventType because it cannot be
    // properly declared as a Q_ENUM, because CommHistory::Event is not a
    // QObject.
    Q_PROPERTY(uint eventType READ eventType WRITE setEventType)
    Q_PROPERTY(QString targetId READ targetId WRITE setTargetId)
    // ...and the same goes for Group::ChatType.
    Q_PROPERTY(uint chatType READ chatType WRITE setChatType)

    Q_PROPERTY(int contactid READ contactId WRITE setContactId)
    Q_PROPERTY(QString notificationtext READ notificationText
                                        WRITE setNotificationText)
    Q_PROPERTY(bool haspendingevents READ hasPendingEvents
                                     WRITE setHasPendingEvents)
    Q_PROPERTY(QString chatName READ chatName
                                WRITE setChatName)
    Q_PROPERTY(QString eventToken READ eventToken
                                  WRITE setEventToken)
    Q_PROPERTY(QString smsReplaceNumber READ smsReplaceNumber
                                        WRITE setSmsReplaceNumber)

public:
    PersonalNotification(QObject* parent = 0);
    PersonalNotification(const QString& remoteUid,
                         const QString& account,
                         CommHistory::Event::EventType eventType = CommHistory::Event::UnknownType,
                         const QString& targetId = QString(),
                         CommHistory::Group::ChatType chatType = CommHistory::Group::ChatTypeP2P,
                         int contactId = 0,
                         const QString& lastNotification = QString(),
                         QObject* parent = 0);
    PersonalNotification(const PersonalNotification& other);
    PersonalNotification& operator = (const PersonalNotification& other);
    bool operator == (const PersonalNotification& other) const;
    bool operator != (const PersonalNotification& other) const;

public:
    QString remoteUid() const;
    QString account() const;
    uint eventType() const;
    /* remoteUid = sender of the message, targetId = channel target id.
       In P2P conversations they are the same, in Skype MUC targetId =
       persistent room id. TargetId is used for Telepathy channel
       requests. */
    QString targetId() const;
    uint chatType() const;
    int contactId() const;
    QString notificationText() const;
    bool hasPendingEvents() const;
    QString chatName() const;
    QString eventToken() const;
    QString smsReplaceNumber() const;

    void setRemoteUid(const QString& remoteUid);
    void setAccount(const QString& account);
    void setEventType(uint eventType);
    void setTargetId(const QString& targetId);
    void setChatType(uint chatType);
    void setContactId(int contactId);
    void setNotificationText(const QString& notificationText);
    void setHasPendingEvents(bool hasPendingEvents = true);
    void setChatName(const QString& chatName);
    void setEventToken(const QString& eventToken);
    void setSmsReplaceNumber(const QString& number);

private:
    QString m_remoteUid;
    QString m_account;
    uint m_eventType;
    QString m_targetId;
    uint m_chatType;
    int m_contactId;
    QString m_notificationText;
    bool m_hasPendingEvents;
    QString m_chatName;
    QString m_eventToken;
    QString m_smsReplaceNumber;
};

} // namespace

Q_DECLARE_METATYPE(RTComLogger::PersonalNotification)

QDataStream& operator<<(QDataStream &out, const RTComLogger::PersonalNotification &key);
QDataStream& operator>>(QDataStream &in, RTComLogger::PersonalNotification &key);

#endif // PERSONALNOTIFICATION_H
