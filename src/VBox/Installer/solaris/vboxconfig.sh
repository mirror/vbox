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


# Never use exit 2 or exit 20 etc., the return codes are used in
# SRv4 postinstall procedures which carry special meaning. Just use exit 1 for failure.

# S10 or OpenSoalris
HOST_OS_MAJORVERSION=`uname -r`
# Which OpenSolaris version (snv_xxx)?
HOST_OS_MINORVERSION=`uname -v | cut -f2 -d'_'`

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
DESC_VBOXDRV="Host"

MOD_VBOXNET=vboxnet
DESC_VBOXNET="NetAdapter"

MOD_VBOXFLT=vboxflt
DESC_VBOXFLT="NetFilter"

# No Separate VBI since (3.1)
#MOD_VBI=vbi
#DESC_VBI="Kernel Interface"

MOD_VBOXUSBMON=vboxusbmon
DESC_VBOXUSBMON="USBMonitor"

MOD_VBOXUSB=vboxusb
DESC_VBOXUSB="USB"

FATALOP=fatal
NULLOP=nulloutput
SILENTOP=silent
IPSOP=ips
ISSILENT=
ISIPS=

infoprint()
{
    if test "$ISSILENT" != "$SILENTOP"; then
        echo 1>&2 "$1"
    fi
}

subprint()
{
    if test "$ISSILENT" != "$SILENTOP"; then
        echo 1>&2 "   - $1"
    fi
}

warnprint()
{
    if test "$ISSILENT" != "$SILENTOP"; then
        echo 1>&2 "* Warning!! $1"
    fi
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
        exit 1
    fi

    if test !  -x "$1"; then
        errorprint "$1 missing or is not an executable"
        exit 1
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
        found=`which id 2> /dev/null`
        if test ! -x "$found"; then
            errorprint "Failed to find a suitable user id executable."
            exit 1
        else
            idbin=$found
        fi
    fi

    if test `$idbin -u` -ne 0; then
        errorprint "This script must be run with administrator privileges."
        exit 1
    fi
}

# check_zone()
# !! failure is always fatal
check_zone()
{
    currentzone=`zonename`
    if test "$currentzone" != "global"; then
        errorprint "This script must be run from the global zone."
        exit 1
    fi
}

# check_isa()
# !! failure is always fatal
check_isa()
{
    currentisa=`uname -i`
    if test "$currentisa" = "i86xpv"; then
        errorprint "VirtualBox cannot run under xVM Dom0! Fatal Error, Aborting installation!"
        exit 1
    fi
}

# check_module_arch()
# !! failure is always fatal
check_module_arch()
{
    cputype=`isainfo -k`
    if test "$cputype" != "amd64" && test "$cputype" != "i386"; then
        errorprint "VirtualBox works only on i386/amd64 hosts, not $cputype"
        exit 1
    fi
}

# module_added(modname)
# returns 1 if added, 0 otherwise
module_added()
{
    if test -z "$1"; then
        errorprint "missing argument to module_added()"
        exit 1
    fi

    loadentry=`cat /etc/name_to_major | grep $1`
    if test -z "$loadentry"; then
        return 1
    fi
    return 0
}

# module_loaded(modname)
# returns 1 if loaded, 0 otherwise
module_loaded()
{
    if test -z "$1"; then
        errorprint "missing argument to module_loaded()"
        exit 1
    fi

    modname=$1
    # modinfo should now work properly since we prevent module autounloading
    loadentry=`$BIN_MODINFO | grep $modname`
    if test -z "$loadentry"; then
        return 1
    fi
    return 0
}

