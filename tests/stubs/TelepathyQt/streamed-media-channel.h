/* StreamedMedia channel client-side proxy
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

#ifndef _TelepathyQt4_streamed_media_channel_h_HEADER_GUARD_
#define _TelepathyQt4_streamed_media_channel_h_HEADER_GUARD_

#include "Channel"
#include "PendingOperation"
#include "Types"

namespace Tp
{

typedef QList<StreamedMediaStreamPtr> StreamedMediaStreams;

class StreamedMediaStream : public Object,
                    private ReadyObject
{
    Q_OBJECT
    Q_DISABLE_COPY(StreamedMediaStream)

public:
    enum SendingState {
        SendingStateNone = 0,
        SendingStatePendingSend = 1,
        SendingStateSending = 2
    };

    //~MediaStream(){}

//    StreamedMediaChannelPtr channel() const;

//    uint id() const;

//    TELEPATHY_QT4_DEPRECATED Contacts members() const;

//    ContactPtr contact() const;

    //MediaStreamState state() const;
    MediaStreamType type() const{return streamType;};

    SendingState localSendingState() const {return localState;}
//    SendingState remoteSendingState() const;
//    TELEPATHY_QT4_DEPRECATED SendingState remoteSendingState(const ContactPtr &contact) const;

//    bool sending() const;
//    bool receiving() const;

//    bool localSendingRequested() const;
//    bool remoteSendingRequested() const;

//    MediaStreamDirection direction() const;
//    MediaStreamPendingSend pendingSend() const;

    StreamedMediaStream(const StreamedMediaChannelPtr &channel): m_Channel(channel){}


    void ut_setLocalPendingState(SendingState state){localState = state;}
    void ut_setType(MediaStreamType type){streamType = type;}

private:
    StreamedMediaChannelPtr m_Channel;
    SendingState localState;
    MediaStreamType streamType;
};

class StreamedMediaChannel : public Channel
{
    Q_OBJECT
    Q_DISABLE_COPY(StreamedMediaChannel)
    Q_ENUMS(StateChangeReason)

public:
    static const Feature FeatureStreams;

    enum StateChangeReason {
        StateChangeReasonUnknown = 0,
        StateChangeReasonUserRequested = 1
    };

    StreamedMediaChannel(const QString &objectPath, const QVariantMap &immutableProperties);

    virtual ~StreamedMediaChannel();

    StreamedMediaStreams streams() const;
    StreamedMediaStreams streamsForType(MediaStreamType type) const;

    void ut_addStream();
    StreamedMediaStreams ut_streams();

Q_SIGNALS:
    void streamStateChanged(const Tp::StreamedMediaStreamPtr &stream,
            Tp::MediaStreamState state);

private:
    struct Private;
    friend struct Private;
    Private *mPriv;
};

} // Tp

#endif
