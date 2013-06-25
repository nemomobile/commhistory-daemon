/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2013 Jolla.
** Contact: Aaron McCarthy <aaron.mccarthy@jollamobile.com>
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

#include "voicecountersservice.h"
#include "constants.h"

#include <QtCore/QSettings>
#include <QtCore/QCoreApplication>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusError>
#include <Qt5Sparql/QSparqlConnection>
#include <Qt5Sparql/QSparqlQuery>
#include <Qt5Sparql/QSparqlResult>

#include <QtCore/QDebug>

VoiceCountersService::VoiceCountersService(QObject *parent)
:   QObject(parent), m_isRegistered(false)
{
    if (!QDBusConnection::sessionBus().isConnected()) {
        qCritical() << "ERROR: No DBus session bus found!";
        return;
    }

    if (!QDBusConnection::sessionBus().registerObject(VOICE_COUNTERS_OBJECT_PATH, this)) {
        qWarning() << "Object registration failed!";
    } else {
        if (!QDBusConnection::sessionBus().registerService(VOICE_COUNTERS_SERVICE_NAME)) {
            qWarning() << "Unable to register voice counters service!"
                       << QDBusConnection::sessionBus().lastError();
        } else {
            m_isRegistered = true;
        }
    }
}

VoiceCountersService::~VoiceCountersService()
{
    QDBusConnection::sessionBus().unregisterObject(VOICE_COUNTERS_OBJECT_PATH);
    QDBusConnection::sessionBus().unregisterService(VOICE_COUNTERS_SERVICE_NAME);
}

bool VoiceCountersService::isRegistered() const
{
    return m_isRegistered;
}

void VoiceCountersService::addReceivedCall(const QDateTime &startTime, const QDateTime &endTime)
{
    QSettings settings(QLatin1String("nemomobile"), QLatin1String("commhistoryd"));
    settings.beginGroup(VOICE_COUNTERS_GROUP);

    int received = settings.value(VOICE_COUNTERS_RECEIVED).toInt();
    received += startTime.secsTo(endTime);

    settings.setValue(VOICE_COUNTERS_RECEIVED, received);
    settings.setValue(VOICE_COUNTERS_LASTUPDATED, endTime);
    settings.endGroup();

    emit countersChanged();
}

void VoiceCountersService::addDialledCall(const QDateTime &startTime, const QDateTime &endTime)
{
    QSettings settings(QLatin1String("nemomobile"), QLatin1String("commhistoryd"));
    settings.beginGroup(VOICE_COUNTERS_GROUP);

    int dialled = settings.value(VOICE_COUNTERS_DIALLED).toInt();
    dialled += startTime.secsTo(endTime);

    settings.setValue(VOICE_COUNTERS_DIALLED, dialled);
    settings.setValue(VOICE_COUNTERS_LASTUPDATED, endTime);
    settings.endGroup();

    emit countersChanged();
}

VoiceCountersService *VoiceCountersService::instance()
{
    return qApp->findChild<VoiceCountersService *>();
}

QVariantMap VoiceCountersService::getCounters()
{
    QSettings settings(QLatin1String("nemomobile"), QLatin1String("commhistoryd"));

    settings.beginGroup(VOICE_COUNTERS_GROUP);
    int dialled = settings.value(VOICE_COUNTERS_DIALLED).toInt();
    int received = settings.value(VOICE_COUNTERS_RECEIVED).toInt();
    QDateTime lastUpdated = settings.value(VOICE_COUNTERS_LASTUPDATED,
                                           QDateTime::fromMSecsSinceEpoch(0).toUTC()).toDateTime();
    settings.endGroup();

    QSparqlConnection connection(QLatin1String("QTRACKER_DIRECT"));

    QString queryString =
        QString::fromLatin1("SELECT ?dialled ?received "
                            "WHERE {"
                            "    {"
                            "        SELECT (SUM(nmo:duration(?message)) AS ?dialled)"
                            "        WHERE {"
                            "            ?message a nmo:Call ;"
                            "            nmo:isSent false ."
                            "            FILTER( nmo:receivedDate(?message) > \"%1\" )"
                            "        }"
                            "    }"
                            "    {"
                            "        SELECT (SUM(nmo:duration(?message)) AS ?received)"
                            "        WHERE {"
                            "            ?message a nmo:Call ;"
                            "            nmo:isSent true ."
                            "            FILTER( nmo:receivedDate(?message) > \"%1\" )"
                            "        }"
                            "    }"
                            "}").arg(lastUpdated.toString(Qt::ISODate));

    QSparqlQuery query;
    query.setQuery(queryString);
    QSparqlResult *result = connection.exec(query);
    result->waitForFinished();

    if (result->size() == 1) {
        result->first();
        dialled += result->value(0).toInt();
        received += result->value(1).toInt();
    }

    QVariantMap counters;
    counters.insert(QLatin1String("dialled"), dialled);
    counters.insert(QLatin1String("received"), received);
    counters.insert(QLatin1String("all"), dialled + received);

    return counters;
}

void VoiceCountersService::resetCounters()
{
    QSettings settings(QLatin1String("nemomobile"), QLatin1String("commhistoryd"));

    settings.beginGroup(VOICE_COUNTERS_GROUP);
    settings.setValue(VOICE_COUNTERS_DIALLED, 0);
    settings.setValue(VOICE_COUNTERS_RECEIVED, 0);
    settings.setValue(VOICE_COUNTERS_LASTUPDATED, QDateTime::currentDateTimeUtc());
    settings.endGroup();

    emit countersChanged();
}
