#!/bin/sh
# Sun xVM VirtualBox
# VirtualBox postinstall script for Solaris.
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

uncompress_files()
{
    # self-overwriting
    uncompress -f "$1/VBoxClient.Z" > /dev/null 2>&1
    uncompress -f "$1/VBoxService.Z" > /dev/null 2>&1
    uncompress -f "$1/VBoxControl.Z" > /dev/null 2>&1
    uncompress -f "$1/vboxvideo_drv_13.so.Z" > /dev/null 2>&1
    uncompress -f "$1/vboxvideo_drv_14.so.Z" > /dev/null 2>&1
    uncompress -f "$1/vboxvideo_drv_15.so.Z" > /dev/null 2>&1
    uncompress -f "$1/vboxvideo_drv_71.so.Z" > /dev/null 2>&1
    uncompress -f "$1/vboxmouse_drv_14.so.Z" > /dev/null 2>&1
    uncompress -f "$1/vboxmouse_drv_15.so.Z" > /dev/null 2>&1
    uncompress -f "$1/vboxmouse_drv_70.so.Z" > /dev/null 2>&1
    uncompress -f "$1/vboxmouse_drv_71.so.Z" > /dev/null 2>&1
}

vboxadditions_path="/opt/VirtualBoxAdditions"
vboxadditions64_path=$vboxadditions_path/amd64
solaris64dir="amd64"

# uncompress if necessary
if test -f "$vboxadditions_path/VBoxClient.Z" || test -f "$vboxadditions64_path/VBoxClient.Z"; then
    echo "Uncompressing files..."
    if test -f "$vboxadditions_path/VBoxClient.Z"; then
        uncompress_files "$vboxadditions_path"
    fi
    if test -f "$vboxadditions64_path/VBoxClient.Z"; then
        uncompress_files "$vboxadditions64_path"
    fi
fi

# vboxguest.sh would've been installed, we just need to call it.
echo "Configuring VirtualBox guest kernel module..."
$vboxadditions_path/vboxguest.sh restart silentunload

sed -e '
/name=vboxguest/d' /etc/devlink.tab > /etc/devlink.vbox
echo "type=ddi_pseudo;name=vboxguest	\D" >> /etc/devlink.vbox
mv -f /etc/devlink.vbox /etc/devlink.tab

# create the device link
/usr/sbin/devfsadm -i vboxguest
sync

# get what ISA the guest is running 
cputype=`isainfo -k` 
isadir="" 
if test "$cputype" = "amd64"; then 
    isadir="amd64" 
fi

# create links
echo "Creating links..."
/usr/sbin/installf -c none $PKGINST /dev/vboxguest=../devices/pci@0,0/pci80ee,cafe@4:vboxguest s
/usr/sbin/installf -c none $PKGINST /usr/bin/VBoxClient=$vboxadditions_path/VBox.sh s
/usr/sbin/installf -c none $PKGINST /usr/bin/VBoxService=$vboxadditions_path/VBox.sh s
/usr/sbin/installf -c none $PKGINST /usr/bin/VBoxControl=$vboxadditions_path/VBox.sh s
/usr/sbin/installf -c none $PKGINST /usr/bin/VBoxRandR=$vboxadditions_path/VBoxRandR.sh s

# Install Xorg components to the required places
xorgversion_long=`/usr/X11/bin/Xorg -version 2>&1 | grep "X Window System Version"`
xorgversion=`/usr/bin/expr "${xorgversion_long}" : 'X Window System Version \([^ ]*\)'`
if test -z "$xorgversion_long"; then
    xorgversion_long=`/usr/X11/bin/Xorg -version 2>&1 | grep "X.Org X Server"`
    xorgversion=`/usr/bin/expr "${xorgversion_long}" : 'X.Org X Server \([^ ]*\)'`
fi

vboxmouse_src=""
vboxvideo_src=""

case "$xorgversion" in
    1.3.* )
        vboxmouse_src="vboxmouse_drv_71.so"
        vboxvideo_src="vboxvideo_drv_13.so"
        ;;
    1.4.* )
        vboxmouse_src="vboxmouse_drv_14.so"
        vboxvideo_src="vboxvideo_drv_14.so"
        ;;
    1.5.* )
        vboxmouse_src="vboxmouse_drv_15.so"
        vboxvideo_src="vboxvideo_drv_15.so"
        ;;    
    7.1.* | *7.2.* )
        vboxmouse_src="vboxmouse_drv_71.so"
        vboxvideo_src="vboxvideo_drv_71.so"
	    ;;
    6.9.* | 7.0.* )
        vboxmouse_src="vboxmouse_drv_70.so"
        vboxvideo_src="vboxvideo_drv_70.so"
        ;;
esac

