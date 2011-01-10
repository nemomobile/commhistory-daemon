
#ifndef _TelepathyQt4_cli_account_h_HEADER_GUARD_
#define _TelepathyQt4_cli_account_h_HEADER_GUARD_

#include "Types"
#include "PendingReady"
#include "Connection"
#include "DBusProxy"
#include "ReadyObject"

#include <QVariantMap>
#include <QString>

namespace Tp
{

class Account;

class Account : public StatelessDBusProxy
{

    Q_OBJECT
    Q_DISABLE_COPY(Account)

public:
    Account(ConnectionPtr &conn, const QString &objectPath);
    ~Account();

    static const Feature FeatureCore;

    QString protocol() const;
    QString protocolName() const;
    ConnectionPtr connection() const;

public: // stubs
    void ut_setProtocolName( const QString& newUID );
    void ut_setProtocol( const QString& newProtocol );

private:
    struct Private;
    friend struct Private;
    Private *mPriv;
};

} // Tp

#endif
