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

#ifndef LOGGERCLIENTOBSERVER_H
#define LOGGERCLIENTOBSERVER_H

#include <QObject>

#include <TelepathyQt4/AbstractClientObserver>
#include <TelepathyQt4/Types>

namespace RTComLogger
{
    class Logger;

/*!
 * \class LoggerClientObserver
 * \brief realisation of Tp::AbstractClientObserver
 * observeChannels is called, when there are new channels to be observed
 */
class LoggerClientObserver : public QObject,
                             public Tp::AbstractClientObserver
{
    Q_OBJECT

public:
    /*!
     * \brief Constructor
     * \param filter list, passed to the base class, defines filter for channels
     * we are interested in
     * \param instance of logger. When new channel should be observed, logger
     * interface is used to create new channel listeners
     */
    explicit LoggerClientObserver(const Tp::ChannelClassSpecList &channelFilter,
                         Logger *logger);

    /*!
     * \brief Realisation of Tp::AbstractClientObserver
     */
    virtual void observeChannels(const Tp::MethodInvocationContextPtr<> &context,
            const Tp::AccountPtr &account,
            const Tp::ConnectionPtr &connection,
            const QList<Tp::ChannelPtr> &channels,
            const Tp::ChannelDispatchOperationPtr &dispatchOperation,
            const QList<Tp::ChannelRequestPtr> &requestsSatisfied,
            const Tp::AbstractClientObserver::ObserverInfo &observerInfo);

private:
    Logger* m_pLogger;
};

} // namespace RTComLogger

#endif // LOGGERCLIENTOBSERVER_H
