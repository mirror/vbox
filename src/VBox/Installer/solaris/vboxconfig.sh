#!/bin/sh
# $Id$

# Sun VirtualBox
# VirtualBox Configuration Script, Solaris host.
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


HOST_OS_VERSION=`uname -r`

DIR_VBOXBASE=/opt/VirtualBox
DIR_MOD_32="/platform/i86pc/kernel/drv"
DIR_MOD_64=$DIR_MOD_32/amd64

BIN_ADDDRV=/usr/sbin/add_drv
BIN_REMDRV=/usr/sbin/rem_drv
BIN_MODLOAD=/usr/sbin/modload
BIN_MODUNLOAD=/usr/sbin/modunload
BIN_MODINFO=/usr/sbin/modinfo
BIN_DEVFSADM=/usr/sbin/devfsadm
BIN_BOOTADM=/sbin/bootadm
BIN_SVCADM=/usr/sbin/svcadm
BIN_SVCCFG=/usr/sbin/svccfg
BIN_IFCONFIG=/sbin/ifconfig

# "vboxdrv" is also used in sed lines here (change those as well if it ever changes)
MOD_VBOXDRV=vboxdrv
MOD_VBOXNET=vboxnet
MOD_VBOXFLT=vboxflt
MOD_VBI=vbi
MOD_VBOXUSBMON=vboxusbmon
FATALOP=fatal


infoprint()
{
    echo 1>&2 "$1"
}

warnprint()
{
    echo 1>&2 "* Warning!! $1"
}

success()
{
    echo 1>&2 "$1"
}

errorprint()
{
    echo 1>&2 "## $1"
}


# check_bin_path()
# !! failure is always fatal
check_bin_path()
{
    if test -z "$1"; then
        errorprint "missing argument to check_bin_path()"
        exit 50
    fi

    if test !  -x "$1"; then
        errorprint "$1 missing or is not an executable"
        exit 51
    fi
    return 0
}

# find_bins()
# !! failure is always fatal
find_bins()
{
    # Search only for binaries that might be in different locations
    BIN_IFCONFIG=`which ifconfig 2> /dev/null`
    BIN_SVCS=`which svcs 2> /dev/null`

    check_bin_path "$BIN_ADDDRV"
    check_bin_path "$BIN_REMDRV"
    check_bin_path "$BIN_MODLOAD"
    check_bin_path "$BIN_MODUNLOAD"
    check_bin_path "$BIN_MODINFO"
    check_bin_path "$BIN_DEVFSADM"
    check_bin_path "$BIN_BOOTADM"
    check_bin_path "$BIN_SVCADM"
    check_bin_path "$BIN_SVCCFG"
    check_bin_path "$BIN_SVCS"
    check_bin_path "$BIN_IFCONFIG"
}

# check_root()
# !! failure is always fatal
check_root()
{
    idbin=/usr/xpg4/bin/id
    if test ! -x "$idbin"; then
        found=`which id`
        if test ! -x "$found"; then
            errorprint "Failed to find a suitable user id executable."
            exit 1
        else
            idbin=$found
        fi
    fi

    if test `$idbin -u` -ne 0; then
        errorprint "This script must be run with administrator privileges."
        exit 2
    fi
}

# check_zone()
# !! failure is always fatal
check_zone()
{
    currentzone=`zonename`
    if test "$currentzone" != "global"; then
        errorprint "This script must be run from the global zone."
        exit 3
    fi
}

# check_isa()
# !! failure is always fatal
check_isa()
{
    currentisa=`uname -i`
    if test "$currentisa" = "i86xpv"; then
        errorprint "VirtualBox cannot run under xVM Dom0! Fatal Error, Aborting installation!"
        exit 4
    fi
}

