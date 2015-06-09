#!/bin/sh

#
# Script to install services within a VirtualBox installation.
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

# Clean up before we start.
cr="
"
tab="   "
IFS=" ${cr}${tab}"
'unset' -f unalias
'unalias' -a 2>/dev/null
'unset' -f command
PATH=/bin:/sbin:/usr/bin:/usr/sbin:$PATH

# Get the folder we are running from, as we need other files there.
script_folder="`dirname "$0"`"

## Script usage documentation.
usage() {
  cat << EOF
Usage:

  `basename $0` --help|--enable|--disable|--force-enable|--force-disable
                      |--remove [--prefix <prefix>]
                      -- <pass-through parameters>

Create a system service which runs a command.  In order to make it possible to
do this in a simple and portable manner, we place a number of requirements on
the command to be run:
 - That it can be started safely even if all its dependencies are not started
   and will sleep if necessary until it can start work.  Ideally it should
   start accepting input as early as it can, but delay handling it if
   necessary, and delay accessing its dependencies until it actually needs
   them.
 - That it does not background to simplify service process management.
 - That it can be safely shut down using SIGTERM.
 - That if all running copies of the main process binary are stopped first the
   service can be re-started and will do any necessary clean-up automatically.
 - That any output which must not be lost go either to the system log or to the
   service's private log.

We currently support System V init only.  This will probably soon be extended
to BSD init, OpenRC and systemd, but probably not Upstart which currently
requires modifying init files to disable a service.  We also try to enable our
service (if requested) in all init systems we find, as we do not know which one
is in active use.  We assume that this will not have any adverse effects.

 --help|--usage
     Print this help text and exit.

  --enable|--disable|--force-enable|--force-disable
      These actions install the service.  If a version of the service was not
      installed previously, "--enable" and "--force-enable" make it start when
      entering normal user run-levels and "--disable" and "--force-disable"
      prevents it from starting when entering any run-level.  If a version of
      the service was already installed previously, "--enable" and "--disable"
      simply update it without changing when it starts; "--force-enable" and
      "--force-disable" behave the same as when no previous version was found.
      Only one of these options or "--remove" may be specified.

  --remove
      This action uninstalls the service.  It may not be used in combination
      with "--enable", "--disable", "--force-enable" or "--force-disable".

Option:

  --prefix <prefix>
      Treat all paths as relative to <prefix> rather than /etc.

Pass-through parameters will be passed through to the "generate_service_file"
tool.
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

ACTION=""
PREFIX="/etc/"
SERVICE_NAME=""

# Process arguments.
while test x"${1}" != "x--"; do
    case "${1}" in
    "--help"|"--usage")
        usage
        exit 0;;
    "--enable"|"--disable"|"--force-enable"|"--force-disable"|"--remove")
        test -z "${ACTION}" || abort "More than one action specified."
        ACTION="true"
        ENABLE=""
        INSTALL="true"
        UPDATE=""
        { test "${1}" = "--enable" || test "${1}" = "--disable"; } &&
            UPDATE="true"
        { test "${1}" = "--enable" || test "${1}" = "--force-enable"; } &&
            ENABLE="true"
        test "${1}" = "--remove" &&
            INSTALL=""
        shift;;
    "--prefix")
        test -z "${2}" && abort "${1}: missing argument."
        PREFIX="${2}"
        shift 2;;
    *)
        abort "Unknown option ${1}.";;
    esac
done
shift

# Check required options and set default values for others.
test -z "${ACTION}" &&
    abort "Please supply an install action."

# Get the service name.
SERVICE_NAME=`echo "%SERVICE_NAME%" |
    "${script_folder}/../helpers/generate_service_file" --format shell "${@}"`
test -z "${SERVICE_NAME}" &&
    abort "Please supply a command path."

# Keep track of whether we found at least one initialisation system.
found_init=""

# Find the best System V/BSD init path if any is present.
for path in "${PREFIX}/init.d/rc.d" "${PREFIX}/init.d/" "${PREFIX}/rc.d/init.d" "${PREFIX}/rc.d"; do
    if test -d "${path}"; then
        # Check permissions for the init path.
        test -w "${path}" || abort "No permission to write to \"${path}\"."
        # And for the System V symlink directories.
        for i in rc0.d rc1.d rc6.d rc.d/rc0.d rc.d/rc1.d rc.d/rc6.d; do
            if test -d "${PREFIX}/${i}"; then
                test -w "${PREFIX}/${i}" ||
                    abort "No permission to write to \"${PREFIX}/${i}\"."
            fi
        done
        # And for the OpenRC symlink directories.
        if test -d "${PREFIX}/runlevel/"; then
            test -w "${PREFIX}/runlevel/" ||
                abort "No permission to write to \"${PREFIX}/runlevel\"".
        fi
        found_init="true"
        update=""
        test -f "${path}/${SERVICE_NAME}" && update="${UPDATE}"
        if test -n "${INSTALL}"; then
            "${script_folder}/../helpers/generate_service_file" --format shell "${@}" < "${script_folder}/init_template.sh" > "${path}/${SERVICE_NAME}"
            chmod a+x "${path}/${SERVICE_NAME}"
        else
            rm "${path}/${SERVICE_NAME}"
        fi
        # Attempt to install using both system V symlinks and OpenRC, assuming
        # that both will not be in operation simultaneously (but may be
        # switchable).  BSD init expects the user to enable services
        # explicitly.
        if test -z "${update}"; then
            # Various known combinations of sysvinit rc directories.
            for i in "${PREFIX}"/rc*.d/[KS]??"${SERVICE_NAME}" "${PREFIX}"/rc.d/rc*.d/[KS]??"${SERVICE_NAME}"; do
                rm -f "${i}"
            done
            # And OpenRC.
            test -d "${PREFIX}/runlevel/" &&
                for i in "/${PREFIX}/runlevel"/*/"${SERVICE_NAME}"; do
                    rm -f "${i}"
                done
            # Various known combinations of sysvinit rc directories.
            if test -n "${ENABLE}"; then
                for i in rc0.d rc1.d rc6.d rc.d/rc0.d rc.d/rc1.d rc.d/rc6.d; do
                    if test -d "${PREFIX}/${i}"; then
                        # Paranoia test first.
                        test -d "${PREFIX}/${i}/K80${SERVICE_NAME}" ||
                            ln -sf "${path}/${SERVICE_NAME}" "${PREFIX}/${i}/K80${SERVICE_NAME}"
                    fi
                done
                for i in rc2.d rc3.d rc4.d rc5.d rc.d/rc2.d rc.d/rc3.d rc.d/rc4.d rc.d/rc5.d; do
                    if test -d "${PREFIX}/${i}"; then
                        # Paranoia test first.
                        test -d "${PREFIX}/${i}/S20${SERVICE_NAME}" ||
                            ln -sf "${path}/${SERVICE_NAME}" "${PREFIX}/${i}/S20${SERVICE_NAME}"
                    fi
                done
                # And OpenRC.
                test -d "${PREFIX}/runlevel/default" &&
                    ln -sf "${path}/${SERVICE_NAME}" "/${PREFIX}/runlevel/default/"
            fi
        fi
        break
    fi
done

test -z "${found_init}" &&
    abort "No supported initialisation system found."
exit 0
