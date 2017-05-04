#!/usr/bin/env bash
# $Id$
## @file
# VirtualBox Validation Kit - TestBoxScript Service Setup on Unixy platforms.
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


#
# !WARNING! Running the whole script in exit-on-failure mode.
#
# Note! Looking at the ash sources, it seems flags will be saved and restored
#       when calling functions.  That's comforting.
#
set -e
#set -x # debug only, disable!

##
# Get the host OS name, returning it in RETVAL.
#
get_host_os() {
    RETVAL=`uname`
    case "$RETVAL" in
        Darwin|darwin)
            RETVAL=darwin
            ;;

        DragonFly)
            RETVAL=dragonfly
            ;;

        freebsd|FreeBSD|FREEBSD)
            RETVAL=freebsd
            ;;

        Haiku)
            RETVAL=haiku
            ;;

        linux|Linux|GNU/Linux|LINUX)
            RETVAL=linux
            ;;

        netbsd|NetBSD|NETBSD)
            RETVAL=netbsd
            ;;

        openbsd|OpenBSD|OPENBSD)
            RETVAL=openbsd
            ;;

        os2|OS/2|OS2)
            RETVAL=os2
            ;;

        SunOS)
            RETVAL=solaris
            ;;

        WindowsNT|CYGWIN_NT-*)
            RETVAL=win
            ;;

        *)
            echo "$0: unknown os $RETVAL" 1>&2
            exit 1
            ;;
    esac
    return 0;
}

##
# Get the host OS/CPU arch, returning it in RETVAL.
#
get_host_arch() {
    if [ "${HOST_OS}" = "solaris" ]; then
        RETVAL=`isainfo | cut -f 1 -d ' '`
    else
        RETVAL=`uname -m`
    fi
    case "${RETVAL}" in
        amd64|AMD64|x86_64|k8|k8l|k9|k10)
            RETVAL='amd64'
            ;;
        x86|i86pc|ia32|i[3456789]86|BePC)
            RETVAL='x86'
            ;;
        sparc32|sparc|sparcv8|sparcv7|sparcv8e)
            RETVAL='sparc32'
            ;;
        sparc64|sparcv9)
            RETVAL='sparc64'
            ;;
        s390)
            RETVAL='s390'
            ;;
        s390x)
            RETVAL='s390x'
            ;;
        ppc32|ppc|powerpc)
            RETVAL='ppc32'
            ;;
        ppc64|powerpc64)
            RETVAL='ppc64'
            ;;
        mips32|mips)
            RETVAL='mips32'
            ;;
        mips64)
            RETVAL='mips64'
            ;;
        ia64)
            RETVAL='ia64'
            ;;
        hppa32|parisc32|parisc)
            RETVAL='hppa32'
            ;;
        hppa64|parisc64)
            RETVAL='hppa64'
            ;;
        arm|armv4l|armv5tel|armv5tejl)
            RETVAL='arm'
            ;;
        alpha)
            RETVAL='alpha'
            ;;

        *)
            echo "$0: unknown cpu/arch - $RETVAL" 1>&$2
            exit 1
            ;;
    esac
    return 0;
}


##
# Loads config values from the current installation.
#
os_load_config() {
    echo "os_load_config is not implemented" 2>&1
    exit 1
}

##
# Installs, configures and starts the service.
#
os_install_service() {
    echo "os_install_service is not implemented" 2>&1
    exit 1
}

##
# Enables (starts) the service.
os_enable_service() {
    echo "os_enable_service is not implemented" 2>&1
    return 0;
}

##
# Disables (stops) the service.
os_disable_service() {
    echo "os_disable_service is not implemented" 2>&1
    return 0;
}

##
# Adds the testbox user
#
os_add_user() {
    echo "os_add_user is not implemented" 2>&1
    exit 1
}

##
# Prints a final message after successful script execution.
# This can contain additional instructions which needs to be carried out
# manually or similar.
os_final_message() {
    return 0;
}

##
# Checks the installation, verifying that files are there and scripts work fine.
#
check_testboxscript_install() {

    # Presence
    test -r "${TESTBOXSCRIPT_DIR}/testboxscript/testboxscript.py"
    test -r "${TESTBOXSCRIPT_DIR}/testboxscript/testboxscript_real.py"
    test -r "${TESTBOXSCRIPT_DIR}/testboxscript/linux/testboxscript-service.sh" -o "${HOST_OS}" != "linux"
    test -r "${TESTBOXSCRIPT_DIR}/${HOST_OS}/${HOST_ARCH}/TestBoxHelper"

    # Zip file may be missing the x bits, so set them.
    chmod a+x \
        "${TESTBOXSCRIPT_DIR}/testboxscript/testboxscript.py" \
        "${TESTBOXSCRIPT_DIR}/testboxscript/testboxscript_real.py" \
        "${TESTBOXSCRIPT_DIR}/${HOST_OS}/${HOST_ARCH}/TestBoxHelper" \
        "${TESTBOXSCRIPT_DIR}/testboxscript/linux/testboxscript-service.sh"


    # Check that the scripts work.
    set +e
    "${TESTBOXSCRIPT_PYTHON}" "${TESTBOXSCRIPT_DIR}/testboxscript/testboxscript.py" --version > /dev/null
    if [ $? -ne 2 ]; then
        echo "$0: error: testboxscript.py didn't respons correctly to the --version option."
        exit 1;
    fi

    "${TESTBOXSCRIPT_PYTHON}" "${TESTBOXSCRIPT_DIR}/testboxscript/testboxscript_real.py" --version > /dev/null
    if [ $? -ne 2 ]; then
        echo "$0: error: testboxscript.py didn't respons correctly to the --version option."
        exit 1;
    fi
    set -e

    return 0;
}

