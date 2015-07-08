#!/bin/sh
#
# Linux Additions Guest Additions service daemon init script.
#
# Copyright (C) 2006-2012 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

# chkconfig: 345 35 65
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
    PIDFILE="/var/run/vboxadd-service"
elif [ -f /etc/debian_version ]; then
    system=debian
    PIDFILE="/var/run/vboxadd-service"
elif [ -f /etc/gentoo-release ]; then
    system=gentoo
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
        start-stop-daemon --stop --retry 2 --exec $@
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
    if [ -f /sbin/functions.sh ]; then
        . /sbin/functions.sh
    elif [ -f /etc/init.d/functions.sh ]; then
        . /etc/init.d/functions.sh
    fi
    daemon() {
        start-stop-daemon --start --exec $1 -- $2
    }

    killproc() {
        start-stop-daemon --stop --retry 2 --exec $@
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

if [ "$system" = "slackware" -o "$system" = "other" ]; then
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

binary=/usr/sbin/VBoxService

testbinary() {
    test -x "$binary" || {
        echo "Cannot run $binary"
        exit 1
    }
}

vboxaddrunning() {
    lsmod | grep -q "vboxguest[^_-]"
}

start() {
    if ! test -f $PIDFILE; then
        begin "Starting VirtualBox Guest Addition service ";
        vboxaddrunning || {
            echo "VirtualBox Additions module not loaded!"
            exit 1
        }
        testbinary
        daemon $binary > /dev/null
        RETVAL=$?
        test $RETVAL -eq 0 && echo `pidof VBoxService` > $PIDFILE
        succ_msg
    fi
    return $RETVAL
}

stop() {
    if test -f $PIDFILE; then
        begin "Stopping VirtualBox Guest Addition service ";
        killproc $binary
        RETVAL=$?
        if ! pidof VBoxService > /dev/null 2>&1; then
            rm -f $PIDFILE
            succ_msg
        else
            fail_msg
        fi
    fi
    return $RETVAL
}

restart() {
    stop && start
}

status() {
    echo -n "Checking for VBoxService"
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
setup)
    ;;
cleanup)
    ;;
*)
    echo "Usage: $0 {start|stop|restart|status}"
    exit 1
esac

exit $RETVAL
