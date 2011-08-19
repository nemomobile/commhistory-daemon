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

#ifndef VOICEMAILHANDLER_H
#define VOICEMAILHANDLER_H

#include <qcontact.h>
QTM_USE_NAMESPACE

QTM_BEGIN_NAMESPACE
class QContactManager;
class QContactFetchRequest;
class QContactFilter;
QTM_END_NAMESPACE

namespace RTComLogger {

/*!
 * \class VoiceMailHandler
 * \brief class responsible for fetching and maintaining the voice mail phonenumbers
 */
class VoiceMailHandler : public QObject
{
    Q_OBJECT

public:
    /*!
     *  \param QObject parent object
     *  \returns VoiceMailHandler singleton
     */
    static VoiceMailHandler* instance();

    /*!
     * \brief Tells if given phone number is a voice mail number or not.
     * \param Phone number to be checked
     * \returns true if given number is a voice mail number, false if it is not
     */
    bool isVoiceMailNumber(QString phoneNumber);

private Q_SLOTS:
    void slotContactsAdded(const QList<QContactLocalId> &contactIds);
    void slotContactsRemoved(const QList<QContactLocalId> &contactIds);
    void slotContactsChanged(const QList<QContactLocalId> &contactIds);
    QContactFetchRequest* startContactRequest(QContactFilter &filter, QStringList &details, const char *resultSlot);
    void slotContactRequestTimeout();
    void slotVoiceMailContactsAvailable();
    void slotContactsAvailable();

private:
    VoiceMailHandler(QObject* parent = 0);
    ~VoiceMailHandler();
    void init();
    void fetchVoiceMailContact();

private:
    static VoiceMailHandler* m_pInstance;
    QContactManager *m_pContactManager;
    QStringList m_voiceMailPhoneNumbers;
    QContactLocalId m_localContactId;
};

} // namespace RTComLogger

#endif // VOICEMAILHANDLER_H
