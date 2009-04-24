#!/bin/sh
# Sun VirtualBox
# VirtualBox Guest Additions kernel module control script for Solaris.
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

SILENTUNLOAD=""
MODNAME="vboxguest"
VFSMODNAME="vboxfs"
MODDIR32="/usr/kernel/drv"
MODDIR64=$MODDIR32/amd64
VFSDIR32="/usr/kernel/fs"
VFSDIR64="/usr/kernel/fs/amd64"

abort()
{
    echo 1>&2 "## $1"
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
    abort "VirtualBox kernel module ($MODNAME) NOT installed."
}

module_loaded()
{
    if test -f "/etc/name_to_major"; then
        loadentry=`cat /etc/name_to_major | grep $1`
    else
        loadentry=`/usr/sbin/modinfo | grep $1`
    fi
    if test -z "$loadentry"; then
        return 1
    fi
    return 0
}

vboxguest_loaded()
{
    module_loaded $MODNAME
    return $?
}

vboxfs_loaded()
{
    module_loaded $VFSMODNAME
    return $?
}

check_root()
{
    # the reason we don't use "-u" is that some versions of id are old and do not
    # support this option (eg. Solaris 10) and do not have a "--version" to check it either
    # so go with the uglier but more generic approach
    idbin=`which id`
    isroot=`$idbin | grep "uid=0"`
    if test -z "$isroot"; then
        abort "This program must be run with administrator privileges.  Aborting"
    fi
}

start_module()
{
    if vboxguest_loaded; then
        info "VirtualBox guest kernel module already loaded."
    else
        /usr/sbin/add_drv -i'pci80ee,cafe' -m'* 0666 root sys' $MODNAME
        if test ! vboxguest_loaded; then
            abort "Failed to load VirtualBox guest kernel module."
        elif test -c "/devices/pci@0,0/pci80ee,cafe@4:$MODNAME"; then
            info "VirtualBox guest kernel module loaded."
        else
            stop
            abort "Aborting due to attach failure."
        fi
    fi
}

stop_module()
{
    if vboxguest_loaded; then
        /usr/sbin/rem_drv $MODNAME || abort "## Failed to unload VirtualBox guest kernel module."
        info "VirtualBox guest kernel module unloaded."
    elif test -z "$SILENTUNLOAD"; then
        info "VirtualBox guest kernel module not loaded."
    fi
}

start_vboxfs()
{
    if vboxfs_loaded; then
        info "VirtualBox FileSystem kernel module already loaded."
    else
        /usr/sbin/modload -p fs/$VFSMODNAME || abort "Failed to load VirtualBox FileSystem kernel module."
        if test ! vboxfs_loaded; then
            abort "Failed to load VirtualBox FileSystem kernel module."
        else
            info "VirtualBox FileSystem kernel module loaded."
        fi
    fi
}

stop_vboxfs()
{
    if vboxfs_loaded; then
        vboxfs_mod_id=`/usr/sbin/modinfo | grep $VFSMODNAME | cut -f 1 -d ' ' `
        if test -n "$vboxfs_mod_id"; then
            /usr/sbin/modunload -i $vboxfs_mod_id || abort "Failed to unload VirtualBox FileSystem module."
            info "VirtualBox FileSystem kernel module unloaded."
        fi
    elif test -z "$SILENTUNLOAD"; then
        info "VirtualBox FileSystem kernel module not loaded."
    fi
}

restart_module()
{
    stop_module
    sync
    start_module
    return 0
}

restart_all()
{
    stop_module
    sync
    start_module
    return 0
}

status_module()
{
    if vboxguest_loaded; then
        info "Running."
    else
        info "Stopped."
    fi
}

stop_all()
{
    stop_vboxfs
    stop_module
    return 0
}

check_root
check_if_installed

if test "$2" = "silentunload"; then
    SILENTUNLOAD="$2"
fi

case "$1" in
stopall)
    stop_all
    ;;
restartall)
    restart_all
    ;;
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
vfsstart)
    start_vboxfs
    ;;
vfsstop)
    stop_vboxfs
    ;;
*)
    echo "Usage: $0 {start|stop|restart|status}"
    exit 1
esac

exit 0

