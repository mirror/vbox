#!/bin/sh
# Sun VirtualBox
# VirtualBox postinstall script for Solaris.
#
# Copyright (C) 2008-2009 Sun Microsystems, Inc.
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
    # Remove compressed names from the pkg
    /usr/sbin/removef $PKGINST "$1/VBoxClient.Z" 1>/dev/null
    /usr/sbin/removef $PKGINST "$1/VBoxService.Z" 1>/dev/null
    /usr/sbin/removef $PKGINST "$1/VBoxControl.Z" 1>/dev/null
    /usr/sbin/removef $PKGINST "$1/vboxvideo_drv_13.so.Z" 1>/dev/null
    /usr/sbin/removef $PKGINST "$1/vboxvideo_drv_14.so.Z" 1>/dev/null
    /usr/sbin/removef $PKGINST "$1/vboxvideo_drv_15.so.Z" 1>/dev/null
    /usr/sbin/removef $PKGINST "$1/vboxvideo_drv_16.so.Z" 1>/dev/null
    /usr/sbin/removef $PKGINST "$1/vboxvideo_drv_71.so.Z" 1>/dev/null
    /usr/sbin/removef $PKGINST "$1/vboxmouse_drv_14.so.Z" 1>/dev/null
    /usr/sbin/removef $PKGINST "$1/vboxmouse_drv_15.so.Z" 1>/dev/null
    /usr/sbin/removef $PKGINST "$1/vboxmouse_drv_16.so.Z" 1>/dev/null
    /usr/sbin/removef $PKGINST "$1/vboxmouse_drv_70.so.Z" 1>/dev/null
    /usr/sbin/removef $PKGINST "$1/vboxmouse_drv_71.so.Z" 1>/dev/null

    # Add uncompressed names to the pkg
    /usr/sbin/installf -c none $PKGINST "$1/VBoxClient" f
    /usr/sbin/installf -c none $PKGINST "$1/VBoxService" f
    /usr/sbin/installf -c none $PKGINST "$1/VBoxControl" f
    /usr/sbin/installf -c none $PKGINST "$1/vboxvideo_drv_13.so" f
    /usr/sbin/installf -c none $PKGINST "$1/vboxvideo_drv_14.so" f
    /usr/sbin/installf -c none $PKGINST "$1/vboxvideo_drv_15.so" f
    /usr/sbin/installf -c none $PKGINST "$1/vboxvideo_drv_16.so" f
    /usr/sbin/installf -c none $PKGINST "$1/vboxvideo_drv_71.so" f
    /usr/sbin/installf -c none $PKGINST "$1/vboxmouse_drv_14.so" f
    /usr/sbin/installf -c none $PKGINST "$1/vboxmouse_drv_15.so" f
    /usr/sbin/installf -c none $PKGINST "$1/vboxmouse_drv_16.so" f
    /usr/sbin/installf -c none $PKGINST "$1/vboxmouse_drv_70.so" f
    /usr/sbin/installf -c none $PKGINST "$1/vboxmouse_drv_71.so" f

    # Overwrite compressed with uncompressed file
    uncompress -f "$1/VBoxClient.Z" > /dev/null 2>&1
    uncompress -f "$1/VBoxService.Z" > /dev/null 2>&1
    uncompress -f "$1/VBoxControl.Z" > /dev/null 2>&1
    uncompress -f "$1/vboxvideo_drv_13.so.Z" > /dev/null 2>&1
    uncompress -f "$1/vboxvideo_drv_14.so.Z" > /dev/null 2>&1
    uncompress -f "$1/vboxvideo_drv_15.so.Z" > /dev/null 2>&1
    uncompress -f "$1/vboxvideo_drv_16.so.Z" > /dev/null 2>&1
    uncompress -f "$1/vboxvideo_drv_71.so.Z" > /dev/null 2>&1
    uncompress -f "$1/vboxmouse_drv_14.so.Z" > /dev/null 2>&1
    uncompress -f "$1/vboxmouse_drv_15.so.Z" > /dev/null 2>&1
    uncompress -f "$1/vboxmouse_drv_16.so.Z" > /dev/null 2>&1
    uncompress -f "$1/vboxmouse_drv_70.so.Z" > /dev/null 2>&1
    uncompress -f "$1/vboxmouse_drv_71.so.Z" > /dev/null 2>&1
}

solaris64dir="amd64"
vboxadditions_path="/opt/VirtualBoxAdditions"
vboxadditions64_path=$vboxadditions_path/$solaris64dir

# get what ISA the guest is running
cputype=`isainfo -k`
if test "$cputype" = "amd64"; then
    isadir=$solaris64dir
else
    isadir=""
fi
vboxadditionsisa_path=$vboxadditions_path/$isadir


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
$vboxadditions_path/vboxguest.sh restartall silentunload

sed -e '
/name=vboxguest/d' /etc/devlink.tab > /etc/devlink.vbox
echo "type=ddi_pseudo;name=vboxguest	\D" >> /etc/devlink.vbox
mv -f /etc/devlink.vbox /etc/devlink.tab

# create the device link
/usr/sbin/devfsadm -i vboxguest
sync

