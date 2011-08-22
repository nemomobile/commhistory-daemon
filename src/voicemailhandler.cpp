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

// Qt includes
#include <QCoreApplication>
#include <QDebug>
#include <QDir>

#include <CommHistory/commonutils.h>

// contacts
#include <QContactManager>
#include <QContactFetchRequest>
#include <QContactDetailFilter>
#include <QContactLocalIdFilter>
#include <QContactPhoneNumber>
#include <QContactGuid>

// Our includes
#include "voicemailhandler.h"
#include "notificationmanager.h"
#include "locstrings.h"
#include "constants.h"

using namespace RTComLogger;

VoiceMailHandler* VoiceMailHandler::m_pInstance = 0;

// P U B L I C  M E T H O D S

VoiceMailHandler* VoiceMailHandler::instance()
{
    qDebug() << Q_FUNC_INFO;

    if (!m_pInstance) {
        m_pInstance = new VoiceMailHandler(QCoreApplication::instance());
        m_pInstance->init();
    }

    return m_pInstance;
}

bool VoiceMailHandler::isVoiceMailNumber(QString phoneNumber)
{
    qDebug() << Q_FUNC_INFO << "Voice mail numbers are " << m_voiceMailPhoneNumbers << ", compared number is " << phoneNumber;

    bool match = false;

    foreach (QString numberInStore, m_voiceMailPhoneNumbers) {
        if (CommHistory::remoteAddressMatch(numberInStore, phoneNumber)) {
            qDebug() << Q_FUNC_INFO << "MATCH: " << numberInStore << " : " << phoneNumber;
            match = true;
            break;
        }
    }
    return match;
}

bool VoiceMailHandler::isVoiceMailContact(QContactLocalId localId)
{
    qDebug() << Q_FUNC_INFO << "Voice mail contact's local id is " << m_localContactId << ", compared local id is " << localId;
    return (m_localContactId = localId);
}

void VoiceMailHandler::fetchVoiceMailContact()
{
    qDebug() << Q_FUNC_INFO;

    QContactDetailFilter filter;
    filter.setDetailDefinitionName(QContactGuid::DefinitionName,
                                   QContactGuid::FieldGuid);
    filter.setValue(VOICEMAIL_CONTACT_GUID);

    QStringList details;
    details << QContactPhoneNumber::DefinitionName;

    startContactRequest(filter, details, SLOT(slotVoiceMailContactsAvailable()));
}


// P R I V A T E  M E T H O D S

// constructor
//
VoiceMailHandler::VoiceMailHandler(QObject* parent)
        : QObject(parent)
        , m_localContactId(0)
        , m_pVoiceMailFileWatcher(0)
        , m_voiceMailFileExists(false)
{
    qDebug() << Q_FUNC_INFO;
}

// destructor
//
VoiceMailHandler::~VoiceMailHandler()
{
    qDebug() << Q_FUNC_INFO;
    delete m_pVoiceMailFileWatcher;
}

void VoiceMailHandler::init()
{
    qDebug() << Q_FUNC_INFO;

    QDir mainDir = QDir(VOICEMAIL_CONTACT_VMID_MAIN);
    if (!mainDir.exists(VOICEMAIL_CONTACT_VMID_DIR)) {
        // If contacts directory does not exist then create it so that we can monitor its changes:
        if (!mainDir.mkdir(VOICEMAIL_CONTACT_VMID_DIR)) {
            qWarning() << "Creation of " << VOICEMAIL_CONTACT_VMID_MAIN << "/" << VOICEMAIL_CONTACT_VMID_DIR << " failed!";
        }
    }

    if (QFile::exists(QString("%1/%2/%3").arg(VOICEMAIL_CONTACT_VMID_MAIN).arg(VOICEMAIL_CONTACT_VMID_DIR).arg(VOICEMAIL_CONTACT_VMID_FILE))) {
        qDebug() << Q_FUNC_INFO << "Voice mail vmid file exists.";
        m_voiceMailFileExists = true;
    }

    QStringList watchedPaths;
    watchedPaths << QString("%1/%2").arg(VOICEMAIL_CONTACT_VMID_MAIN).arg(VOICEMAIL_CONTACT_VMID_DIR);
    m_pVoiceMailFileWatcher = new QFileSystemWatcher(watchedPaths);
    connect(m_pVoiceMailFileWatcher,SIGNAL(directoryChanged(QString)),SLOT(slotVoiceMailDirectoryChanged()));

    m_pContactManager = NotificationManager::instance()->contactManager();

    fetchVoiceMailContact();
}

