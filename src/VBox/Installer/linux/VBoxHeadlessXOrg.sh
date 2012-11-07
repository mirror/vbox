#!/bin/sh
# $Id$
#
# VirtualBox X Server auto-start service.
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
# rendering.  For details, options and configuration see the usage() function
# further down.
#
# I have tried to follow the best practices I could find for writing a Linux
# service (and doing it in shell script) which should work well with
# traditional and modern service systems using minimal init or service files. 
# In our case this boils down to:
#  * Start with a single command line, stop using one of ${EXIT_SIGNALS} below.
#  * Stopping with a signal can be done safely using the pid stored in the
#    pid-file and our (presumably unique) command name.  For this reason we
#    only support running one instance of the service though.
#  * Start in the foreground.  Systems without proper service control can take
#    care of the backgrounding in the init script.
#  * Clean up all sub-processes (X servers) ourselves when we are stopped
#    cleanly and don't provide any other way to clean them up automatically (in
#    case we are stopped uncleanly) as we don't know of a generic safe way to
#    do so, though some service management systems (i.e. systemd) can do so.
#    (A more thorough automatic clean-up would be possible if Xorg didn't
#    potentially have to be run as root, so that we could run all processes
#    using a service-specific user account and just terminate all processes
#    run by that user to clean up.)

## Default configuration file name.
# @note This is not very nice - /etc/default is actually Debian-specific.
CONFIGURATION_FILE=/etc/default/virtualbox
## The name of this script.
SCRIPT_NAME="$0"
## Command line we were called with.
SCRIPT_COMMAND_LINE="$0 $@"
## The service name.  Should match the init script name.
SERVICE_NAME="vboxheadlessxorg"
## The descriptive service name.
SERVICE_LONG_NAME="VBoxHeadless X Server service"
## Signals and conditions which may be used to terminate the service.
EXIT_SIGNALS="EXIT HUP INT QUIT ABRT TERM"
## The default X server configuration directory.
DEFAULT_CONFIGURATION_FOLDER="/etc/vbox/headlessxorg.conf.d"

## Print usage information for the service script.
## @todo Perhaps we should support some of the configuration file options from
#        the command line.  Opinions welcome.
## @todo Possibly extract this information for the user manual.
usage() {
  cat << EOF
Usage:

  $(basename "${SCRIPT_NAME}") [<options>]

Start one or several X servers in the background for use with headless
rendering.  We only support X.Org Server at the moment.  Starting the X servers
is managed by dropping one or more files xorg.conf.<n> into a configuration
directory, by default ${DEFAULT_CONFIGURATION_FOLDER}, but this can be overridden
in the configuration file (see below).  We will attempt to start an X server
process for each configuration file using display number <n>.

Options:

  -c|--conf-file)        Specify an alternative locations for the configuration
                         file.  The default location is "${CONFIGURATION_FILE}".

  --help|--usage         Print this text.

The optional configuration file should contain a series of lines of the form
"KEY=value".  It will be read in as a command shell sub-script.  Here is the
current list of possible key settings with a short explanation.  Usually it
should be sufficient to change the value of \${HEADLESS_X_ORG_USERS} and to
leave all other settings unchanged.

  HEADLESS_X_ORG_CONFIGURATION_FOLDER
    The folder where the configuration files for the X servers are to be found.

  HEADLESS_X_ORG_LOG_FOLDER
    The folder where log files will be created.

  HEADLESS_X_ORG_LOG_FILE
    The main log file name.

  HEADLESS_X_ORG_RUN_FOLDER
    The folder to store run-time data in.

  HEADLESS_X_ORG_CHECK_PREREQUISITES
    Shell command to execute to check whether all dependencies for the X
    servers are available - usually a test for a device node.  This will be
    repeated at regular intervals until it returns successfully, so a command
    which can be executed internally be the shell (like "[") is preferable.
    The default command waits until the udev event queue has settled.

  HEADLESS_X_ORG_USERS
    List of users who will have access to the X servers started and for whom we
    will provide the configuration details via VirtualBox extra data.  This
    variable is only used by the commands in the default configuration
    (\${HEADLESS_X_ORG_SERVER_PRE_COMMAND} and
    \${HEADLESS_X_ORG_SERVER_POST_COMMAND}), and not by the service itself.

  HEADLESS_X_ORG_SERVER_PRE_COMMAND
    Command to execute once to perform any set-up needed before starting the
    X servers, such as setting up the X server authentication.  The default
    command creates an authority file for each of the users in the list
    \${HEADLESS_X_ORG_USERS}.

  HEADLESS_X_ORG_SERVER_COMMAND
    The default X server start-up command.  It will be passed three parameters
    - in order, the screen number to use, the path of the X.Org configuration
    file to use and the path of the X server log file to create.

  HEADLESS_X_ORG_SERVER_POST_COMMAND
    Command to execute once the X servers have been successfully started.  By
    default this stores the service configuration information to VirtualBox
    extra data for each of the users in the list \${HEADLESS_X_ORG_USERS}.
    It will be passed a single parameter which is a space-separated list of the
    X server screen numbers.
