#!/bin/sh
## @file
# Sun xVM VirtualBox
# VirtualBox postinstall script for Solaris.
#

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

# Check for xVM/Xen
currentisa=`uname -i`
if test "$currentisa" = "i86xpv"; then
    echo "## VirtualBox cannot run under xVM Dom0! Fatal Error, Aborting installation!"
    exit 2
fi

currentzone=`zonename`
if test "$currentzone" = "global"; then
    echo "Configuring VirtualBox kernel module(s)..."
    /opt/VirtualBox/vboxdrv.sh stopall silentunload
    rc=$?
    if test "$rc" -eq 0; then
        /opt/VirtualBox/vboxdrv.sh start
        rc=$?
        if test "$rc" -eq 0; then
            if test -f /platform/i86pc/kernel/drv/vboxflt.conf; then
                /opt/VirtualBox/vboxdrv.sh fltstart
                rc=$?            
            fi
        fi
    fi

    # Fail on any errors while unloading previous modules because it makes it very hard to
    # track problems when older vboxdrv is hanging about in memory and add_drv of the new
    # one suceeds and it appears as though the new one is being used.
    if test "$rc" -ne 0; then
        echo "## Configuration failed. Aborting installation."
        exit 2
    fi
fi

# create links
echo "Creating links..."
if test -f /opt/VirtualBox/VirtualBox; then
    /usr/sbin/installf -c none $PKGINST /usr/bin/VirtualBox=/opt/VirtualBox/VBox.sh s
    # Qt links
    /usr/sbin/installf -c none $PKGINST /usr/bin/VBoxQtconfig=/opt/VirtualBox/VBox.sh s
fi
/usr/sbin/installf -c none $PKGINST /usr/bin/VBoxManage=/opt/VirtualBox/VBox.sh s
/usr/sbin/installf -c none $PKGINST /usr/bin/VBoxSDL=/opt/VirtualBox/VBox.sh s
if test -f /opt/VirtualBox/VBoxHeadless; then
    /usr/sbin/installf -c none $PKGINST /usr/bin/VBoxHeadless=/opt/VirtualBox/VBox.sh s
    if test -f /opt/VirtualBox/VBoxVRDP.so; then
        /usr/sbin/installf -c none $PKGINST /usr/bin/VBoxVRDP=/opt/VirtualBox/VBox.sh s
    fi
fi

if test "$currentzone" = "global"; then
    # Web service
    if test -f /var/svc/manifest/application/virtualbox/webservice.xml; then
        /usr/sbin/svccfg import /var/svc/manifest/application/virtualbox/webservice.xml
        /usr/sbin/svcadm disable -s svc:/application/virtualbox/webservice:default
    fi

    # add vboxdrv to the devlink.tab
    sed -e '
/name=vboxdrv/d' /etc/devlink.tab > /etc/devlink.vbox
    echo "type=ddi_pseudo;name=vboxdrv	\D" >> /etc/devlink.vbox
    mv -f /etc/devlink.vbox /etc/devlink.tab

    # create the device link
    /usr/sbin/devfsadm -i vboxdrv

    # We need to touch the desktop link in order to add it to the menu right away
    if test -f "/usr/share/applications/virtualbox.desktop"; then
        touch /usr/share/applications/virtualbox.desktop
    fi

    # Zone access service
    if test -f /var/svc/manifest/application/virtualbox/zoneaccess.xml; then
        /usr/sbin/svccfg import /var/svc/manifest/application/virtualbox/zoneaccess.xml
        /usr/sbin/svcadm enable -s svc:/application/virtualbox/zoneaccess
    fi
fi

/usr/sbin/installf -f $PKGINST

echo "Done."

# return 20 = requires reboot, 2 = partial failure, 0  = success
exit 0