QContactFetchRequest* VoiceMailHandler::startContactRequest(QContactFilter &filter, QStringList &details, const char *resultSlot)
{
    qDebug() << Q_FUNC_INFO;

    QContactFetchRequest* request = 0;

    if (!m_pContactManager.isNull()) {
        request = new QContactFetchRequest();
        request->setManager(m_pContactManager.data());
        request->setParent(this);

        connect(request,
                SIGNAL(resultsAvailable()),
                resultSlot);

        request->setFilter(filter);

        QContactFetchHint hint;
        hint.setDetailDefinitionsHint(details);
        request->setFetchHint(hint);

        request->start();

        // setup timeout for request
        QTimer *timer = new QTimer(request);
        connect(timer, SIGNAL(timeout()),
                this, SLOT(slotContactRequestTimeout()));
        timer->start(CONTACT_REQUEST_TIMEROUT);
    }

    return request;
}

// P R I V A T E  S L O T S

void VoiceMailHandler::slotContactRequestTimeout()
{
    qDebug() << Q_FUNC_INFO;

    Q_ASSERT(sender());

    QContactFetchRequest *request = qobject_cast<QContactFetchRequest*>(sender()->parent());

    Q_ASSERT(request);

    request->deleteLater();
}

void VoiceMailHandler::slotVoiceMailContactsAvailable()
{
    qDebug() << Q_FUNC_INFO;

    QContactFetchRequest *request = qobject_cast<QContactFetchRequest *>(sender());

    if(!request || !request->isFinished()) {
        return;
    }

    QList<QContact> contacts = request->contacts();

    qDebug() << __PRETTY_FUNCTION__ << "Number of voice mail contacts returned: " << contacts.size();

    if (!contacts.isEmpty()) {
        // There should be just one voice mail contact (that can have multiple numbers).
        QContact voiceMailContact = contacts.first();
        if (!voiceMailContact.isEmpty()) {
            m_localContactId = voiceMailContact.localId();
            qDebug() << __PRETTY_FUNCTION__ << "Local id of voice mail contact is: " << m_localContactId;
            QList<QContactPhoneNumber> phoneNumbers = voiceMailContact.details<QContactPhoneNumber>();

            m_voiceMailPhoneNumbers.clear();

            foreach (QContactPhoneNumber phoneNumber, phoneNumbers) {
                m_voiceMailPhoneNumbers << phoneNumber.number();
            }
            qDebug() << __PRETTY_FUNCTION__ << "Voice mail phone numbers are: " << m_voiceMailPhoneNumbers;
        } else {
            qDebug() << __PRETTY_FUNCTION__ << "Empty voice mail contact!";
        }
    } else {
        qDebug() << __PRETTY_FUNCTION__ << "No voice mail contacts found!";
    }

    request->deleteLater();
}

void VoiceMailHandler::slotVoiceMailDirectoryChanged()
{
    qDebug() << Q_FUNC_INFO;

    bool vmIdFileFound = QFile::exists(QString("%1/%2/%3").arg(VOICEMAIL_CONTACT_VMID_MAIN).arg(VOICEMAIL_CONTACT_VMID_DIR).arg(VOICEMAIL_CONTACT_VMID_FILE));
    if (m_voiceMailFileExists && !vmIdFileFound) {
        qDebug() << Q_FUNC_INFO << "wmid file removed!";
        m_localContactId = 0;
        m_voiceMailPhoneNumbers.clear();
    } else if (!m_voiceMailFileExists && vmIdFileFound) {
        qDebug() << Q_FUNC_INFO << "vmid file created!";
        m_voiceMailFileExists = true;
        fetchVoiceMailContact();
    } else {
        qDebug() << Q_FUNC_INFO << "Some unimportant directory change happened.";
    }

}
