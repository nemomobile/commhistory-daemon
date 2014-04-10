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

#include "smartmessaging.h"
#include "notificationmanager.h"
#include "constants.h"
#include "debug.h"

#include "qofonomanager.h"
#include "qofonosmartmessaging.h"

#include <CommHistory/event.h>
#include <CommHistory/messagepart.h>

#define SMART_MESSAGING     "org.ofono.SmartMessaging"
#define AGENT_PATH          "/SmartMessagingAgent"
#define AGENT_SERVICE       "org.ofono.SmartMessagingAgent"

#define VCARD_CONTENT_TYPE  "text/x-vcard"
#define VCARD_EXTENSION     "vcf"

using namespace CommHistory;
using namespace RTComLogger;

SmartMessaging::SmartMessaging(QObject* parent) :
    MessageHandlerBase(parent, AGENT_PATH, AGENT_SERVICE)
{
    QOfonoManager* ofono = new QOfonoManager(this);
    connect(ofono, SIGNAL(modemAdded(QString)), this, SLOT(onModemAdded(QString)));
    connect(ofono, SIGNAL(modemRemoved(QString)), this, SLOT(onModemRemoved(QString)));
    connect(ofono, SIGNAL(availableChanged(bool)), this, SLOT(onAvailableChanged(bool)));
    QStringList modems = ofono->modems();
    DEBUG() << "SmartMessaging" << modems;
    foreach (QString path, modems) addModem(path);
}

void SmartMessaging::addModem(QString path)
{
    removeModem(path);
    QOfonoModem* modem = new QOfonoModem(this);
    modem->setModemPath(path);
    modems.insert(path, modem);
    registerAgent(modem, modem->interfaces());
    connect(modem, SIGNAL(interfacesChanged(QStringList)), this, SLOT(onInterfacesChanged(QStringList)));
}

void SmartMessaging::removeModem(QString path)
{
    QOfonoModem* modem = modems.value(path);
    if (modem) {
        modems.remove(path);
        delete modem;
    }
}

void SmartMessaging::registerAgent(QOfonoModem* modem, QStringList interfaces)
{
    if (interfaces.contains(SMART_MESSAGING)) {
        DEBUG() << "Registering SmartMessaging agent";
        QOfonoSmartMessaging messaging;
        messaging.setModemPath(modem->modemPath());
        messaging.registerAgent(AGENT_PATH);
    }
}

void SmartMessaging::onAvailableChanged(bool available)
{
    DEBUG() << "onAvailableChanged" << available;
    if (!available) {
        QStringList keys = modems.keys();
        foreach (QString path, keys) removeModem(path);
    }
}

void SmartMessaging::onModemAdded(QString path)
{
    DEBUG() << "onModemAdded" << path;
    addModem(path);
}

void SmartMessaging::onModemRemoved(QString path)
{
    DEBUG() << "onModemRemoved" << path;
    removeModem(path);
}

void SmartMessaging::onInterfacesChanged(QStringList interfaces)
{
    QOfonoModem* modem = (QOfonoModem*)sender();
    DEBUG() << "onInterfacesChanged" << modem->modemPath() << interfaces;
    registerAgent(modem, modem->interfaces());
}

void SmartMessaging::ReceiveAppointment(QByteArray, QVariantHash)
{
    DEBUG() << "ReceiveAppointment";
}

void SmartMessaging::ReceiveBusinessCard(QByteArray vcard, QVariantHash info)
{
    QString from = info.value("Sender").toString();
    DEBUG() << "ReceiveBusinessCard" << vcard.length() << "bytes from" << from;
    if (vcard.isEmpty()) {
        qWarning () << "Empty vcard";
        return;
    }

    Event event;
    event.setType(Event::SMSEvent);
    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(event.startTime());
    event.setDirection(Event::Inbound);
    event.setLocalUid(RING_ACCOUNT_PATH);
    event.setRemoteUid(from);
    event.setStatus(Event::DownloadingStatus);
    if (!setGroupForEvent(event)) {
        qCritical() << "Failed to handle group for vCard notification event; message dropped:" << event.toString();
        return;
    }

    EventModel model;
    if (!model.addEvent(event)) {
        qCritical() << "Failed to save vCard notification event; message dropped" << event.toString();
        return;
    }

    MessagePart part;
    if (!save(event.id(), vcard, part)) {
        qWarning () << "Failed to store vCard";
        model.deleteEvent(event.id());
        return;
    }

    event.setStatus(Event::ReceivedStatus);
    event.setMessageParts(QList<MessagePart>() << part);
    if (!model.modifyEvent(event)) {
        qCritical() << "Failed to update vCard event:" << event.toString();
        model.deleteEvent(event.id());
    }

    NotificationManager::instance()->showNotification(event, from, Group::ChatTypeP2P);
}

void SmartMessaging::Release()
{
    DEBUG() << "Release";
}

bool SmartMessaging::save(int id, QByteArray vcard, MessagePart& part)
{
    bool ok = false;
    if (vcard.size()) {
        QString contentId("card." VCARD_EXTENSION);
        QString path = messagePartPath(id, contentId);
        if (!path.isEmpty()) {
            QFile file(path);
            if (file.open(QIODevice::WriteOnly)) {
                if (file.write(vcard) == vcard.size()) {
                    DEBUG() << "Stored vCard to" << path;
                    part.setContentType(VCARD_CONTENT_TYPE);
                    part.setContentId(contentId);
                    part.setPath(path);
                    ok = true;
                }
                file.close();
            }
        }
    } else {
        qWarning () << "Empty vcard";
    }
    return ok;
}
