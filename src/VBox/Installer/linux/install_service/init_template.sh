#!/bin/sh
#
# VirtualBox generic init script.
#
# Copyright (C) 2012 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

### BEGIN INIT INFO
# Required-Start:  $local_fs
# Should-Start:    $syslog
# Required-Stop:   $local_fs
# Should-Stop:     $syslog
# Default-Start:   2 3 4 5
# Default-Stop:    0 1 6
# Short-Description: %DESCRIPTION%
### END INIT INFO

cr="
"
tab="	"
IFS=" ${cr}${tab}"
'unset' -f unalias
'unalias' -a
unset -f command
PATH=/bin:/sbin:/usr/bin:/usr/sbin:$PATH

## A generic service script which can be used, after substituting some place-
# holders with service-specific values, to run most services on LSB, System V
# or BSD-compatible service management systems.  As we control both the service
# code and the init script we try to push as much as possible of the logic into
# the service and out of the very system-dependent service configuration
# scripts and files.  See the help text of the "install_service.sh" helper
# script for more details.
#
# Furthermore, to simplify deployment, we will install all init scripts using
# this generic template manually during the post install phase or at run time
# using LSB functions if they are available (as they should be on most common
# modern distributions) or manually placing the file in the appropriate
# directory and creating symbolic links on System V or writing to rc.local on
# BSD-compatible systems.  Systems requiring different treatment will be added
# here when we add support for them, but we will try to keep everything as
# generic as we can.
#
# In general, we try to behave as natively as we reasonably can on the most
# important target systems we support and to work well enough on as many others
# as possible, but in particular without trying to look perfectly native.
#
# To use this template as an init script, replace the following text sequences
# (wrapped in percent characters) with the values you need:
# COMMAND: Path to the service binary or script, with all required escaping for
# characters which are special in shell scripts.
# ARGUMENTS: The arguments to pass to the binary when starting the service,
# with all required escaping for characters which are special in shell scripts.
# SERVICE_NAME: The name of the service, using ASCII characters 33 to 126 only.
# DESCRIPTION: Short description of the service, suitable for use in texts like
# "DESCRIPTION successfully started", using Utf-8 characters 32 to 126 and 128
# and upwards. 

## Time out in seconds when shutting down the service.
SHUT_DOWN_TIME_OUT=5
## If this is set to an empty value then the LSB init functions will not be
# used.  This is intended for testing the fallback commands.
LSB_FUNCTIONS="/lib/lsb/init-functions"

## Redhat and Fedora lock directory
LOCK_FOLDER="/var/lock/subsys/"
LOCK_FILE="${LOCK_FOLDER}/%SERVICE_NAME%"

# Silently exit if the package was uninstalled but not purged.
[ -r %COMMAND% ] || exit 0

## The function definition at the start of every non-trivial shell script!
abort()
{
    log_failure_msg "$*"
    exit 1
}

## Exit successfully.
do_success()
{
    log_success_msg "%DESCRIPTION% successfully started."
    exit 0
}

# Use LSB functions if available.  Success and failure messages default to just
# "echo" if the LSB functions are not available, so call these functions with
# messages which clearly read as success or failure messages.
[ -n "${LSB_FUNCTIONS}" ] && [ -f "${LSB_FUNCTIONS}" ] &&
  . "${LSB_FUNCTIONS}"

type log_success_msg >/dev/null 2>&1 ||
    log_success_msg()
    {
        echo "${*}"
    }

type log_success_fail >/dev/null 2>&1 ||
    log_success_fail()
    {
        echo "${*}"
    }

## Get the LSB standard PID-file name for a binary.
pidfilename()
{
    echo "/var/run/${1##*/}.pid"
}

## Get the PID-file for a process like the LSB functions do ( "-p" or by name).
pidfileofproc()
{
    if [ x"${1}" = "x-p" ]; then
        echo "${2}"
    else
        pidfilename "${1}"
    fi
}

## Read the pids from an LSB PID-file, checking that they are positive numbers.
pidsfromfile()
{
    read -r pids < "${1}" 2>/dev/null
    for i in $pids; do
        [ 1 -le "${i}" ] || return 1
    echo "${pids}"
}

## Check whether the binary $1 with the pids $2... is running.
procrunning()
{
    binary="${1}"
    shift
    ps -p "${@}" -f 2>/dev/null | grep -q "${binary}"
}

type pidofproc >/dev/null 2>&1 ||
    pidofproc()
    {
        pidfile="$(pidfileofproc "${@}")"
        [ "x${1}" = "x-p" ] && shift 2
        pids="$(pidsfromfile "${pidfile}")"
        procrunning "${1}" ${pids} && echo "${pids}"
    }

type killproc >/dev/null 2>&1 ||
    killproc()
    {
        pidfile="$(pidfileofproc "${@}")"
        [ "x${1}" = "x-p" ] && shift 2
        pids="$(pidsfromfile "${pidfile}")"
        if [ -n "${2}" ]; then
            procrunning "${1}" ${pids} || return 1
            kill "${2}" ${pids}
            return 0
        else
            rm -f "${pidfile}"
            procrunning "${1}" ${pids} || return 0
            kill "${pid}"
            time=0
            while true; do
                sleep 1
                procrunning "${1}" ${pids} || break
                time=$((${time} + 1))
                if [ "${time}" = "${SHUT_DOWN_TIME_OUT}" ]; then
                    kill -9 "${pid}"
                    break
                fi
            done
            return 0
        fi
    }

start()
{
    [ -d "${LOCK_FOLDER}" ] && touch "${LOCK_FILE}"
    [ -n "$(pidofproc %COMMAND%)" ] && exit 0
    %COMMAND% %ARGUMENTS% >/dev/null 2>&1 &
    pid="$!"
    pidfile="$(pidfilename %COMMAND%)"
    echo "${pid}" > "${pidfile}"
    do_success
}

stop()
{
    killproc %COMMAND% || abort "%DESCRIPTION% failed to stop!"
    rm -f "${LOCK_FILE}"
    log_success_msg "%DESCRIPTION% successfully stopped."
    return 0
}

status()
{
    [ -n "$(pidofproc %COMMAND%)" ] && exit 0
    [ -f "$(pidfilename %COMMAND%)" ] && exit 1
    [ -f "${LOCK_FILE}" ] && exit 2
    exit 3
}

# Gentoo/OpenRC perculiarity.
if [ "$(which "${0}")" = "/sbin/rc" ]; then
    shift
fi

if [ "${1}" = "--lsb-functions" ]; then
    LSB_FUNCTIONS="${2}"
    shift 2
fi

case "${1}" in
start)
    start
    ;;
stop)
    stop
    ;;
restart|force-reload)
    stop
    start
    ;;
condrestart|try-restart)
    status || exit 0
    stop
    start
    ;;
reload)
    ;;
status)
    status
    ;;
*)
    echo "Usage: ${0} {start|stop|restart|status}"
    exit 1
esac
