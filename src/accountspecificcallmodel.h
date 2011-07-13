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

#ifndef ACCOUNTSPECIFIC_CALLMODEL_H
#define ACCOUNTSPECIFIC_CALLMODEL_H

#include <CommHistory/EventModel>

namespace RTComLogger
{

class AccountSpecificCallModelPrivate;

/*!
 * \class AccountSpecificCallModel
 *
 * \brief Model for accessing call events of a specific account.
 *        Initialize with getEvents(<account's local uid>).
 *
 */
class AccountSpecificCallModel: public CommHistory::EventModel
{
    Q_OBJECT

public:
    /*!
     * \brief Model constructor.
     *
     * \param parent Parent object.
     */
    AccountSpecificCallModel(QObject *parent = 0);

    /*!
     * Destructor.
     */
    ~AccountSpecificCallModel();

    /*!
     * \brief Resets model and fetch call events belonging to given account.
     *
     * \param accountPath Path of the account which call events should be brought into the model.
     * \return true if successful; false, otherwise.
     */
    bool getEvents(QString accountPath);

    /*!
     * \brief Resets model and fetch call events created earlier than given date.
     *
     * \param date Model is populated with calls created before this date.
     * \return true if successful; false, otherwise.
     */
    bool getEvents(QDateTime date);

private:
    Q_DECLARE_PRIVATE(AccountSpecificCallModel);
};

}

#endif
