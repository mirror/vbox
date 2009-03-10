#!/bin/sh
# Sun xVM VirtualBox
# VirtualBox kernel module control script, Solaris hosts.
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

CHECKARCH=""
SILENTUNLOAD=""
ALWAYSREMDRV=""

MODNAME="vboxdrv"
VBIMODNAME="vbi"
FLTMODNAME="vboxflt"
USBMODNAME="vboxusbmon"
MODDIR32="/platform/i86pc/kernel/drv"
MODDIR64=$MODDIR32/amd64

abort()
{
    echo 1>&2 "## $1"
    exit 1
}

info()
{
    echo 1>&2 "$1"
}

warn()
{
    echo 1>&2 "WARNING!! $1"
}

check_if_installed()
{
    cputype=`isainfo -k`
    modulepath="$MODDIR32/$MODNAME"
    if test "$cputype" = "amd64"; then
        modulepath="$MODDIR64/$MODNAME"
    fi
    if test -f "$modulepath"; then
        return 0
    fi

    # Check arch only while installing (because rem_drv works for both arch.)
    if test ! -z "$CHECKARCH"; then
        # Let us go a step further and check if user has mixed up x86/amd64
        # amd64 ISA, x86 kernel module??
        if test "$cputype" = "amd64"; then
            modulepath="$MODDIR32/$MODNAME"
            if test -f "$modulepath"; then
                abort "Found 32-bit module instead of 64-bit. Please install the amd64 package!"
            fi
        else
            # x86 ISA, amd64 kernel module??
            modulepath="$MODDIR64/$MODNAME"
            if test -f "$modulepath"; then
                abort "Found 64-bit module instead of 32-bit. Please install the x86 package!"
            fi
        fi

        abort "VirtualBox Host kernel module NOT installed."
    else
        info "## VirtualBox Host kernel module NOT installed."
        return 0
    fi
}

module_added()
{
    loadentry=`cat /etc/name_to_major | grep $1`
    if test -z "$loadentry"; then
        return 1
    fi
    return 0
}

module_loaded()
{
    # modinfo should now work properly since we prevent module autounloading
    loadentry=`/usr/sbin/modinfo | grep $1`
    if test -z "$loadentry"; then
        return 1
    fi
    return 0    
}

vboxdrv_loaded()
{
    module_loaded $MODNAME
    return $?
}

vboxdrv_added()
{
    module_added $MODNAME
    return $?
}

vboxflt_loaded()
{
    module_loaded $FLTMODNAME
    return $?
}

vboxflt_added()
{
    module_added $FLTMODNAME
    return $?
}

vboxusbmon_added()
{
    module_added $USBMODNAME
    return $?
}

vboxusbmon_loaded()
{
    module_loaded $USBMODNAME
    return $?
}

check_root()
{
    idbin=/usr/xpg4/bin/id
    if test ! -f "$idbin"; then
        found=`which id | grep "no id"`
        if test ! -z "$found"; then
            abort "Failed to find a suitable user id binary! Aborting"
        else
            idbin=$found
        fi
    fi

    if test `$idbin -u` -ne 0; then
        abort "This program must be run with administrator privileges.  Aborting"
    fi
}

start_module()
{
    if vboxdrv_loaded; then
        info "VirtualBox Host kernel module already loaded."
    else
        if test -n "_HARDENED_"; then
            /usr/sbin/add_drv -m'* 0600 root sys' $MODNAME || abort "Failed to add VirtualBox Host Kernel module."
        else
            /usr/sbin/add_drv -m'* 0666 root sys' $MODNAME || abort "Failed to add VirtualBox Host Kernel module."
        fi
        /usr/sbin/modload -p drv/$MODNAME
        if test ! vboxdrv_loaded; then
            abort "Failed to load VirtualBox Host kernel module."
        elif test -c "/devices/pseudo/$MODNAME@0:$MODNAME"; then
            info "VirtualBox Host kernel module loaded."
        else
            abort "Aborting due to attach failure."
        fi
    fi
}

stop_module()
{
    if vboxdrv_loaded; then
        vboxdrv_mod_id=`/usr/sbin/modinfo | grep $MODNAME | cut -f 1 -d ' ' `
        if test -n "$vboxdrv_mod_id"; then
            /usr/sbin/modunload -i $vboxdrv_mod_id

            # While uninstalling we always remove the driver whether we unloaded successfully or not,
            # while installing we make SURE if there is an existing driver about, it is cleanly unloaded
            # and the new one is added hence the "alwaysremdrv" option.
            if test -n "$ALWAYSREMDRV"; then
                /usr/sbin/rem_drv $MODNAME
            else
                if test "$?" -eq 0; then
                    /usr/sbin/rem_drv $MODNAME || abort "Unloaded VirtualBox Host kernel module, but failed to remove it!"
                else
                    abort "Failed to unload VirtualBox Host kernel module. Old one still active!!"
                fi
            fi

            info "VirtualBox Host kernel module unloaded."
        fi
    elif vboxdrv_added; then
        /usr/sbin/rem_drv $MODNAME || abort "Unloaded VirtualBox Host kernel module, but failed to remove it!"
        info "VirtualBox Host kernel module unloaded."
    elif test -z "$SILENTUNLOAD"; then
        info "VirtualBox Host kernel module not loaded."
    fi

    # check for vbi and force unload it
    vbi_mod_id=`/usr/sbin/modinfo | grep $VBIMODNAME | cut -f 1 -d ' ' `
    if test -n "$vbi_mod_id"; then
        /usr/sbin/modunload -i $vbi_mod_id
    fi
}

