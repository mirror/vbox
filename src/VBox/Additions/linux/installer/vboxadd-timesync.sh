#!/bin/sh
#
#  innotek VirtualBox
#
#  Linux Additions timesync daemon init script
#
# Copyright (C) 2006-2007 innotek GmbH
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

# chkconfig: 35 35 56
# description: VirtualBox Additions timesync
#
### BEGIN INIT INFO
# Provides:       vboxadd-timesync
# Required-Start: vboxadd
# Required-Stop:  vboxadd
# Default-Start:  3 5
# Default-Stop:
# Description:    VirtualBox Additions timesync
### END INIT INFO

PATH=$PATH:/bin:/sbin:/usr/sbin

system=unknown
if [ -f /etc/redhat-release ]; then
    system=redhat
    PIDFILE="/var/lock/subsys/vboxadd-timesync"
elif [ -f /etc/SuSE-release ]; then
    system=suse
    PIDFILE="/var/lock/subsys/vboxadd-timesync"
elif [ -f /etc/debian_version ]; then
    system=debian
    PIDFILE="/var/run/vboxadd-timesync"
elif [ -f /etc/gentoo-release ]; then
    system=gentoo
    PIDFILE="/var/run/vboxadd-timesync"
else
    system=other
    if [ -d /var/run -a -w /var/run ]; then
        PIDFILE="/var/run/vboxadd-timesync"
    fi
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
fi

if [ "$system" = "suse" ]; then
    . /etc/rc.status
    daemon() {
        startproc ${1+"$@"}
    }

    fail_msg() {
        rc_failed 1
        rc_status -v
    }

    succ_msg() {
        rc_reset
        rc_status -v
    }
fi

if [ "$system" = "debian" ]; then
    daemon() {
        start-stop-daemon --start --exec $1 -- $2
    }

    killproc() {
        start-stop-daemon --stop --exec $@
    }

    fail_msg() {
        echo " ...fail!"
    }

    succ_msg() {
        echo " ...done."
    }
fi

if [ "$system" = "gentoo" ]; then
    . /sbin/functions.sh
    daemon() {
        start-stop-daemon --start --exec $1 -- $2
    }

    killproc() {
        start-stop-daemon --stop --exec $@
    }

    fail_msg() {
        echo " ...fail!"
    }

    succ_msg() {
        echo " ...done."
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
        echo -n "$1"
    }
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
        echo -n "Starting VirtualBox host to guest time synchronisation ";
        vboxaddrunning || {
            echo "VirtualBox Additions module not loaded!"
            exit 1
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
        echo -n "Stopping VirtualBox host to guest time synchronisation ";
        vboxaddrunning || {
            echo "VirtualBox Additions module not loaded!"
            exit 1
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
