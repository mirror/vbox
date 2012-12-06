#!/bin/sh

#
# Script to install services within a VirtualBox installation.
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

# Clean up before we start.
cr="
"
tab="	"
IFS=" ${cr}${tab}"
'unset' -f unalias
'unalias' -a 2>/dev/null
'unset' -f command
PATH=/bin:/sbin:/usr/bin:/usr/sbin:$PATH

## Script usage documentation.
## @todo generate_service_file could be called to print its own documentation.
usage() {
  cat << EOF
Usage:

  `basename $0` --help|--usage|<options>

Create a system service which runs a command.  In order to make it possible to
do this in a simple and portable manner, we place a number of requirements on
the command to be run:
 - That it can be started safely even if all its dependencies are not started
   and will sleep if necessary until it can start work.  Ideally it should start
   accepting input as early as it can, but delay handling it if necessary.
 - That it does not background to simplify service process management.
 - That it can be safely shut down using SIGTERM.
 - That if all running copies of the main process binary are stopped first the
   service can be re-started and will do any necessary clean-up automatically.
 - That any output which must not be lost go either to the system log or to the
   service's private log.

We currently support System V init only.  This will probably soon be extended
to BSD init, OpenRC, Pardus Comar and systemd, but probably not Upstart which
currently requires modifying init files to disable a service.  We also try to
enable our service (if requested) in all init systems we find, as we do not know
which one is in active use.  We assume that this will not have any adverse
effects.

 --help|--usage
     Print this help text and exit.

Required options:

  --command <command>
      The absolute path of the executable file to be started by the service.  No
      form of quoting should be used here.

  --description <description>
      A short description of the service which can also be used in sentences
      like "<description> failed to start", as a single parameter.  ASCII
      characters 0 to 31 and 127 should not be used.

Other options:

  --arguments <arguments>
      The arguments to pass to the executable file when it is started, as a
      single parameter. ASCII characters 0 to 31, "'" and 127 should be escaped
      in C string-style and spaces inside words should be preceeded by a back
      slash.  Some systemd-style % sequences may be added at a future time.

  --service-name <name>
      Specify the name of the service.  By default the base name without the
      extension of the command binary is used.  Only ASCII characters 33 to 126
      should be used.

  --enabled
      Enable the service in normal user run-levels by default.  If this option
      is not used the service will be disabled by default.  If a version of the
      service was already installed this option (or its absence) will be
      ignored unless the "--force" option is also specified.

  --force
      Respect the presence or absence of the "--enabled" flag even if a previous
      version of the service was already installed with a different enablement
      state.

  --prefix <prefix>
      Treat all paths as relative to <prefix> rather than /etc.
EOF
}

## The function definition at the start of every non-trivial shell script!
abort() {
    ## $1 Error text to output to standard error.
    cat >&2 << EOF
$1
EOF
    exit 1
}

enabled=""
force=""
PREFIX="/etc/"
ARGUMENTS=""
SERVICE_NAME=""

# Process arguments.
## @todo Pass more through unmodified to generate_service_file to reduce
#        duplication.  Or then again, maybe the hassle of perserving the
#        positional parameters is not worth it.
while test x"${#}" != "x0"; do
    case "${1}" in
    "--help|--usage")
        usage
        exit 0;;
    "--enabled")
        enabled=true
        shift;;
    "--force")
        force=true
        shift;;
    "--prefix")
        test -z "${2}" && abort "${1}: missing argument."
        PREFIX="${2}"
        shift 2;;
    "--command")
        test -z "${2}" && abort "${1}: missing argument."
        COMMAND="${2}"
        shift 2;;
    "--arguments")
        test -z "${2}" && abort "${1}: missing argument."
        ARGUMENTS="${2}"
        shift 2;;
    "--description")
        test -z "${2}" && abort "${1}: missing argument."
        DESCRIPTION="${2}"
        shift 2;;
    "--service-name")
        test -z "${2}" && abort "${1}: missing argument."
        SERVICE_NAME="${2}"
        shift 2;;
    *)
        abort "Unknown option ${1}.";;
    esac