start_vboxflt()
{
    if vboxflt_loaded; then
        info "VirtualBox NetFilter kernel module already loaded."
    else
        /usr/sbin/add_drv -m'* 0600 root sys' $FLTMODNAME || abort "Failed to add VirtualBox NetFilter Kernel module."
        /usr/sbin/modload -p drv/$FLTMODNAME
        if test ! vboxflt_loaded; then
            abort "Failed to load VirtualBox NetFilter kernel module."
        else
            info "VirtualBox NetFilter kernel module loaded."
        fi
    fi
}

stop_vboxflt()
{
    if vboxflt_loaded; then
        vboxflt_mod_id=`/usr/sbin/modinfo | grep $FLTMODNAME | cut -f 1 -d ' '`
        if test -n "$vboxflt_mod_id"; then
            /usr/sbin/modunload -i $vboxflt_mod_id

            # see stop_vboxdrv() for why we have "alwaysremdrv".
            if test -n "$ALWAYSREMDRV"; then
                /usr/sbin/rem_drv $FLTMODNAME
            else
                if test "$?" -eq 0; then
                    /usr/sbin/rem_drv $FLTMODNAME || abort "Unloaded VirtualBox NetFilter kernel module, but failed to remove it!"
                else
                    abort "Failed to unload VirtualBox NetFilter kernel module. Old one still active!!"
                fi
            fi

            info "VirtualBox NetFilter kernel module unloaded."
        fi
    elif vboxflt_added; then
        /usr/sbin/rem_drv $FLTMODNAME || abort "Unloaded VirtualBox NetFilter kernel module, but failed to remove it!"
        info "VirtualBox NetFilter kernel module unloaded."
    elif test -z "$SILENTUNLOAD"; then
        info "VirtualBox NetFilter kernel module not loaded."
    fi
}


start_vboxusbmon()
{
    if vboxusbmon_loaded; then
        info "VirtualBox USB Monitor kernel module already loaded."
    else
        /usr/sbin/add_drv -m'* 0666 root sys' $USBMODNAME || abort "Failed to add VirtualBox USB Monitor Kernel module."
        /usr/sbin/modload -p drv/$USBMODNAME
        if test ! vboxusbmon_loaded; then
            abort "Failed to load VirtualBox USB kernel module."
        else
            info "VirtualBox USB kernel module loaded."
        fi
    fi
}

stop_vboxusbmon()
{
    if vboxusbmon_loaded; then
        vboxusbmon_mod_id=`/usr/sbin/modinfo | grep $USBMODNAME | cut -f 1 -d ' '`
        if test -n "$vboxusbmon_mod_id"; then
            /usr/sbin/modunload -i $vboxusbmon_mod_id

            # see stop_vboxdrv() for why we have "alwaysremdrv".
            if test -n "$ALWAYSREMDRV"; then
                /usr/sbin/rem_drv $USBMODNAME
            else
                if test "$?" -eq 0; then
                    /usr/sbin/rem_drv $USBMODNAME || abort "Unloaded VirtualBox USB Monitor kernel module, but failed to remove it!"
                else
                    abort "Failed to unload VirtualBox USB Monitor kernel module. Old one still active!!"
                fi
            fi

            info "VirtualBox USB kernel module unloaded."
        fi
    elif vboxusbmon_added; then
        /usr/sbin/rem_drv $USBMODNAME || abort "Unloaded VirtualBox USB Monitor kernel module, but failed to remove it!"
        info "VirtualBox USB kernel module unloaded."
    elif test -z "$SILENTUNLOAD"; then
        info "VirtualBox USB kernel module not loaded."
    fi
}

status_vboxdrv()
{
    if vboxdrv_loaded; then
        info "Running."
    elif vboxdrv_added; then
        info "Inactive."
    else
        info "Not installed."
    fi
}

stop_all_modules()
{
    stop_vboxusbmon
    stop_vboxflt
    stop_module
}

start_all_modules()
{
    start_module
    start_vboxflt
    start_vboxusbmon
}

check_root
check_if_installed

if test "$2" = "silentunload" || test "$3" = "silentunload"; then
    SILENTUNLOAD="$2"
fi

if test "$2" = "alwaysremdrv" || test "$3" = "alwaysremdrv"; then
    ALWAYSREMDRV="alwaysremdrv"
fi

if test "$2" = "checkarch" || test "$3" = "checkarch"; then
    CHECKARCH="checkarch"
fi

case "$1" in
stopall)
    stop_all_modules
    ;;
startall)
    start_all_modules
    ;;
start)
    start_module
    ;;
stop)
    stop_module
    ;;
status)
    status_vboxdrv
    ;;
fltstart)
    start_vboxflt
    ;;
fltstop)
    stop_vboxflt
    ;;
usbstart)
    start_vboxusbmon
    ;;
usbstop)
    stop_vboxusbmon
    ;;
*)
    echo "Usage: $0 {start|stop|status|fltstart|fltstop|usbstart|usbstop|stopall|startall}"
    exit 1
esac

exit 0

