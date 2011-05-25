#include "text-channel.h"
#include "Message"

using namespace Tp;

const Tp::Feature Tp::TextChannel::FeatureMessageQueue = Tp::Feature(Tp::TextChannel::staticMetaObject.className(), 0, true);
const Tp::Feature Tp::TextChannel::FeatureMessageSentSignal = Tp::Feature(Tp::TextChannel::staticMetaObject.className(), 1, true);

struct TextChannel::Private
{
    QList<ReceivedMessage> m_messageQueue;
};

TextChannel::TextChannel(const QString &objectPath) :
        Channel(objectPath),
        mPriv(new Private)

{
}

QList<ReceivedMessage> TextChannel::messageQueue() const
{
    return mPriv->m_messageQueue;
}

void TextChannel::acknowledge(const QList<ReceivedMessage> &messages)
{
    foreach(ReceivedMessage msg, messages) {
        mPriv->m_messageQueue.removeOne(msg);
    }
}

void TextChannel::forget(const QList<ReceivedMessage> &messages)
{
    foreach(ReceivedMessage msg, messages) {
        mPriv->m_messageQueue.removeOne(msg);
    }
}

void TextChannel::ut_setMessageQueue(const QList<ReceivedMessage>& receivedMessages)
{
    mPriv->m_messageQueue = receivedMessages;
}

void TextChannel::ut_receiveMessage(const Tp::ReceivedMessage &message)
{
    mPriv->m_messageQueue.append(message);
    emit messageReceived(message);
}

void TextChannel::ut_sendMessage(const Tp::Message &message,
                    Tp::MessageSendingFlags flags,
                    const QString &sentMessageToken)
{
    emit messageSent(message, flags, sentMessageToken);
}

TextChannel::~TextChannel()
{
    delete mPriv;
}
