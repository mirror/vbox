#!/bin/sh
# Sun xVM VirtualBox
# VirtualBox preremove script for Solaris.
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

echo "Sun xVM VirtualBox - preremove script"
echo "This script will unload the VirtualBox kernel module..."

currentzone=`zonename`
if test "$currentzone" = "global"; then
    # stop and unregister webservice SMF (if present)
    webservicefound=`svcs -a | grep "virtualbox/webservice"`
    if test ! -z "$webservicefound"; then
        /usr/sbin/svcadm disable -s svc:/application/virtualbox/webservice:default
        /usr/sbin/svccfg delete svc:/application/virtualbox/webservice:default
    fi

    # stop and unregister zoneaccess SMF (if present)
    zoneaccessfound=`svcs -a | grep "virtualbox/zoneaccess"`
    if test ! -z "$zoneaccessfound"; then
        /usr/sbin/svcadm disable -s svc:/application/virtualbox/zoneaccess
        /usr/sbin/svccfg delete svc:/application/virtualbox/zoneaccess
    fi

    # vboxdrv.sh would've been installed, we just need to call it.
    /opt/VirtualBox/vboxdrv.sh usbstop alwaysremdrv
    /opt/VirtualBox/vboxdrv.sh netstop alwaysremdrv
    /opt/VirtualBox/vboxdrv.sh fltstop alwaysremdrv
    /opt/VirtualBox/vboxdrv.sh stop alwaysremdrv

    # remove devlink.tab entry for vboxdrv
    sed -e '
/name=vboxdrv/d' /etc/devlink.tab > /etc/devlink.vbox
    mv -f /etc/devlink.vbox /etc/devlink.tab

    # remove devlink.tab entry for vboxusbmon
    sed -e '
/name=vboxusbmon/d' /etc/devlink.tab > /etc/devlink.vbox
    mv -f /etc/devlink.vbox /etc/devlink.tab

    # remove the devlinks
    if test -h "/dev/vboxdrv" || test -f "/dev/vboxdrv"; then
        rm -f /dev/vboxdrv
    fi
    if test -h "/dev/vboxusbmon" || test -f "/dev/vboxusbmon"; then
        rm -f /dev/vboxusbmon
    fi
fi

echo "Done."