EOF
}

# Default configuration.
HEADLESS_X_ORG_CONFIGURATION_FOLDER="${DEFAULT_CONFIGURATION_FOLDER}"
HEADLESS_X_ORG_LOG_FOLDER="/var/log/${SERVICE_NAME}"
HEADLESS_X_ORG_LOG_FILE="${SERVICE_NAME}.log"
HEADLESS_X_ORG_RUN_FOLDER="/var/run/${SERVICE_NAME}"
HEADLESS_X_ORG_CHECK_PREREQUISITES="udevadm settle || ! udevadm -V"
HEADLESS_X_ORG_USERS=""

X_AUTH_FILE="${HEADLESS_X_ORG_RUN_FOLDER}/xauth"
default_pre_command()
{
  echo > "${X_AUTH_FILE}"
  key="$(dd if=/dev/urandom count=1 bs=16 2>/dev/null | od -An -x)"
  xauth -f "${X_AUTH_FILE}" add :0 . "${key}"
  for i in ${HEADLESS_X_ORG_USERS}; do
    cp "${X_AUTH_FILE}" "${X_AUTH_FILE}.${i}"
    chown "${i}" "${X_AUTH_FILE}.${i}"
  done
}
HEADLESS_X_ORG_SERVER_PRE_COMMAND="default_pre_command"

default_command()
{
  auth="${HEADLESS_X_ORG_RUN_FOLDER}/xauth"
  # screen=$1
  # conf_file=$2
  # log_file=$3
  Xorg :"${1}" -auth "${auth}" -config "${2}" -logverbose 0 -logfile /dev/null -verbose 7 > "${3}" 2>&1
}
HEADLESS_X_ORG_SERVER_COMMAND="default_command"

default_post_command()
{
  # screens=$1
  for i in ${HEADLESS_X_ORG_USERS}; do
    su ${i} -c "VBoxManage setextradata global HeadlessXServer/Screens \"${1}\""
    su ${i} -c "VBoxManage setextradata global HeadlessXServer/AuthFile \"${HEADLESS_X_ORG_RUN_FOLDER}/xauth\""
  done
}
HEADLESS_X_ORG_SERVER_POST_COMMAND="default_post_command"

## The function definition at the start of every non-trivial shell script!
abort() {
  ## $@, ... Error text to output to standard error in printf format.
  printf "$@" >&2
  exit 1
}

## Milder version of abort, when we can't continue because of a valid condition.
abandon() {
  ## $@, ... Text to output to standard error in printf format.
  printf "$@" >&2
  exit 0
}

abort_usage() {
  usage >&2
  abort "$@"
}

# Print a banner message
banner() {
  cat << EOF
${VBOX_PRODUCT} VBoxHeadless X Server start-up service Version ${VBOX_VERSION_STRING}
(C) 2005-${VBOX_C_YEAR} ${VBOX_VENDOR}
All rights reserved.

EOF
}

# Get the directory where the script is located.
VBOX_FOLDER="$(dirname "${SCRIPT_NAME}")"
VBOX_FOLDER=$(cd "${VBOX_FOLDER}" && pwd)
[ -d "${VBOX_FOLDER}" ] ||
  abort "Failed to change to directory ${VBOX_FOLDER}.\n"
