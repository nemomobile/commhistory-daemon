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

// TODO: (API/ABI break) split StreamedMediaChannel in CallChannel and StreamedMediaChannel

//class StreamedMediaChannel;

typedef QList<MediaContentPtr> MediaContents;
typedef QList<MediaStreamPtr> MediaStreams;

// FIXME: (API/ABI break) Rename MediaStream to StreamedMediaStream
class MediaStream : public QObject,
                    private ReadyObject,
                    public RefCounted
{
    Q_OBJECT
    Q_DISABLE_COPY(MediaStream)

public:
    enum SendingState {
        SendingStateNone = 0,
        SendingStatePendingSend = 1,
        SendingStateSending = 2
    };

    //~MediaStream(){}

    MediaContentPtr content() const {return m_MediaContent;}

//    StreamedMediaChannelPtr channel() const;

//    uint id() const;

//    TELEPATHY_QT4_DEPRECATED Contacts members() const;

//    ContactPtr contact() const;

    //MediaStreamState state() const;
    //MediaStreamType type() const;

    SendingState localSendingState() const {return localState;}
//    SendingState remoteSendingState() const;
//    TELEPATHY_QT4_DEPRECATED SendingState remoteSendingState(const ContactPtr &contact) const;

//    bool sending() const;
//    bool receiving() const;

//    bool localSendingRequested() const;
//    bool remoteSendingRequested() const;

//    MediaStreamDirection direction() const;
//    MediaStreamPendingSend pendingSend() const;

    MediaStream(const MediaContentPtr &content): m_MediaContent(content){}


    void ut_setLocalPendingState(SendingState state){localState = state;}

private:
    MediaContentPtr m_MediaContent;
    SendingState localState;
};

// FIXME: (API/ABI break) Remove MediaContent
class MediaContent : public QObject,
                    private ReadyObject,
                    public RefCounted
{
    Q_OBJECT
    Q_DISABLE_COPY(MediaContent)

public:
      //      ~MediaContent(){}

//    StreamedMediaChannelPtr channel() const;

//    QString name() const;
    MediaStreamType type() const {return Tp::MediaStreamTypeAudio;}
//    ContactPtr creator() const;

    MediaStreams streams() const {return m_streams;}

// test methods
    void ut_addStream()
    {
        m_streams.append(MediaStreamPtr(new MediaStream(MediaContentPtr(this))));
    }

private:
    friend class StreamedMediaChannel;

    MediaContent()
    {
    }

    MediaStreams m_streams;

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
    MediaContents contentsForType(MediaStreamType type) const;

    //MediaStreams streams() const;
    //MediaStreams streamsForType(MediaStreamType type) const;

    MediaContents& ut_mediaContents();
    void ut_addContent();

Q_SIGNALS:
    void streamStateChanged(const Tp::MediaStreamPtr &stream,
            Tp::MediaStreamState state);

private:
    struct Private;
    friend struct Private;
    Private *mPriv;
};

} // Tp

#endif
