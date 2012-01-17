#!/bin/sh
# $Id$

#
# VirtualBox Configuration Script, Solaris host.
#
# Copyright (C) 2009-2010 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

# Never use exit 2 or exit 20 etc., the return codes are used in
# SRv4 postinstall procedures which carry special meaning. Just use exit 1 for failure.

LANG=C
export LANG

DIR_VBOXBASE="$PKG_INSTALL_ROOT/opt/VirtualBox"
DIR_CONF="$PKG_INSTALL_ROOT/platform/i86pc/kernel/drv"
DIR_MOD_32="$PKG_INSTALL_ROOT/platform/i86pc/kernel/drv"
DIR_MOD_64="$DIR_MOD_32/amd64"

# Default paths, these will be overridden by 'which' if they don't exist
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
BIN_SVCS=/usr/bin/svcs
BIN_ID=/usr/bin/id
BIN_PKILL=/usr/bin/pkill

# "vboxdrv" is also used in sed lines here (change those as well if it ever changes)
MOD_VBOXDRV=vboxdrv
DESC_VBOXDRV="Host"

MOD_VBOXNET=vboxnet
DESC_VBOXNET="NetAdapter"
MOD_VBOXNET_INST=32

MOD_VBOXFLT=vboxflt
DESC_VBOXFLT="NetFilter (STREAMS)"

MOD_VBOXBOW=vboxbow
DESC_VBOXBOW="NetFilter (Crossbow)"

# No Separate VBI since (3.1)
#MOD_VBI=vbi
#DESC_VBI="Kernel Interface"

MOD_VBOXUSBMON=vboxusbmon
DESC_VBOXUSBMON="USBMonitor"

MOD_VBOXUSB=vboxusb
DESC_VBOXUSB="USB"

UPDATEBOOTARCHIVE=0
REMOTEINST=0
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
        echo 1>&2 "   * Warning!! $1"
    fi
}

errorprint()
{
    echo 1>&2 "## $1"
}

helpprint()
{
    echo 1>&2 "$1"
}

printusage()
{
    helpprint "VirtualBox Configuration Script"
    helpprint "usage: $0 <operation> [options]"
    helpprint
    helpprint "<operation> must be one of the following:"
    helpprint "  --postinstall      Perform full post installation procedure"
    helpprint "  --preremove        Perform full pre remove procedure"
    helpprint "  --installdrivers   Only install the drivers"
    helpprint "  --removedrivers    Only remove the drivers"
    helpprint "  --setupdrivers     Setup drivers, reloads existing drivers"
    helpprint
    helpprint "[options] are one or more of the following:"
    helpprint "  --silent           Silent mode"
    helpprint "  --fatal            Don't continue on failure (required for postinstall)"
    helpprint "  --ips              This is an IPS package postinstall/preremove"
    helpprint "  --altkerndir       Use /usr/kernel/drv as the driver directory"
    helpprint
}

# find_bin_path()
# !! failure is always fatal
find_bin_path()
{
    if test -z "$1"; then
        errorprint "missing argument to find_bin_path()"
        exit 1
    fi

    binfilename=`basename $1`
    binfilepath=`which $binfilename 2> /dev/null`
    if test -x "$binfilepath"; then
        echo "$binfilepath"
        return 0
    else
        errorprint "$1 missing or is not an executable"
        exit 1
    fi
}

