#!/bin/sh
#
# Oracle VM VirtualBox
# VirtualBox linux installation script

#
# Copyright (C) 2007-2015 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

# This script tests whether a Linux system is set up to build kernel modules,
# and if not attempts to discover and optionally perform the necessary steps to
# set up the system correctly, based on knowledge of popular Linux
# distributions.  It is written for use with the VirtualBox and the VirtualBox
# Guest Additions kernel modules but should be useful for any package which
# needs to build kernel modules on an unknown target system.  While patches to
# extend the range of distributions supported are welcome, please bear in mind
# that we try to keep distribution-specific parts as small as possible, and to
# cover as many distributions as possible in single "families".  Any
# distribution which requires specific code to support should be sufficiently
# widely used to justify the addition.

PATH=$PATH:/bin:/sbin:/usr/sbin

## @todo include routines.sh once it has been cleaned up

## Set all commands used in the script to their normal values.  These are the
# script's interface to the outside world and should be overridden for self
# testing.
set_up_helpers()
{
    exit=exit
    get_kernel_version="uname -r"
    file_exists="test -f"
    string_in_file="grep -q -s"
    do_type=type
    find_rpm_for_file="rpm -q --whatprovides"
    find_deb_for_file="dpkg -S"
}


## uname -r stand-in for self testing
test_get_kernel_version()
{
    echo "$test_uname_r";
}


## test -f $1 or grep $1 $2 stand-in for self testing
test_file_exists()
{
    test '(' -z "$2" -a "$test_test_f" = "$1" ')' -o \
         '(' "$test_grep_string" = "$1" -a "$test_grep_file" = "$2" ')';
}


## rpm -q --whatprovides stand-in for self testing
test_find_rpm_for_file()
{
    echo "$test_rpm_q";
}


## exit stand-in for self testing
test_exit()
{
    test_exit_return="$1";
}


## dpkg -S stand-in for self testing
test_find_deb_for_file()
{
    echo "$test_dpkg_s";
}


## type stand-in for self testing
test_type()
{
    test "$1" = "$test_type_binary" && echo "$test_type_binary";
}


## Evaluate a line describing the results of a self test.  Outputs a line of
#  text describing each failure.
eval_test_line()
{
    ## @param test_line      The test description line to evaluate in the
    #                        format:
    # "<line num>: <variable to set>=<value> ...=><variable to check>=<expected value>..."
    #                        All output variables will be nulled first.  Unused
    #                        "variables to set" should be explicitly set to
    #                        null in the list.  Spaces in variable names are
    #                        not allowed, in values they can be escaped with
    #                        backslash, and "=>" can be embedded as "=\>".
    test_line="$1"
    ## @param test_function  The function to call after setting the variables
    #                        from the test line
    test_function="$2"

    line_num=${test_line%%:*=>*}
    env_all=${test_line#*:}
    env_in=${env_all%%=>*}
    env_out=${env_all#*=>}
    # Blank out output variables first.
    eval set $env_out  # Trick for dealing with escaped embedded spaces.
    for j in "$@"; do
        var_name="${j%%=*}"
        eval $var_name=
    done
    # Set input variables.
    eval set $env_in
    for j in "$@"; do
        var_name=${j%%=*}
        var_value="${j#*=}"
        eval $var_name=\"$var_value\"  # More tricks
    done
    eval $test_function
    eval set $env_out
    for j in "$@"; do
        var_name=${j%%=*}
        var_value="$(eval echo \$$var_name)"
        var_expected="${j#*=}"
        case $var_value in
        $var_expected) ;;
        *)
            echo "$test_function: failure: $var_name=$var_value in line $line_num"
            ;;
        esac
    done
}


## List of tests to be executed in a test run
self_tests=


## Aborts the script and prints a printf-type error message to stderr.
abort()
{
    ## Message to print to stderr before aborting.
    message="$1"

    echo 1>&2 "$message"
    echo 1>&2 "Unable to continue."
    $exit 1
}


## Abort if we are not running as root
check_root() {
    case $(id -u) in 0) ;; *)
        abort "This program must be run with administrator privileges."
    esac
}