# add_driver(modname, moddesc, fatal, nulloutput, [driverperm])
# failure: depends on "fatal"
add_driver()
{
    if test -z "$1" || test -z "$2"; then
        errorprint "missing argument to add_driver()"
        exit 1
    fi

    modname="$1"
    moddesc="$2"
    fatal="$3"
    nullop="$4"
    modperm="$5"

    if test -n "$modperm"; then
        if test "$nullop" = "$NULLOP"; then
            $BIN_ADDDRV -m"$modperm" $modname  >/dev/null 2>&1
        else
            $BIN_ADDDRV -m"$modperm" $modname
        fi    
    else
        if test "$nullop" = "$NULLOP"; then        
            $BIN_ADDDRV $modname >/dev/null 2>&1
        else
            $BIN_ADDDRV $modname
        fi        
    fi

    if test $? -ne 0; then
        subprint "Adding: $moddesc module ...FAILED!"
        if test "$fatal" = "$FATALOP"; then
            exit 1
        fi
        return 1
    fi
    return 0
}

# rem_driver(modname, moddesc, [fatal])
# failure: depends on [fatal]
rem_driver()
{
    if test -z "$1" || test -z "$2"; then
        errorprint "missing argument to rem_driver()"
        exit 1
    fi

    modname=$1
    moddesc=$2
    fatal=$3
    module_added $modname
    if test "$?" -eq 0; then
        if test "$ISIPS" != "$IPSOP"; then
            $BIN_REMDRV $modname
        else
            $BIN_REMDRV $modname >/dev/null 2>&1
        fi
        if test $? -eq 0; then
            subprint "Removed: $moddesc module"
            return 0
        else
            subprint "Removing: $moddesc  ...FAILED!"
            if test "$fatal" = "$FATALOP"; then
                exit 1
            fi
            return 1
        fi
    fi
}

# unload_module(modname, moddesc, [fatal])
# failure: fatal
unload_module()
{
    if test -z "$1" || test -z "$2"; then
        errorprint "missing argument to unload_module()"
        exit 1
    fi

    modname=$1
    moddesc=$2
    fatal=$3
    modid=`$BIN_MODINFO | grep $modname | cut -f 1 -d ' ' `
    if test -n "$modid"; then
        $BIN_MODUNLOAD -i $modid
        if test $? -eq 0; then
            subprint "Unloaded: $moddesc module"
        else
            subprint "Unloading: $moddesc  ...FAILED!"
            if test "$fatal" = "$FATALOP"; then
                exit 1
            fi
            return 1
        fi
    fi
    return 0
}

# load_module(modname, moddesc, [fatal])
# pass "drv/modname" or "misc/vbi" etc.
# failure: fatal
load_module()
{
    if test -z "$1" || test -z "$2"; then
        errorprint "missing argument to load_module()"
        exit 1
    fi

    modname=$1
    moddesc=$2
    fatal=$3
    $BIN_MODLOAD -p $modname
    if test $? -eq 0; then
        subprint "Loaded: $moddesc module"
        return 0
    else
        subprint "Loading: $moddesc  ...FAILED!"
        if test "$fatal" = "$FATALOP"; then
            exit 1
        fi
        return 1
    fi
}

