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

# CONFIG PARAMS --------------------------------------------------------------

# Name of the catalog:
CATALOGNAME = commhistoryd

# Paths of source code files:
SOURCEPATHS = $${_PRO_FILE_PWD_}/../src

# Installation paths:
QM_INSTALL_PATH = $${INSTALL_PREFIX}/share/l10n/meegotouch/
TS_INSTALL_PATH = $${INSTALL_PREFIX}/share/doc/commhistory-daemon-l10n-engineering-english/

#-----------------------------------------------------------------------------
# DO NOT EDIT THIS PART ------------------------------------------------------
#-----------------------------------------------------------------------------

TEMPLATE =
SUBDIRS  =
CONFIG   = warn_on

# "unarm" the primary target generation by disabling linking
QMAKE_LFLAGS = --version

# translation input/output
TS_FILENAME = $${_PRO_FILE_PWD_}/$${CATALOGNAME}.ts
QM_FILENAME = $${_PRO_FILE_PWD_}/$${CATALOGNAME}.qm

# target name
TARGET       = $${CATALOGNAME}.qm
QMAKE_TARGET = $${TARGET}

# LUPDATE and LRELEASE --------------------------------------------------------
LUPDATE_CMD = lupdate \
              -no-obsolete \
              $${SOURCEPATHS} \
              -ts $${TS_FILENAME} && \
              lrelease \
              -idbased \
              -markuntranslated \"!! \" \
              $${TS_FILENAME} \
              -qm $${QM_FILENAME}

# extra target for generating the .qm file
lupdate.target       = .lupdate
lupdate.commands     = $$LUPDATE_CMD
QMAKE_EXTRA_TARGETS += lupdate
PRE_TARGETDEPS      += .lupdate

# installation target for the .ts file
tsfile.CONFIG += no_check_exist no_link
tsfile.files   = $${TS_FILENAME}
tsfile.path    = $${TS_INSTALL_PATH}
tsfile.target  = tsfile
INSTALLS      += tsfile

# installation target for the .qm file
qmfile.CONFIG  += no_check_exist no_link
qmfile.files    = $${QM_FILENAME}
qmfile.path     = $${QM_INSTALL_PATH}
qmfile.target   = qmfile
INSTALLS       += qmfile

QMAKE_CLEAN += $${TS_FILENAME} $${QM_FILENAME}

# End of File
