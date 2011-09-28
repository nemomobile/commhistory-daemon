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

#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <CommHistory/Event>

namespace RTComLogger {

#define OBSERVED_CONVERSATION_KEY QLatin1String("Messaging.ObservedConversation")
#define OBSERVED_INBOX_KEY QLatin1String("Messaging.ObservedInbox")
#define FILTERED_INBOX_KEY QLatin1String("Messaging.FilteredInbox")
#define CALL_HISTORY_KEY QLatin1String("CallHistory.Inbox")

// Used to generate duiremoteaction strings
#define OBJECT_PATH QLatin1String("/")

#define COMM_HISTORY_SERVICE_NAME    QLatin1String("com.nokia.CommHistory")
#define COMM_HISTORY_OBJECT_PATH     QLatin1String("/com/nokia/CommHistory")
#define COMM_HISTORY_INTERFACE       QLatin1String("com.nokia.CommHistoryIf")

#define MESSAGING_SERVICE_NAME    QLatin1String("com.nokia.Messaging")
#define MESSAGING_INTERFACE       QLatin1String("com.nokia.MessagingIf")
#define SHOW_INBOX_METHOD         QLatin1String("showInbox")
#define START_CONVERSATION_METHOD QLatin1String("startConversation")

#define CALL_HISTORY_SERVICE_NAME QLatin1String("com.nokia.telephony.callhistory")
#define CALL_HISTORY_OBJECT_PATH  QLatin1String("/org/maemo/m")
#define CALL_HISTORY_INTERFACE    QLatin1String("com.nokia.MApplicationIf")
#define CALL_HISTORY_METHOD       QLatin1String("launch")
#define CALL_HISTORY_PARAMETER    QLatin1String("callHistory")

#define VOICEMAIL_INTERFACE    QLatin1String("com.nokia.telephony.callhistory")
#define VOICEMAIL_OBJECT_PATH  QLatin1String("/callhistory")
#define VOICEMAIL_METHOD       QLatin1String("voicemail")
#define REPLACE_TYPE           QLatin1String("sms-replace-number")

#define CSD_SIM_SERVICE_NAME       QLatin1String("com.nokia.csd.SIM")
#define CSD_SIM_OBJECT_PATH        QLatin1String("/com/nokia/csd/sim")
#define CSD_SIM_INTERFACE          QLatin1String("com.nokia.csd.SIM")
#define CSD_SIM_SETTINGS_INTERFACE QLatin1String("com.nokia.csd.SIM.Settings")
#define CSD_SIM_SET_MWI_METHOD     QLatin1String("SetMessageWaitingIndicationStatus")
#define CSD_SIM_GET_MWI_METHOD     QLatin1String("GetMessageWaitingIndicationStatus")
#define CSD_SIM_GET_SIM_STATUS     QLatin1String("GetSIMStatus")
#define CSD_SIM_STATUS_SIGNAL      QLatin1String("SIMStatus")
#define CSD_SIM_STATUS_OK          QLatin1String("Ok")
#define CSD_SIM_STATUS_REMOVED     QLatin1String("Removed")
#define CSD_SIM_STATUS_NO_SIM      QLatin1String("NoSIM")

#define RING_ACCOUNT_PATH QLatin1String("/org/freedesktop/Telepathy/Account/ring/tel/ring")
#define MMS_ACCOUNT_PATH  QLatin1String("/org/freedesktop/Telepathy/Account/mmscm/mms/mms0")
#define MAP_MMS_TO_RING(x) ((x) == MMS_ACCOUNT_PATH ? RING_ACCOUNT_PATH : (x))

#define CONTACT_SEPARATOR_IN_NOTIFICATION_GROUP QLatin1String(", ")

#define TELEPATHY_CHANNEL_INTERFACE_PERSISTENT_ID QLatin1String("com.nokia.Telepathy.Channel.Interface.Persistent.PersistentID")

// DUI didnt define namespace for events, remove this
#define CONTACT_STORAGE_TYPE QLatin1String("tracker")
// notification threshold is 3 seconds
#define NOTIFICATION_THRESHOLD 3000
// threshold to trigger contact request after added/modified contacts signals
#define CONTACT_REQUEST_THRESHOLD 5000
// give up on contact fetch request
#define CONTACT_REQUEST_TIMEROUT 3000
/* Clean up check -period for old calls as DAYS.
   Note: this cannot be > 24 days, because then the int value given for QTimer::setInterval(int)
   will go out of int range. */
#define CLEANUP_PERIOD_DAYS 7
// First clean up after boot in ms.
#define BOOT_CLEANUP_MS 3600000
// This amount of days is counted back to determine how old calls should be deleted.
#define REMOVAL_TARGET_DAYS -90

// Voice mail contact's GUID, used by MeeGo Harmattan Contacts application.
#define VOICEMAIL_CONTACT_GUID QLatin1String("9aeeea49-fab9-4d4e-b0cc-497f2aeac9a8")

// This event type is used only locally (in NotificationManager) and is not defined in CommHistory::Event::EvenType enum.
#define VOICEMAIL_SMS_EVENT_TYPE 100

// Path and name of the vmid file used to contain local id of of a voice mail contact when such a contact is added.
#define VOICEMAIL_CONTACT_VMID_MAIN QLatin1String("/dev/shm")
#define VOICEMAIL_CONTACT_VMID_DIR  QLatin1String("contacts")
#define VOICEMAIL_CONTACT_VMID_FILE QLatin1String("vmid")

// events
struct EventTypes {
    int type;
    const char* event;
};

static const EventTypes _eventTypes[] =
{
    {CommHistory::Event::IMEvent,       "x-nokia.messaging.im"},
    {CommHistory::Event::SMSEvent,      "x-nokia.messaging.sms"},
    {CommHistory::Event::MMSEvent,      "x-nokia.messaging.mms"},
    {CommHistory::Event::CallEvent,     "x-nokia.call.missed"},
    {CommHistory::Event::VoicemailEvent,"x-nokia.messaging.voicemail"},
    {VOICEMAIL_SMS_EVENT_TYPE,          "x-nokia.messaging.voicemail-SMS"}
};

static const int _eventTypesCount = sizeof(_eventTypes) / sizeof(EventTypes);
}

// Custom system info banner types for commhistoryd:
typedef QString BannerType;
const BannerType ErrorBanner = "x-nokia.messaging.error";
const BannerType ErrorBannerStrong = "x-nokia.messaging.error.strong";

#endif //#define CONSTANTS_H
