#include "streamed-media-channel.h"

using namespace Tp;

const Tp::Feature Tp::StreamedMediaChannel::FeatureStreams = Tp::Feature(Tp::StreamedMediaChannel::staticMetaObject.className(), 0, true);


struct StreamedMediaChannel::Private
{
    MediaContents m_mediaContents;
};

StreamedMediaChannel::StreamedMediaChannel(const QString &objectPath, const QVariantMap &immutableProperties) :
        Channel(objectPath, 0),
        mPriv(new Private)

{
    ut_setImmutableProperties(immutableProperties);
}

MediaContents StreamedMediaChannel::contentsForType(MediaStreamType type) const
{
    Q_UNUSED(type)
    return mPriv->m_mediaContents;
}

MediaContents& StreamedMediaChannel::ut_mediaContents()
{
    return mPriv->m_mediaContents;
}

void StreamedMediaChannel::ut_addContent()
{
    mPriv->m_mediaContents.append(MediaContentPtr(new MediaContent()));
}


StreamedMediaChannel::~StreamedMediaChannel()
{
    delete mPriv;
}
