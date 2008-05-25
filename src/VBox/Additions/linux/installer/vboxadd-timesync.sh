#!/bin/sh
#
# Sun xVM VirtualBox
# Linux Additions timesync daemon init script
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

# chkconfig: 35 35 56
# description: VirtualBox Additions timesync
#
### BEGIN INIT INFO
# Provides:       vboxadd-timesync
# Required-Start: vboxadd
# Required-Stop:  vboxadd
# Default-Start:  2 3 4 5
# Default-Stop:   0 1 6
# Description:    VirtualBox Additions timesync
### END INIT INFO

PATH=$PATH:/bin:/sbin:/usr/sbin

[ -f /lib/lsb/init-functions ] || NOLSB=yes

if [ -f /etc/redhat-release ]; then
    system=redhat
elif [ -f /etc/SuSE-release ]; then
    system=suse
elif [ -f /etc/debian_version ]; then
    system=debian
elif [ -f /etc/gentoo-release ]; then
    system=gentoo
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

if [ "$system" = "redhat" ]; then
    PIDFILE="/var/lock/subsys/vboxadd-timesync"
elif [ "$system" = "suse" ]; then
    PIDFILE="/var/lock/subsys/vboxadd-timesync"
    daemon() {
        startproc ${1+"$@"}
    }
elif [ "$system" = "debian" ]; then
    PIDFILE="/var/run/vboxadd-timesync"
    daemon() {
        start-stop-daemon --start --exec $1 -- $2
    }
    killproc() {
        start-stop-daemon --stop --exec $@
    }
elif [ "$system" = "gentoo" ]; then
    PIDFILE="/var/run/vboxadd-timesync"
    daemon() {
        start-stop-daemon --start --exec $1 -- $2
    }
    killproc() {
        start-stop-daemon --stop --exec $@
    }
else
    if [ -d /var/run -a -w /var/run ]; then
        PIDFILE="/var/run/vboxadd-timesync"
    fi
fi

binary=/usr/sbin/vboxadd-timesync

test -x "$binary" || {
    echo "Cannot run $binary"
    exit 1
}

vboxaddrunning() {
    lsmod | grep -q vboxadd[^_-]
}

start() {
    if ! test -f $PIDFILE; then
        begin_msg "Starting VirtualBox host to guest time synchronisation";
        vboxaddrunning || {
            failure "VirtualBox Additions module not loaded!"
        }
        daemon $binary --daemonize
        RETVAL=$?
        test $RETVAL -eq 0 && touch $PIDFILE
        succ_msg
    fi
    return $RETVAL
}

stop() {
    if test -f $PIDFILE; then
        begin_msg "Stopping VirtualBox host to guest time synchronisation ";
        vboxaddrunning || {
            failure "VirtualBox Additions module not loaded!"
        }
        killproc $binary
        RETVAL=$?
        test $RETVAL -eq 0 && rm -f $PIDFILE
        succ_msg
    fi
    return $RETVAL
}

restart() {
    stop && start
}

dmnstatus() {
    status vboxadd-timesync
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

exit $RETVAL