retval=0
if test -z "$vboxmouse_src"; then
    echo "*** Unknown version of the X Window System installed."
    echo "*** Failed to install the VirtualBox X Window System drivers."

    # Exit as partially failed installation
    retval=2
else
    echo "Configuring Xorg..."

    # 32-bit x11 drivers
    if test -f "$vboxadditions_path/$vboxmouse_src"; then
        vboxmouse_dest="/usr/X11/lib/modules/input/vboxmouse_drv.so"
        vboxvideo_dest="/usr/X11/lib/modules/drivers/vboxvideo_drv.so"
        /usr/sbin/installf -c none $PKGINST "$vboxmouse_dest" f
        /usr/sbin/installf -c none $PKGINST "$vboxvideo_dest" f
        cp "$vboxadditions_path/$vboxmouse_src" "$vboxmouse_dest"
        cp "$vboxadditions_path/$vboxvideo_src" "$vboxvideo_dest"

        # Removing redundent files
        /usr/sbin/removef $PKGINST $vboxadditions_path/vboxmouse_drv_* 1>/dev/null 2>/dev/null
        /usr/sbin/removef $PKGINST $vboxadditions_path/vboxvideo_drv_* 1>/dev/null 2>/dev/null
        rm -f $vboxadditions_path/vboxmouse_drv_*
        rm -f $vboxadditions_path/vboxvideo_drv_*
    fi

    # 64-bit x11 drivers
    if test -f "$vboxadditions64_path/$vboxmouse_src"; then
        vboxmouse_dest="/usr/X11/lib/modules/input/$solaris64dir/vboxmouse_drv.so"
        vboxvideo_dest="/usr/X11/lib/modules/drivers/$solaris64dir/vboxvideo_drv.so"
        /usr/sbin/installf -c none $PKGINST "$vboxmouse_dest" f
        /usr/sbin/installf -c none $PKGINST "$vboxvideo_dest" f
        cp "$vboxadditions64_path/$vboxmouse_src" "$vboxmouse_dest"
        cp "$vboxadditions64_path/$vboxvideo_src" "$vboxvideo_dest"

        # Removing redundent files
        /usr/sbin/removef $PKGINST $vboxadditions64_path/vboxmouse_drv_* 1>/dev/null 2>/dev/null
        /usr/sbin/removef $PKGINST $vboxadditions64_path/vboxvideo_drv_* 1>/dev/null 2>/dev/null
        rm -f $vboxadditions64_path/vboxmouse_drv_*
        rm -f $vboxadditions64_path/vboxvideo_drv_*
    fi

    # Some distros like Indiana have no xorg.conf, deal with this
    if test ! -f '/etc/X11/xorg.conf' && test ! -f '/etc/X11/.xorg.conf'; then
        mv -f $vboxadditions_path/solaris_xorg.conf /etc/X11/.xorg.conf
    fi

    $vboxadditions_path/x11config.pl
fi


# Setup our VBoxClient
echo "Configuring client..."
vboxclient_src=$vboxadditions_path
vboxclient_dest="/usr/dt/config/Xsession.d"
if test -d "$vboxclient_dest"; then
    /usr/sbin/installf -c none $PKGINST "$vboxclient_dest/1099.vboxclient" f
    cp "$vboxclient_src/1099.vboxclient" "$vboxclient_dest/1099.vboxclient"
    chmod a+rx "$vboxclient_dest/1099.vboxclient"
elif test -d "/usr/share/gnome/autostart"; then
    vboxclient_dest="/usr/share/gnome/autostart"
    /usr/sbin/installf -c none $PKGINST "$vboxclient_dest/vboxclient.desktop" f
    cp "$vboxclient_src/vboxclient.desktop" "$vboxclient_dest/vboxclient.desktop"
else
    echo "*** Failed to configure client!! Couldn't find autostart directory."
    retval=2
fi


# Remove redundant files
/usr/sbin/removef $PKGINST $vboxadditions_path/etc/devlink.tab 1>/dev/null
/usr/sbin/removef $PKGINST $vboxadditions_path/etc 1>/dev/null
rm -rf $vboxadditions_path/etc

# Finalize
/usr/sbin/removef -f $PKGINST
/usr/sbin/installf -f $PKGINST


# Setup our VBoxService SMF service
echo "Configuring service..."

/usr/sbin/svccfg import /var/svc/manifest/system/virtualbox/vboxservice.xml
/usr/sbin/svcadm enable svc:/system/virtualbox/vboxservice

/usr/sbin/devfsadm -i vboxguest

echo "Done."
if test $retval -eq 0; then
    echo "Please re-login to activate the X11 guest additions."
    echo "If you have just un-installed the previous guest additions a REBOOT is required."
fi
exit $retval

