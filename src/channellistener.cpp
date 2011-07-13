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

#include "channellistener.h"
#include "constants.h"

#include <QDebug>
#include <CommHistory/EventModel>

#include <TelepathyQt4/PendingReady>
#include <TelepathyQt4/TextChannel>
#include <TelepathyQt4/StreamedMediaChannel>
#include <TelepathyQt4/Channel>
#include <TelepathyQt4/ContactManager>
#include <TelepathyQt4/PendingContacts>
#include <TelepathyQt4/Types>
#include <TelepathyQt4/Contact>

using namespace RTComLogger;

ChannelListener::ChannelListener(const Tp::AccountPtr &account,
                                 const Tp::ChannelPtr &channel,
                                 const Tp::MethodInvocationContextPtr<> &context,
                                 QObject *parent)
    : QObject(parent), m_Account(account), m_Channel(channel), m_InvocationContext(context),
      m_pEventModel(0)
{
    connect(m_Account.data(),
            SIGNAL(invalidated(Tp::DBusProxy*, const QString&, const QString&)),
            this,
            SLOT(invalidated(Tp::DBusProxy*, const QString&, const QString&)));
}

void ChannelListener::slotChannelReady(Tp::PendingOperation* operation)
{
    qDebug() << __PRETTY_FUNCTION__ << channel();

    if (!checkIfError(operation) && m_Channel) {
        m_Direction = m_Channel->isRequested() ?
            CommHistory::Event::Outbound :
            CommHistory::Event::Inbound;
        m_Connection = m_Channel->connection(); //ready channel has ready connection
        channelReady();
    } else {
        qCritical() << "Channel is not ready";
    }

}

bool ChannelListener::checkIfError(Tp::PendingOperation* operation)
{
    bool error = true;

    if( operation && !operation->isError() ) {
        error = false;
    } else if( operation ) {
        finishedWithError( operation->errorName(),
                           operation->errorMessage() );
    }
    return error;
}

void ChannelListener::invalidated(Tp::DBusProxy *proxy,
            const QString &errorName, const QString &errorMessage)
{
    qDebug() << __PRETTY_FUNCTION__ << proxy->objectPath();

    Q_UNUSED(proxy)
    finishedWithError(errorName, errorMessage);
}

void ChannelListener::finishedWithError(const QString& errorName,
                                        const QString& errorMessage)
{
    if( !m_InvocationContext.isNull() ) {
        m_InvocationContext->setFinishedWithError(errorName, errorMessage);
    }
    closed();
}

ChannelListener::~ChannelListener()
{
}

QString ChannelListener::channel() const
{
    QString channel;
    if(!m_Channel.isNull()) {
        channel = m_Channel->objectPath();
    }
    return channel;
}

void ChannelListener::closed()
{
    qDebug() << "channelClosed";
    emit channelClosed(this);
}

void ChannelListener::makeChannelReady(const Tp::Features &features)
{
    if (m_Channel) {
        connect(m_Channel->becomeReady(features),
                SIGNAL(finished(Tp::PendingOperation*)),
                this,
                SLOT(slotChannelReady(Tp::PendingOperation*)));
        connect(m_Channel.data(),
                SIGNAL( invalidated(Tp::DBusProxy*, const QString&, const QString& ) ),
                this,
                SLOT( invalidated(Tp::DBusProxy*, const QString&, const QString& ) ) );
    } else {
        qCritical() << Q_FUNC_INFO << "NULL channel provided to listener";
    }
}

void ChannelListener::invocationContextFinished()
{
    if(!m_InvocationContext.isNull()) {
        m_InvocationContext->setFinished();
    }
}

void ChannelListener::invocationContextError()
{
    finishedWithError(QLatin1String(TELEPATHY_ERROR_INVALID_ARGUMENT),QString());
}

CommHistory::EventModel& ChannelListener::eventModel()
{
    if(!m_pEventModel){
        m_pEventModel = new CommHistory::EventModel(this);
    }

    return *m_pEventModel;
}

QString ChannelListener::targetId() const
{
    QString targetId;

    QVariantMap properties = m_Channel->immutableProperties();

    if (properties.contains(TELEPATHY_CHANNEL_INTERFACE_PERSISTENT_ID)) {
        targetId = properties.value(TELEPATHY_CHANNEL_INTERFACE_PERSISTENT_ID).toString();
    } else if (properties.contains(QLatin1String(TELEPATHY_INTERFACE_CHANNEL ".TargetID"))) {
        targetId = properties.value(QLatin1String(TELEPATHY_INTERFACE_CHANNEL ".TargetID")).toString();
    }

    return targetId;
}

void ChannelListener::channelReady()
{
    qDebug() << Q_FUNC_INFO << "not implemented";
}
