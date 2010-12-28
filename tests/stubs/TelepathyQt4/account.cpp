
#include "account.h"

#include <QDebug>
#include <QTimer>
#include <QString>

const Tp::Feature Tp::Account::FeatureCore = Tp::Feature(Tp::Account::staticMetaObject.className(), 0, true);
const Tp::Feature Tp::Account::FeatureAvatar = Tp::Feature(Tp::Account::staticMetaObject.className(), 1);
const Tp::Feature Tp::Account::FeatureProtocolInfo = Tp::Feature(Tp::Account::staticMetaObject.className(), 2);

Tp::Account::Account()
        :Tp::PendingReady()
        ,m_uniqueIdentifier("account/tester")
        ,m_protocol(QString(""))
        ,m_CMname(QString(""))
        ,m_NickName(QString(""))
        ,m_DisplayName(QString(""))
        ,m_protocolName(QString(""))
        ,m_Enabled(true)
{
    m_CurrentPresence.type = Tp::ConnectionPresenceTypeUnset;
    m_AutomaticPresence.type = Tp::ConnectionPresenceTypeUnset;
    m_CurrentAvatar.avatarData = QByteArray();
    m_CurrentAvatar.MIMEType = QString();

    ut_setObject( this );

    connect( this, SIGNAL( ut_validityChanged(bool)),
             this, SIGNAL( validityChanged(bool)) );

}

Tp::SimplePresence Tp::Account::requestedPresence() const
{
    return m_CurrentPresence;
}


Tp::SimplePresence Tp::Account::automaticPresence() const
{
    return m_AutomaticPresence;
}


Tp::SimplePresence Tp::Account::currentPresence() const
{
    return m_CurrentPresence;
}

Tp::PendingOperation* Tp::Account::setRequestedPresence(
        const Tp::SimplePresence &value)
{
    if ( differentPresences( value, m_CurrentPresence ) ) {
        m_CurrentPresence = value;
        QTimer::singleShot(m_pendingDelay, this, SLOT(ut_emitRequestetPresenceChanged()) );
        QTimer::singleShot(m_pendingDelay+200, this, SLOT(ut_emitCurrentPresenceChanged()) );
    }
    return this;
}

Tp::PendingOperation* Tp::Account::setAutomaticPresence(
        const Tp::SimplePresence &value)
{
    m_AutomaticPresence = value;
    QTimer::singleShot(m_pendingDelay, this, SLOT(ut_emitAutomaticPresenceChanged()) );
    return this;
}

QString Tp::Account::protocol() const
{
    return m_protocol;
}

QString Tp::Account::protocolName() const
{
    return m_protocolName;
}

QString Tp::Account::uniqueIdentifier() const
{
    return m_uniqueIdentifier;
}

QString Tp::Account::displayName() const
{
    return  m_DisplayName;
}


QString  Tp::Account::normalizedName() const
{
    return uniqueIdentifier();
}


QString Tp::Account::connectionObjectPath() const
{
    QString path = QString("/org/freedesktop/telepathy/%1/%2/%3")
                                .arg( m_CMname )
                                .arg( m_protocol )
                                .arg( m_uniqueIdentifier );
    return path;
}


bool Tp::Account::hasBeenOnline() const
{
    return true;
}


bool Tp::Account::isEnabled() const
{
    return m_Enabled;
}

const Tp::Avatar& Tp::Account::avatar()
{
    return m_CurrentAvatar;
}

void Tp::Account::ut_emitRequestetPresenceChanged()
{
//    qDebug() << "#### fake requestedPresenceChanged";
    emit requestedPresenceChanged (m_CurrentPresence);
}

void Tp::Account::ut_emitAutomaticPresenceChanged()
{
//    qDebug() << "#### fake automaticPresenceChanged";
    emit automaticPresenceChanged (m_AutomaticPresence);
}

void Tp::Account::ut_emitCurrentPresenceChanged()
{
//    qDebug() << "#### fake currentPresenceChanged";
    emit currentPresenceChanged( m_CurrentPresence );
}

void Tp::Account::ut_emitStateChanged()
{
    emit stateChanged( m_Enabled );
}

void Tp::Account::connectNotify( const char* signal )
{
    Q_UNUSED( signal );
//    qDebug() << "### Account - - connected" << signal;
}

void Tp::Account::disconnectNotify( const char * signal)
{
    Q_UNUSED( signal );
//    qDebug() << "### Account - - !!!!!!!!!! DISconnected() " << signal;
}

void Tp::Account::ut_setUniqueID( const QString& newUID )
{
    m_uniqueIdentifier = newUID;
}

void Tp::Account::ut_setProtocol( const QString& newProtocol )
{
   m_protocol = newProtocol;
}

void Tp::Account::ut_setConnectionManagerName( const QString& newCMname )
{
   m_CMname = newCMname;
}

void Tp::Account::ut_setProtocolName(const QString& protocolName)
{
    m_protocolName = protocolName;
}

void Tp::Account::ut_setNickname( const QString& nickname )
{
    if ( m_NickName != nickname ){
        m_NickName = nickname;
        emit nicknameChanged(  m_NickName );
    }
}

void Tp::Account::ut_setDisplaynName( const QString& displayname )
{
    if ( m_DisplayName != displayname ){
        m_DisplayName = displayname;
        emit displayNameChanged( m_DisplayName );
    }
}

Tp::ConnectionStatus Tp::Account::connectionStatus() const
{
    return Tp::ConnectionStatusConnected;
}

Tp::ConnectionStatusReason Tp::Account::connectionStatusReason() const
{
    return Tp::ConnectionStatusReasonNoneSpecified;
}

bool Tp::Account::differentPresences( Tp::SimplePresence lhPresence, Tp::SimplePresence rhPresence )
{
    return ( lhPresence.status != rhPresence.status ) ||
           ( lhPresence.statusMessage != rhPresence.statusMessage ) ||
           ( lhPresence.type != rhPresence.type );
}


bool Tp::Account::connectsAutomatically() const
{
    return true;
}


Tp::PendingOperation* Tp::Account::setConnectsAutomatically( bool _bValue )
{
    Q_UNUSED(_bValue);

    return this;
}

QString Tp::Account::objectPath()
{
    return "ConMan/Service/AccID";
}

QString Tp::Account::nickname()
{
    return m_NickName;
}

Tp::PendingOperation* Tp::Account::setEnabled( bool state )
{
    if ( m_Enabled != state ) {
        m_Enabled = state;
        QTimer::singleShot(m_pendingDelay, this, SLOT(ut_emitStateChanged()));
        return this;
    } else {
        return new Tp::PendingOperation();
    }
}
