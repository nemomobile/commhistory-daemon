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

#ifndef NOTIFICATIONGROUP_H
#define NOTIFICATIONGROUP_H

#include <QObject>
#include <QString>
#include <QMetaType>

#include "serialisable.h"

namespace RTComLogger {

class NotificationGroup : public QObject, public Serialisable
{
    Q_OBJECT

    Q_PROPERTY(int type READ type WRITE setType)

public:
    NotificationGroup(QObject* parent = 0);
    explicit NotificationGroup(int notificationGroupType,
                               QObject* parent = 0);
    NotificationGroup(const NotificationGroup& other);
    NotificationGroup& operator = (const NotificationGroup& other);
    bool operator == (const NotificationGroup& other) const;
    bool operator != (const NotificationGroup& other) const;
    bool isValid() const;

public:

    int type() const;
    void setType(int notificationType);

private:
    int m_type;
};

} // namespace

Q_DECLARE_METATYPE(RTComLogger::NotificationGroup)

uint qHash(const RTComLogger::NotificationGroup& group);

QDataStream& operator<<(QDataStream &out, const RTComLogger::NotificationGroup &key);
QDataStream& operator>>(QDataStream &in, RTComLogger::NotificationGroup &key);

#endif // NOTIFICATIONGROUP_H
