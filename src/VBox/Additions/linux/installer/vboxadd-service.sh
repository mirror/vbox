#!/bin/sh
#
# Sun VirtualBox
# Linux Additions Guest Additions service daemon init script.
#
# Copyright (C) 2006-2009 Sun Microsystems, Inc.
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

# chkconfig: 35 35 65
# description: VirtualBox Additions service
#
### BEGIN INIT INFO
# Provides:       vboxadd-service
# Required-Start: vboxadd
# Required-Stop:  vboxadd
# Default-Start:  2 3 4 5
# Default-Stop:   0 1 6
# Description:    VirtualBox Additions Service
### END INIT INFO

PATH=$PATH:/bin:/sbin:/usr/sbin

system=unknown
if [ -f /etc/redhat-release ]; then
    system=redhat
    PIDFILE="/var/lock/subsys/vboxadd-service"
elif [ -f /etc/SuSE-release ]; then
    system=suse
    PIDFILE="/var/lock/subsys/vboxadd-service"
elif [ -f /etc/debian_version ]; then
    system=debian
    PIDFILE="/var/run/vboxadd-service"
elif [ -f /etc/gentoo-release ]; then
    system=gentoo
    PIDFILE="/var/run/vboxadd-service"
elif [ -f /etc/arch-release ]; then
     system=arch
     PIDFILE="/var/run/vboxadd-service"
elif [ -f /etc/slackware-version ]; then
    system=slackware
    PIDFILE="/var/run/vboxadd-service"
elif [ -f /etc/lfs-release ]; then
    system=lfs
    PIDFILE="/var/run/vboxadd-service.pid"
else
    system=other
    if [ -d /var/run -a -w /var/run ]; then
        PIDFILE="/var/run/vboxadd-service"
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

    begin() {
        echo -n "$1"
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

    begin() {
        echo -n "$1"
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

    begin() {
        echo -n "$1"
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

    begin() {
        echo -n "$1"
    }

    if [ "`which $0`" = "/sbin/rc" ]; then
        shift
    fi
fi

if [ "$system" = "arch" ]; then
    USECOLOR=yes
    . /etc/rc.d/functions
    daemon() {
        $@
        test $? -eq 0 && add_daemon rc.`basename $1`
    }

    killproc() {
        killall $@
        rm_daemon `basename $@`
    }

    fail_msg() {
        stat_fail
    }

    succ_msg() {
        stat_done
    }

    begin() {
        stat_busy "$1"
    }

fi

if [ "$system" = "slackware" ]; then
    daemon() {
        $1 $2
    }

    killproc() {
        killall $1
        rm -f $PIDFILE
    }

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

if [ "$system" = "lfs" ]; then
    . /etc/rc.d/init.d/functions
    daemon() {
        loadproc $1 $2
    }

    fail_msg() {
        echo_failure
    }

    succ_msg() {
        echo_ok
    }

    begin() {
        echo $1
    }

    status() {
        statusproc $1
    }
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

binary=/usr/sbin/vboxadd-service

test -x "$binary" || {
    echo "Cannot run $binary"
    exit 1
}

vboxaddrunning() {
    lsmod | grep -q "vboxadd[^_-]"
}

start() {
    if ! test -f $PIDFILE; then
        begin "Starting VirtualBox host to guest time synchronization ";
        vboxaddrunning || {
            echo "VirtualBox Additions module not loaded!"
            exit 1
        }
        daemon $binary --daemonize
        RETVAL=$?
        test $RETVAL -eq 0 && echo `pidof vboxadd-service` > $PIDFILE
        succ_msg
    fi
    return $RETVAL
}

stop() {
    if test -f $PIDFILE; then
        begin "Stopping VirtualBox host to guest time synchronisation ";
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

    status() {
        echo -n "Checking for vboxadd-service"
        if [ -f $PIDFILE ]; then
            echo " ...running"
        else
            echo " ...not running"
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
    status
    ;;
*)
    echo "Usage: $0 {start|stop|restart|status}"
    exit 1
esac

exit $RETVAL