##
# Check that sudo is installed.
#
check_for_sudo() {
    which sudo
    test -f "${MY_ETC_SUDOERS}"
}

##
# Check that sudo is installed.
#
check_for_cifs() {
    return 0;
}

##
# Checks if the testboxscript_user exists.
does_testboxscript_user_exist() {
    id "${TESTBOXSCRIPT_USER}" > /dev/null 2>&1
    return $?;
}

##
# hushes up the root login.
maybe_hush_up_root_login() {
    # This is a solaris hook.
    return 0;
}

##
# Adds the testbox user and make sure it has unrestricted sudo access.
maybe_add_testboxscript_user() {
    if ! does_testboxscript_user_exist; then
        os_add_user "${TESTBOXSCRIPT_USER}"
    fi

    SUDOERS_LINE="${TESTBOXSCRIPT_USER} ALL=(ALL) NOPASSWD: ALL"
    if ! ${MY_FGREP} -q "${SUDOERS_LINE}" ${MY_ETC_SUDOERS}; then
        echo "# begin tinderboxscript setup.sh" >> ${MY_ETC_SUDOERS}
        echo "${SUDOERS_LINE}"                  >> ${MY_ETC_SUDOERS}
        echo "# end tinderboxscript setup.sh"   >> ${MY_ETC_SUDOERS}
    fi

    maybe_hush_up_root_login;
}


##
# Test the user.
#
test_user() {
    su - "${TESTBOXSCRIPT_USER}" -c "true"

    # sudo 1.7.0 adds the -n option.
    MY_TMP="`sudo -V 2>&1 | head -1 | sed -e 's/^.*version 1\.[6543210]\..*$/old/'`"
    if [ "${MY_TMP}" != "old" ]; then
        echo "Warning: If sudo starts complaining about not having a tty,"
        echo "         disable the requiretty option in /etc/sudoers."
        su - "${TESTBOXSCRIPT_USER}" -c "sudo -n -i true"
    else
        echo "Warning: You've got an old sudo installed. If it starts"
        echo "         complaining about not having a tty, disable the"
        echo "         requiretty option in /etc/sudoers."
        su - "${TESTBOXSCRIPT_USER}" -c "sudo true"
    fi
}

##
# Test if core dumps are enabled. See https://wiki.ubuntu.com/Apport!
#
test_coredumps() {
    # This is a linux hook.
    return 0;
}


##
# Grants the user write access to the testboxscript files so it can perform
# upgrades.
#
grant_user_testboxscript_write_access() {
    chown -R "${TESTBOXSCRIPT_USER}" "${TESTBOXSCRIPT_DIR}"
}

##
# Check the proxy setup.
#
check_proxy_config() {
    if [ -n "${http_proxy}"  -o  -n "${ftp_proxy}" ]; then
        if [ -z "${no_proxy}" ]; then
            echo "Error: Env.vars. http_proxy/ftp_proxy without no_proxy is going to break upgrade among other things."
            exit 1
        fi
    fi
}


#
#
# main()
#
#


#
# Get our bearings and include the host specific code.
#
MY_ETC_SUDOERS="/etc/sudoers"
MY_FGREP=fgrep
DIR=`dirname "$0"`
DIR=`cd "${DIR}"; /bin/pwd`

get_host_os
HOST_OS=${RETVAL}
get_host_arch
HOST_ARCH=${RETVAL}

. "${DIR}/${HOST_OS}/setup-routines.sh"


#
# Config.
#
TESTBOXSCRIPT_PYTHON=""
TESTBOXSCRIPT_USER=""
TESTBOXSCRIPT_HWVIRT=""
TESTBOXSCRIPT_IOMMU=""
TESTBOXSCRIPT_NESTED_PAGING=""
TESTBOXSCRIPT_SYSTEM_UUID=""
TESTBOXSCRIPT_PATH_TESTRSRC=""
TESTBOXSCRIPT_TEST_MANAGER=""
TESTBOXSCRIPT_SCRATCH_ROOT=""
TESTBOXSCRIPT_BUILDS_PATH=""
TESTBOXSCRIPT_BUILDS_TYPE="cifs"
TESTBOXSCRIPT_BUILDS_NAME="vboxstor.de.oracle.com"
TESTBOXSCRIPT_BUILDS_SHARE="builds"
TESTBOXSCRIPT_BUILDS_USER="guestr"
TESTBOXSCRIPT_BUILDS_PASSWD="guestr"
TESTBOXSCRIPT_TESTRSRC_PATH=""
TESTBOXSCRIPT_TESTRSRC_TYPE="cifs"
TESTBOXSCRIPT_TESTRSRC_NAME="teststor.de.oracle.com"
TESTBOXSCRIPT_TESTRSRC_SHARE="testrsrc"
TESTBOXSCRIPT_TESTRSRC_USER="guestr"
TESTBOXSCRIPT_TESTRSRC_PASSWD="guestr"
declare -a TESTBOXSCRIPT_ENVVARS

