#!/bin/sh
## @file
# Sun VirtualBox - Manual IPS/pkg(5) postinstall script for Solaris.
#
# If you just installed VirtualBox using IPS/pkg(5), you should run this
# script once to avoid rebooting the system before using VirtualBox.
#

#
# Copyright (C) 2009 Sun Microsystems, Inc.
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

check_root()
{
    idbin=/usr/xpg4/bin/id
    if test ! -f "$idbin"; then
        found=`which id`
        if test ! -f "$found" || test ! -h "$found"; then
            echo "## Failed to find a suitable user id binary."
            exit 1
        else
            idbin=$found
        fi
    fi

    if test `$idbin -u` -ne 0; then
        echo "## This script must be run with administrator privileges."
        exit 2
    fi
}

check_zone()
{
    currentzone=`zonename`
    if test "$currentzone" != "global"; then
        echo "## This script must be run from the global zone."
        exit 3
    fi
}

install_python_bindings()
{
    PYTHONBIN=$1
    if test -x "$PYTHONBIN"; then
        VBOX_INSTALL_PATH=/opt/VirtualBox
        export VBOX_INSTALL_PATH
        cd /opt/VirtualBox/sdk/installer
        $PYTHONBIN ./vboxapisetup.py install > /dev/null
        return 0
    fi
    return 1
}

check_root
check_zone

osversion=`uname -r`

BIN_REMDRV=/usr/sbin/rem_drv
BIN_ADDDRV=/usr/sbin/add_drv
BIN_MODLOAD=/usr/sbin/modload
BIN_DEVFSADM=/usr/sbin/devfsadm

# Halt services in case of installation update
zoneaccessfound=`svcs -a | grep "virtualbox/zoneaccess"`
if test ! -z "$zoneaccessfound"; then
    /usr/sbin/svcadm disable -s svc:/application/virtualbox/zoneaccess
fi

# Remove drivers ignoring errors as they are not really loaded
# just updated various boot archive files without really loading
# them... But we _want_ them to be loaded.
echo "Removing stale driver configurations..."

$BIN_REMDRV vboxflt > /dev/null 2>&1
/sbin/ifconfig vboxnet0 unplumb > /dev/null 2>&1
$BIN_REMDRV vboxnet > /dev/null 2>&1
$BIN_REMDRV vboxusbmon > /dev/null 2>&1
$BIN_REMDRV vboxdrv > /dev/null 2>&1

echo "Loading VirtualBox Drivers:"
# Add drivers the proper way and load them immediately
/opt/VirtualBox/vboxdrv.sh start
rc=$?
if test "$rc" -eq 0; then
    # Add vboxdrv to the devlink.tab
    sed -e '/name=vboxdrv/d' /etc/devlink.tab > /etc/devlink.vbox
    echo "type=ddi_pseudo;name=vboxdrv	\D" >> /etc/devlink.vbox
    mv -f /etc/devlink.vbox /etc/devlink.tab

    # Create the device link
    /usr/sbin/devfsadm -i vboxdrv
    rc=$?

    if test "$rc" -eq 0; then
        # Load VBoxNetAdapter vboxnet
        if test -f /platform/i86pc/kernel/drv/vboxnet.conf; then
            /opt/VirtualBox/vboxdrv.sh netstart
            rc=$?

            if test "$rc" -eq 0; then
                # nwam/dhcpagent fix
                nwamfile=/etc/nwam/llp
                nwambackupfile=$nwamfile.vbox
                if test -f "$nwamfile"; then
                    sed -e '/vboxnet/d' $nwamfile > $nwambackupfile
                    echo "vboxnet0	static 192.168.56.1" >> $nwambackupfile
                    mv -f $nwambackupfile $nwamfile
                    echo "   -> patched /etc/nwam/llp to use static IP for vboxnet0"
                fi
            fi
        fi
    else
        echo "## Failed to create device link in /dev for vboxdrv"
    fi

    # Load VBoxNetFilter vboxflt
    if test "$rc" -eq 0 && test -f /platform/i86pc/kernel/drv/vboxflt.conf; then
        /opt/VirtualBox/vboxdrv.sh fltstart
        rc=$?
    fi

    # Load VBoxUSBMonitor vboxusbmon (do NOT load for Solaris 10)
    if test "$rc" -eq 0 && test -f /platform/i86pc/kernel/drv/vboxusbmon.conf && test "$osversion" != "5.10"; then
        /opt/VirtualBox/vboxdrv.sh usbstart
        rc=$?
        if test "$rc" -eq 0; then

            # Add vboxusbmon to the devlink.tab
            sed -e '/name=vboxusbmon/d' /etc/devlink.tab > /etc/devlink.vbox
            echo "type=ddi_pseudo;name=vboxusbmon	\D" >> /etc/devlink.vbox
            mv -f /etc/devlink.vbox /etc/devlink.tab

            /usr/sbin/devfsadm -i vboxusbmon
            rc=$?
            if test "$rc" -ne 0; then
                echo "## Failed to create device link in /dev for vboxusbmon"
            fi
        fi
    fi
