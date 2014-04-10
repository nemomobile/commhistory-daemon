###############################################################################
#
# This file is part of commhistory-daemon.
#
# Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
# Contact: Reto Zingg <reto.zingg@nokia.com>
#
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License version 2.1 as
# published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
# License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
#
###############################################################################

include( ../common-installs-config.pri )
TEMPLATE = aux

client.path = $${INSTALL_PREFIX}/share/telepathy/clients
client.files = CommHistory.client

service.path = $${INSTALL_PREFIX}/lib/systemd/user
service.files = commhistoryd.service

dbus_conf.path = /etc/dbus-1/system.d/
dbus_conf.files = org.nemomobile.MmsHandler.conf org.ofono.SmartMessagingAgent.conf

notification_types.path  = $${INSTALL_PREFIX}/share/lipstick/notificationcategories
notification_types.files = notifications/*.conf

INSTALLS += client service dbus_conf notification_types
