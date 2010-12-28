
#ifndef _TelepathyQt4_cli_account_h_HEADER_GUARD_
#define _TelepathyQt4_cli_account_h_HEADER_GUARD_

#include "Types"
#include "PendingReady"
#include "Connection"

#include <QVariantMap>
#include <QString>

namespace Tp
{

class Account : public PendingReady, public QAtomicInt
{

    Q_OBJECT
    Q_DISABLE_COPY(Account)

public:

    Account();

    static const Feature FeatureCore;
    static const Feature FeatureAvatar;
    static const Feature FeatureProtocolInfo;

    SimplePresence requestedPresence() const;

    PendingOperation* setRequestedPresence( const SimplePresence &value);

    PendingOperation* setAutomaticPresence( const SimplePresence &value);

    SimplePresence automaticPresence() const;

    SimplePresence currentPresence() const;

    QString protocol() const;
    QString protocolName() const;

    QString uniqueIdentifier() const;

    QString displayName() const;

    QString normalizedName() const;

    QString connectionObjectPath() const;

    bool hasBeenOnline() const;

    bool isEnabled() const;

    const Avatar& avatar();

    void ut_setUniqueID( const QString& newUID );
    void ut_setProtocolName( const QString& newUID );
    void ut_setProtocol( const QString& newProtocol );
    void ut_setConnectionManagerName( const QString& newCMname );
    void ut_setNickname( const QString& nickname );
    void ut_setDisplaynName( const QString& displayName );

    Tp::ConnectionStatus connectionStatus() const;
    Tp::ConnectionStatusReason connectionStatusReason() const;

    bool connectsAutomatically() const;

    Tp::PendingOperation* setConnectsAutomatically( bool _bValue );

    QString objectPath();

    QString nickname();

    PendingOperation* setEnabled( bool state );

signals:

    void requestedPresenceChanged(const Tp::SimplePresence &);
    void automaticPresenceChanged(const Tp::SimplePresence &);
    void currentPresenceChanged(const Tp::SimplePresence &);
    void validityChanged (bool);
    void nicknameChanged (const QString &);
    void displayNameChanged(const QString &);
    void stateChanged (bool);
    void changingPresence( bool );

    // still unemited
    void iconChanged(const QString &);
    void normalizedNameChanged (const QString &);
    void connectsAutomaticallyPropertyChanged(bool);
    void firstOnline ();
    void parametersChanged (const QVariantMap &);
    void avatarChanged (const Tp::Avatar &);
    void connectionStatusChanged (Tp::ConnectionStatus, Tp::ConnectionStatusReason);
    void haveConnectionChanged (bool );

protected:

    virtual void connectNotify( const char* signal);
    virtual void disconnectNotify( const char* signal);

private slots:

    void ut_emitRequestetPresenceChanged();
    void ut_emitAutomaticPresenceChanged();
    void ut_emitCurrentPresenceChanged();
    void ut_emitStateChanged();

private:

    bool differentPresences( Tp::SimplePresence lhPresence, Tp::SimplePresence rhPresence );

    Tp::SimplePresence m_CurrentPresence;
    Tp::SimplePresence m_AutomaticPresence;
    Tp::Avatar m_CurrentAvatar;
    QString m_uniqueIdentifier;
    QString m_protocol;
    QString m_CMname;
    QString m_NickName;
    QString m_DisplayName;
    QString m_protocolName;
    bool m_Enabled;
};

} // Tp

#endif