fi

rc2=0
if test -f "/var/svc/manifest/application/virtualbox/virtualbox-zoneaccess.xml"; then
    /usr/sbin/svccfg import /var/svc/manifest/application/virtualbox/virtualbox-zoneaccess.xml
    rc2=$?
    if test "$rc2" -eq 0; then
        /usr/sbin/svcadm enable -s svc:/application/virtualbox/zoneaccess
        rc2=$?
        if test "$rc2" -eq 0; then
            echo "Enabled VirtualBox zone service."
        else
            echo "## Failed to enable VirtualBox zone service."
        fi
    else
        echo "## Failed to import VirtualBox zone service."
    fi
fi

# We need to touch the desktop link in order to add it to the menu right away
if test -f "/usr/share/applications/virtualbox.desktop"; then
    touch /usr/share/applications/virtualbox.desktop
    echo "Added VirtualBox shortcut menu item."
fi

# Install python bindings
rc3=0
if test -f "/opt/VirtualBox/sdk/installer/vboxapisetup.py" || test -h "/opt/VirtualBox/sdk/installer/vboxapisetup.py"; then
    PYTHONBIN=`which python`
    if test -f "$PYTHONBIN" || test -h "$PYTHONBIN"; then
        echo "Installing Python bindings..."

        INSTALLEDIT=1
        PYTHONBIN=`which python2.4`
        install_python_bindings "$PYTHONBIN"
        if test "$?" -eq 0; then
            INSTALLEDIT=0
        fi
        PYTHONBIN=`which python2.5`
        install_python_bindings "$PYTHONBIN"
        if test "$?" -eq 0; then
            INSTALLEDIT=0
        fi
        PYTHONBIN=`which python2.6`
        install_python_bindings "$PYTHONBIN"
        if test "$?" -eq 0; then 
            INSTALLEDIT=0
        fi

        # remove files installed by Python build
        rm -rf /opt/VirtualBox/sdk/installer/build

        if test "$INSTALLEDIT" -ne 0; then
            echo "** No suitable Python version found. Requires Python 2.4, 2.5 or 2.6."
        fi
        rc3=$INSTALLEDIT
    else
        echo "** WARNING! Python not found, skipped installed Python bindings."
        echo "   Manually run '/opt/VirtualBox/sdk/installer/vboxapisetup.py install'"
        echo "   to install the bindings when python is available."
        rc3=1
    fi
fi

# Update boot archive (only when driver's were all successfully loaded)
rc4=0
if test "$rc" -eq 0; then
    BOOTADMBIN=/sbin/bootadm
    if test -f "$BOOTADMBIN" || test -h "$BOOTADMBIN"; then
        echo "Updating boot archive..."
        $BOOTADMBIN update-archive > /dev/null
        rc4=$?
    fi
fi

echo "Done."
if test "$rc" -eq 0 && test "$rc2" -eq 0 && test "$rc3" -eq 0 && test "$rc4" -eq 0; then
    echo "Post install successfully completed."
else
    echo "Post install completed but with some errors."
    # 20 - partially failed installed
    $rc=20
fi
exit $rc

