
#include "account.h"

#include <QDebug>
#include <QTimer>
#include <QString>

const Tp::Feature Tp::Account::FeatureCore = Tp::Feature(Tp::Account::staticMetaObject.className(), 0, true);

Tp::Account::Account()
        :Tp::ReadyObject()
        ,m_protocol(QString(""))
        ,m_protocolName(QString(""))
{

}

QString Tp::Account::protocol() const
{
    return m_protocol;
}

QString Tp::Account::protocolName() const
{
    return m_protocolName;
}

void Tp::Account::ut_setProtocol( const QString& newProtocol )
{
   m_protocol = newProtocol;
}

void Tp::Account::ut_setProtocolName(const QString& protocolName)
{
    m_protocolName = protocolName;
}
