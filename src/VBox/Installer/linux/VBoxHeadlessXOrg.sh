#!/bin/sh
#
# VirtualBox X Server auto-start script.
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

PATH=$PATH:/bin:/sbin:/usr/sbin

## Start one or several X servers in the background for use with headless
# rendering.  We only support X.Org Server at the moment.  Starting the X
# servers is managed by dropping one or more files xorg.conf.<n> into the
# directory ${HEADLESS_X_ORG_CONFIGURATION_DIRECTORY} - the default value
# below can be overridden in the configuration file.  We will attempt to start
# an X server process for each configuration file using display number <n>.

# I have tried to follow the best practices I could find for writing a Linux
# daemon process (and doing it in shell script) which should work well with
# traditional and modern service systems using minimal init or service files. 
# In our case this boils down to:
#  * Start with a single command line, stop using one of ${EXIT_SIGNALS} below.
#  * Stopping with a signal can be done safely using the pid stored in the
#    pid-file and our (presumably unique) command name.  For this reason we only
#    support running one instance of the service though.
#  * Support starting in the foreground for systems with proper service control.
#  * Support backgrounding with a pid-file for other systems.
#  * Clean up all sub-processes (X servers) ourselves when we are stopped
#    cleanly and don't provide any other way to clean them up automatically (in
#    case we are stopped uncleanly) as we don't know of a generic safe way to
#    do so, though some service management systems (i.e. systemd) can do so.
#    (A more thorough automatic clean-up would be possible if Xorg didn't
#    potentially have to be run as root, so that we could run all processes
#    using a service-specific user account and just terminate all processes
#    run by that user to clean up.)
#  * For this reason, our pid-file only contains the pid of the master process.
#  * To simplify system service and start-up message handling, we write
#    Debian-conform progress information to standard output (what processes we
#    are starting, information if we do something slow) and errors to standard
#    error.  The init script or service file can log a single success or
#    failure line when we are ready in whatever way the system expects and deal
#    with our output in the most appropriate way for the system.  Some systems
#    require output, others don't mind, and Debian-conform information should
#    actually fit everyone's requirements (others are more lax about the
#    format).

## Configuration file name.
# Don't use vbox.cfg as that is currently automatically created and deleted.
# Don't use /etc/default/virtualbox as that is Debian policy only and not very
# nice.  Let's try to use this in future and phase out the other two.
## @todo Should we be using /etc/virtualbox instead of /etc/vbox?
CONFIGURATION_FILE=/etc/vbox/vbox.conf

## The service name.  Should match the init script name.
SERVICE_NAME="vboxheadlessxorg"
## The descriptive service name.
SERVICE_LONG_NAME="VBoxHeadless X Server service"
## Timeout in seconds when shutting down the service.
SERVICE_SHUTDOWN_TIMEOUT=5
## Signals and conditions which may be used to terminate the service.
EXIT_SIGNALS="EXIT HUP INT QUIT ABRT TERM"

# Default settings values, override in the configuration file.
## The directory where the configuration files for the X servers are dropped.
HEADLESS_X_ORG_CONFIGURATION_DIRECTORY=/etc/vbox/headlessxorg.conf.d
## The path to the pidfile for the service.
HEADLESS_X_ORG_PIDFILE="/var/run/${SERVICE_NAME}.pid"

## The function definition at the start of every non-trivial shell script!
abort() {
  stop 2>/dev/null
  ## $@, ... Error text to output to standard error in printf format.
  printf "$@" >&2
  exit 1
}

## Milder version of abort, when we can't continue because of a valid condition.
abandon() {
  stop 2>/dev/null
  ## $@, ... Text to output to standard error in printf format.
  printf "$@" >&2
  exit 0
}

usage() {
  cat << EOF
Usage:

  $(basename $0) [<options>]

Do any system-wide set-up required to properly run the copy of VirtualBox
residing in the current directory.  If no options are specified, everything
except --no-udev-and --log-file is assumed.

Options:

  -p|--pidfile <name>    Specify the name of the file to save the pids of child
                         processes in.  Pass in an empty string to disable
                         pid-file creation.

  -d|-daemonize)         Detach fron the terminal and  continue running in the
                         background.

  --help|--usage         Print this text.