# install_drivers()
# !! failure is always fatal
install_drivers()
{
    if test -n "_HARDENED_"; then
        add_driver "$MOD_VBOXDRV" "$DESC_VBOXDRV" "$FATALOP" "not-$NULLOP" "'* 0600 root sys'"
    else
        add_driver "$MOD_VBOXDRV" "$DESC_VBOXDRV" "$FATALOP" "not-$NULLOP" "'* 0666 root sys'"
    fi
    load_module "drv/$MOD_VBOXDRV" "$DESC_VBOXDRV" "$FATALOP"

    # Add vboxdrv to devlink.tab
    sed -e '/name=vboxdrv/d' /etc/devlink.tab > /etc/devlink.vbox
    echo "type=ddi_pseudo;name=vboxdrv	\D" >> /etc/devlink.vbox
    mv -f /etc/devlink.vbox /etc/devlink.tab

    # Create the device link
    /usr/sbin/devfsadm -i "$MOD_VBOXDRV"

    if test $? -eq 0 && test -h "/dev/vboxdrv"; then

        if test -f /platform/i86pc/kernel/drv/vboxnet.conf; then
            add_driver "$MOD_VBOXNET" "$DESC_VBOXNET" "$FATALOP"
            load_module "drv/$MOD_VBOXNET" "$DESC_VBOXNET" "$FATALOP"
        fi

        if test -f /platform/i86pc/kernel/drv/vboxflt.conf; then
            add_driver "$MOD_VBOXFLT" "$DESC_VBOXFLT" "$FATALOP"
            load_module "drv/$MOD_VBOXFLT" "$DESC_VBOXFLT" "$FATALOP"
        fi

        if test -f /platform/i86pc/kernel/drv/vboxusbmon.conf && test "$HOST_OS_MAJORVERSION" != "5.10"; then
            # For VirtualBox 3.1 the new USB code requires Nevada >= 124
            if test "$HOST_OS_MINORVERSION" -gt 123; then
                add_driver "$MOD_VBOXUSBMON" "$DESC_VBOXUSBMON" "$FATALOP"
                load_module "drv/$MOD_VBOXUSBMON" "$DESC_VBOXUSBMON" "$FATALOP"

                # Add vboxusbmon to devlink.tab
                sed -e '/name=vboxusbmon/d' /etc/devlink.tab > /etc/devlink.vbox
                echo "type=ddi_pseudo;name=vboxusbmon	\D" >> /etc/devlink.vbox
                mv -f /etc/devlink.vbox /etc/devlink.tab

                # Create the device link
                /usr/sbin/devfsadm -i  "$MOD_VBOXUSBMON"
                if test $? -ne 0; then
                    errorprint "Failed to create device link for $MOD_VBOXUSBMON."
                    exit 1
                fi
                
                # Add vboxusb if present
                # This driver is special, we need it in the boot-archive but since there is no
                # USB device to attach to now (it's done at runtime) it will fail to attach so
                # redirect attaching failure output to /dev/null
                if test -f /platform/i86pc/kernel/drv/vboxusb.conf; then
                    add_driver "$MOD_VBOXUSB" "$DESC_VBOXUSB" "$FATALOP" "$NULLOP"
                fi
            else
                warnprint "Solaris Nevada 124 or higher required for USB support. Skipped installing USB support."
            fi
        fi
    else
        errorprint "Failed to create device link for $MOD_VBOXDRV."
        exit 1
    fi

    return $?
}

# remove_all([fatal])
# failure: depends on [fatal]
remove_drivers()
{
    fatal=$1

    # Remove vboxdrv from devlink.tab
    devlinkfound=`cat /etc/devlink.tab | grep vboxdrv`
    if test -n "$devlinkfound"; then
        sed -e '/name=vboxdrv/d' /etc/devlink.tab > /etc/devlink.vbox
        mv -f /etc/devlink.vbox /etc/devlink.tab
    fi

    # Remove vboxusbmon from devlink.tab
    devlinkfound=`cat /etc/devlink.tab | grep vboxusbmon`
    if test -n "$devlinkfound"; then
        sed -e '/name=vboxusbmon/d' /etc/devlink.tab > /etc/devlink.vbox
        mv -f /etc/devlink.vbox /etc/devlink.tab
    fi

    unload_module "$MOD_VBOXUSB" "$DESC_VBOXUSB" "$fatal"
    rem_driver "$MOD_VBOXUSB" "$DESC_VBOXUSB" "$fatal"

    unload_module "$MOD_VBOXUSBMON" "$DESC_VBOXUSBMON" "$fatal"
    rem_driver "$MOD_VBOXUSBMON" "$DESC_VBOXUSBMON" "$fatal"

    unload_module "$MOD_VBOXFLT" "$DESC_VBOXFLT" "$fatal"
    rem_driver "$MOD_VBOXFLT" "$DESC_VBOXFLT" "$fatal"

    unload_module "$MOD_VBOXNET" "$DESC_VBOXNET" "$fatal"
    rem_driver "$MOD_VBOXNET" "$DESC_VBOXNET" "$fatal"

    unload_module "$MOD_VBOXDRV" "$DESC_VBOXDRV" "$fatal"
    rem_driver "$MOD_VBOXDRV" "$DESC_VBOXDRV" "$fatal"

# No separate VBI since 3.1
#    unload_module "$MOD_VBI" "$DESC_VBI" "$fatal"

    # remove devlinks
    if test -h "/dev/vboxdrv" || test -f "/dev/vboxdrv"; then
        rm -f /dev/vboxdrv
    fi
    if test -h "/dev/vboxusbmon" || test -f "/dev/vboxusbmon"; then
        rm -f /dev/vboxusbmon
    fi

    # unpatch nwam/dhcpagent fix
    nwamfile=/etc/nwam/llp
    nwambackupfile=$nwamfile.vbox
    if test -f "$nwamfile"; then
        sed -e '/vboxnet/d' $nwamfile > $nwambackupfile
        mv -f $nwambackupfile $nwamfile
    fi

    return 0
}