# check_module_arch()
# !! failure is always fatal
check_module_arch()
{
    cputype=`isainfo -k`
    modulepath="$DIR_MOD_32/$MOD_VBOXDRV"
    if test "$cputype" = "amd64"; then
        modulepath="$DIR_MOD_64/$MOD_VBOXDRV"
    elif test "$cputype" != "i386"; then
        errorprint "VirtualBox works only on i386/amd64 architectures, not $cputype"
        exit 98
    fi

    # If things are where they should be, return success
    if test -f "$modulepath" || test -h "$modulepath"; then
        return 0
    fi

    # Something's screwed, let us go a step further and check if user has mixed up x86/amd64
    # amd64 ISA, x86 kernel module??
    if test "$cputype" = "amd64"; then
        modulepath="$DIR_MOD_32/$MOD_VBOXDRV"
        if test -f "$modulepath"; then
            errorprint "Found 32-bit module instead of 64-bit. Please install the amd64 package!"
            exit 97
        fi
    else
        # x86 ISA, amd64 kernel module??
        modulepath="$DIR_MOD_64/$MOD_VBOXDRV"
        if test -f "$modulepath"; then
            errorprint "Found 64-bit module instead of 32-bit. Please install the x86 package!"
            exit 96
        fi
    fi

    # Shouldn't really happen...
    errorprint "VirtualBox Host kernel module NOT installed."
    exit 99
}

# module_added(modname)
# returns 0 if added, 1 otherwise
module_added()
{
    if test -z "$1"; then
        errorprint "missing argument to module_added()"
        exit 5
    fi

    loadentry=`cat /etc/name_to_major | grep $1`
    if test -z "$loadentry"; then
        return 0
    fi
    return 1
}

# module_loaded(modname)
# returns 1 if loaded, 0 otherwise
module_loaded()
{
    if test -z "$1"; then
        errorprint "missing argument to module_loaded()"
        exit 6
    fi

    modname=$1
    # modinfo should now work properly since we prevent module autounloading
    loadentry=`$BIN_MODINFO | grep $modname`
    if test -z "$loadentry"; then
        return 0
    fi
    return 1
}

# add_driver(modname, [driverperm], [fatal])
# failure: depends on [fatal]
add_driver()
{
    if test -z "$1"; then
        errorprint "missing argument to add_driver()"
        exit 7
    fi

    modname=$1
    modperm=$2
    fatal=$3
    if test -n "$modperm"; then
        $BIN_ADDDRV -m'$modperm' $modname
    else
        $BIN_ADDDRV $modname
    fi

    if test $? -ne 0; then
        errorprint "Failed to load: $modname"
        if test "$fatal" = "$FATALOP"; then
            exit 8
        fi
        return 1
    fi
    return 0
}

# rem_driver(modname, [fatal])
# failure: depends on [fatal]
rem_driver()
{
    if test -z "$1"; then
        errorprint "missing argument to rem_driver()"
        exit 9
    fi

    modname=$1
    fatal=$2
    module_added $modname
    if test "$?" -eq 0; then
        $BIN_REMDRV $modname
        if test $? -eq 0; then
            success "Removed: $modname successfully"
            return 0
        else
            errorprint "Failed to remove: $modname"
            if test "$fatal" = "$FATALOP"; then
                exit 10
            fi
            return 1
        fi
    fi
}

# unload_module(modname, [fatal])
# failure: fatal
unload_module()
{
    if test -z "$1"; then
        errorprint "missing argument to unload_module()"
        exit 11
    fi

    modname=$1
    fatal=$2
    modid=`$BIN_MODINFO | grep $modname | cut -f 1 -d ' ' `
    if test -n "$modid"; then
        $BIN_MODUNLOAD -i $modid
        if test $? -eq 0; then
            success "Unloaded: $modname successfully"
        else
            errorprint "Failed to unload: $modname"
            if test "$fatal" = "$FATALOP"; then
                exit 12
            fi
            return 1
        fi
    fi
    return 0
}

# load_module(modname)
# pass "drv/modname" or "misc/vbi" etc.
# failure: fatal
load_module()
{
    if test -z "$1"; then
        errorprint "missing argument to load_module()"
        exit 14
    fi

    modname=$1
    fatal=$2
    $BIN_MODLOAD -p $modname
    if test $? -eq 0; then
        success "Loaded: $modname successfully"
        return 0
    else
        errorprint "Failed to load: $modname"
        if test "$fatal" = "$FATALOP"; then
            exit 15
        fi
        return 1
    fi
}

