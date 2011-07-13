/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Reto Zingg <reto.zingg@nokia.com>
**
** This library is free software; you can redistribute it and/or modify it
** under the terms of the GNU Lesser General Public License version 2.1 as
** published by the Free Software Foundation.
**
** This library is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
** or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
** License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this library; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
**
******************************************************************************/

#ifndef MWI_LISTENER_H
#define MWI_LISTENER_H

#include <QObject>
#include <QDBusPendingCallWatcher>
#include <QDBusInterface>

namespace RTComLogger
{

/*!
 * \class MWIListener
 * \brief class responsible for checking any unhandled messages and redelivering them
 *  using stored messages interface
 *  Once all stored messages are processed, the object will delete itself.
 */
class MWIListener : public QObject
{
    Q_OBJECT

public:
    explicit MWIListener(QObject *parent = 0);

    /*!
     * \breif
     * Set voice mail MWI count
     *
     * \param count voice mail count, -1 means unknown
     */
    void saveMWI(int count);
    /*!
     * \breif
     * \return voice mail MWI count, -1 means unknown
     */
    int MWICount() const;

Q_SIGNALS:
    void MWICountChanged(int count);

private Q_SLOTS:
    void onSIMStatusChanged(const QString &status);
    void onGetSIMStatus(QDBusPendingCallWatcher *call);
    void onGetMWICount(QDBusPendingCallWatcher *call);
    void onSetMWICount(QDBusPendingCallWatcher *call);

private:
    void loadMWI();
    void updateCount(int count);
    void doSaveMWI();

private:
    struct {
        int voicemail; // only voicemail count is used
        int fax;    // rest are kept to avoid overwritting
        int email;
        int other;
    } m_counts;
    bool m_countsLoaded; //set after loadMWI finish
    bool m_savePending; // set if saveMWI called before loadMWI finish
                        // in that case save is postponed until
                        // valid fax/email/other counts are available
    QDBusInterface m_simSettings;
};

} // namespace RTComLogger

#endif // MWI_LISTENER_H
