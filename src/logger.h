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

#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <TelepathyQt4/Types>

namespace RTComLogger {

class ChannelListener;
class MessageReviver;

/*!
 * \class Logger
 * \brief Class that registers itself as Telepathy observer and responsible
 * for lifycycle of channel listeners
 */
class Logger : public QObject
{
    Q_OBJECT

public:
    explicit Logger(const Tp::AccountManagerPtr &accountManager,
                    MessageReviver *reviver,
                    QObject *parent = 0);
    ~Logger();

    /*!
     * \brief Creates new channel listener, if same listener already exist
     * method returns
     */
    void createChannelListener(const QString &channelType,
                               const Tp::MethodInvocationContextPtr<> &context,
                               const Tp::AccountPtr &account,
                               const Tp::ChannelPtr &channel);

public Q_SLOTS:
    /*!
     * \brief Slot for notifying logger, that channel is closed and listener is
     * no longer needed. In this slot, ChannelLister would be deleted.
     * \param Pointer to ChannelListener
     */
    void channelClosed(ChannelListener *listener);

private Q_SLOTS:
    void slotInvalidated(Tp::DBusProxy *proxy, const QString &errorName, const QString &errorMessage);

private:
    QStringList m_Channels;
    Tp::ClientRegistrarPtr m_Registrar;
    MessageReviver *m_Reviver;
};

} // namespace RTComLogger

#endif // LOGGER_H
