
#include "account.h"

#include "Connection"

#include <QDebug>
#include <QTimer>
#include <QString>

namespace Tp
{

const Feature  Account::FeatureCore =  Feature(Account::staticMetaObject.className(), 0, true);

struct Account::Private
{
    QString m_protocol;
    QString m_protocolName;
    ConnectionPtr m_conn;
};

Account::Account(ConnectionPtr &conn, const QString &objectPath)
    : StatelessDBusProxy(objectPath),
    mPriv(new Private)
{
    mPriv->m_conn = conn;
}

Account::~Account()
{
    delete mPriv;
}

QString Account::protocol() const
{
    return mPriv->m_protocol;
}

QString Account::protocolName() const
{
    return mPriv->m_protocolName;
}

void Account::ut_setProtocol( const QString& newProtocol )
{
   mPriv->m_protocol = newProtocol;
}

void Account::ut_setProtocolName(const QString& protocolName)
{
    mPriv->m_protocolName = protocolName;
}

ConnectionPtr Account::connection() const
{
    return mPriv->m_conn;
}

}
