#!/bin/sh
## @file
#
# VirtualBox checkinstall script for Solaris.
#

#
# Copyright (C) 2009-2010 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

infoprint()
{
    echo 1>&2 "$1"
}

errorprint()
{
    echo 1>&2 "## $1"
}

abort_error()
{
    errorprint "Please close all VirtualBox processes and re-run this installer."
    exit 1
}

checkdep_svr4()
{
    if test -z "$1"; then
        errorprint "Missing argument to checkdep_svr4"
        return 1
    fi
    $BIN_PKGINFO $BASEDIR_OPT "$1" >/dev/null 2>&1
    if test $? -eq 0; then
        return 0
    fi
    PKG_MISSING_SVR4="$PKG_MISSING_SVR4 $1"
    return 1
}

checkdep_ips()
{
    if test -z "$1"; then
        errorprint "Missing argument to checkdep_svr4"
        return 1
    fi
    # using "list" without "-a" only lists installed pkgs which is what we need
    $BIN_PKG $BASEDIR_OPT list "$1" >/dev/null 2>&1
    if test $? -eq 0; then
        return 0
    fi
    PKG_MISSING_IPS="$PKG_MISSING_IPS $1"
    return 1

}

# nothing to check for remote install
REMOTE_INST=0
if test "x${PKG_INSTALL_ROOT:=/}" != "x/"; then
    BASEDIR_OPT="-R \"$PKG_INSTALL_ROOT\""
    REMOTE_INST=1
fi

# nothing to check for non-global zones
currentzone=`zonename`
if test "$currentzone" != "global"; then
    exit 0
fi


infoprint "Checking package dependencies..."

PKG_MISSING_IPS=""
PKG_MISSING_SVR4=""
BIN_PKGINFO=`which pkginfo 2> /dev/null`
BIN_PKG=`which pkg 2> /dev/null`

if test -x "$BIN_PKG"; then
    checkdep_ips "runtime/python-26"
    checkdep_ips "system/library/iconv/utf-8"
else
    PKG_MISSING_IPS="runtime/python-26 system/library/iconv/utf-8"
fi
if test -x "$BIN_PKGINFO"; then
    checkdep_svr4 "SUNWPython"
    checkdep_svr4 "SUNWPython-devel"
    checkdep_svr4 "SUNWuiu8"
else
    PKG_MISSING_SVR4="SUNWPython SUNWPython-devel SUNWuiu8"
fi

if test "$PKG_MISSING_IPS" != "" && test "$PKG_MISSING_SVR4" != ""; then
    if test ! -x "$BIN_PKG" && test ! -x "$BIN_PKGINFO"; then
        errorprint "Missing or non-executable binaries: pkg ($BIN_PKG) and pkginfo ($BIN_PKGINFO)."
        errorprint "Cannot check for dependencies."
        errorprint ""
        errorprint "Please install one of the required packaging system."
        exit 1
    fi
    errorprint "Missing packages: "
    errorprint "IPS : $PKG_MISSING_IPS"
    errorprint "SVr4: $PKG_MISSING_SVR4"
    errorprint ""
    errorprint "Please install either the IPS or SVr4 packages before installing VirtualBox."
    exit 1
else
    infoprint "Done."
fi

# nothing more to do for remote installs
if test "$REMOTE_INST" -eq 1; then
    exit 0
fi

# Check if the Zone Access service is holding open vboxdrv, if so stop & remove it
servicefound=`svcs -H "svc:/application/virtualbox/zoneaccess" 2> /dev/null | grep '^online'`
if test ! -z "$servicefound"; then
    infoprint "VirtualBox's zone access service appears to still be running."
    infoprint "Halting & removing zone access service..."
    /usr/sbin/svcadm disable -s svc:/application/virtualbox/zoneaccess
    # Don't delete the service, handled by manifest class action
    # /usr/sbin/svccfg delete svc:/application/virtualbox/zoneaccess
fi

# Check if the Web service is running, if so stop & remove it
servicefound=`svcs -H "svc:/application/virtualbox/webservice" 2> /dev/null | grep '^online'`
if test ! -z "$servicefound"; then
    infoprint "VirtualBox web service appears to still be running."
    infoprint "Halting & removing webservice..."
    /usr/sbin/svcadm disable -s svc:/application/virtualbox/webservice
    # Don't delete the service, handled by manifest class action
    # /usr/sbin/svccfg delete svc:/application/virtualbox/webservice
fi

# Check if VBoxSVC is currently running
VBOXSVC_PID=`ps -eo pid,fname | grep VBoxSVC | grep -v grep | awk '{ print $1 }'`
if test ! -z "$VBOXSVC_PID" && test "$VBOXSVC_PID" -ge 0; then
    errorprint "VirtualBox's VBoxSVC (pid $VBOXSVC_PID) still appears to be running."
    abort_error
fi

# Check if VBoxNetDHCP is currently running
VBOXNETDHCP_PID=`ps -eo pid,fname | grep VBoxNetDHCP | grep -v grep | awk '{ print $1 }'`
if test ! -z "$VBOXNETDHCP_PID" && test "$VBOXNETDHCP_PID" -ge 0; then
    errorprint "VirtualBox's VBoxNetDHCP (pid $VBOXNETDHCP_PID) still appears to be running."
    abort_error
fi

# Check if vboxnet is still plumbed, if so try unplumb it
BIN_IFCONFIG=`which ifconfig 2> /dev/null`
if test -x "$BIN_IFCONFIG"; then
    vboxnetup=`$BIN_IFCONFIG vboxnet0 >/dev/null 2>&1`
    if test "$?" -eq 0; then
        infoprint "VirtualBox NetAdapter is still plumbed"
        infoprint "Trying to remove old NetAdapter..."
        $BIN_IFCONFIG vboxnet0 unplumb
        if test "$?" -ne 0; then
            errorprint "VirtualBox NetAdapter 'vboxnet0' couldn't be unplumbed (probably in use)."
            abort_error
        fi
    fi
    vboxnetup=`$BIN_IFCONFIG vboxnet0 inet6 >/dev/null 2>&1`
    if test "$?" -eq 0; then
        infoprint "VirtualBox NetAdapter (Ipv6) is still plumbed"
        infoprint "Trying to remove old NetAdapter..."
        $BIN_IFCONFIG vboxnet0 inet6 unplumb
        if test "$?" -ne 0; then
            errorprint "VirtualBox NetAdapter 'vboxnet0' IPv6 couldn't be unplumbed (probably in use)."
            abort_error
        fi
    fi
fi

exit 0