EOF
}

abort_usage() {
  usage > &2
  abort "$@"
}

# Change to the directory where the script is located.
MY_DIR="$(dirname "$0")"
MY_DIR=`cd "${MY_DIR}" && pwd`
[ -d "${MY_DIR}" ] ||
  abort "Failed to change to directory ${MY_DIR}.\n"

# Get installation configuration
[ -r /etc/vbox/vbox.cfg ] || abort "/etc/vbox/vbox.cfg not found.\n"
. /etc/vbox/vbox.cfg

[ -r scripts/generated.sh ] ||
  abort "${LOG_FILE}" "Failed to find installation information in ${MY_DIR}.\n"
. scripts/generated.sh

[ -r /etc/vbox/vbox.conf ] && . /etc/vbox/vbox.conf

cat << EOF
${VBOX_PRODUCT} VBoxHeadless X Server start-up service Version ${VBOX_VERSION_STRING}
(C) 2005-${VBOX_C_YEAR} ${VBOX_VENDOR}
All rights reserved.

EOF

# Parse our arguments.
while true; do
  case $1 in
    -p|--pidfile)
      [ -n "$2" ] || abort_usage "Missing argument for $1.\n"
      ARGUMENT_LIST="$ARGUMENT_LIST $1 $2"
      HEADLESS_X_ORG_PIDFILE="$2"
      shift 2
      ;;HEADLESS_X_ORG_PIDFILE
    -d|-daemonize)
      DAEMONIZE=true
      shift
      ;;
    --help|--usage)
      usage
      exit 0
      ;;
    *)
      abort_usage "Unknown argument $1.\n"
      ;;
  esac
done

[ -f "${HEADLESS_X_ORG_PIDFILE}" ] &&
ps -p "$(cat "${HEADLESS_X_ORG_PIDFILE}")" -o comm | grep -q "${SERVICE_NAME}" &&
  abort "The service appears to be already running.\n"

PIDFILE_DIRECTORY="$(dirname "${HEADLESS_X_ORG_PIDFILE}")"
[ -n "${HEADLESS_X_ORG_PIDFILE}" ] &&
  { [ ! -d "${PIDFILE_DIRECTORY}" ] && [ ! -w "${PIDFILE_DIRECTORY}" ] } &&
    abort "Can't write to pid-file directory ${PIDFILE_DIRECTORY}.\n"

{ [ -d "/var/log" ] && [ -w "/var/log" ] } ||
  abort "Can't write to /var/log.\n"

# Refuse to daemonize without creating a pid-file.  Change this if we think of
# a good reason.
{ [ -n "${DAEMONIZE}" ] && [ -z "${HEADLESS_X_ORG_PIDFILE}" ] } &&
  abort "Daemonizing without creating a pid-file is not supported.\n"

# Double background from shell script, disconnecting all standard streams, to
# daemonise.  This may fail if someone has connected something to another file
# descriptor.  This is indented (see e.g. fghack in the daemontools package).
if [ -n "${DAEMONIZE}" ]; then
  ("$0" -p "${HEADLESS_X_ORG_PIDFILE}" < /dev/null > /dev/null 2>&1 &
     echo "Created daemon process with PID $!.") &
  exit 0
fi

X_SERVER_PIDS=""
trap "kill \"\${X_SERVER_PIDS}\"; exit 0" ${EXIT_SIGNALS}
space=""  # Hack to put spaces between the pids but not before or after.
for file in "${HEADLESS_X_ORG_CONFIGURATION_DIRECTORY}"/xorg.conf.*; do
  filename="$(basename "${file}")"
  expr "${filename}" : "xorg.conf.[0-9]*" ||
    { stop; abort "Badly formed file name \"${file}\".\n" }
  screen="${filename##*.}"
  Xorg ":${screen}" -config "${file}" > /dev/null 2>&1 & ||
    abort "Failed to create screen ${screen}.\n"
  X_SERVER_PIDS="${X_SERVER_PIDS}${space}$!"
  space=" "
done
[ -z "${HEADLESS_X_ORG_PIDFILE}" ] &&
  echo "$$" > "${HEADLESS_X_ORG_PIDFILE}"
wait