## Get information about the kernel running on the system (version and path
# information).
# @param KERN_VER       [out]  Full kernel version (e.g. 3.0.0-15-generic).
# @param KERN_VER_BASE  [out]  Base kernel version (e.g. 3.0.0).
# @param KERN_VER_SHORT [out]  Kernel major and minor version (e.g. 3.0).
# @param KERN_VER_EXTRA [out]  Kernel version "extra" information (e.g.
#                              -15-generic).
# @param KERN_DIR       [out]  The directory where kernel modules and headers
#                              live (e.g. /lib/modules/3.0.0-15-generic)
get_kernel_information()
{
    KERN_VER=$($get_kernel_version 2>/dev/null || abort "'uname' tool not found.")
    kern_ver_rest=${KERN_VER#*.*.}
    kern_ver_micro=${kern_ver_rest%%[!0-9]*}
    KERN_VER_EXTRA=${kern_ver_rest#$kern_ver_micro}
    KERN_VER_BASE=${KERN_VER%$KERN_VER_EXTRA}
    KERN_VER_SHORT=${KERN_VER_BASE%.*}
    KERN_DIR="/lib/modules/$KERN_VER"
}


## Self test for the "get_kernel_information" function.  Outputs a line of
#  text describing each failure.
test_get_kernel_information()
{
    get_kernel_version=test_get_kernel_version
    for i in \
        "1: test_uname_r=3.0.0-15-generic=>KERN_VER=3.0.0-15-generic KERN_VER_BASE=3.0.0 KERN_VER_SHORT=3.0 KERN_VER_EXTRA=-15-generic KERN_DIR=/lib/modules/3.0.0-15-generic" \
        "2: test_uname_r=2.4.21-50.EL=>KERN_VER=2.4.21-50.EL KERN_VER_BASE=2.4.21 KERN_VER_SHORT=2.4 KERN_VER_EXTRA=-50.EL KERN_DIR=/lib/modules/2.4.21-50.EL"
    do eval_test_line "$i" get_kernel_information; done
}

self_tests="$self_tests test_get_kernel_information"


## Test whether a three-digit kernel version number is less than a second one.
kernel_version_lt()
{
    major1=${1%.*.*}
    major2=${2%.*.*}
    short1=${1%.*}
    short2=${2%.*}
    minor1=${short1#*.}
    minor2=${short2#*.}
    micro1=${1#*.*.}
    micro2=${2#*.*.}
    test $major1 -lt $major2 -o \
      '(' $major1 -eq $major2 -a $minor1 -lt $minor2 ')' -o \
      '(' $major1 -eq $major2 -a $minor1 -eq $minor2 -a $micro1 -lt $micro2 ')'
}


## Self test for the "kernel_version_lt" function.  Outputs a line of
#  text describing each failure.
test_kernel_version_lt()
{
    for i in \
        "1: ver1=2.4.6 ver2=3.4.6 =>lt=true" \
        "2: ver1=3.4.6 ver2=3.4.6 =>lt=" \
        "3: ver1=2.4.6 ver2=2.4.7 =>lt=true" \
        "4: ver1=2.4.60 ver2=2.4.7 =>lt=" \
        "5: ver1=2.31.7 ver2=2.4.7 =>lt="
    do eval_test_line "$i" "kernel_version_lt \$ver1 \$ver2 && lt=true"; done
}

self_tests="$self_tests test_kernel_version_lt"


# Some SUSE systems match three packages to the kernel directory.  Selecting
# the first is not quite right, but doesn't matter on those systems.
FIND_KERNEL_RPM='rpm_q_out=$(echo $($find_rpm_for_file "$KERN_DIR/kernel")); KERN_PACKAGE=${rpm_q_out%%\ *}'
FIND_KERNEL_DEB='dpkg_s_out=$($find_deb_for_file "$KERN_DIR/kernel"); KERN_PACKAGE=${dpkg_s_out%\: $KERN_DIR/kernel}'


## Determine the command patterns needed to install gcc, make and the kernel
#  headers on the current distribution.
#  @note  We want to support the distributions we do with the least possible
#         special logic, so please do not add new code if existing code can
#         catch a new distribution (e.g. it is similar to another).
#  @note  This function is very long, but contains nearly all the distribution-
#         specific logic in the file.
#  @param KERN_VER_BASE               The three-point kernel version.
#  @param GET_KERN_PACKAGE      [out] Command to find the kernel package name
#                                     given the path $KERN_DIR.
#  @param PATTERN_GCC_MAKE      [out] A pattern to be resolved to get the
#                                     command to install gcc and make.
#  @param PATTERN_HEADERS       [out] A pattern to be resolved to get the
#                                     command to install the kernel headers for
#                                     the currently running kernel.
#  @param PATTERN_HEADERS_META  [out] A pattern to be resolved to get the
#                                     command to install a dependency on the
#                                     current kernel headers.  May be empty.
#  @note These patterns may all depend on $KERN_VER (uname -r output),
#        $KERN_PACKAGE_BASE (distribution kernel package name without any
#        version information at all), $KERN_DEBIAN_SUFFIX (the Debian-style
#        "-generic", "-586" etc kernel name suffix) and $KERN_RPM_SUFFIX
#        (the kernel-version-plus-rpm-version for the current kernel RPM).
get_system_information()
{
    # SUSE and Man*/Mageia must come before Redhat.
    if $file_exists /etc/SuSE-release; then  # SUSE-based, before Red Hat
        GET_KERN_PACKAGE="$FIND_KERNEL_RPM"
        PATTERN_GCC_MAKE='zypper\ install\ gcc\ make'
        if kernel_version_lt "$KERN_VER_BASE" '2.6.30'; then
            PATTERN_HEADERS='zypper\ install\ kernel-source-$KERN_RPM_SUFFIX\ kernel-syms-$KERN_RPM_SUFFIX'
            PATTERN_HEADERS_META='zypper\ install\ kernel-source\ kernel-syms'
        else
            PATTERN_HEADERS='zypper\ install\ ${KERN_PACKAGE_BASE}devel-$KERN_RPM_SUFFIX'
            PATTERN_HEADERS_META='zypper\ install\ ${KERN_PACKAGE_BASE}devel'
        fi
    elif $file_exists /etc/pclinuxos-release; then  # PCLinuxOS - before Mandrake
        GET_KERN_PACKAGE=
        PATTERN_GCC_MAKE=
        PATTERN_HEADERS=
        PATTERN_HEADERS_META=
    elif $file_exists /etc/mandrake-release; then  # Mandrake family
        GET_KERN_PACKAGE="$FIND_KERNEL_RPM"
        PATTERN_GCC_MAKE='urpmi\ gcc\ make'
        if kernel_version_lt "$KERN_VER_BASE" '2.6.0'; then
            PATTERN_HEADERS='urpmi\ kernel-source-$KERN_VER'
            PATTERN_HEADERS_META='urpmi\ kernel-source'
        elif kernel_version_lt "$KERN_VER_BASE" '2.6.8'; then
            PATTERN_HEADERS='urpmi\ kernel-source-stripped-$KERN_VER'
            PATTERN_HEADERS_META='urpmi\ kernel-source-stripped'
        # Mandrake/Mandriva had a funny naming scheme for a few kernel versions
        elif kernel_version_lt "$KERN_VER_BASE" '2.6.17'; then
            PATTERN_HEADERS='urpmi\ kernel-source-stripped-2.6-${KERN_VER%-*}'
            PATTERN_HEADERS_META='urpmi\ kernel-source-stripped-2.6'
        elif kernel_version_lt "$KERN_VER_BASE" '2.6.22'; then
            PATTERN_HEADERS='urpmi\ kernel-source-stripped-$KERN_RPM_SUFFIX'
            PATTERN_HEADERS_META='urpmi\ kernel-source-stripped-latest'
        else
            PATTERN_HEADERS='urpmi\ ${KERN_PACKAGE_BASE}devel-$KERN_RPM_SUFFIX'
            PATTERN_HEADERS_META='urpmi\ ${KERN_PACKAGE_BASE}devel-latest'
        fi
    elif $file_exists /etc/redhat-release; then  # Red Hat family
        GET_KERN_PACKAGE="$FIND_KERNEL_RPM"
        if $do_type dns >/dev/null 2>&1; then
            PACKAGE_INSTALLER="dns install"
        elif $do_type yum >/dev/null 2>&1; then
            PACKAGE_INSTALLER="yum install"
        else
            PACKAGE_INSTALLER=up2date
        fi
        PATTERN_GCC_MAKE='$PACKAGE_INSTALLER\ gcc\ make'
        if kernel_version_lt "$KERN_VER_BASE" '2.6.9'; then
            PATTERN_HEADERS='$PACKAGE_INSTALLER\ kernel-source-$KERN_RPM_SUFFIX'
            PATTERN_HEADERS_META='$PACKAGE_INSTALLER\ kernel-source'
        else
            PATTERN_HEADERS='$PACKAGE_INSTALLER\ ${KERN_PACKAGE_BASE}devel-$KERN_RPM_SUFFIX'
            PATTERN_HEADERS_META='$PACKAGE_INSTALLER\ ${KERN_PACKAGE_BASE}devel'
        fi
    # Ubuntu must come before Debian.
    elif $string_in_file Ubuntu /etc/lsb-release; then  # Ubuntu before Debian
        GET_KERN_PACKAGE="$FIND_KERNEL_DEB"
        PATTERN_GCC_MAKE='apt-get\ install\ gcc\ make'
        if kernel_version_lt "$KERN_VER_BASE" '2.6.9'; then
            PATTERN_HEADERS='apt-get\ install\ ${KERN_PACKAGE_BASE%-image*}-headers-$KERN_VER_SHORT$KERN_VER'
            PATTERN_HEADERS_META='apt-get\ install\ ${KERN_PACKAGE_BASE%-image*}-headers-$KERN_VER_SHORT$KERN_DEBIAN_SUFFIX'
        else
            PATTERN_HEADERS='apt-get\ install\ ${KERN_PACKAGE_BASE%-image*}-headers-$KERN_VER'
            PATTERN_HEADERS_META='apt-get\ install\ ${KERN_PACKAGE_BASE%-image*}-headers$KERN_DEBIAN_SUFFIX'
        fi
    elif $file_exists /etc/debian_version; then  # Debian family
        GET_KERN_PACKAGE="$FIND_KERNEL_DEB"
        PATTERN_GCC_MAKE='apt-get\ install\ gcc\ make'
        if kernel_version_lt "$KERN_VER_BASE" '2.4.17'; then
            PATTERN_HEADERS='apt-get\ install\ ${KERN_PACKAGE_BASE%-image*}-headers-$KERN_VER'
            PATTERN_HEADERS_META=
        elif kernel_version_lt "$KERN_VER_BASE" '3.0.0'; then
            PATTERN_HEADERS='apt-get\ install\ ${KERN_PACKAGE_BASE%-image*}-headers-$KERN_VER'
            PATTERN_HEADERS_META='apt-get\ install\ ${KERN_PACKAGE_BASE%-image*}-headers-$KERN_VER_SHORT$KERN_DEBIAN_SUFFIX'
        else
            PATTERN_HEADERS='apt-get\ install\ ${KERN_PACKAGE_BASE%-image*}-headers-$KERN_VER'
            PATTERN_HEADERS_META='apt-get\ install\ ${KERN_PACKAGE_BASE%-image*}-headers$KERN_DEBIAN_SUFFIX'
        fi
    elif $file_exists /etc/gentoo-release; then  # Gentoo
        GET_KERN_PACKAGE=
        PATTERN_GCC_MAKE=
        PATTERN_HEADERS=
        PATTERN_HEADERS_META=
    elif $file_exists /etc/lfs-release -a -d /etc/rc.d/init.d; then  # LFS
        GET_KERN_PACKAGE=
        PATTERN_GCC_MAKE=
        PATTERN_HEADERS=
        PATTERN_HEADERS_META=
    elif $file_exists /etc/slackware-version; then  # Slackware
        GET_KERN_PACKAGE=
        PATTERN_GCC_MAKE=
        PATTERN_HEADERS=
        PATTERN_HEADERS_META=
    elif $file_exists /etc/arch-release; then  # Arch Linux
        GET_KERN_PACKAGE=
        PATTERN_GCC_MAKE=
        PATTERN_HEADERS=
        PATTERN_HEADERS_META=
    else
        echo "Linux distribution base type not recognised."
        exit 0
    fi
}


## Determine the commands needed to install gcc, make and the kernel headers on
#  the current system.
#  @param INSTALL_GCC_MAKE      [out] Command to install gcc and make.
#  @param INSTALL_HEADERS       [out] Command to install gcc and make.
#  @param INSTALL_HEADERS_META  [out] Command to install a dependency on the
#                                     current kernel headers.  May be empty.
generate_install_commands()
{
    get_kernel_information
    get_system_information
    case "$GET_KERN_PACKAGE" in
    ?*)
        eval $GET_KERN_PACKAGE
        ;;
    "")
        echo "Unable to determine the software packaging system in use."
        exit 0
        ;;
    esac
    # Needed for many installers
    KERN_PACKAGE_BASE="${KERN_PACKAGE%%$KERN_VER_BASE*}"
    KERN_RPM_SUFFIX="${KERN_PACKAGE#"$KERN_PACKAGE_BASE"}"
    KERN_DEBIAN_SUFFIX="$(expr "$KERN_VER_EXTRA" : '-[^-]*\(.*\)')"
    INSTALL_GCC_MAKE=$(eval echo "$PATTERN_GCC_MAKE")
    INSTALL_HEADERS=$(eval echo "$PATTERN_HEADERS")
    INSTALL_HEADERS_META=$(eval echo "$PATTERN_HEADERS_META")
}


test_generate_install_commands()
{
    get_kernel_version=test_get_kernel_version
    file_exists=test_file_exists
    string_in_file=test_file_exists
    exit=test_exit
    do_type=test_type
    find_rpm_for_file=test_find_rpm_for_file
    find_deb_for_file=test_find_deb_for_file
    for i in \
        "1: test_uname_r=2.4.21-50.EL test_test_f=/etc/redhat-release test_type_binary=yum test_rpm_q=kernel-2.4.21-50.EL test_dpkg_s= =>INSTALL_GCC_MAKE=yum\ install\ gcc\ make INSTALL_HEADERS=yum\ install\ kernel-source-2.4.21-50.EL INSTALL_HEADERS_META=yum\ install\ kernel-source" \
        "2: test_uname_r=2.6.34-12-default test_test_f=/etc/SuSE-release test_rpm_q=kernel-default-2.6.34-12.3 =>INSTALL_GCC_MAKE=zypper\ install\ gcc\ make INSTALL_HEADERS=zypper\ install\ kernel-default-devel-2.6.34-12.3 INSTALL_HEADERS_META=zypper\ install\ kernel-default-devel" \
        "3: test_uname_r=2.6.8.1-12mdkenterprise test_test_f=/etc/mandrake-release test_rpm_q=kernel-enterprise-2.6.8.1.12mdk-1-1mdk test_dpkg_s= =>INSTALL_GCC_MAKE=urpmi\ gcc\ make INSTALL_HEADERS=urpmi\ kernel-source-stripped-2.6-2.6.8.1 INSTALL_HEADERS_META=urpmi\ kernel-source-stripped-2.6" \
        "4: test_uname_r=2.6.26-2-686 test_test_f=/etc/debian_version test_rpm_q= test_dpkg_s=linux-image-2.6.26-2-686:\ /lib/modules/2.6.26-2-686/kernel =>INSTALL_GCC_MAKE=apt-get\ install\ gcc\ make INSTALL_HEADERS=apt-get\ install\ linux-headers-2.6.26-2-686 INSTALL_HEADERS_META=apt-get\ install\ linux-headers-2.6-686" \
        "5: test_uname_r=3.0.0-15-generic test_string_grep=Ubuntu test_string_file=/etc/lsb-release test_rpm_q= test_dpkg_s=linux-image-3.0.0-15-generic:\ /lib/modules/3.0.0-15-generic/kernel =>INSTALL_GCC_MAKE=apt-get\ install\ gcc\ make INSTALL_HEADERS=apt-get\ install\ linux-headers-3.0.0-15-generic INSTALL_HEADERS_META=apt-get\ install\ linux-headers-generic" \
        "6: test_uname_r=2.4.16-686-smp test_test_f=/etc/debian_version test_rpm_q= test_dpkg_s=kernel-image-2.4.16-686-smp =>INSTALL_GCC_MAKE=apt-get\ install\ gcc\ make INSTALL_HEADERS=apt-get\ install\ kernel-headers-2.4.16-686-smp INSTALL_HEADERS_META=" \
        "7: test_uname_r=2.6.38.8-desktop-9.mga test_test_f=/etc/mandrake-release test_rpm_q=kernel-desktop-2.6.38.8-9.mga-1-1.mga1 test_dpkg_s= =>INSTALL_GCC_MAKE=urpmi\ gcc\ make INSTALL_HEADERS=urpmi\ kernel-desktop-devel-2.6.38.8-9.mga-1-1.mga1 INSTALL_HEADERS_META=urpmi\ kernel-desktop-devel-latest" \
        "8: test_uname_r=2.6.17-13mdv test_test_f=/etc/mandrake-release test_rpm_q=kernel-2.6.17.13mdv-1-1mdv2007.1 test_dpkg_s= =>INSTALL_GCC_MAKE=urpmi\ gcc\ make INSTALL_HEADERS=urpmi\ kernel-source-stripped-2.6.17.13mdv-1-1mdv2007.1 INSTALL_HEADERS_META=urpmi\ kernel-source-stripped-latest" \
        "9: test_uname_r=2.6.27.7-9-default test_test_f=/etc/SuSE-release test_rpm_q=kernel-default-base-2.6.27.7-9.1\ kernel-default-2.6.27.7-9.1\ kernel-default-extra-2.6.27.7-9.1 test_dpkg_s= =>INSTALL_GCC_MAKE=zypper\ install\ gcc\ make INSTALL_HEADERS=zypper\ install\ kernel-source-2.6.27.7-9.1\ kernel-syms-2.6.27.7-9.1 INSTALL_HEADERS_META=zypper\ install\ kernel-source\ kernel-syms" \
        "10: test_exit_return= test_test_f=/etc/Windows-release =>test_exit_return=1"
    do eval_test_line "$i" "generate_install_commands 2> /dev/null"; done
}

self_tests="$self_tests test_generate_install_commands"


## Check whether gcc, make and the kernel headers are installed on the system.
# @param MISSING_GCC     [out]  non-empty if make needs to be installed.
#                               @todo Should we check the version of gcc?
# @param MISSING_MAKE    [out]  non-empty if make needs to be installed.
# @param MISSING_HEADERS [out]  non-empty if kernel headers need to be
#                               installed.
check_missing_packages()
{
    if $do_type gcc >/dev/null 2>&1; then
        MISSING_GCC=
    else
        MISSING_GCC=gcc
    fi
    if $do_type make >/dev/null 2>&1; then
        MISSING_MAKE=
    else
        MISSING_MAKE=make
    fi
    # The following test looks valid on all distributions I have checked so
    # far.
    MISSING_HEADERS=$($file_exists "$KERN_DIR"/build/Makefile || echo headers)
}


case "$1" in livetest) LIVE_TEST=true ;; *) LIVE_TEST= ;; esac
case "$1" in
test)
    for i in $self_tests; do $i; done ;;
*)
    set_up_helpers
    get_kernel_information
    check_missing_packages
    case "$MISSING_GCC$MISSING_MAKE$MISSING_HEADERS$LIVE_TEST" in "")
        exit 0
    esac
    generate_install_commands
    case "$1" in install)
        check_root
        $INSTALL_GCC_MAKE || abort "Unable to install tools for building kernel modules."
        case "$INSTALL_HEADERS_META" in
        "")
            $INSTALL_HEADERS || abort "Unable to install files needed to build modules for the current kernel." ;;
        *)
            $INSTALL_HEADERS || printf >&2 "Unable to install files needed to build modules for the current kernel.\nYou may need to restart your system to complete the installation."
            $INSTALL_HEADERS_META || abort "Unable to install files needed to build kernel modules."
        esac
        ;;
    *)
        echo "This system is not currently set up to build kernel modules (system extensions)."
        echo "Running the following commands should set the system up correctly:"
        echo
        case "$MISSING_GCC$MISSING_MAKE$LIVE_TEST" in ?*)
            echo "  $INSTALL_GCC_MAKE" ;;
        esac
        case "$MISSING_HEADERS$LIVE_TEST" in ?*)
            echo "  $INSTALL_HEADERS"
            echo "(The last command may fail if your system is not fully updated.)"
            case "$INSTALL_HEADERS_META" in ?*)
                echo "  $INSTALL_HEADERS_META" ;;
            esac
            ;;
        esac
        exit 1
        ;;
    esac
    ;;
esac
