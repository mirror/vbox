#!/bin/sh
## @file
# VirtualBox Test Execution Service Init Script.
#

#
# Copyright (C) 2018 Oracle Corporation
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
# Provides:       vboxtxs-runvm
# Required-Start: $ALL
# Required-Stop:
# Default-Start:  5
# Default-Stop:   0 1 6
# Description:    VirtualBox Test Execution Service
### END INIT INFO

PATH=$PATH:/bin:/sbin:/usr/sbin
SCRIPTNAME=vboxtxs-runvm.sh

CDROM_PATH=/media/cdrom
SCRATCH_PATH=/tmp/vboxtxs-scratch
SMOKEOUTPUT_PATH=/tmp/vboxtxs-smoketestoutput
DEVKMSG_PATH=/dev/kmsg

PIDFILE="/var/run/vboxtxs"

export TESTBOX_PATH_RESOURCES="/home/vbox/testrsrc"
SMOKETEST_SCRIPT="/opt/validationkit/tests/smoketests/tdSmokeTest1.py"
PYTHON_BINARY="python"

# Preamble for Gentoo
if [ "`which $0`" = "/sbin/rc" ]; then
    shift
fi

kernlog_msg() {
    test -n "$2" && echo "${SCRIPTNAME}: ${1}"
    echo "${SCRIPTNAME}: ${1}" > $DEVKMSG_PATH
}

dumpfile_to_kernlog() {
    if test -f "$1"; then
        kernlog_msg "---------------------- DUMP BEGIN ----------------------"
        cat "$1" | while read LINE
        do
            kernlog_msg "${LINE}"
        done
        kernlog_msg "---------------------- DUMP END ------------------------"
        rm -f "$1"
    else
        kernlog_msg "${1}: file not found" console
    fi
}

killproc()
{
    kp_binary="${1##*/}"
    pkill "${kp_binary}" || return 0
    sleep 1
    pkill "${kp_binary}" || return 0
    sleep 1
    pkill -9 "${kp_binary}"
    return 0
}

case "`uname -m`" in
    AMD64|amd64|X86_64|x86_64)
        binary=/opt/validationkit/linux/amd64/TestExecService
        ;;

    i386|x86|i486|i586|i686|i787)
        binary=/opt/validationkit/linux/x86/TestExecService
        ;;

    *)
        binary=/opt/validationkit/linux/x86/TestExecService
        ;;
esac

fixAndTestBinary() {
    chmod a+x "$binary" 2> /dev/null > /dev/null
    test -x "$binary" || {
        echo "Cannot run $binary"
        exit 1
    }
}

testRsrcPath() {
    test -d "$TESTBOX_PATH_RESOURCES" || {
        echo "TESTBOX_PATH_RESOURCES directory not found"
        exit 1
    }
}

start() {
    if ! test -f $PIDFILE; then
        kernlog_msg "Starting Nested Smoke Test" console
        fixAndTestBinary
        testRsrcPath
        sleep 5
        $PYTHON_BINARY $SMOKETEST_SCRIPT -v -v -d --vbox-session-type gui --quick all 1> "${SMOKEOUTPUT_PATH}" 2>&1
        RETVAL=$?
        dumpfile_to_kernlog "${SMOKEOUTPUT_PATH}"
        if test $RETVAL -eq 0; then
            kernlog_msg "Starting VirtualBox Test Execution service" console
            mount /dev/cdrom "${CDROM_PATH}" 2> /dev/null > /dev/null
            $binary --auto-upgrade --scratch="${SCRATCH_PATH}" --cdrom="${CDROM_PATH}" --no-display-output > /dev/null
            RETVAL=$?
            test $RETVAL -eq 0 && sleep 3 && echo `pidof TestExecService` > $PIDFILE
            if ! test -s "${PIDFILE}"; then
                RETVAL=5
            fi
            if test $RETVAL -eq 0; then
                kernlog_msg "Test Execution service started" console
            else
                kernlog_msg "Test Execution service failed to start" console
            fi
        else
            kernlog_msg "Smoke Test failed! error code ${RETVAL}" console
            RETVAL=7
        fi
    else
        kernlog_msg "Starting Nested Smoke Test failed! Pidfile ${PIDFILE} exists" console
        RETVAL=9
    fi
    return $RETVAL
}

stop() {
    if test -f $PIDFILE; then
        kernlog_msg "Stopping VirtualBox Test Execution Service"
        killproc $binary
        rm -f $PIDFILE
    fi
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

