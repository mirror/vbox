#!/bin/sh
# Sun xVM VirtualBox
# VirtualBox postinstall script for Solaris.
#
# Copyright (C) 2007-2008 Sun Microsystems, Inc.
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

echo "Sun xVM VirtualBox - postinstall script"
sync

currentzone=`zonename`
if test "$currentzone" = "global"; then
    echo "This script will setup and load the VirtualBox kernel module..."
    # vboxdrv.sh would've been installed, we just need to call it.
    /opt/VirtualBox/vboxdrv.sh restart silentunload
fi

# create links
echo "Creating links..."
#/usr/sbin/installf -c none $PKGINST /dev/vboxdrv=../devices/pseudo/vboxdrv@0:vboxdrv s
if test -f /opt/VirtualBox/VirtualBox; then
    /usr/sbin/installf -c none $PKGINST /usr/bin/VirtualBox=/opt/VirtualBox/VBox.sh s
    # Qt links
    /usr/sbin/installf -c none $PKGINST /usr/bin/VBoxQtconfig=/opt/VirtualBox/VBox.sh s
    /usr/sbin/installf -c none $PKGINST /opt/VirtualBox/qtgcc/lib/libqt-mt.so=/opt/VirtualBox/qtgcc/lib/libqt-mt.so.3 s
    /usr/sbin/installf -c none $PKGINST /opt/VirtualBox/qtgcc/lib/libqt-mt.so.3.3=/opt/VirtualBox/qtgcc/lib/libqt-mt.so.3 s
    /usr/sbin/installf -c none $PKGINST /opt/VirtualBox/qtgcc/lib/libqt-mt.so.3.3.8=/opt/VirtualBox/qtgcc/lib/libqt-mt.so.3 s
fi
/usr/sbin/installf -c none $PKGINST /usr/bin/VBoxManage=/opt/VirtualBox/VBox.sh s
/usr/sbin/installf -c none $PKGINST /usr/bin/VBoxSDL=/opt/VirtualBox/VBox.sh s
if test -f /opt/VirtualBox/VBoxHeadless; then
    /usr/sbin/installf -c none $PKGINST /usr/bin/VBoxHeadless=/opt/VirtualBox/VBox.sh s
    if test -f /opt/VirtualBox/VBoxVRDP.so; then
        /usr/sbin/installf -c none $PKGINST /usr/bin/VBoxVRDP=/opt/VirtualBox/VBox.sh s
    fi
fi
/usr/sbin/removef $PKGINST /opt/VirtualBox/etc/devlink.tab 1>/dev/null
/usr/sbin/removef $PKGINST /opt/VirtualBox/etc 1>/dev/null
rm -rf /opt/VirtualBox/etc
/usr/sbin/removef -f $PKGINST

/usr/sbin/installf -f $PKGINST

if test "$currentzone" = "global"; then
    /usr/sbin/devlinks
fi

echo "Done."

# return 20 = requires reboot, 2 = partial failure, 0  = success
exit 0