# install_python_bindings(pythonbin)
# remarks: changes pwd
# failure: non fatal
install_python_bindings()
{
    # The python binary might not be there, so just exit silently
    if test -z "$1"; then
        return 0
    fi

    if test -z "$2"; then
        errorprint "missing argument to install_python_bindings"
        exit 1
    fi

    pythonbin=$1
    pythondesc=$2
    if test -x "$pythonbin"; then
        VBOX_INSTALL_PATH="$DIR_VBOXBASE"
        export VBOX_INSTALL_PATH
        cd $DIR_VBOXBASE/sdk/installer
        $pythonbin ./vboxapisetup.py install > /dev/null
        if test "$?" -eq 0; then
            subprint "Installed: Bindings for $pythondesc"
        fi
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
    servicefound=`$BIN_SVCS -a | grep "virtualbox/webservice" 2>/dev/null`
    if test ! -z "$servicefound"; then
        $BIN_SVCADM disable -s svc:/application/virtualbox/webservice:default
        $BIN_SVCCFG delete svc:/application/virtualbox/webservice:default
        if test "$?" -eq 0; then
            subprint "Unloaded: Web service"
        else
            subprint "Unloading: Web service  ...ERROR(S)."
        fi
    fi

    # stop and unregister zoneaccess SMF
    servicefound=`$BIN_SVCS -a | grep "virtualbox/zoneaccess" 2>/dev/null`
    if test ! -z "$servicefound"; then
        $BIN_SVCADM disable -s svc:/application/virtualbox/zoneaccess
        $BIN_SVCCFG delete svc:/application/virtualbox/zoneaccess
        if test "$?" -eq 0; then
            subprint "Unloaded: Zone access service"
        else
            subprint "Unloading: Zone access service  ...ERROR(S)."
        fi
    fi

    # unplumb vboxnet0
    vboxnetup=`$BIN_IFCONFIG vboxnet0 >/dev/null 2>&1`
    if test "$?" -eq 0; then
        $BIN_IFCONFIG vboxnet0 unplumb
        if test "$?" -ne 0; then
            errorprint "VirtualBox NetAdapter 'vboxnet0' couldn't be unplumbed (probably in use)."
            if test "$fatal" = "$FATALOP"; then
                exit 1
            fi
        fi
    fi

    # unplumb vboxnet0 ipv6
    vboxnetup=`$BIN_IFCONFIG vboxnet0 inet6 >/dev/null 2>&1`
    if test "$?" -eq 0; then
        $BIN_IFCONFIG vboxnet0 inet6 unplumb
        if test "$?" -ne 0; then
            errorprint "VirtualBox NetAdapter 'vboxnet0' IPv6 couldn't be unplumbed (probably in use)."
            if test "$fatal" = "$FATALOP"; then
                exit 1
            fi
        fi
    fi
}


