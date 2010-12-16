###############################################################################
#
# This file is part of commhistory-daemon.
#
# Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
# Contact: Alexander Shalamov <alexander.shalamov@nokia.com>
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

include( ../common-project-config.pri )
include( ../common-vars.pri )

include( ../common-installs-config.pri )

TEMPLATE = subdirs

client.path = $${INSTALL_PREFIX}/share/telepathy/clients
client.files = CommHistory.client

service.path = $${INSTALL_PREFIX}/share/dbus-1/services
service.files = org.freedesktop.Telepathy.Client.CommHistory.service \
    com.nokia.CommHistory.service

script.path = /etc/init.d
script.files = commhistoryd.sh

# -----------------------------------------------------------------------------
# Installation target for prestart file
# Note! this should be removed when new prestart configuration is in use
# -----------------------------------------------------------------------------
prestart.files = commhistoryd.prestart
prestart.path  = /etc/prestart

cud.files = commhistoryd-cud.sh
cud.path  = /etc/osso-cud-scripts

INSTALLS += client service script prestart cud