# install_drivers()
# !! failure is always fatal
install_drivers()
{
    infoprint "Loading Host Driver..."
    add_driver $MOD_VBOXDRV "* 0600 root sys" fatal
    load_module $MOD_VBOXDRV fatal

    # Add vboxdrv to devlink.tab
    sed -e '/name=vboxdrv/d' /etc/devlink.tab > /etc/devlink.vbox
    echo "type=ddi_pseudo;name=vboxdrv	\D" >> /etc/devlink.vbox
    mv -f /etc/devlink.vbox /etc/devlink.tab

    # Create the device link
    /usr/sbin/devfsadm -i $MOD_VBOXDRV

    if test $? -eq 0; then

        if test -f /platform/i86pc/kernel/drv/vboxnet.conf; then
            infoprint "Loading NetAdapter..."
            add_drv $MOD_VBOXNET fatal
            load_module $MOD_VBOXNET fatal
        fi

        if test -f /platform/i86pc/kernel/drv/vboxflt.conf; then
            infoprint "Loading NetFilter..."
            add_driver $MOD_VBOXFLT fatal
            load_module $MOD_VBOXFLT fatal
        fi

        if test -f /platform/i86pc/kernel/drv/vboxusbmon.conf && test "$HOST_OS_VERSION" != "5.10"; then
            infoprint "Loading USBMonitor..."
            add_driver $MOD_VBOXUSBMON fatal
            load_module $MOD_VBOXUSBMON fatal

            # Add vboxusbmon to devlink.tab
            sed -e '/name=vboxusbmon/d' /etc/devlink.tab > /etc/devlink.vbox
            echo "type=ddi_pseudo;name=vboxusbmon	\D" >> /etc/devlink.vbox

            # Create the device link
            /usr/sbin/devfsadm -i  $MOD_VBOXUSBMON
            if test $? -ne 0; then
                errorprint "Failed to create device link for $MOD_VBOXUSBMON."
                exit 16
            fi
        fi
    else
        errorprint "Failed to create device link for $MOD_VBOXDRV."
        exit 17
    fi

    return $?
}

# remove_all([fatal])
# failure: depends on [fatal]
remove_drivers()
{
    $fatal=$1
    # Remove vboxdrv from devlink.tab
    sed -e '/name=vboxdrv/d' /etc/devlink.tab > /etc/devlink.vbox
    mv -f /etc/devlink.vbox /etc/devlink.tab

    # Remove vboxusbmon from devlink.tab
    sed -e '/name=vboxusbmon/d' /etc/devlink.tab > /etc/devlink.vbox
    mv -f /etc/devlink.vbox /etc/devlink.tab

    # USBMonitor might not even be installed, but anyway...
    if test -f /platform/i86pc/kernel/drv/vboxusbmon.conf && test "$HOST_OS_VERSION" != "5.10"; then
        infoprint "Unloading USBMonitor..."
        unload_module $MOD_VBOXUSBMON "$fatal"
        rem_driver $MOD_VBOXUSBMON "$fatal"
    fi

    infoprint "Unloading NetFilter..."
    unload_module $MOD_VBOXFLT "$fatal"
    rem_driver $MOD_VBOXFLT "$fatal"

    infoprint "Unloading NetAdapter..."
    unload_module $MOD_VBOXNET "$fatal"
    rem_driver $MOD_VBOXNET "$fatal"

    infoprint "Unloading Host Driver..."
    unload_module $MOD_VBOXDRV "$fatal"
    rem_driver $MOD_VBOXDRV "$fatal"

    infoprint "Unloading VBI..."
    unload_module $MOD_VBI "$fatal"

    return 0
}

# install_python_bindings(pythonbin)
# remarks: changes pwd
# failure: non fatal
install_python_bindings()
{
    PYTHONBIN=$1
    if test -x "$PYTHONBIN"; then
        VBOX_INSTALL_PATH="$DIR_VBOXBASE"
        export VBOX_INSTALL_PATH
        cd $DIR_VBOXBASE/sdk/installer
        $PYTHONBIN ./vboxapisetup.py install > /dev/null
        return 0
    fi
    return 1
}