# And change to the root directory so we don't hold any other open.
cd /

[ -r "${VBOX_FOLDER}/scripts/generated.sh" ] ||
  abort "${LOG_FILE}" "Failed to find installation information in ${VBOX_FOLDER}.\n"
. "${VBOX_FOLDER}/scripts/generated.sh"

# Parse our arguments.
while [ "$#" -gt 0 ]; do
  case $1 in
    -c|--conf-file)
      [ "$#" -gt 1 ] ||
      {
        banner
        abort "%s requires at least one argument.\n" "$1"
      }
      CONFIGURATION_FILE="$2"
      shift
      ;;
    --help|--usage)
      banner
      usage
      exit 0
      ;;
    *)
      banner
      abort_usage "Unknown argument $1.\n"
      ;;
  esac
  shift
done

[ -r "${CONFIGURATION_FILE}" ] && . "${CONFIGURATION_FILE}"

# If something fails here we will catch it when we create the directory.
[ -e "${HEADLESS_X_ORG_LOG_FOLDER}" ] &&
  [ -d "${HEADLESS_X_ORG_LOG_FOLDER}" ] &&
  rm -rf "${HEADLESS_X_ORG_LOG_FOLDER}.old" 2> /dev/null &&
mv "${HEADLESS_X_ORG_LOG_FOLDER}" "${HEADLESS_X_ORG_LOG_FOLDER}.old" 2> /dev/null
mkdir -p "${HEADLESS_X_ORG_LOG_FOLDER}" 2>/dev/null ||
{
  banner
  abort "Failed to create log folder \"${HEADLESS_X_ORG_LOG_FOLDER}\".\n"
}
mkdir -p "${HEADLESS_X_ORG_RUN_FOLDER}" 2>/dev/null ||
{
  banner
  abort "Failed to create run folder \"${HEADLESS_X_ORG_RUN_FOLDER}\".\n"
}
exec > "${HEADLESS_X_ORG_LOG_FOLDER}/${HEADLESS_X_ORG_LOG_FILE}" 2>&1

banner

# Wait for our dependencies to become available.  The increasing delay is
# probably not the cleverest way to do this.
DELAY=1
while ! eval ${HEADLESS_X_ORG_CHECK_PREREQUISITES}; do
  sleep $((${DELAY} / 10 + 1))
  DELAY=$((${DELAY} + 1))
done

# Do any pre-start setup.
eval "${HEADLESS_X_ORG_SERVER_PRE_COMMAND}"

X_SERVER_PIDS=""
X_SERVER_SCREENS=""
trap "kill \${X_SERVER_PIDS} 2>/dev/null" ${EXIT_SIGNALS}
space=""  # Hack to put spaces between the pids but not before or after.
for conf_file in "${HEADLESS_X_ORG_CONFIGURATION_FOLDER}"/*; do
  [ x"${conf_file}" = x"${HEADLESS_X_ORG_CONFIGURATION_FOLDER}/*" ] &&
    ! [ -e "${conf_file}" ] &&
    abort "No configuration files found.\n"
  filename="$(basename "${conf_file}")"
  screen="$(expr "${filename}" : "xorg\.conf\.\(.*\)")"
  [ 0 -le "${screen}" ] 2>/dev/null ||
    abort "Badly formed file name \"${conf_file}\".\n"
  log_file="${HEADLESS_X_ORG_LOG_FOLDER}/Xorg.${screen}.log"
  eval "${HEADLESS_X_ORG_SERVER_COMMAND} \"\${screen}\" \"\${conf_file}\" \"\${log_file}\"" "&"
  X_SERVER_PIDS="${X_SERVER_PIDS}${space}$!"
  X_SERVER_SCREENS="${X_SERVER_SCREENS}${space}${screen}"
  space=" "
done

# Do any post-start work.
eval "${HEADLESS_X_ORG_SERVER_POST_COMMAND} \"${X_SERVER_SCREENS}\""

wait
