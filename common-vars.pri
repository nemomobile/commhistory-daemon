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

# -----------------------------------------------------------------------------
# Common variables for all projects.
# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------
# Project name (used e.g. in include file and doc install path).
# remember to update debian/* files if you changes this
# -----------------------------------------------------------------------------
PROJECT_NAME = commhistory-daemon

# -----------------------------------------------------------------------------
# Project version
# remember to update debian/* files if you changes this
# -----------------------------------------------------------------------------
PROJECT_VERSION = 0.0.26

# -----------------------------------------------------------------------------
# Project's data directories
# -----------------------------------------------------------------------------
COMMHISTORYD_DATADIR = ".commhistoryd"
DEFINES += COMMHISTORYD_DATADIR="\\\"$$COMMHISTORYD_DATADIR\\\""
DEFINES += COMMHISTORYD_VCARDSDIR="\\\"$$COMMHISTORYD_DATADIR/vcards\\\""
DEFINES += COMMHISTORYD_NOTIFICATIONSDIR="\\\"/$$COMMHISTORYD_DATADIR/notifications\\\""
DEFINES += COMMHISTORYD_NOTIFICATIONSSTORAGE="\\\"/$$COMMHISTORYD_DATADIR/notifications/storage.dat\\\""
