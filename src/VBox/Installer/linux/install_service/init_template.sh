#!/bin/sh
#
# VirtualBox generic init script.
#
# Copyright (C) 2012-2015 Oracle Corporation
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

## @todo We should really replace the daemon starting, stopping and checking
#        code with a tool of our own written in C, which we could always use
#        instead of the LSB functions.

cr="
"
tab="   "
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
# See the inline documentation in the code for generate_service_file for
# details of the generation process.

## Time out in seconds when shutting down the service.
SHUT_DOWN_TIME_OUT=5
## If this is set to an empty value then the LSB init functions will not be
# used.  This is intended for testing the fallback commands.
LSB_FUNCTIONS="/lib/lsb/init-functions"

# Silently exit if the package was uninstalled but not purged.
test -r %COMMAND% || exit 0

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

## Set the error message.
set_error()
{
    test -z "${error}" && error="${1}"
}

# Gentoo/OpenRC perculiarity.
if test "x${0}" = "x/sbin/rc" || test "x${0}" = "xrc"; then
    shift
fi

# Process arguments.
action=""
error=""
prefix="/var"
while test x"${#}" != "x0"; do
    case "${1}" in
    --lsb-functions)
        test x"${#}" = "x1" &&
            set_error "${1}: missing argument."
        LSB_FUNCTIONS="${2}"
        shift 2;;
    --prefix)
        test x"${#}" = "x1" &&
            set_error "${1}: missing argument."
        prefix="${2}"
        shift 2;;
    --help)
        cat << EOF
Usage:

  ${0} {start|stop|restart|status} [<options>]

  start|stop|restart|status
      Start/stop/restart/report status for the service.

Options:

  --lsb-functions <script>
      Take the standard LSB init functions from <script> instead of from the
      normal location, or use our own versions if <script> is an empty string.

  --prefix <folder>
      Use the folder <folder> for storing variable data instead of "/var".  The
      child folder "run" must exist.
EOF
        exit 0;;
    start|stop|restart|force-reload|condrestart|try-restart|reload|status)
        test -z "${action}" ||
            set_error "More than one action requested."
        action="${1}"
        shift;;
    *)
        set_error "Unknown option \"${1}\".  Try \"${0} --help\" for more information."
        shift;;
    esac
done

## Set Redhat and Fedora lock directory
LOCK_FOLDER="${prefix}/lock/subsys/"
LOCK_FILE="${LOCK_FOLDER}/%SERVICE_NAME%"

# Use LSB functions if available.  Success and failure messages default to just
# "echo" if the LSB functions are not available, so call these functions with
# messages which clearly read as success or failure messages.
test -n "${LSB_FUNCTIONS}" && test -f "${LSB_FUNCTIONS}" &&
  . "${LSB_FUNCTIONS}"

type log_success_msg >/dev/null 2>&1 ||
    log_success_msg()
    {
        cat << EOF
${*}
EOF
    }

type log_failure_msg >/dev/null 2>&1 ||
    log_failure_msg()
    {
        cat << EOF
${*}
EOF
    }

## Get the LSB standard PID-file name for a binary.
pidfilename()
{
    echo "${prefix}/run/${1##*/}.pid"
}

## Get the PID-file for a process like the LSB functions do ( "-p" or by name).
pidfileofproc()
{
    if test x"${1}" = "x-p"; then
        echo "${2}"
    else
        pidfilename "${1}"
    fi
}

## Read the pids from an LSB PID-file, checking that they are positive numbers.
pidsfromfile()
{
    pids=""
    test -r "${1}" &&
        read -r pids < "${1}" 2>/dev/null
    for i in $pids; do
        test 1 -le "${i}" || return 1
    done
    echo "${pids}"
}

## Check whether the binary $1 with the pids $2... is running.
procrunning()
{
    binary="${1}"
    shift
    case "`ps -p "${@}" -f 2>/dev/null`" in *"${binary}"*)
        return 0;;
    esac
    return 1
}

# We prefer our own implementations of pidofproc and killproc over falling back
# to distribution ones with unknown quirks.
# type pidofproc >/dev/null 2>&1 ||
    pidofproc()
    {
        pidfile="`pidfileofproc "${@}"`"
        test "x${1}" = "x-p" && shift 2
        pids="`pidsfromfile "${pidfile}"`"
        procrunning "${1}" ${pids} && echo "${pids}"
    }

