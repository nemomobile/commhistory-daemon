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

#include "loggerclientobserver.h"
#include "logger.h"

#include <TelepathyQt4/Channel>

using namespace RTComLogger;

LoggerClientObserver::LoggerClientObserver(const Tp::ChannelClassSpecList &channelFilter,
                                           Logger* logger)
        : QObject(logger),
          Tp::AbstractClientObserver(channelFilter, true),
          m_pLogger(logger)
{
}


void LoggerClientObserver::observeChannels(const Tp::MethodInvocationContextPtr<> &context,
            const Tp::AccountPtr &account,
            const Tp::ConnectionPtr &connection,
            const QList<Tp::ChannelPtr> &channels,
            const Tp::ChannelDispatchOperationPtr &dispatchOperation,
            const QList<Tp::ChannelRequestPtr> &requestsSatisfied,
            const Tp::AbstractClientObserver::ObserverInfo &observerInfo)
{
    Q_UNUSED(dispatchOperation)
    Q_UNUSED(requestsSatisfied)
    Q_UNUSED(observerInfo)
    Q_UNUSED(connection)

    qDebug() << "LoggerClientObserver::observeChannels";

    if(m_pLogger) {
        foreach(Tp::ChannelPtr channel, channels) {
            QVariantMap properties = channel->immutableProperties();
            QString channelType = properties.value(QLatin1String(TELEPATHY_INTERFACE_CHANNEL ".ChannelType")).toString();
            if( !channelType.isNull() && !channelType.isEmpty()) {
                m_pLogger->createChannelListener(channelType, context, account, channel);
            }
        }
    }
}