# cleanup_install([fatal])
# failure: depends on [fatal]
cleanup_install()
{
    fatal=$1

    # stop and unregister webservice SMF
    servicefound=`$BIN_SVCS -a | grep "virtualbox/webservice"`
    if test ! -z "$servicefound"; then
        $BIN_SVCADM disable -s svc:/application/virtualbox/webservice:default
        $BIN_SVCCFG delete svc:/application/virtualbox/webservice:default
    fi

    # stop and unregister zoneaccess SMF
    servicefound=`$BIN_SVCS -a | grep "virtualbox/zoneaccess"`
    if test ! -z "$servicefound"; then
        $BIN_SVCADM disable -s svc:/application/virtualbox/zoneaccess
        $BIN_SVCCFG delete svc:/application/virtualbox/zoneaccess
    fi
    
    # unplumb vboxnet0
    vboxnetup=`$BIN_IFCONFIG vboxnet0 >/dev/null 2>&1`
    if test "$?" -eq 0; then
        $BIN_IFCONFIG vboxnet0 unplumb
        if test "$?" -ne 0; then
            errorprint "VirtualBox NetAdapter 'vboxnet0' couldn't be unplumbed (probably in use)."
            if test "$fatal" = "$FATALOP"; then
                exit 33
            fi
        fi
    fi
}


# post_install()
# !! failure is always fatal
post_install()
{
    infoprint "Loading VirtualBox kernel modules..."
    install_drivers

    if test "$?" -eq 0; then
        if test -f /platform/i86pc/kernel/drv/vboxnet.conf; then        
            # nwam/dhcpagent fix
            nwamfile=/etc/nwam/llp
            nwambackupfile=$nwamfile.vbox
            if test -f "$nwamfile"; then
                sed -e '/vboxnet/d' $nwamfile > $nwambackupfile
                echo "vboxnet0	static 192.168.56.1" >> $nwambackupfile
                mv -f $nwambackupfile $nwamfile
            fi

            # plumb and configure vboxnet0
            $BIN_IFCONFIG vboxnet0 plumb up
            if test "$?" -eq 0; then
                $BIN_IFCONFIG vboxnet0 192.168.56.1 netmask 255.255.255.0 up
            else
                warnprint "Failed to bring up vboxnet0!!"
            fi
        fi

        # Install python bindings
        if test -f "$DIR_VBOXBASE/sdk/installer/vboxapisetup.py" || test -h "$DIR_VBOXBASE/sdk/installer/vboxapisetup.py"; then
            PYTHONBIN=`which python 2> /dev/null`
            if test -f "$PYTHONBIN" || test -h "$PYTHONBIN"; then
                infoprint "Installing Python bindings..."

                INSTALLEDIT=1
                PYTHONBIN=`which python2.4 2>/dev/null`
                install_python_bindings "$PYTHONBIN"
                if test "$?" -eq 0; then
                    INSTALLEDIT=0
                fi
                PYTHONBIN=`which python2.5 2>/dev/null`
                install_python_bindings "$PYTHONBIN"
                if test "$?" -eq 0; then
                    INSTALLEDIT=0
                fi
                PYTHONBIN=`which python2.6 2>/dev/null`
                install_python_bindings "$PYTHONBIN"
                if test "$?" -eq 0; then 
                    INSTALLEDIT=0
                fi

                # remove files installed by Python build
                rm -rf $DIR_VBOXBASE/sdk/installer/build

                if test "$INSTALLEDIT" -ne 0; then
                    warnprint "No suitable Python version found. Required Python 2.4, 2.5 or 2.6."
                fi
            else
                warnprint "Python not found, skipped installed Python bindings."
            fi
        fi

        # Update boot archive
        infoprint "Updating the boot archive..."
        $BIN_BOOTADM update-archive > /dev/null

        return 0
    else
        errorprint "Failed to update boot-archive"
        exit 666
    fi
    return 1
}


# pre_remove([fatal])
# failure: depends on [fatal]
pre_remove()
{
    fatal=$1

    cleanup_install

    remove_drivers "$fatal"
    if test "$?" -eq 0; then
        return 0;
    fi
    return 1
}



# And it begins...
check_root
check_isa
check_zone
find_bins

drvop=$1
fatal=$2
case "$drvop" in
post_install)
    check_module_arch
    post_install
    ;;
pre_remove)
    pre_remove "$fatal"
    ;;
install_drivers)
    check_module_arch
    install_drivers
    ;;
remove_drivers)
    remove_drivers "$fatal"
    ;;
*)
    echo "Usage: $0 post_install|pre_remove|install_drivers|remove_drivers [fatal]"
    exit 13
esac

if test "$?" -eq 0; then
    exit 0
fi

exit 1