# postinstall()
# !! failure is always fatal
postinstall()
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
                # Should this be fatal?
                warnprint "Failed to bring up vboxnet0!!"
            fi
        fi

        if test -f /var/svc/manifest/application/virtualbox/virtualbox-webservice.xml || test -f /var/svc/manifest/application/virtualbox/virtualbox-zoneaccess.xml; then
            infoprint "Configuring services..."
        fi

        # Web service
        if test -f /var/svc/manifest/application/virtualbox/virtualbox-webservice.xml; then
            /usr/sbin/svccfg import /var/svc/manifest/application/virtualbox/virtualbox-webservice.xml
            /usr/sbin/svcadm disable -s svc:/application/virtualbox/webservice:default
            if test "$?" -eq 0; then
                subprint "Loaded: Web service"
            else
                subprint "Loading: Web service  ...ERROR(S)."
            fi
        fi

        # Zone access service
        if test -f /var/svc/manifest/application/virtualbox/virtualbox-zoneaccess.xml; then
            /usr/sbin/svccfg import /var/svc/manifest/application/virtualbox/virtualbox-zoneaccess.xml
            /usr/sbin/svcadm enable -s svc:/application/virtualbox/zoneaccess
            if test "$?" -eq 0; then
                subprint "Loaded: Zone access service"
            else
                subprint "Loading: Zone access service  ...ERROR(S)."
            fi
        fi

        # Install python bindings
        if test -f "$DIR_VBOXBASE/sdk/installer/vboxapisetup.py" || test -h "$DIR_VBOXBASE/sdk/installer/vboxapisetup.py"; then
            PYTHONBIN=`which python 2> /dev/null`
            if test -f "$PYTHONBIN" || test -h "$PYTHONBIN"; then
                infoprint "Installing Python bindings..."

                INSTALLEDIT=1
                PYTHONBIN=`which python2.4 2>/dev/null`
                install_python_bindings "$PYTHONBIN" "Python 2.4"
                if test "$?" -eq 0; then
                    INSTALLEDIT=0
                fi
                PYTHONBIN=`which python2.5 2>/dev/null`
                install_python_bindings "$PYTHONBIN"  "Python 2.5"
                if test "$?" -eq 0; then
                    INSTALLEDIT=0
                fi
                PYTHONBIN=`which python2.6 2>/dev/null`
                install_python_bindings "$PYTHONBIN" "Python 2.6"
                if test "$?" -eq 0; then
                    INSTALLEDIT=0
                fi

                # remove files installed by Python build
                rm -rf $DIR_VBOXBASE/sdk/installer/build

                if test "$INSTALLEDIT" -ne 0; then
                    warnprint "No suitable Python version found. Required Python 2.4, 2.5 or 2.6."
                    warnprint "Skipped installing the Python bindings."
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

# preremove([fatal])
# failure: depends on [fatal]
preremove()
{
    fatal=$1

    cleanup_install "$fatal"

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

# Get command line options
while test $# -gt 0;
do
    case "$1" in
        --postinstall | --preremove | --installdrivers | --removedrivers)
            drvop="$1"
            ;;
        --fatal)
            fatal="$FATALOP"
            ;;
        --silent)
            ISSILENT="$SILENTOP"
            ;;
        --ips)
            ISIPS="$IPSOP"
            ;;
        *)
            break
            ;;
    esac
    shift
done

case "$drvop" in
--postinstall)
    check_module_arch
    postinstall
    ;;
--preremove)
    preremove "$fatal"
    ;;
--installdrivers)
    check_module_arch
    install_drivers
    ;;
--removedrivers)
    remove_drivers "$fatal"
    ;;
*)
    errorprint "Invalid operation $drvop"
    exit 1
esac

exit "$?"

