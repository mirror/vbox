#!/bin/sh
# innotek VirtualBox
# VirtualBox postinstall script for Solaris.
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

echo "innotek VirtualBox Guest Additions - postinstall script"
echo "This script will setup and load the VirtualBox Guest kernel module..."

sync
vboxadditions_path="/opt/VirtualBoxAdditions"

# vboxguest.sh would've been installed, we just need to call it.
$vboxadditions_path/vboxguest.sh restart silentunload

# suid permissions for timesync
echo "Setting permissions..."
chmod 04755 $vboxadditions_path/VBoxService

# create links
echo "Creating links..."
#/usr/sbin/installf -c none $PKGINST /dev/vboxguest=../devices/pci@0,0/pci80ee,cafe@4:vboxguest s
/usr/sbin/installf -c none $PKGINST /usr/bin/VBoxClient=$vboxadditions_path/VBoxClient s
/usr/sbin/installf -c none $PKGINST /usr/bin/VBoxService=$vboxadditions_path/VBoxService s

# Install Xorg components to the required places
xorgversion_long=`/usr/bin/X11/Xorg -version 2>&1 | grep "X Window System Version"`
xorgversion=`/usr/bin/expr "${xorgversion_long}" : 'X Window System Version \([^ ]*\)'`

vboxmouse_src=""
vboxvideo_src=""

case "$xorgversion" in
    1.3.* )
        vboxmouse_src="$vboxadditions_path/vboxmouse_drv_71.so"
        vboxvideo_src="$vboxadditions_path/vboxvideo_drv_13.so"
        ;;
    1.4.* )
        vboxmouse_src="$vboxadditions_path/vboxmouse_drv_14.so"
        vboxvideo_src="$vboxadditions_path/vboxvideo_drv_14.so"
        ;;
    7.1.* | *7.2.* )
        vboxmouse_src="$vboxadditions_path/vboxmouse_drv_71.so"
        vboxvideo_src="$vboxadditions_path/vboxvideo_drv_71.so"
	    ;;
    6.9.* | 7.0.* )
        vboxmouse_src="$vboxadditions_path/vboxmouse_drv_70.so"
        vboxvideo_src="$vboxadditions_path/vboxvideo_drv_70.so"
        ;;
esac

retval=0
if test -z "$vboxmouse_src"; then
    echo "Unknown version of the X Window System installed."
    echo "Failed to install the VirtualBox X Window System drivers."
    
    # Exit as partially failed installation
    retval=2
else
    vboxmouse_dest="/usr/lib/X11/modules/input/vboxmouse_drv.so"
    vboxvideo_dest="/usr/lib/X11/modules/input/vboxvideo_drv.so"
    /usr/sbin/installf -c none $PKGINST "$vboxmouse_dest" f
    /usr/sbin/installf -c none $PKGINST "$vboxvideo_dest" f
    cp "$vboxmouse_src" "$vboxmouse_dest"
    cp "$vboxvideo_src" "$vboxvideo_dest"
    echo "Installed VirtualBox mouse and video drivers for Xorg $xorgversion"

    # Removing redudant files
    /usr/sbin/removef $PKGINST $vboxadditions_path/vboxmouse_drv_* 1>/dev/null 2>/dev/null
    /usr/sbin/removef $PKGINST $vboxadditions_path/vboxvideo_drv_* 1>/dev/null 2>/dev/null
    rm -f $vboxadditions_path/vboxmouse_drv_*
    rm -f $vboxadditions_path/vboxvideo_drv_*
    /usr/sbin/removef -f $PKGINST

    # Some distros like Indiana have no xorg.conf, deal with this
    if ! (test -f '/etc/X11/xorg.conf' -o -f '/etc/X11/.xorg.conf'); then
        mv -f $vboxadditions_path/solaris_xorg.conf /etc/X11/.xorg.conf
    fi

    echo "Configuring Xorg..."
    $vboxadditions_path/x11config.pl
fi

# Remove redundant files
#/usr/sbin/removef $PKGINST $vboxadditions_path/etc/devlink.tab 1>/dev/null
#/usr/sbin/removef $PKGINST $vboxadditions_path/etc 1>/dev/null
#rm -rf $vboxadditions_path/etc
#/usr/sbin/removef -f $PKGINST

/usr/sbin/installf -f $PKGINST

#/usr/sbin/devlinks

# Setup our VBoxService SMF service
echo "Configuring service..."

/usr/sbin/svccfg import /var/svc/manifest/system/virtualbox/vboxservice.xml
/usr/sbin/svcadm enable svc:/system/virtualbox/vboxservice

echo "Done."
if test $retval -eq 0; then
    echo "Please restart X Window System for activating the X11 guest additions."
fi
exit $retval

