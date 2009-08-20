#!/bin/sh
## @file
# Sun VirtualBox
# VirtualBox checkinstall script for Solaris.
#

#
# Copyright (C) 2009 Sun Microsystems, Inc.
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#
# Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
# Clara, CA 95054 USA or visit http://www.sun.com if you need
# additional information or have any questions.
#

abort_error()
{
    echo "## Please close all VirtualBox processes and re-run this installer."
    exit 1
}


# Check if VBoxSVC is currently running
VBOXSVC_PID=`ps -eo pid,fname | grep VBoxSVC | grep -v grep | awk '{ print $1 }'`
if test ! -z "$VBOXSVC_PID" && test "$VBOXSVC_PID" -ge 0; then
    echo "## VirtualBox's VBoxSVC (pid $VBOXSVC_PID) still appears to be running."
    abort_error
fi

# Check if the Zone Access service is holding open vboxdrv, if so stop & remove it
servicefound=`svcs -H "svc:/application/virtualbox/zoneaccess" 2> /dev/null | grep '^online'`
if test ! -z "$servicefound"; then
    echo "## VirtualBox's zone access service appears to still be running."
    echo "## Halting & removing zone access service..."
    /usr/sbin/svcadm disable -s svc:/application/virtualbox/zoneaccess
    /usr/sbin/svccfg delete svc:/application/virtualbox/zoneaccess
fi

# Check if the Web service is running, if so stop & remove it
servicefound=`svcs -H "svc:/application/virtualbox/webservice" 2> /dev/null | grep '^online'`
if test ! -z "$servicefound"; then
    echo "## VirtualBox web service appears to still be running."
    echo "## Halting & removing webservice..."
    /usr/sbin/svcadm disable -s svc:/application/virtualbox/webservice
    /usr/sbin/svccfg delete svc:/application/virtualbox/webservice
fi

# Check if vboxnet is still plumbed, if so try unplumb it
BIN_IFCONFIG=`which ifconfig 2> /dev/null`
if test -x "$BIN_IFCONFIG"; then
    vboxnetup=`$BIN_IFCONFIG vboxnet0 >/dev/null 2>&1`
    if test "$?" -eq 0; then
        echo "## VirtualBox NetAdapter is still plumbed"
        echo "## Trying to remove old NetAdapter..."
        $BIN_IFCONFIG vboxnet0 unplumb
        if test "$?" -ne 0; then
            echo "## VirtualBox NetAdapter 'vboxnet0' couldn't be unplumbed (probably in use)."
            abort_error
        fi
    fi
    vboxnetup=`$BIN_IFCONFIG vboxnet0 inet6 >/dev/null 2>&1`
    if test "$?" -eq 0; then
        echo "## VirtualBox NetAdapter (Ipv6) is still plumbed"
        echo "## Trying to remove old NetAdapter..."
        $BIN_IFCONFIG vboxnet0 inet6 unplumb
        if test "$?" -ne 0; then
            echo "## VirtualBox NetAdapter 'vboxnet0' IPv6 couldn't be unplumbed (probably in use)."
            abort_error
        fi
    fi
fi

exit 0

