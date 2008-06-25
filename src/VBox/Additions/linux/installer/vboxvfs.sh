#! /bin/sh
# Sun xVM VirtualBox
# Linux Additions VFS kernel module init script
#
# Copyright (C) 2006-2007 Sun Microsystems, Inc.
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


# chkconfig: 35 30 60
# description: VirtualBox Linux Additions VFS kernel module
#
### BEGIN INIT INFO
# Provides:       vboxvfs
# Required-Start: vboxadd
# Required-Stop:
# Default-Start:  S
# Default-Stop:   0 6
# Description:    VirtualBox Linux Additions VFS kernel module
### END INIT INFO

PATH=$PATH:/bin:/sbin:/usr/sbin

if [ -f /etc/redhat-release ]; then
    system=redhat
elif [ -f /etc/SuSE-release ]; then
    system=suse
elif [ -f /etc/gentoo-release ]; then
    system=gentoo
else
    system=other
fi

if [ "$system" = "redhat" ]; then
    . /etc/init.d/functions
    fail_msg() {
        echo_failure
        echo
    }

    succ_msg() {
        echo_success
        echo
    }

    begin() {
        echo -n "$1"
    }
fi

if [ "$system" = "suse" ]; then
    . /etc/rc.status
    fail_msg() {
        rc_failed 1
        rc_status -v
    }

    succ_msg() {
        rc_reset
        rc_status -v
    }

    begin() {
        echo -n "$1"
    }
fi

if [ "$system" = "gentoo" ]; then
    . /sbin/functions.sh
    fail_msg() {
        eend 1
    }

    succ_msg() {
        eend $?
    }

    begin() {
        ebegin $1
    }

    if [ "`which $0`" = "/sbin/rc" ]; then
        shift
    fi
fi

if [ "$system" = "other" ]; then
    fail_msg() {
        echo " ...fail!"
    }

    succ_msg() {
        echo " ...done."
    }

    begin() {
        echo -n $1
    }
fi

kdir=/lib/modules/`uname -r`/misc
modname=vboxvfs
module="$kdir/$modname"

fail() {
    if [ "$system" = "gentoo" ]; then
        eerror $1
        exit 1
    fi
    fail_msg
    echo "($1)"
    exit 1
}

running() {
    lsmod | grep -q "$modname[^_-]"
}

start() {
    begin "Starting VirtualBox Additions shared folder support ";
    running || {
        modprobe $modname > /dev/null 2>&1 || {
            if dmesg | grep "vboxConnect failed" > /dev/null 2>&1; then
                fail_msg
                echo "You may be trying to run Guest Additions from binary release of VirtualBox"
                echo "in the Open Source Edition."
                exit 1
            fi
            fail "modprobe $modname failed"
        }
    }
    # Mount all shared folders from /etc/fstab. Normally this is done by some
    # other startup script but this requires the vboxdrv kernel module loaded.
    mount -a -t vboxsf
    succ_msg
    return 0
}

stop() {
    begin "Stopping VirtualBox Additions shared folder support ";
    if umount -a -t vboxsf 2>/dev/null; then
        if running; then
            rmmod $modname 2>/dev/null || fail "Cannot unload module $modname"
        fi
        succ_msg
    else
        fail "Cannot unmount vboxsf folders"
    fi
    return 0
}

restart() {
    stop && start
    return 0
}

dmnstatus() {
    if running; then
        echo "VirtualBox Additions shared folder support is currently running."
    else
        echo "VirtualBox Additions shared folder support is not currently running."
    fi
}

case "$1" in
start)
    start
    ;;
stop)
    stop
    ;;
restart)
    restart
    ;;
status)
    dmnstatus
    ;;
*)
    echo "Usage: $0 {start|stop|restart|status}"
    exit 1
esac

exit
