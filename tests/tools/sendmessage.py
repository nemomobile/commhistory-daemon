#!/usr/bin/python
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

from xmpp import *
import sys

if len(sys.argv) < 2:
    print "Usage: sendmessage.py domain from to message [times]"
    print "Example: sendmesage.py localhost td dut Hello"
    print "Example: sendmesage.py localhost td dut Hello 1000"
    exit(1);

domain = sys.argv[1]
user = sys.argv[2]
to = sys.argv[3] + "@" + domain
message = sys.argv[4]
password = "astral"
times = 1

if len(sys.argv) > 5:
    times = int(sys.argv[5])

client=Client(domain)
if not client.connect():
    raise IOError('Cant connect to the server')
if not client.auth(user,password):
    raise IOError('Cant authenticate user')
client.sendInitPresence()
client.Process(1)
if not client.isConnected():
    client.reconnectAndReauth()

for i in xrange(times):
    client.send(Message(to,message,'chat'))
    client.Process()
