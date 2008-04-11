#!/bin/sh
# innotek VirtualBox
# VirtualBox Guest Additions kernel module control script for Solaris.
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

VBOXGUESTFILE=""
SILENTUNLOAD=""

abort()
{
    echo 1>&2 "$1"
    exit 1
}

info()
{
    echo 1>&2 "$1"
}

get_module_path()
{
    cputype=`isainfo -k`
    moduledir="/platform/i86pc/kernel/drv";
    if test "$cputype" = "amd64"; then
        moduledir=$moduledir/amd64
    fi
    modulepath=$moduledir/vboxguest
    if test -f "$modulepath"; then
        VBOXGUESTFILE="$modulepath"
    else
        VBOXGUESTFILE=""
    fi
}

check_if_installed()
{
    if test "$VBOXGUESTFILE" -a -f "$VBOXGUESTFILE"; then
        return 0
    fi
    abort "VirtualBox kernel module (vboxguest) not installed."
}

module_loaded()
{
    loadentry=`cat /etc/name_to_major | grep vboxguest`
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
        info "vboxguest already loaded..."
    else
        /usr/sbin/add_drv -i'pci80ee,cafe' -m'* 0666 root sys' vboxguest
        if test ! module_loaded; then
            abort "Failed to load vboxguest."
        elif test -c "/devices/pci@0,0/pci80ee,cafe@4:vboxguest"; then
            info "Loaded vboxguest."
        else
            stop
            abort "Aborting due to attach failure."
        fi
    fi
}

stop_module()
{
    if module_loaded; then
        /usr/sbin/rem_drv vboxguest
        info "Unloaded vboxguest."
    elif test -z "$SILENTUNLOAD"; then
        info "vboxguest not loaded."
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
        info "vboxguest running."
    else
        info "vboxguest stopped."
    fi
}

check_root
get_module_path
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

