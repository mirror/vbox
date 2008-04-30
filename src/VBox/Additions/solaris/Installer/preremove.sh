#!/bin/sh
# Sun xVM VirtualBox
# VirtualBox pre-remove script for Solaris Guest Additions.
#
# Copyright (C) 2008 Sun Microsystems, Inc.
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#
# Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
# Clara, CA 95054 USA or visit http://www.sun.com if you need
# additional information or have any questions.
#

echo "Sun xVM VirtualBox Guest Additions - preremove script"
echo "This script will unload the VirtualBox Guest kernel module..."

# stop and unregister VBoxService daemon
/usr/sbin/svcadm disable -s svc:/system/virtualbox/vboxservice:default
/usr/sbin/svccfg delete svc:/system/virtualbox/vboxservice:default

# stop VBoxClient
pkill -INT VBoxClient

# vboxguest.sh would've been installed, we just need to call it.
/opt/VirtualBoxAdditions/vboxguest.sh stop

# Try and restore xorg.conf!
echo "Restoring Xorg..."
/opt/VirtualBoxAdditions/x11restore.pl

echo "Done."

