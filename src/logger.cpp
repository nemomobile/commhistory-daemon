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

#include <QtDBus/QtDBus>
#include <QDebug>

#include <TelepathyQt4/ClientRegistrar>
#include <TelepathyQt4/Debug>
#include <TelepathyQt4/Channel>
#include <TelepathyQt4/ChannelClassSpec>

#include "logger.h"
#include "channellistener.h"
#include "textchannellistener.h"
#include "streamchannellistener.h"
#include "loggerclientobserver.h"
#include "messagereviver.h"

#define COMMHISTORY_CHANNEL_OBSERVER QLatin1String("CommHistory")

using namespace RTComLogger;
using namespace Tp;

Logger::Logger(const Tp::AccountManagerPtr &accountManager,
               MessageReviver *reviver,
               QObject *parent)
    : QObject(parent), m_Reviver(reviver)
{
    Tp::registerTypes();
#ifdef QT_DEBUG
    Tp::enableDebug(true);
    Tp::enableWarnings(true);
#endif

    m_Registrar = ClientRegistrar::create(accountManager);

    ChannelClassSpecList channelFilters;
    channelFilters << ChannelClassSpec::textChat()
                   << ChannelClassSpec::textChatroom()
                   << ChannelClassSpec::unnamedTextChat()
                   << ChannelClassSpec::streamedMediaCall();

    LoggerClientObserver* observer = new LoggerClientObserver(channelFilters, this );
    bool registered = m_Registrar->registerClient(
      AbstractClientPtr::dynamicCast(SharedPtr<LoggerClientObserver>(observer)),
      COMMHISTORY_CHANNEL_OBSERVER);

    if(registered) {
        qDebug() << "commhistoryd: started";
    } else {
        qCritical() << "commhistoryd: observer registration failed";
    }
}

Logger::~Logger()
{
}

void Logger::createChannelListener(const QString &channelType,
                                   const Tp::MethodInvocationContextPtr<> &context,
                                   const Tp::AccountPtr &account,
                                   const Tp::ChannelPtr &channel)
{
    qDebug() << __PRETTY_FUNCTION__;

    QString channelObjectPath = channel->objectPath();

    if ( m_Channels.contains( channelObjectPath ) &&
         !channelType.isEmpty() &&
         !channelObjectPath.isEmpty() ) {
         context->setFinishedWithError(QLatin1String(TELEPATHY_ERROR_INVALID_ARGUMENT), QString());
         return;
    }

    connect(channel.data(),
            SIGNAL( invalidated(Tp::DBusProxy*, const QString&, const QString& ) ),
            this,
            SLOT( slotInvalidated(Tp::DBusProxy*, const QString&, const QString& ) ) );

    qDebug() << "creating listener for: " << channelObjectPath << " type " << channelType;

    ChannelListener* listener = 0;
    if( channelType == QLatin1String(TELEPATHY_INTERFACE_CHANNEL_TYPE_TEXT) ) {
        listener = new TextChannelListener(account, channel, context, this);
        connect(listener, SIGNAL(savingFailed(const Tp::ConnectionPtr&)),
                m_Reviver, SLOT(checkConnection(const Tp::ConnectionPtr&)));
    } else if ( channelType == QLatin1String(TELEPATHY_INTERFACE_CHANNEL_TYPE_STREAMED_MEDIA) ) {
        listener = new StreamChannelListener(account, channel, context, this);
    }

    if(listener) {
        connect(listener, SIGNAL(channelClosed(ChannelListener *)),
                this, SLOT(channelClosed(ChannelListener *)));
        m_Channels.append( channelObjectPath );
    }
}

void Logger::channelClosed(ChannelListener *listener)
{
    qDebug() << __FUNCTION__ << "Got channelClosed signal from listener. Deleting listener.";

    if(listener) {
        listener->deleteLater();
        listener = 0;
    }
}

void Logger::slotInvalidated(Tp::DBusProxy *proxy,
            const QString &errorName, const QString &errorMessage)
{
    qDebug() << __PRETTY_FUNCTION__ << proxy->objectPath();

    Q_UNUSED(proxy)
    Q_UNUSED(errorName)
    Q_UNUSED(errorMessage)

    Tp::Channel *channel = qobject_cast<Tp::Channel *>(sender());
    if (channel) {
        qDebug() << __PRETTY_FUNCTION__ << "Removing " << channel->objectPath() << " from channels list";
        m_Channels.removeAll(channel->objectPath());
    }

    disconnect(channel,
            SIGNAL( invalidated(Tp::DBusProxy*, const QString&, const QString& ) ),
            this,
            SLOT( slotInvalidated(Tp::DBusProxy*, const QString&, const QString& ) ) );
}
