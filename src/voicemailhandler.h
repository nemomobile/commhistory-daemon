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

#include <QWeakPointer>

#include <qcontact.h>
QTM_USE_NAMESPACE

QTM_BEGIN_NAMESPACE
class QContactManager;
class QContactFetchRequest;
class QContactFilter;
QTM_END_NAMESPACE

class QFileSystemWatcher;

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

    virtual ~VoiceMailHandler();

    /*!
     * \brief Tells if given phone number is a voice mail number or not.
     * \param Phone number to be checked
     * \returns true if given number is a voice mail number, false if it is not
     */
    bool isVoiceMailNumber(QString phoneNumber);

    /*!
     * \brief Tells if given QContactLocalId belongs to a voice mail contact or not.
     * \param QContactLocalId to be checked
     * \returns true if given id belongs to a voice mail contact, false if not
     */
    bool isVoiceMailContact(QContactLocalId localId);

    /*!
     * \brief Refreshes VoiceMailHandler by fetching possible voice mail contact from tracker based on GUID.
     */
    void fetchVoiceMailContact();

    /*!
     * \brief Clears voicemail contact data.
     */
    void clear();

    /*!
     * \brief Returns voice mail contact id.
     * \returns voice mail contact's local id
     */
    QContactLocalId voiceMailContactId();

    /*!
     * \brief Starts to listen QFileSystemWatcher for vmid dir and file changes.
     */
    void startObservingVmcFile();

private Q_SLOTS:
    void slotVoiceMailContactsAvailable();
    void slotVoiceMailFileChanged(QString path);
    void slotVoiceMailDirectoryChanged(QString path);

private:
    VoiceMailHandler();
    void init();    
    QContactFetchRequest* startContactRequest(QContactFilter &filter, QStringList &details, const char *resultSlot);

private:
    QWeakPointer<QContactManager> m_pContactManager;
    QStringList m_voiceMailPhoneNumbers;
    QContactLocalId m_localContactId;
    QFileSystemWatcher *m_pVoiceMailDirWatcher;
};

} // namespace RTComLogger

#endif // VOICEMAILHANDLER_H
