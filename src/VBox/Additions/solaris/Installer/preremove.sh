#!/bin/sh
# innotek VirtualBox
# VirtualBox pre-remove script for Solaris Guest Additions.
#
# Copyright (C) 2008 innotek GmbH
#
# innotek GmbH confidential
# All rights reserved
#

echo "innotek VirtualBox Guest Additions - preremove script"
echo "This script will unload the VirtualBox Guest kernel module..."

# stop and unregister VBoxService daemon
/usr/sbin/svcadm disable -s svc:/system/virtualbox/vboxservice:default
/usr/sbin/svccfg delete svc:/system/virtualbox/vboxservice:default

# vboxguest.sh would've been installed, we just need to call it.
/opt/VirtualBoxAdditions/vboxguest.sh stop

echo "Done."