# find_bins()
# !! failure is always fatal
find_bins()
{
    # Search only for binaries that might be in different locations
    if test ! -x "$BIN_ID"; then
        BIN_ID=`find_bin_path "$BIN_ID"`
    fi

    if test ! -x "$BIN_ADDDRV"; then
        BIN_ADDDRV=`find_bin_path "$BIN_ADDDRV"`
    fi

    if test ! -x "$BIN_REMDRV"; then
        BIN_REMDRV=`find_bin_path "$BIN_REMDRV"`
    fi

    if test ! -x "$BIN_MODLOAD"; then
        BIN_MODLOAD=`check_bin_path "$BIN_MODLOAD"`
    fi

    if test ! -x "$BIN_MODUNLOAD"; then
        BIN_MODUNLOAD=`find_bin_path "$BIN_MODUNLOAD"`
    fi

    if test ! -x "$BIN_MODINFO"; then
        BIN_MODINFO=`find_bin_path "$BIN_MODINFO"`
    fi

    if test ! -x "$BIN_DEVFSADM"; then
        BIN_DEVFSADM=`find_bin_path "$BIN_DEVFSADM"`
    fi

    if test ! -x "$BIN_BOOTADM"; then
        BIN_BOOTADM=`find_bin_path "$BIN_BOOTADM"`
    fi

    if test ! -x "$BIN_SVCADM"; then
        BIN_SVCADM=`find_bin_path "$BIN_SVCADM"`
    fi

    if test ! -x "$BIN_SVCCFG"; then
        BIN_SVCCFG=`find_bin_path "$BIN_SVCCFG"`
    fi

    if test ! -x "$BIN_SVCS"; then
        BIN_SVCS=`find_bin_path "$BIN_SVCS"`
    fi

    if test ! -x "$BIN_IFCONFIG"; then
        BIN_IFCONFIG=`find_bin_path "$BIN_IFCONFIG"`
    fi

    if test ! -x "$BIN_PKILL"; then
        BIN_PKILL=`find_bin_path "$BIN_PKILL"`
    fi
}

# check_root()
# !! failure is always fatal
check_root()
{
    # Don't use "-u" option as some id binaries don't support it, instead
    # rely on "uid=101(username) gid=10(groupname) groups=10(staff)" output
    curuid=`$BIN_ID | cut -f 2 -d '=' | cut -f 1 -d '('`
    if test "$curuid" -ne 0; then
        errorprint "This script must be run with administrator privileges."
        exit 1
    fi
}

