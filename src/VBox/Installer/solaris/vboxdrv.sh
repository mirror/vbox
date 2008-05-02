#!/bin/sh
# Sun xVM VirtualBox
# VirtualBox kernel module control script for Solaris.
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

SILENTUNLOAD=""
MODNAME="vboxdrv"
MODDIR32="/platform/i86pc/kernel/drv"
MODDIR64=$MODDIR32/amd64

abort()
{
    echo 1>&2 "$1"
    exit 1
}

info()
{
    echo 1>&2 "$1"
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
    abort "VirtualBox kernel module NOT installed."
}

module_loaded()
{
    if test -f "/etc/name_to_major"; then
        loadentry=`cat /etc/name_to_major | grep $MODNAME`
    else
        loadentry=`/usr/sbin/modinfo | grep $MODNAME`
    fi
    if test -z "$loadentry"; then
        return 1
    fi
    return 0
}

check_root()
{
    if test `/usr/xpg4/bin/id -u` -ne 0; then
        abort "This program must be run with administrator privileges.  Aborting"
    fi
}

start_module()
{
    if module_loaded; then
        info "VirtualBox kernel module already loaded."
    else
        /usr/sbin/add_drv -m'* 0666 root sys' $MODNAME
        if test ! module_loaded; then
            abort "## Failed to load VirtualBox kernel module."
        elif test -c "/devices/pseudo/$MODNAME@0:$MODNAME"; then
            info "VirtualBox kernel module successfully loaded."
        else
            abort "Aborting due to attach failure."
        fi
    fi
}

stop_module()
{
    if module_loaded; then
        /usr/sbin/rem_drv $MODNAME || abort "## Failed to unload VirtualBox kernel module."
        info "VirtualBox kernel module unloaded."
    elif test -z "$SILENTUNLOAD"; then
        info "VirtualBox kernel module not loaded."
    fi
}

restart_module()
{
    stop_module
    sync
    start_module
    return 0
}

status_module()
{
    if module_loaded; then
        info "Running."
    else
        info "Stopped."
    fi
}

check_root
check_if_installed

if test "$2" = "silentunload"; then
    SILENTUNLOAD="$2"
fi

case "$1" in
start)
    start_module
    ;;
stop)
    stop_module
    ;;
restart)
    restart_module
    ;;
status)
    status_module
    ;;
*)
    echo "Usage: $0 {start|stop|restart|status}"
    exit 1
esac

exit

