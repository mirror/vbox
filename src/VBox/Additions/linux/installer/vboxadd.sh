#! /bin/sh
# Sun xVM VirtualBox
# Linux Additions kernel module init script
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
# description: VirtualBox Linux Additions kernel module
#
### BEGIN INIT INFO
# Provides:       vboxadd
# Required-Start:
# Required-Stop:
# Default-Start:  2 3 4 5
# Default-Stop:   0 1 6
# Description:    VirtualBox Linux Additions kernel module
### END INIT INFO

PATH=$PATH:/bin:/sbin:/usr/sbin

[ -f /lib/lsb/init-functions ] || NOLSB=yes

if [ -n "$NOLSB" ]; then
    if [ -f /etc/redhat-release ]; then
        system=redhat
    elif [ -f /etc/SuSE-release ]; then
        system=suse
    elif [ -f /etc/gentoo-release ]; then
        system=gentoo
    fi
fi

if [ -z "$NOLSB" ]; then
    . /lib/lsb/init-functions
    fail_msg() {
        echo ""
        log_failure_msg "$1"
    }
    succ_msg() {
        log_end_msg 0
    }
    begin_msg() {
        log_daemon_msg "$@"
    }
else
    if [ "$system" = "redhat" ]; then
        . /etc/init.d/functions
        fail_msg() {
            echo -n " "
            echo_failure
            echo
            echo "  ($1)"
        }
        succ_msg() {
            echo -n " "
            echo_success
            echo
        }
    elif [ "$system" = "suse" ]; then
        . /etc/rc.status
        fail_msg() {
            rc_failed 1
            rc_status -v
            echo "  ($1)"
        }
        succ_msg() {
            rc_reset
            rc_status -v
        }
    elif [ "$system" = "gentoo" ]; then
        . /sbin/functions.sh
        fail_msg() {
            eerror "$1"
        }
        succ_msg() {
            eend "$?"
        }
        begin_msg() {
            ebegin "$1"
        }
        if [ "`which $0`" = "/sbin/rc" ]; then
            shift
        fi
    else
        fail_msg() {
            echo " ...failed!"
            echo "  ($1)"
        }
        succ_msg() {
            echo " ...done."
        }
    fi
    if [ "$system" != "gentoo" ]; then
        begin_msg() {
            [ -z "${1:-}" ] && return 1
            if [ -z "${2:-}" ]; then
                echo -n "$1"
            else
                echo -n "$1: $2"
            fi
        }
    fi
fi

kdir=/lib/modules/`uname -r`/misc
dev=/dev/vboxadd
modname=vboxadd
module=$kdir/$modname
owner=vboxadd
group=1

failure()
{
    fail_msg "$1"
    exit 1
}

running() {
    lsmod | grep -q "$modname[^_-]"
}

start() {
    begin_msg "Starting VirtualBox Additions";
    running || {
        rm -f $dev || {
            failure "Cannot remove $dev"
        }

        modprobe $modname >/dev/null 2>&1 || {
            failure "modprobe $modname failed"
        }

        sleep .5
    }
    if [ ! -c $dev ]; then
        maj=`sed -n 's;\([0-9]\+\) vboxadd;\1;p' /proc/devices`
        if [ ! -z "$maj" ]; then
            min=0
        else
            min=`sed -n 's;\([0-9]\+\) vboxadd;\1;p' /proc/misc`
            if [ ! -z "$min" ]; then
                maj=10
            fi
        fi
        test -z "$maj" && {
            rmmod $modname 2>/dev/null
            failure "Cannot locate the VirtualBox device"
        }

        mknod -m 0664 $dev c $maj $min 2>/dev/null || {
            rmmod $modname 2>/dev/null
            failure "Cannot create device $dev with major $maj and minor $min"
        }
    fi

    chown $owner:$group $dev 2>/dev/null || {
        rmmod $modname 2>/dev/null
        failure "Cannot change owner $owner:$group for device $dev"
    }

    succ_msg
    return 0
}

stop() {
    begin_msg "Stopping VirtualBox Additions";
    if running; then
        rmmod $modname 2>/dev/null || failure "Cannot unload module $modname"
        rm -f $dev || failure "Cannot unlink $dev"
    fi
    succ_msg
    return 0
}

restart() {
    stop && start
    return 0
}

dmnstatus() {
    if running; then
        echo "The VirtualBox Additions are currently running."
    else
        echo "The VirtualBox Additions are not currently running."
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