# get_sysinfo
# cannot fail
get_sysinfo()
{
    BIN_PKG=`which pkg 2> /dev/null`
    if test -x "$BIN_PKG"; then
        PKGFMRI=`$BIN_PKG $BASEDIR_PKGOPT contents -H -t set -a name=pkg.fmri -o pkg.fmri pkg:/system/kernel 2> /dev/null`
        if test -z "$PKGFMRI"; then
            # Perhaps this is old pkg without '-a' option and/or system/kernel is missing and it's part of 'entire'
            # Try fallback.
            PKGFMRI=`$BIN_PKG $BASEDIR_PKGOPT contents -H -t set -o pkg.fmri entire | head -1 2> /dev/null`
            if test -z "$PKGFMRI"; then
                # Perhaps entire is conflicting. Try using opensolaris/entire.
                # Last fallback try.
                PKGFMRI=`$BIN_PKG $BASEDIR_PKGOPT contents -H -t set -o pkg.fmri opensolaris.org/entire | head -1 2> /dev/null`
            fi
        fi
        if test ! -z "$PKGFMRI"; then
            # The format is "pkg://solaris/system/kernel@0.5.11,5.11-0.161:20110315T070332Z"
            #            or "pkg://solaris/system/kernel@0.5.11,5.11-0.175.0.0.0.1.0:20111012T032837Z"
            STR_KERN=`echo "$PKGFMRI" | sed 's/^.*\@//;s/\:.*//;s/.*,//'`
            if test ! -z "$STR_KERN"; then
                # The format is "5.11-0.161" or "5.11-0.175.0.0.0.1.0"
                HOST_OS_MAJORVERSION=`echo "$STR_KERN" | cut -f1 -d'-'`
                HOST_OS_MINORVERSION=`echo "$STR_KERN" | cut -f2 -d'-' | cut -f2 -d '.'`
            else
                errorprint "Failed to parse the Solaris kernel version."
                exit 1
            fi        
        else
            errorprint "Failed to detect the Solaris kernel version."
            exit 1
        fi
    else
        HOST_OS_MAJORVERSION=`uname -r`
        if test -z "$HOST_OS_MAJORVERSION" || test "$HOST_OS_MAJORVERSION" != "5.10";  then
            # S11 without 'pkg' ?? Something's wrong... bail.
            errorprint "Solaris $HOST_OS_MAJOR_VERSION detected without executable $BIN_PKG !? Confused."
            exit 1
        fi
        if test "$REMOTEINST" -eq 0; then
            # Use uname to verify it's S10.
            # Major version is S10, Minor version is no longer relevant (or used), use uname -v so it gets something
            # like "Generic_blah" for purely cosmetic purposes
            HOST_OS_MINORVERSION=`uname -v`
        else
            # Remote installs from S10 local.
            BIN_PKGCHK=`which pkgchk 2> /dev/null`
            if test ! -x "$BIN_PKGCHK"; then
                errorprint "Failed to find an executable pkgchk binary $BIN_PKGCHK."
                errorprint "Cannot determine Solaris version on remote target $PKG_INSTALL_ROOT"
                exit 1
            fi

            REMOTE_S10=`$BIN_PKGCHK -l -p /kernel/amd64/genunix $BASEDIR_PKGOPT 2> /dev/null | grep SUNWckr | tr -d ' \t'`
            if test ! -z "$REMOTE_S10" && test "$REMOTE_S10" = "SUNWckr"; then
                HOST_OS_MAJORVERSION="5.10"
                HOST_OS_MINORVERSION=""
            else
                errorprint "Remote target $PKG_INSTALL_ROOT is not Solaris 10."
                errorprint "Will not attempt to install to an unidentified remote target."
                exit 1
            fi
        fi
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

# update_boot_archive()
# cannot fail
update_boot_archive()
{
    infoprint "Updating the boot archive..."
    if test "$REMOTEINST" -eq 0; then
        $BIN_BOOTADM update-archive > /dev/null
    else
        $BIN_BOOTADM update-archive -R "$PKG_INSTALL_ROOT" > /dev/null
    fi
    UPDATEBOOTARCHIVE=0
}


# module_added(modname)
# returns 1 if added, 0 otherwise
module_added()
{
    if test -z "$1"; then
        errorprint "missing argument to module_added()"
        exit 1
    fi

    # Add a space at end of module name to make sure we have a perfect match to avoid
    # any substring matches: e.g "vboxusb" & "vboxusbmon"
    loadentry=`cat "$PKG_INSTALL_ROOT/etc/name_to_major" | grep "$1 "`
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
    # modinfo should now work properly since we prevent module autounloading.
    loadentry=`$BIN_MODINFO | grep "$modname "`
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
            $BIN_ADDDRV $BASEDIR_OPT -m"$modperm" $modname  >/dev/null 2>&1
        else
            $BIN_ADDDRV $BASEDIR_OPT -m"$modperm" $modname
        fi
    else
        if test "$nullop" = "$NULLOP"; then
            $BIN_ADDDRV $BASEDIR_OPT $modname >/dev/null 2>&1
        else
            $BIN_ADDDRV $BASEDIR_OPT $modname
        fi
    fi

    if test $? -ne 0; then
        subprint "Adding: $moddesc module ...FAILED!"
        if test "$fatal" = "$FATALOP"; then
            exit 1
        fi
        return 1
    elif test "$REMOTEINST" -eq 1 && test "$?" -eq 0; then
        subprint "Added: $moddesc driver"
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
        UPDATEBOOTARCHIVE=1
        if test "$ISIPS" != "$IPSOP"; then
            $BIN_REMDRV $BASEDIR_OPT $modname
        else
            $BIN_REMDRV $BASEDIR_OPT $modname >/dev/null 2>&1
        fi
        # for remote installs, don't bother with return values of rem_drv
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

    # No-OP for non-root installs
    if test "$REMOTEINST" -eq 1; then
        return 0
    fi

    modname=$1
    moddesc=$2
    fatal=$3
    modid=`$BIN_MODINFO | grep "$modname " | cut -f 1 -d ' ' `
    if test -n "$modid"; then
        $BIN_MODUNLOAD -i $modid
        if test $? -eq 0; then
            subprint "Unloaded: $moddesc module"
        else
            subprint "Unloading: $moddesc module ...FAILED!"
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

    # No-OP for non-root installs
    if test "$REMOTEINST" -eq 1; then
        return 0
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

load_vboxflt()
{
    if test -f "$DIR_CONF/vboxflt.conf"; then
        add_driver "$MOD_VBOXFLT" "$DESC_VBOXFLT" "$FATALOP"
        load_module "drv/$MOD_VBOXFLT" "$DESC_VBOXFLT" "$FATALOP"
    else
        # For custom pkgs that optionally ship this module, let's not fail but just warn
        warnprint "$DESC_VBOXFLT installation requested but not shipped in this package."
    fi
}

load_vboxbow()
{
    if test -f "$DIR_CONF/vboxbow.conf"; then
        add_driver "$MOD_VBOXBOW" "$DESC_VBOXBOW" "$FATALOP"
        load_module "drv/$MOD_VBOXBOW" "$DESC_VBOXBOW" "$FATALOP"
    else
        # For custom pkgs that optionally ship this module, let's not fail but just warn
        warnprint "$DESC_VBOXBOW installation requested but not shipped in this package."
    fi
}

# install_drivers()
# !! failure is always fatal
install_drivers()
{
    if test -f "$DIR_CONF/vboxdrv.conf"; then
        if test -n "_HARDENED_"; then
            add_driver "$MOD_VBOXDRV" "$DESC_VBOXDRV" "$FATALOP" "not-$NULLOP" "'* 0600 root sys'"
        else
            add_driver "$MOD_VBOXDRV" "$DESC_VBOXDRV" "$FATALOP" "not-$NULLOP" "'* 0666 root sys'"
        fi
        load_module "drv/$MOD_VBOXDRV" "$DESC_VBOXDRV" "$FATALOP"
    else
        errorprint "Extreme error! Missing $DIR_CONF/vboxdrv.conf, aborting."
        return 1
    fi

    # Add vboxdrv to devlink.tab
    if test -f "$PKG_INSTALL_ROOT/etc/devlink.tab"; then
        sed -e '/name=vboxdrv/d' "$PKG_INSTALL_ROOT/etc/devlink.tab" > "$PKG_INSTALL_ROOT/etc/devlink.vbox"
        echo "type=ddi_pseudo;name=vboxdrv	\D" >> "$PKG_INSTALL_ROOT/etc/devlink.vbox"
        mv -f "$PKG_INSTALL_ROOT/etc/devlink.vbox" "$PKG_INSTALL_ROOT/etc/devlink.tab"
    else
        errorprint "Missing $PKG_INSTALL_ROOT/etc/devlink.tab, aborting install"
        return 1
    fi

    # Create the device link for non-remote installs
    if test "$REMOTEINST" -eq 0; then
        /usr/sbin/devfsadm -i "$MOD_VBOXDRV"
        if test $? -ne 0 || test ! -h "/dev/vboxdrv"; then
            errorprint "Failed to create device link for $MOD_VBOXDRV."
            exit 1
        fi
    fi

    # Load VBoxNetAdp
    if test -f "$DIR_CONF/vboxnet.conf"; then
        add_driver "$MOD_VBOXNET" "$DESC_VBOXNET" "$FATALOP"
        load_module "drv/$MOD_VBOXNET" "$DESC_VBOXNET" "$FATALOP"
    fi

    # If both vboxinst_vboxbow and vboxinst_vboxflt exist, bail.
    if test -f "$PKG_INSTALL_ROOT/etc/vboxinst_vboxflt" && test -f "$PKG_INSTALL_ROOT/etc/vboxinst_vboxbow"; then
        errorprint "Force-install files '$PKG_INSTALL_ROOT/etc/vboxinst_vboxflt' and '$PKG_INSTALL_ROOT/etc/vboxinst_vboxbow' both exist."
        errorprint "Cannot load $DESC_VBOXFLT and $DESC_VBOXBOW drivers at the same time."
        return 1
    fi

    # If the force-install files exists, install blindly
    if test -f "$PKG_INSTALL_ROOT/etc/vboxinst_vboxflt"; then
        load_vboxflt
    elif test -f "$PKG_INSTALL_ROOT/etc/vboxinst_vboxbow"; then
        infoprint "here"
        load_vboxbow
    else
        # If host is S10 or S11 (< snv_159) or vboxbow isn't shipped, then load vboxflt
        if test "$HOST_OS_MAJORVERSION" = "5.10" || test "$HOST_OS_MINORVERSION" -lt 159 || test ! -f "$DIR_CONF/vboxbow.conf"; then
            load_vboxflt
        else
            # For S11 snv_159+ load vboxbow
            load_vboxbow
        fi
    fi

    # Load VBoxUSBMon, VBoxUSB
    if test -f "$DIR_CONF/vboxusbmon.conf" && test "$HOST_OS_MAJORVERSION" != "5.10"; then
        # For VirtualBox 3.1 the new USB code requires Nevada > 123
        if test "$HOST_OS_MINORVERSION" -gt 123; then
            # Add a group "vboxuser" (8-character limit) for USB access.
            # All users which need host USB-passthrough support will have to be added to this group.
            groupadd vboxuser >/dev/null 2>&1

            add_driver "$MOD_VBOXUSBMON" "$DESC_VBOXUSBMON" "$FATALOP" "not-$NULLOP" "'* 0666 root sys'"
            load_module "drv/$MOD_VBOXUSBMON" "$DESC_VBOXUSBMON" "$FATALOP"

            chown root:vboxuser "/devices/pseudo/vboxusbmon@0:vboxusbmon"

            # Add vboxusbmon to devlink.tab
            sed -e '/name=vboxusbmon/d' "$PKG_INSTALL_ROOT/etc/devlink.tab" > "$PKG_INSTALL_ROOT/etc/devlink.vbox"
            echo "type=ddi_pseudo;name=vboxusbmon	\D" >> "$PKG_INSTALL_ROOT/etc/devlink.vbox"
            mv -f "$PKG_INSTALL_ROOT/etc/devlink.vbox" "$PKG_INSTALL_ROOT/etc/devlink.tab"

            # Create the device link for non-remote installs
            if test "$REMOTEINST" -eq 0; then
                /usr/sbin/devfsadm -i  "$MOD_VBOXUSBMON"
                if test $? -ne 0; then
                    errorprint "Failed to create device link for $MOD_VBOXUSBMON."
                    exit 1
                fi
            fi

            # Add vboxusb if present
            # This driver is special, we need it in the boot-archive but since there is no
            # USB device to attach to now (it's done at runtime) it will fail to attach so
            # redirect attaching failure output to /dev/null
            if test -f "$DIR_CONF/vboxusb.conf"; then
                add_driver "$MOD_VBOXUSB" "$DESC_VBOXUSB" "$FATALOP" "$NULLOP"
                load_module "drv/$MOD_VBOXUSB" "$DESC_VBOXUSB" "$FATALOP"
            fi
        else
            if test -n "$HOST_OS_MINORVERSION"; then
                warnprint "Solaris 5.11 build 124 or higher required for USB support. Skipped installing USB support."
            else
                warnprint "Failed to determine Solaris 5.11 snv version. Skipped installing USB support."
            fi
        fi
    fi

    return $?
}

# remove_drivers([fatal])
# failure: depends on [fatal]
remove_drivers()
{
    fatal=$1

    # Remove vboxdrv from devlink.tab
    if test -f "$PKG_INSTALL_ROOT/etc/devlink.tab"; then
        devlinkfound=`cat "$PKG_INSTALL_ROOT/etc/devlink.tab" | grep vboxdrv`
        if test -n "$devlinkfound"; then
            sed -e '/name=vboxdrv/d' "$PKG_INSTALL_ROOT/etc/devlink.tab" > "$PKG_INSTALL_ROOT/etc/devlink.vbox"
            mv -f "$PKG_INSTALL_ROOT/etc/devlink.vbox" "$PKG_INSTALL_ROOT/etc/devlink.tab"
        fi

        # Remove vboxusbmon from devlink.tab
        devlinkfound=`cat "$PKG_INSTALL_ROOT/etc/devlink.tab" | grep vboxusbmon`
        if test -n "$devlinkfound"; then
            sed -e '/name=vboxusbmon/d' "$PKG_INSTALL_ROOT/etc/devlink.tab" > "$PKG_INSTALL_ROOT/etc/devlink.vbox"
            mv -f "$PKG_INSTALL_ROOT/etc/devlink.vbox" "$PKG_INSTALL_ROOT/etc/devlink.tab"
        fi
    fi

    unload_module "$MOD_VBOXUSB" "$DESC_VBOXUSB" "$fatal"
    rem_driver "$MOD_VBOXUSB" "$DESC_VBOXUSB" "$fatal"

    unload_module "$MOD_VBOXUSBMON" "$DESC_VBOXUSBMON" "$fatal"
    rem_driver "$MOD_VBOXUSBMON" "$DESC_VBOXUSBMON" "$fatal"

    unload_module "$MOD_VBOXFLT" "$DESC_VBOXFLT" "$fatal"
    rem_driver "$MOD_VBOXFLT" "$DESC_VBOXFLT" "$fatal"

    unload_module "$MOD_VBOXBOW" "$DESC_VBOXBOW" "$fatal"
    rem_driver "$MOD_VBOXBOW" "$DESC_VBOXBOW" "$fatal"

    unload_module "$MOD_VBOXNET" "$DESC_VBOXNET" "$fatal"
    rem_driver "$MOD_VBOXNET" "$DESC_VBOXNET" "$fatal"

    unload_module "$MOD_VBOXDRV" "$DESC_VBOXDRV" "$fatal"
    rem_driver "$MOD_VBOXDRV" "$DESC_VBOXDRV" "$fatal"

# No separate VBI since 3.1
#    unload_module "$MOD_VBI" "$DESC_VBI" "$fatal"

    # remove devlinks
    if test -h "$PKG_INSTALL_ROOT/dev/vboxdrv" || test -f "$PKG_INSTALL_ROOT/dev/vboxdrv"; then
        rm -f "$PKG_INSTALL_ROOT/dev/vboxdrv"
    fi
    if test -h "$PKG_INSTALL_ROOT/dev/vboxusbmon" || test -f "$PKG_INSTALL_ROOT/dev/vboxusbmon"; then
        rm -f "$PKG_INSTALL_ROOT/dev/vboxusbmon"
    fi

    # unpatch nwam/dhcpagent fix
    nwamfile="$PKG_INSTALL_ROOT/etc/nwam/llp"
    nwambackupfile=$nwamfile.vbox
    if test -f "$nwamfile"; then
        sed -e '/vboxnet/d' $nwamfile > $nwambackupfile
        mv -f $nwambackupfile $nwamfile
    fi

    # remove netmask configuration
    if test -h "$PKG_INSTALL_ROOT/etc/netmasks"; then
        nmaskfile="$PKG_INSTALL_ROOT/etc/inet/netmasks"
    else
        nmaskfile="$PKG_INSTALL_ROOT/etc/netmasks"
    fi
    nmaskbackupfile=$nmaskfile.vbox
    if test -f "$nmaskfile"; then
        sed -e '/#VirtualBox_SectionStart/,/#VirtualBox_SectionEnd/d' $nmaskfile > $nmaskbackupfile
        mv -f $nmaskbackupfile $nmaskfile
    fi

    if test $UPDATEBOOTARCHIVE -eq 1; then
        update_boot_archive
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

# stop_process(processname)
# failure: depends on [fatal]
stop_process()
{
    if test -z "$1"; then
        errorprint "missing argument to stop_process()"
        exit 1
    fi

    procname=$1
    procpid=`ps -eo pid,fname | grep $procname | grep -v grep | awk '{ print $1 }'`
    if test ! -z "$procpid" && test "$procpid" -ge 0; then
        $BIN_PKILL "$procname"
        sleep 2
        procpid=`ps -eo pid,fname | grep $procname | grep -v grep | awk '{ print $1 }'`
        if test ! -z "$procpid" && test "$procpid" -ge 0; then
            subprint "Terminating: $procname  ...FAILED!"
            if test "$fatal" = "$FATALOP"; then
                exit 1
            fi
        else
            subprint "Terminated: $procname"
        fi
    fi
}


# stop_service(servicename, shortFMRI-suitable for grep, full FMRI)
# failure: non fatal
stop_service()
{
    if test -z "$1" || test -z "$2" || test -z "$3"; then
        errorprint "missing argument to stop_service()"
        exit 1
    fi
    servicefound=`$BIN_SVCS -a | grep "$2" 2>/dev/null`
    if test ! -z "$servicefound"; then
        $BIN_SVCADM disable -s "$3"
        # Don't delete the manifest, this is handled by the manifest class action
        # $BIN_SVCCFG delete "$3"
        if test "$?" -eq 0; then
            subprint "Unloaded: $1"
        else
            subprint "Unloading: $1  ...ERROR(S)."
        fi
    fi
}


# cleanup_install([fatal])
# failure: depends on [fatal]
cleanup_install()
{
    fatal=$1

    # No-Op for remote installs
    if test "$REMOTEINST" -eq 1; then
        return 0
    fi

    # stop the services
    stop_service "Web service" "virtualbox/webservice" "svc:/application/virtualbox/webservice:default"
    stop_service "Balloon control service" "virtualbox/balloonctrl" "svc:/application/virtualbox/balloonctrl:default"
    stop_service "Zone access service" "virtualbox/zoneaccess" "svc:/application/virtualbox/zoneaccess:default"

    # unplumb all vboxnet instances for non-remote installs
    inst=0
    while test $inst -ne $MOD_VBOXNET_INST; do
        vboxnetup=`$BIN_IFCONFIG vboxnet$inst >/dev/null 2>&1`
        if test "$?" -eq 0; then
            $BIN_IFCONFIG vboxnet$inst unplumb
            if test "$?" -ne 0; then
                errorprint "VirtualBox NetAdapter 'vboxnet$inst' couldn't be unplumbed (probably in use)."
                if test "$fatal" = "$FATALOP"; then
                    exit 1
                fi
            fi
        fi

        # unplumb vboxnet0 ipv6
        vboxnetup=`$BIN_IFCONFIG vboxnet$inst inet6 >/dev/null 2>&1`
        if test "$?" -eq 0; then
            $BIN_IFCONFIG vboxnet$inst inet6 unplumb
            if test "$?" -ne 0; then
                errorprint "VirtualBox NetAdapter 'vboxnet$inst' IPv6 couldn't be unplumbed (probably in use)."
                if test "$fatal" = "$FATALOP"; then
                    exit 1
                fi
            fi
        fi

        inst=`expr $inst + 1`
    done

    # Stop our other daemons, non-fatal
    stop_process VBoxSVC
    stop_process VBoxNetDHCP
}


# postinstall()
# !! failure is always fatal
postinstall()
{
    infoprint "Detected Solaris $HOST_OS_MAJORVERSION Version $HOST_OS_MINORVERSION"
    infoprint "Loading VirtualBox kernel modules..."
    install_drivers

    if test "$?" -eq 0; then
        if test -f "$DIR_CONF/vboxnet.conf"; then
            # nwam/dhcpagent fix
            nwamfile="$PKG_INSTALL_ROOT/etc/nwam/llp"
            nwambackupfile=$nwamfile.vbox
            if test -f "$nwamfile"; then
                sed -e '/vboxnet/d' $nwamfile > $nwambackupfile

                # add all vboxnet instances as static to nwam
                inst=0
                networkn=56
                while test $inst -ne 1; do
                    echo "vboxnet$inst	static 192.168.$networkn.1" >> $nwambackupfile
                    inst=`expr $inst + 1`
                    networkn=`expr $networkn + 1`
                done
                mv -f $nwambackupfile $nwamfile
            fi

            # plumb and configure vboxnet0 for non-remote installs
            if test "$REMOTEINST" -eq 0; then
                # S11 175a renames vboxnet0 as 'netX', undo this and rename it back
                if test "$HOST_OS_MAJORVERSION" = "5.11" && test "$HOST_OS_MINORVERSION" -gt 174; then
                    vanityname=`dladm show-phys -po link,device | grep vboxnet0 | cut -f1 -d':'`
                    if test $? -eq 0 && test ! -z "$vanityname" && test "$vanityname" != "vboxnet0"; then
                        dladm rename-link "$vanityname" vboxnet0
                        if test $? -ne 0; then
                            errorprint "Failed to rename vanity interface ($vanityname) to vboxnet0"
                        fi
                    fi
                fi

                $BIN_IFCONFIG vboxnet0 plumb
                $BIN_IFCONFIG vboxnet0 up
                if test "$?" -eq 0; then
                    $BIN_IFCONFIG vboxnet0 192.168.56.1 netmask 255.255.255.0 up

                    # /etc/netmasks is a symlink, older installers replaced this with
                    # a copy of the actual file, repair that behaviour here.
                    recreatelink=0
                    if test -h "$PKG_INSTALL_ROOT/etc/netmasks"; then
                        nmaskfile="$PKG_INSTALL_ROOT/etc/inet/netmasks"
                    else
                        nmaskfile="$PKG_INSTALL_ROOT/etc/netmasks"
                        recreatelink=1
                    fi

                    # add the netmask to stay persistent across host reboots
                    nmaskbackupfile=$nmaskfile.vbox
                    if test -f $nmaskfile; then
                        sed -e '/#VirtualBox_SectionStart/,/#VirtualBox_SectionEnd/d' $nmaskfile > $nmaskbackupfile

                        if test $recreatelink -eq 1; then
                            # Check after removing our settings if /etc/netmasks is identifcal to /etc/inet/netmasks 
                            anydiff=`diff $nmaskbackupfile "$PKG_INSTALL_ROOT/etc/inet/netmasks"`
                            if test ! -z "$anydiff"; then
                                # User may have some custom settings in /etc/netmasks, don't overwrite /etc/netmasks!
                                recreatelink=2
                            fi
                        fi                        

                        echo "#VirtualBox_SectionStart" >> $nmaskbackupfile
                        inst=0
                        networkn=56
                        while test $inst -ne 1; do
                            echo "192.168.$networkn.0 255.255.255.0" >> $nmaskbackupfile
                            inst=`expr $inst + 1`
                            networkn=`expr $networkn + 1`
                        done
                        echo "#VirtualBox_SectionEnd" >> $nmaskbackupfile
                        mv -f $nmaskbackupfile $nmaskfile

                        # Recreate /etc/netmasks as a link if necessary
                        if test $recreatelink -eq 1; then
                            cp -f "$PKG_INSTALL_ROOT/etc/netmasks" "$PKG_INSTALL_ROOT/etc/inet/netmasks"
                            ln -sf ./inet/netmasks "$PKG_INSTALL_ROOT/etc/netmasks"
                        elif test $recreatelink -eq 2; then
                            warnprint "/etc/netmasks is a symlink (to /etc/inet/netmasks) that older"
                            warnprint "VirtualBox installers incorrectly overwrote. Now the contents"
                            warnprint "of /etc/netmasks and /etc/inet/netmasks differ, therefore "
                            warnprint "VirtualBox will not attempt to overwrite /etc/netmasks as a"
                            warnprint "symlink to /etc/inet/netmasks. Please resolve this manually."
                        fi
                    fi
                else
                    # Should this be fatal?
                    warnprint "Failed to bring up vboxnet0!!"
                fi
            fi
        fi

        if test -f "$PKG_INSTALL_ROOT/var/svc/manifest/application/virtualbox/virtualbox-webservice.xml" || test -f "$PKG_INSTALL_ROOT/var/svc/manifest/application/virtualbox/virtualbox-zoneaccess.xml"; then
            infoprint "Configuring services..."
            if test "$REMOTEINST" -eq 1; then
                subprint "Skipped for targetted installs."
            else
                # Enable Zone access service for non-remote installs, other services (Webservice) are delivered disabled by the manifest class action
                servicefound=`$BIN_SVCS -a | grep "virtualbox/zoneaccess" | grep "disabled" 2>/dev/null`
                if test ! -z "$servicefound"; then
                    $BIN_SVCADM enable -s svc:/application/virtualbox/zoneaccess
                    if test "$?" -eq 0; then
                        subprint "Loaded: Zone access service"
                    else
                        subprint "Loading Zone access service  ...FAILED."
                    fi
                fi
            fi
        fi

        # Update mime and desktop databases to get the right menu entries
        # and icons. There is still some delay until the GUI picks it up,
        # but that cannot be helped.
        if test -d "$PKG_INSTALL_ROOT/usr/share/icons"; then
            infoprint "Installing MIME types and icons..."
            if test "$REMOTEINST" -eq 0; then
                /usr/bin/update-mime-database /usr/share/mime >/dev/null 2>&1
                /usr/bin/update-desktop-database -q 2>/dev/null
            else
                subprint "Skipped for targetted installs."
            fi
        fi

        # Install python bindings for non-remote installs
        if test "$REMOTEINST" -eq 0; then
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
        else
            warnprint "Skipped installing Python bindings. Run, as root, 'vboxapisetup.py install' manually from the booted system."
        fi

        update_boot_archive
    
        return 0
    else
        errorprint "Failed to install drivers"
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
if test "x${PKG_INSTALL_ROOT:=/}" != "x/"; then
    BASEDIR_OPT="-b $PKG_INSTALL_ROOT"
    BASEDIR_PKGOPT="-R $PKG_INSTALL_ROOT"
    REMOTEINST=1
fi
find_bins
check_root
check_isa
check_zone
get_sysinfo


# Get command line options
while test $# -gt 0;
do
    case "$1" in
        --postinstall | --preremove | --installdrivers | --removedrivers | --setupdrivers)
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
        --altkerndir)
            # Use alternate kernel driver config folder (dev only)
            DIR_CONF="/usr/kernel/drv"
            ;;
        --help)
            printusage
            exit 1
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
--setupdrivers)
    remove_drivers "$fatal"
    infoprint "Installing VirtualBox drivers:"
    install_drivers
    ;;
*)
    printusage
    exit 1
esac

exit "$?"

