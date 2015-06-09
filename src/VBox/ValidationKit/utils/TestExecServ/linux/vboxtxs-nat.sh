#!/bin/sh
## @file
# VirtualBox Test Execution Service Init Script for NATted setups.
#

#
# Copyright (C) 2006-2015 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#
# The contents of this file may alternatively be used under the terms
# of the Common Development and Distribution License Version 1.0
# (CDDL) only, as it comes in the "COPYING.CDDL" file of the
# VirtualBox OSE distribution, in which case the provisions of the
# CDDL are applicable instead of those of the GPL.
#
# You may elect to license modified versions of this file under the
# terms and conditions of either the GPL or the CDDL or both.
#

# chkconfig: 35 35 65
# description: VirtualBox Test Execution Service
#
### BEGIN INIT INFO
# Provides:       vboxtxs
# Required-Start: $network
# Required-Stop:
# Default-Start:  2 3 4 5
# Default-Stop:   0 1 6
# Description:    VirtualBox Test Execution Service
### END INIT INFO

PATH=$PATH:/bin:/sbin:/usr/sbin

CDROM_PATH=/media/cdrom
SCRATCH_PATH=/tmp/vboxtxs-scratch

system=unknown
if [ -f /etc/redhat-release ]; then
    system=redhat
    PIDFILE="/var/lock/subsys/vboxtxs"
elif [ -f /etc/SuSE-release ]; then
    system=suse
    PIDFILE="/var/lock/subsys/vboxtxs"
elif [ -f /etc/debian_version ]; then
    system=debian
    PIDFILE="/var/run/vboxtxs"
elif [ -f /etc/gentoo-release ]; then
    system=gentoo
    PIDFILE="/var/run/vboxtxs"
elif [ -f /etc/arch-release ]; then
     system=arch
     PIDFILE="/var/run/vboxtxs"
elif [ -f /etc/slackware-version ]; then
    system=slackware
    PIDFILE="/var/run/vboxtxs"
elif [ -f /etc/lfs-release ]; then
    system=lfs
    PIDFILE="/var/run/vboxtxs.pid"
else
    system=other
    if [ -d /var/run -a -w /var/run ]; then
        PIDFILE="/var/run/vboxtxs"
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
    if [ -f /sbin/functions.sh ]; then
        . /sbin/functions.sh
    elif [ -f /etc/init.d/functions.sh ]; then
        . /etc/init.d/functions.sh
    fi
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

case "`uname -m`" in
    AMD64|amd64|X86_64|x86_64)
        binary=/root/validationkit/linux/amd64/TestExecService
        ;;

    i386|x86|i486|i586|i686|i787)
        binary=/root/validationkit/linux/x86/TestExecService
        ;;

    *)
        binary=/root/validationkit/linux/x86/TestExecService
        ;;
esac

fixAndTestBinary() {
    chmod a+x "$binary" 2> /dev/null > /dev/null
    test -x "$binary" || {
        echo "Cannot run $binary"
        exit 1
    }
}

start() {
    if ! test -f $PIDFILE; then
        begin "Starting VirtualBox Test Execution Service ";
        fixAndTestBinary
        mount /dev/cdrom "${CDROM_PATH}" 2> /dev/null > /dev/null
        daemon $binary --auto-upgrade --scratch="${SCRATCH_PATH}" --cdrom="${CDROM_PATH}" \
            --no-display-output --tcp-connect 10.0.2.2 > /dev/null
        RETVAL=$?
        test $RETVAL -eq 0 && sleep 2 && echo `pidof TestExecService` > $PIDFILE
        if ! test -s "${PIDFILE}"; then
            RETVAL=5
        fi
        if test $RETVAL -eq 0; then
            succ_msg
        else
            fail_msg
        fi
    fi
    return $RETVAL
}

stop() {
    if test -f $PIDFILE; then
        begin "Stopping VirtualBox Test Execution Service ";
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
    echo -n "Checking for vboxtxs"
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
