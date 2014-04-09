/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2014 Jolla Ltd.
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

#ifndef MESSAGEHANDLERBASE_H
#define MESSAGEHANDLERBASE_H

#include <QObject>

namespace CommHistory {
    class Event;
    class GroupManager;
}

// Base class for MmsHandler and SmartMessaging
class MessageHandlerBase : public QObject
{
    Q_OBJECT

protected:
    MessageHandlerBase(QObject* parent, QString path, QString service);

    bool isRegistered() const { return m_isRegistered; }
    bool setGroupForEvent(CommHistory::Event& event);

    static QString sanitizeName(QString name);
    static QString messagePartPath(int eventId, QString contentId);

private:
    bool m_isRegistered;
    CommHistory::GroupManager* groupManager;
};

#endif // MESSAGEHANDLERBASE_H
