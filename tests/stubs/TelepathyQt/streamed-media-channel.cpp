#include "streamed-media-channel.h"

using namespace Tp;

const Tp::Feature Tp::StreamedMediaChannel::FeatureStreams = Tp::Feature(Tp::StreamedMediaChannel::staticMetaObject.className(), 0, true);


struct StreamedMediaChannel::Private
{
    StreamedMediaStreams m_streams;
};

StreamedMediaChannel::StreamedMediaChannel(const QString &objectPath, const QVariantMap &immutableProperties) :
        Channel(objectPath),
        mPriv(new Private)

{
    ut_setImmutableProperties(immutableProperties);
}

StreamedMediaStreams StreamedMediaChannel::streamsForType(MediaStreamType type) const
{
    Q_UNUSED(type)
    return mPriv->m_streams;
}

StreamedMediaStreams StreamedMediaChannel::streams() const
{
    return mPriv->m_streams;
}

void StreamedMediaChannel::ut_addStream()
{
    mPriv->m_streams.append(StreamedMediaStreamPtr(new StreamedMediaStream(StreamedMediaChannelPtr(this))));
}

StreamedMediaStreams StreamedMediaChannel::ut_streams()
{
    return mPriv->m_streams;
}

StreamedMediaChannel::~StreamedMediaChannel()
{
    delete mPriv;
}
