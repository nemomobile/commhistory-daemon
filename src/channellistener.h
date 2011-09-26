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

#ifndef CHANNEL_LISTENER_H
#define CHANNEL_LISTENER_H

#include <CommHistory/Event>
#include <TelepathyQt4/Types>
#include <TelepathyQt4/Channel>
#include <TelepathyQt4/Account>

namespace CommHistory {
    class EventModel;
}

namespace RTComLogger
{

/*!
 * \class ChannelListener
 * \brief Base class for channel listeners. Handles basic channel, connection
 * preparation, error handling.
 */
class ChannelListener : public QObject
{
    Q_OBJECT

public:
    /*!
     * \brief Constructor
     * \param account, Tp Account that is used for that communication channel
     * \param channel, channel object representing communication channel
     * \param invocation context, obect that could be used to tell if invocation
     * was successful or not
     * \param parent
     */
    explicit ChannelListener(const Tp::AccountPtr &account,
                    const Tp::ChannelPtr &channel,
                    const Tp::MethodInvocationContextPtr<> &context,
                    QObject *parent = 0);

    virtual ~ChannelListener();

    /*!
     * \brief returns dbus object path of channel that we are listening to
     * \return string containing dbus object path of the Tp::Channel
     */
    QString channel() const;

    /*!
     * \brief returns target id from immutable properties
     * \return string with target id
     */
    QString targetId() const;

public Q_SLOTS:
    /*!
     * \brief emits channelClosed signal
     */
    void closed();

Q_SIGNALS:
    /*!
     * \brief emitted to notify logger that channel was closed and ChannelListener
     * could be deleted
     */
    void channelClosed(ChannelListener *listener);

protected:
    void makeChannelReady(const Tp::Features &features);
    virtual void channelReady();

    void invocationContextFinished();
    void invocationContextError();

    CommHistory::EventModel& eventModel();

    /*!
     * \brief helper methods, checks if ChannelListener is ready, if there is
     * an error or returns channel type
     */
    bool checkIfError(Tp::PendingOperation* operation);

    /*!
     * \brief handles error. Tells to invocation context object that invocation
     * was unsuccessful and then closes channel. closed();
     */
    virtual void finishedWithError(const QString& errorName, const QString& errorMessage);

protected Q_SLOTS:
    /*!
     * \brief called by Tp when channel becomes invalid
     */
    virtual void invalidated(Tp::DBusProxy *proxy,
        const QString &errorName, const QString &errorMessage);

private Q_SLOTS:
    /*!
     * \brief Slots to handle different stages of channel preparation
     */
    void slotChannelReady(Tp::PendingOperation* operation);

protected:
    Tp::AccountPtr m_Account;
    Tp::ChannelPtr m_Channel;
    Tp::ConnectionPtr m_Connection;
    Tp::MethodInvocationContextPtr<> m_InvocationContext;
    CommHistory::Event::EventDirection m_Direction;
    CommHistory::EventModel* m_pEventModel;
};

} // namespace RTComLogger

#endif // CHANNEL_LISTENER_H
