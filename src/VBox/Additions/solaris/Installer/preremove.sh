#!/bin/sh
# innotek VirtualBox
# VirtualBox pre-remove script for Solaris Guest Additions.
#
# Copyright (C) 2008 innotek GmbH
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

echo "innotek VirtualBox Guest Additions - preremove script"
echo "This script will unload the VirtualBox Guest kernel module..."

# stop and unregister VBoxService daemon
/usr/sbin/svcadm disable -s svc:/system/virtualbox/vboxservice:default
/usr/sbin/svccfg delete svc:/system/virtualbox/vboxservice:default

# vboxguest.sh would've been installed, we just need to call it.
/opt/VirtualBoxAdditions/vboxguest.sh stop

echo "Done."

