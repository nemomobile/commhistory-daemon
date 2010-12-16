#! /bin/sh
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

DAEMON=/usr/bin/commhistoryd
PIDDIR=/var/run
PIDFILE=$PIDDIR/commhistoryd.pid
NAME=commhistoryd
DESC="commhistory daemon"

. /lib/lsb/init-functions
. /etc/osso-af-init/af-defines.sh

test -x $DAEMON || exit 0

set -e

do_start() {
	if [ ! -d $PIDDIR ]; then
		mkdir -p $PIDDIR
	fi
	start-stop-daemon --start -b -m --oknodo --pidfile $PIDFILE \
		--exec $DAEMON
}

do_stop() {
	start-stop-daemon --stop --oknodo --quiet --pidfile $PIDFILE \
		--exec $DAEMON
}

case "$1" in
  start)
	log_daemon_msg "Starting $DESC" "$NAME"
	do_start
	log_end_msg $?
	;;
  stop)
	log_daemon_msg "Stopping $DESC" "$NAME"
	do_stop
	log_end_msg $?
	;;
  restart|force-reload)
	log_daemon_msg "Restarting $DESC" "$NAME"
	do_stop
	sleep 1
	do_start
	log_end_msg $?
	;;
  *)
	log_success_msg "Usage: $0 {start|stop|restart|force-reload}" >&2
	exit 1
	;;
esac

exit 0
