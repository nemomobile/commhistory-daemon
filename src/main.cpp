/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Alexander Shalamov <alexander.shalamov@nokia.com>
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

#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <MApplication>
#include <MLocale>

#include "logger.h"
#include "notificationmanager.h"
#include "commhistoryservice.h"
#include "commhistoryifadaptor.h"
#include "messagereviver.h"
#include "contactauthorizationlistener.h"
#include "connectionutils.h"

QString applicationName("commhistoryd");

using namespace RTComLogger;

#include <syslog.h>

namespace {

void messageHandler(QtMsgType type, const char *msg)
{
    const char *logLevel = "";
    int priority = LOG_DEBUG;

    switch (type) {
    case QtDebugMsg:
        // don't use any prefix for debug messages
        priority = LOG_DEBUG;
        break;
    case QtWarningMsg:
        logLevel = "Warning: ";
        priority = LOG_WARNING;
        break;
    case QtCriticalMsg:
        logLevel = "CRITICAL: ";
        priority = LOG_CRIT;
        break;
    case QtFatalMsg:
        logLevel = "FATAL: ";
        priority = LOG_ALERT;
    }

    QByteArray timestamp = QDateTime::currentDateTime().toString("ss:zzz").toLocal8Bit();
    syslog(LOG_MAKEPRI(LOG_USER, priority),
           "%s [%s] %s",
           logLevel,
           timestamp.constData(),
           msg);

    if (type == QtFatalMsg)
        abort();
}

// handle SIGTERM to cleanup on exit
static int sigtermFd[2];

void termSignalHandler(int)
{
    char a = 1;
    ::write(sigtermFd[0], &a, sizeof(a));
}

void setupSigtermHandler()
{
    struct sigaction term;

    term.sa_handler = termSignalHandler;
    sigemptyset(&term.sa_mask);
    term.sa_flags |= SA_RESTART;

    if(::sigaction(SIGTERM, &term, 0))
        qFatal("Failed setup SIGTERM signal handler");
}

}

int main(int argc, char **argv)
{
    MApplication app(argc, argv, applicationName);

#ifdef QT_DEBUG
    int logOption = LOG_NDELAY;

    if (app.arguments().contains(QLatin1String("-log-console")))
        logOption |= LOG_PERROR;
    openlog("COMMHISTORYD", logOption, 0);

    qInstallMsgHandler(messageHandler);
#endif
    qDebug() << "MApplication created";

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigtermFd))
       qFatal("Couldn't create TERM socketpair");

    QSocketNotifier *snTerm = new QSocketNotifier(sigtermFd[1], QSocketNotifier::Read, &app);
    QObject::connect(snTerm, SIGNAL(activated(int)), &app, SLOT(quit()));
    setupSigtermHandler();

    app.setQuitOnLastWindowClosed(false);

    MLocale locale;
    locale.installTrCatalog("messaging");
    locale.installTrCatalog("telephony");
    locale.installTrCatalog("mms");
    locale.installTrCatalog("presence");
    MLocale::setDefault(locale);

    qDebug() << "Translation catalogs loaded";

    CommHistoryService *service = new CommHistoryService(&app);
    new CommHistoryIfAdaptor(service);
    qDebug() << "Service created";

    ConnectionUtils *utils = new ConnectionUtils(&app);
    new ContactAuthorizationListener(utils, service);

    new Logger(utils->accountManager(), &app);
    qDebug() << "Logger created";

    NotificationManager::instance();
    qDebug() << "NotificationManager created";

    new MessageReviver(utils, &app);
    qDebug() << "Message reviver created, starting main loop";

    int result = app.exec();

    close(sigtermFd[0]);
    close(sigtermFd[1]);

    qDebug() << "exit";

    return result;
}