done

# Check required options and set default values for others.
test -z "${COMMAND}" &&
    abort "Please supply a start command."
test -f "${COMMAND}" && test -x "${COMMAND}" ||
    abort "The start command must be an executable file."
case "${COMMAND}" in
    /*) ;;
    *) abort "The start command must have an absolute path." ;;
esac
test -z "${DESCRIPTION}" &&
    abort "Please supply a service description."
# Get the service name from the command path if not explicitly
# supplied.
test -z "${SERVICE_NAME}" &&
    SERVICE_NAME="`expr "${COMMAND}" : '.*/\(.*\)\..*'`"
test -z "${SERVICE_NAME}" &&
    SERVICE_NAME="`expr "${COMMAND}" : '.*/\(.*\)'`"

# Get the folder we are running from, as we need other files there.
script_folder="`dirname "$0"`"
script_folder="`cd "${script_folder}" && pwd`"
test -d "${script_folder}" ||
    abort "Failed to find the folder this command is running from."

# Keep track of whether we found at least one initialisation system.
found_init=""
# And whether we found a previous service script/file.
update=""

# Find the best System V/BSD init path if any is present.
for path in "${PREFIX}/init.d/rc.d" "${PREFIX}/init.d/" "${PREFIX}/rc.d/init.d" "${PREFIX}/rc.d"; do
    if test -d "${path}"; then
        found_init="true"
        test -f "${path}/${SERVICE_NAME}" && update="true"
        "${script_folder}/generate_service_file" --format shell --command "${COMMAND}" --arguments "${ARGUMENTS}" --description "${DESCRIPTION}" --service-name "${SERVICE_NAME}" < "${script_folder}/init_template.sh" > "${path}/${SERVICE_NAME}"
        chmod a+x "${path}/${SERVICE_NAME}"
        # Attempt to install using both system V symlinks and OpenRC, assuming
        # that both will not be in operation simultaneously (but may be
        # switchable).  BSD init expects the user to enable services explicitly.
        if test -z "${update}" || test -n "${force}"; then
            # Various known combinations of sysvinit rc directories.
            for i in ${PREFIX}/rc*.d/[KS]??"${SERVICE_NAME}" ${PREFIX}/rc.d/rc*.d/[KS]??"${SERVICE_NAME}"; do
                rm -f "$i"
            done
            # And OpenRC.
            type rc-update > /dev/null 2>&1 &&
                rc-update del "${1}" > /dev/null 2>&1
            # Various known combinations of sysvinit rc directories.
            if test -n "${enabled}"; then
                for i in rc0.d rc1.d rc6.d rc.d/rc0.d rc.d/rc1.d rc.d/rc6.d; do
                    if test -d "${PREFIX}/${i}"; then
                        # Paranoia test first.
                        test -d "${PREFIX}/${i}/K80${SERVICE_NAME}" ||
                            ln -sf "${path}/${SERVICE_NAME}" "${PREFIX}/${i}/K80${SERVICE_NAME}"
                    fi
                done
                for i in rc3.d rc4.d rc5.d rc.d/rc3.d rc.d/rc4.d rc.d/rc5.d; do
                    if test -d "${PREFIX}/${i}"; then
                        # Paranoia test first.
                        test -d "${PREFIX}/${i}/S20${SERVICE_NAME}" ||
                            ln -sf "${path}/${SERVICE_NAME}" "${PREFIX}/${i}/S20${SERVICE_NAME}"
                    fi
                done
                # And OpenRC.
                type rc-update > /dev/null 2>&1 &&
                    rc-update add "${1}" default > /dev/null 2>&1
            fi
        fi
        break
    fi
done

test -z "${found_init}" &&
    abort "No supported initialisation system found."
