
#include "account-manager.h"
#include <QDebug>
#include <QTimer>

Tp::AccountManager::AccountManager()
        :PendingReady()
{
    ut_restoreDefaultState();
}

QList<Tp::AccountPtr> Tp::AccountManager::validAccounts()
{
    return m_fakeAccountList;
}

Tp::AccountManagerPtr  Tp::AccountManager::create()
{
    return Tp::AccountManagerPtr( new Tp::AccountManager() );
}

Tp::AccountPtr Tp::AccountManager::accountForPath(const QString &path)
{
    Q_UNUSED( path );

    foreach( Tp::AccountPtr accptr, m_fakeAccountList ) {
        if ( path == accptr.data()->uniqueIdentifier() ) {
            return accptr;
        }
    }
    // !!!! This is an invalid pointer
    return Tp::AccountPtr();
}

void Tp::AccountManager::ut_clearAccountList()
{
    foreach ( Tp::AccountPtr accptr, m_fakeAccountList){

        disconnect( accptr.data(), SIGNAL(validityChanged(bool)),
                 this, SLOT(ut_accountValidityChanged(bool)));

    }

    m_fakeAccountList.clear();
}

void Tp::AccountManager::ut_restoreDefaultState()
{
    ut_clearAccountList();

    // Just one Account
    Tp::Account* account = new Tp::Account();
    account->ut_setUniqueID(QString("%1").arg(1));
    account->ut_setProtocol("jabber");
    account->ut_setConnectionManagerName( "gabble" );
    Tp::AccountPtr accP ( account );

    ut_addAccount( accP );
}

void Tp::AccountManager::ut_addAccount( AccountPtr accPtr )
{
    m_fakeAccountList << accPtr;

    connect( accPtr.data(), SIGNAL(validityChanged(bool)),
             this, SLOT(ut_accountValidityChanged(bool)));
}

void Tp::AccountManager::ut_deletAccount( AccountPtr accPtr )
{
    int deleteIndex = -1;
    for( int accounts = 0; accounts < m_fakeAccountList.size(); accounts++ ){

        if ( m_fakeAccountList.at(accounts) == accPtr ){
            deleteIndex = accounts;
            break;
        }
    }

    if ( -1 < deleteIndex ){
        m_fakeAccountList.removeAt( deleteIndex );
    }
}

void Tp::AccountManager::ut_accountValidityChanged(const bool validity)
{
    foreach( Tp::AccountPtr accptr, m_fakeAccountList ) {
        if ( accptr.data() == sender() ) {
            emit accountValidityChanged( accptr.data()->uniqueIdentifier(), validity);
        }
    }
}