# Load old config values (platform specific).
os_load_config

# Set defaults.
if [ -z "${TESTBOXSCRIPT_USER}" ]; then
    TESTBOXSCRIPT_USER=vbox;
fi;
TESTBOXSCRIPT_DIR=`dirname "${DIR}"`


#
# Parse arguments.
#
while test $# -gt 0;
do
    case "$1" in
        -h|--help)
            echo "TestBox Script setup utility."
            echo "";
            echo "Usage: setup.sh [options]";
            echo "";
            echo "Options:";
            echo "    Later...";
            exit 0;
            ;;
        -V|--version)
            echo '$Revision$'
            exit 0;
            ;;

        --python)                   TESTBOXSCRIPT_PYTHON="$2"; shift;;
        --test-manager)             TESTBOXSCRIPT_TEST_MANAGER="$2"; shift;;
        --scratch-root)             TESTBOXSCRIPT_SCRATCH_ROOT="$2"; shift;;
        --system-uuid)              TESTBOXSCRIPT_SYSTEM_UUID="$2"; shift;;
        --hwvirt)                   TESTBOXSCRIPT_HWVIRT="yes";;
        --no-hwvirt)                TESTBOXSCRIPT_HWVIRT="no";;
        --nested-paging)            TESTBOXSCRIPT_NESTED_PAGING="yes";;
        --no-nested-paging)         TESTBOXSCRIPT_NESTED_PAGING="no";;
        --io-mmu)                   TESTBOXSCRIPT_IOMMU="yes";;
        --no-io-mmu)                TESTBOXSCRIPT_IOMMU="no";;
        --builds-path)              TESTBOXSCRIPT_BUILDS_PATH="$2"; shift;;
        --builds-server-type)       TESTBOXSCRIPT_BUILDS_TYPE="$2"; shift;;
        --builds-server-name)       TESTBOXSCRIPT_BUILDS_NAME="$2"; shift;;
        --builds-server-share)      TESTBOXSCRIPT_BUILDS_SHARE="$2"; shift;;
        --builds-server-user)       TESTBOXSCRIPT_BUILDS_USER="$2"; shift;;
        --builds-server-passwd)     TESTBOXSCRIPT_BUILDS_PASSWD="$2"; shift;;
        --testrsrc-path)            TESTBOXSCRIPT_TESTRSRC_PATH="$2"; shift;;
        --testrsrc-server-type)     TESTBOXSCRIPT_TESTRSRC_TYPE="$2"; shift;;
        --testrsrc-server-name)     TESTBOXSCRIPT_TESTRSRC_NAME="$2"; shift;;
        --testrsrc-server-share)    TESTBOXSCRIPT_TESTRSRC_SHARE="$2"; shift;;
        --testrsrc-server-user)     TESTBOXSCRIPT_TESTRSRC_USER="$2"; shift;;
        --testrsrc-server-passwd)   TESTBOXSCRIPT_TESTRSRC_PASSWD="$2"; shift;;
        *)
            echo 'Syntax error: Unknown option:' "$1" >&2;
            exit 1;
            ;;
    esac
    shift;
done


#
# Find usable python if not already specified.
#
if [ -z "${TESTBOXSCRIPT_PYTHON}" ]; then
    set +e
    MY_PYTHON_VER_TEST="\
import sys;\
x = sys.version_info[0] == 2 and (sys.version_info[1] >= 6 or (sys.version_info[1] == 5 and sys.version_info[2] >= 1));\
sys.exit(not x);\
";
    for python in python2.7 python2.6 python2.5 python;
    do
        python=`which ${python} 2> /dev/null`
        if [ -n "${python}" -a -x "${python}" ]; then
            if ${python} -c "${MY_PYTHON_VER_TEST}"; then
                TESTBOXSCRIPT_PYTHON="${python}";
                break;
            fi
        fi
    done
    set -e
    test -n "${TESTBOXSCRIPT_PYTHON}";
fi


#
# Do the job
#
set -e
check_testboxscript_install;
check_for_sudo;
check_for_cifs;
check_proxy_config;

maybe_add_testboxscript_user;
test_user;
test_coredumps;

grant_user_testboxscript_write_access;

os_disable_service;
os_install_service;
os_enable_service;

#
# That's all folks.
#
echo "done"
os_final_message;
