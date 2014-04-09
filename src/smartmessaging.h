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

#ifndef SMARTMESSAGING_H
#define SMARTMESSAGING_H

#include "qofonomodem.h"

namespace CommHistory {
    class Event;
    class GroupManager;
    class MessagePart;
}

class SmartMessaging: public QObject
{
    Q_OBJECT

public:
    SmartMessaging(QObject *parent);

public Q_SLOTS:
    void onAvailableChanged(bool available);
    void onModemAdded(QString path);
    void onModemRemoved(QString path);
    void onInterfacesChanged(QStringList interfaces);
    void ReceiveAppointment(QByteArray appointment, QVariantHash info);
    void ReceiveBusinessCard(QByteArray card, QVariantHash info);
    void Release();

private:
    void addModem(QString path);
    void removeModem(QString path);
    void registerAgent(QOfonoModem* modem, QStringList interfaces);
    bool setGroupForEvent(CommHistory::Event &event);

private:
    CommHistory::GroupManager *groupManager;
    QHash<QString,QOfonoModem*> modems;
};

#endif // SMARTMESSAGING_H