# create links
echo "Creating links..."
/usr/sbin/installf -c none $PKGINST /dev/vboxguest=../devices/pci@0,0/pci80ee,cafe@4:vboxguest s
/usr/sbin/installf -c none $PKGINST /usr/bin/VBoxClient=$vboxadditions_path/VBox.sh s
/usr/sbin/installf -c none $PKGINST /usr/bin/VBoxService=$vboxadditions_path/VBox.sh s
/usr/sbin/installf -c none $PKGINST /usr/bin/VBoxControl=$vboxadditions_path/VBox.sh s
/usr/sbin/installf -c none $PKGINST /usr/bin/VBoxRandR=$vboxadditions_path/VBoxRandR.sh s
/usr/sbin/installf -c none $PKGINST /usr/bin/VBoxClient-all=$vboxadditions_path/1099.vboxclient s

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
    1.5.99 | 1.6.* )
        vboxmouse_src="vboxmouse_drv_16.so"
        vboxvideo_src="vboxvideo_drv_16.so"
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

        # Removing redundent names from pkg and files from disk
        /usr/sbin/removef $PKGINST $vboxadditions_path/vboxmouse_drv_* 1>/dev/null
        /usr/sbin/removef $PKGINST $vboxadditions_path/vboxvideo_drv_* 1>/dev/null
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

        # Removing redundent names from pkg and files from disk
        /usr/sbin/removef $PKGINST $vboxadditions64_path/vboxmouse_drv_* 1>/dev/null
        /usr/sbin/removef $PKGINST $vboxadditions64_path/vboxvideo_drv_* 1>/dev/null
        rm -f $vboxadditions64_path/vboxmouse_drv_*
        rm -f $vboxadditions64_path/vboxvideo_drv_*
    fi

    # Some distros like Indiana have no xorg.conf, deal with this
    if test ! -f '/etc/X11/xorg.conf' && test ! -f '/etc/X11/.xorg.conf'; then

        # Xorg 1.3.x+ should use the modeline less Xorg confs while older should
        # use ones with all the video modelines in place. Argh.
        xorgconf_file="solaris_xorg_modeless.conf"
        xorgconf_unfit="solaris_xorg.conf"
        case "$xorgversion" in
            7.1.* | 7.2.* | 6.9.* | 7.0.* )
                xorgconf_file="solaris_xorg.conf"
                xorgconf_unfit="solaris_xorg_modeless.conf"
                ;;
        esac

        /usr/sbin/removef $PKGINST $vboxadditions_path/$xorgconf_file 1>/dev/null
        mv -f $vboxadditions_path/$xorgconf_file /etc/X11/.xorg.conf

        /usr/sbin/removef $PKGINST $vboxadditions_path/$xorgconf_unfit 1>/dev/null
        rm -f $vboxadditions_path/$xorgconf_unfit
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

# Shared Folder kernel module (different for S10 & Nevada)
osverstr=`uname -r`
vboxfsmod="vboxfs"
vboxfsunused="vboxfs_s10"
if test "$osverstr" = "5.10"; then
    vboxfsmod="vboxfs_s10"
    vboxfsunused="vboxfs"
fi

# Move the appropriate module to kernel/fs & remove the unused module name from pkg and file from disk
# 64-bit shared folder module
if test -f "$vboxadditions64_path/$vboxfsmod"; then
    /usr/sbin/installf -c none $PKGINST "/usr/kernel/fs/$solaris64dir/vboxfs" f
    mv -f $vboxadditions64_path/$vboxfsmod /usr/kernel/fs/$solaris64dir/vboxfs
    /usr/sbin/removef $PKGINST $vboxadditions64_path/$vboxfsmod 1>/dev/null
    /usr/sbin/removef $PKGINST $vboxadditions64_path/$vboxfsunused 1>/dev/null
    rm -f $vboxadditions64_path/$vboxfsunused
fi

# 32-bit shared folder module
if test -f "$vboxadditions_path/$vboxfsmod"; then
    /usr/sbin/installf -c none $PKGINST "/usr/kernel/fs/vboxfs" f
    mv -f $vboxadditions_path/$vboxfsmod /usr/kernel/fs/vboxfs
    /usr/sbin/removef $PKGINST $vboxadditions_path/$vboxfsmod 1>/dev/null
    /usr/sbin/removef $PKGINST $vboxadditions_path/$vboxfsunused 1>/dev/null
    rm -f $vboxadditions_path/$vboxfsunused
fi


# 32-bit crogl opengl library replacement
if test -f "/usr/lib/VBoxOGL.so"; then
    cp -f /usr/X11/lib/mesa/libGL.so.1 /usr/X11/lib/mesa/libGL_original_.so.1
    ln -sf /usr/lib/VBoxOGL.so /usr/X11/lib/mesa/libGL.so.1
fi

# 64-bit crogl opengl library replacement
if test -f "/usr/lib/amd64/VBoxOGL.so"; then
    cp -f /usr/X11/lib/mesa/amd64/libGL.so.1 /usr/X11/lib/mesa/amd64/libGL_original_.so.1
    ln -sf /usr/lib/amd64/VBoxOGL.so /usr/X11/lib/mesa/amd64/libGL.so.1
fi

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

