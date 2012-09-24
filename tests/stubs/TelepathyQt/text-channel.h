/* Text channel client-side proxy
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _TelepathyQt4_text_channel_h_HEADER_GUARD_
#define _TelepathyQt4_text_channel_h_HEADER_GUARD_

#include "Channel"
#include "PendingOperation"
#include "Types"
#include "Constants"

namespace Tp
{

class Message;
class ReceivedMessage;
class TextChannel;

class TextChannel : public Channel
{
    Q_OBJECT
    Q_DISABLE_COPY(TextChannel)

public:
    static const Feature FeatureMessageQueue;
    static const Feature FeatureMessageSentSignal;

    TextChannel(const QString &objectPath = QString());

    virtual ~TextChannel();

    // requires FeatureMessageQueue
    QList<ReceivedMessage> messageQueue() const;

public Q_SLOTS:
    void acknowledge(const QList<ReceivedMessage> &messages);
    void forget(const QList<ReceivedMessage> &messages);

Q_SIGNALS:
    // FeatureMessageSentSignal
    void messageSent(const Tp::Message &message,
                     Tp::MessageSendingFlags flags,
                     const QString &sentMessageToken);

    // FeatureMessageQueue
    void messageReceived(const Tp::ReceivedMessage &message);

public:
    void ut_setMessageQueue(const QList<ReceivedMessage>& receivedMessages);
    void ut_receiveMessage(const Tp::ReceivedMessage &message);
    void ut_sendMessage(const Tp::Message &message,
                        Tp::MessageSendingFlags flags,
                        const QString &sentMessageToken);

private:
    struct Private;
    friend struct Private;
    Private *mPriv;
};

} // Tp

#endif
