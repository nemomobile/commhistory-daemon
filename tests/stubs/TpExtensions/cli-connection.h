#ifndef _CommHistoryTp_Connection_HEADER_GUARD_
#define _CommHistoryTp_Connection_HEADER_GUARD_


#include "TelepathyQt4/Connection"

#include <QtGlobal>

#include <QString>
#include <QObject>
#include <QVariant>

#include <QDBusPendingReply>

#include "TelepathyQt4/AbstractInterface"
#include "TelepathyQt4/DBusProxy"
#include <QDebug>

// basically the same as GLib's G_GNUC_DEPRECATED
#ifndef TELEPATHY_GNUC_DEPRECATED
#   if defined(Q_CC_GNUC) && __GNUC__ >= 4
#       define TELEPATHY_GNUC_DEPRECATED __attribute__((__deprecated__))
#   else
#       define TELEPATHY_GNUC_DEPRECATED
#   endif
#endif
namespace CommHistoryTp
{
namespace Client
{


/**
 * \class ConnectionInterfaceStoredMessagesInterface
 *
 * Proxy class providing a 1:1 mapping of the D-Bus interface "com.nokia.Telepathy.Connection.Interface.StoredMessages."
 */
class  ConnectionInterfaceStoredMessagesInterface : public Tp::AbstractInterface
{
    Q_OBJECT

public:
    /**
     * Returns the name of the interface "com.nokia.Telepathy.Connection.Interface.StoredMessages", which this class
     * represents.
     *
     * \return The D-Bus interface name.
     */

    static inline const char *staticInterfaceName()
    {
        return "com.nokia.Telepathy.Connection.Interface.StoredMessages";
    }

    ConnectionInterfaceStoredMessagesInterface(QObject* parent = 0)
    {
        Q_UNUSED(parent)
    }

    QStringList& ut_getExpungedMessages()
    {
        return m_ExpungedMessages;
    }

    QStringList& ut_getDeliveredMessages()
    {
        return m_DeliveredMessages;
    }

public Q_SLOTS:
    /**
     * Begins a call to the D-Bus method "DeliverStoredMessages" on the remote object.
     *
     * <p>Ask server to deliver again stored messages.</p>
     *
     * \param storedMessageTokens
     *
     *     The values of &quot;message-token&quot; key of permanently stored messages.
     */
    inline QDBusPendingReply<> DeliverStoredMessages(const QStringList& storedMessageTokens)
    {
        m_DeliveredMessages << storedMessageTokens;

        QList<QVariant> argumentList;
        argumentList << QVariant::fromValue(storedMessageTokens);
        return asyncCallWithArgumentList(QLatin1String("DeliverStoredMessages"), argumentList);
    }

    /**
     * Begins a call to the D-Bus method "ExpungeMessages" on the remote object.
     *
     * <p>Expunge messages from permanent storage.</p>
     *
     * \param storedMessageTokens
     *
     *     The values of 'message-token' key of permanently stored messages.
     */
    inline QDBusPendingReply<> ExpungeMessages(const QStringList& storedMessageTokens)
    {
        m_ExpungedMessages << storedMessageTokens;

        QList<QVariant> argumentList;
        argumentList << QVariant::fromValue(storedMessageTokens);
        return asyncCallWithArgumentList(QLatin1String("ExpungeMessages"), argumentList);
    }

    /**
     * Begins a call to the D-Bus method "SetStorageState" on the remote object.
     *
     * <p>Try to pause or continue reception.</p>
     *
     * \param outOfStorage
     *
     *     <p><em>True</em> if client has run out of memory to store the incoming
     *     messages. The protocol will then pause receiving new messages as if it
     *     had run out of storage space itself.</p>
     *
     *     <p>If <em>false</em> the protocol will try to resume receiving new
     *     messages.</p>
     *
     *     <p>Please note that most protocols, including SMS, try eventually
     *     to resume receiving SMSs even after the client has indicated that
     *     it is out of memory.
     *     </p>
     */
    inline QDBusPendingReply<> SetStorageState(bool outOfStorage)
    {
        QList<QVariant> argumentList;
        argumentList << QVariant::fromValue(outOfStorage);
        return asyncCallWithArgumentList(QLatin1String("SetStorageState"), argumentList);
    }

Q_SIGNALS:
    /**
     * Represents the signal "MessagesExpunged" on the remote object.
     *
     * The messages with the given message-tokens have been expunged. Clients
     * SHOULD NOT attempt to expunge those messages.
     *
     * \param storedMessageTokens
     *
     *     The messages that have been removed from the stored message list.
     */
    void MessagesExpunged(const QStringList& storedMessageTokens);

private:
    QStringList m_ExpungedMessages;
    QStringList m_DeliveredMessages;

};
}
}

Q_DECLARE_METATYPE(CommHistoryTp::Client::ConnectionInterfaceStoredMessagesInterface*)


#endif
