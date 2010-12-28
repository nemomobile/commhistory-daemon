
#include "account-manager.h"
#include <QDebug>
#include <QTimer>

Tp::AccountManager::AccountManager()
        :PendingReady()
{
}

Tp::AccountManagerPtr  Tp::AccountManager::create()
{
    return Tp::AccountManagerPtr( new Tp::AccountManager() );
}


void Tp::AccountManager::ut_addAccount( AccountPtr accPtr )
{
    m_fakeAccountList << accPtr;
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