# type killproc >/dev/null 2>&1 ||
    killproc()
    {
        pidfile="`pidfileofproc "${@}"`"
        test "x${1}" = "x-p" && shift 2
        pids="`pidsfromfile "${pidfile}"`"
        if test -n "${2}"; then
            procrunning "${1}" ${pids} || return 1
            kill "${2}" ${pids}
            return 0
        else
            rm -f "${pidfile}"
            procrunning "${1}" ${pids} || return 0
            kill "${pids}"
            # Short busy wait for the process to terminate.
            stamp="`times`"
            while test x"${stamp}" = x"`times`"; do
                procrunning "${1}" ${pids} || return 0
            done
            # Slow sleeping wait if it is still running.
            for high in "" 1 2 3 4 5 6 7 8 9; do
                for time in ${high}0 ${high}1 ${high}2 ${high}3 ${high}4 ${high}5 ${high}6 ${high}7 ${high}8 ${high}9; do
                    sleep 1
                    procrunning "${1}" ${pids} || return 0
                    if test "${time}" = "${SHUT_DOWN_TIME_OUT}"; then
                        kill -9 "${pid}"
                        return 0
                    fi
                done
            done
            return 0
        fi
    }

start()
{
    test -d "${LOCK_FOLDER}" && touch "${LOCK_FILE}"
    test -n "`pidofproc %COMMAND%`" && exit 0
%HAVE_DAEMON%    %COMMAND% %ARGUMENTS% >/dev/null 2>&1 &
%HAVE_DAEMON%    pid="$!"
%HAVE_DAEMON%    pidfile="`pidfilename %COMMAND%`"
%HAVE_DAEMON%    echo "${pid}" > "${pidfile}"
%HAVE_ONESHOT%    %COMMAND% %ARGUMENTS% >/dev/null 2>&1 || abort "%DESCRIPTION% failed to start!"
    do_success
}

stop()
{
%HAVE_STOP_COMMAND%    %STOP_COMMAND% %STOP_ARGUMENTS% || abort "%DESCRIPTION% failed to stop!"
%HAVE_DAEMON%    killproc %COMMAND% || abort "%DESCRIPTION% failed to stop!"
    rm -f "${LOCK_FILE}"
    log_success_msg "%DESCRIPTION% successfully stopped."
    return 0
}

status()
{
%HAVE_STATUS_COMMAND%    %STATUS_COMMAND% %STATUS_ARGUMENTS%
%HAVE_STATUS_COMMAND%    exit
%NO_STATUS_COMMAND%    pid="`pidofproc %COMMAND%`"
%NO_STATUS_COMMAND%    test -n "${pid}" &&
%NO_STATUS_COMMAND%    {
%NO_STATUS_COMMAND%        echo "%SERVICE_NAME% running, process ${pid}"
%NO_STATUS_COMMAND%        exit 0
%NO_STATUS_COMMAND%    }
%NO_STATUS_COMMAND%    test -f "`pidfilename %COMMAND%`" &&
%NO_STATUS_COMMAND%    {
%NO_STATUS_COMMAND%        echo "%SERVICE_NAME% not running but PID-file present."
%NO_STATUS_COMMAND%        exit 1
%NO_STATUS_COMMAND%    }
%NO_STATUS_COMMAND%    test -f "${LOCK_FILE}" &&
%NO_STATUS_COMMAND%    {
%NO_STATUS_COMMAND%        echo "%SERVICE_NAME% not running but lock file present."
%NO_STATUS_COMMAND%        exit 2
%NO_STATUS_COMMAND%    }
%NO_STATUS_COMMAND%    echo "%SERVICE_NAME% not running."
%NO_STATUS_COMMAND%    exit 3
}

test -z "${error}" || abort "${error}"

case "${action}" in
start)
    start;;
stop)
    stop;;
restart|force-reload)
    start
    stop;;
condrestart|try-restart)
    status || exit 0
    stop
    start;;
reload)
    ;;
status)
    status;;
esac
