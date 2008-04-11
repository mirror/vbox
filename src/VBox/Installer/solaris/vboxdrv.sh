#!/bin/sh
# innotek VirtualBox
# VirtualBox kernel module control script for Solaris.
#
# Copyright (C) 2007-2008 innotek GmbH
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

VBOXDRVFILE=""
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
    modulepath=$moduledir/vboxdrv
    if test -f "$modulepath"; then
        VBOXDRVFILE="$modulepath"
    else
        VBOXDRVFILE=""
    fi
}

check_if_installed()
{
    if test "$VBOXDRVFILE" -a -f "$VBOXDRVFILE"; then
        return 0
    fi
    abort "VirtualBox kernel module (vboxdrv) not installed."
}

module_loaded()
{
    if test -f "/etc/name_to_major"; then
        loadentry=`cat /etc/name_to_major | grep vboxdrv`
    else
        loadentry=`/usr/sbin/modinfo | grep vboxdrv`
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
        info "vboxdrv already loaded..."
    else
        /usr/sbin/add_drv -m'* 0666 root sys' vboxdrv
        if test ! module_loaded; then
            abort "Failed to load vboxdrv."
        elif test -c "/devices/pseudo/vboxdrv@0:vboxdrv"; then
            info "Loaded vboxdrv."
        else
            stop
            abort "Aborting due to attach failure."
        fi
    fi
}

stop_module()
{
    if module_loaded; then
        /usr/sbin/rem_drv vboxdrv
        info "Unloaded vboxdrv."
    elif test -z "$SILENTUNLOAD"; then
        info "vboxdrv not loaded."
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
        info "vboxdrv running."
    else
        info "vboxdrv stopped."
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

