#!/bin/sh
## @file
# Sun VirtualBox
# VirtualBox postinstall script for Solaris.
#

#
# Copyright (C) 2007-2009 Sun Microsystems, Inc.
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

# Check for xVM/Xen
currentisa=`uname -i`
if test "$currentisa" = "i86xpv"; then
    echo "## VirtualBox cannot run under xVM Dom0! Fatal Error, Aborting installation!"
    exit 2
fi

osversion=`uname -r`
currentzone=`zonename`
if test "$currentzone" = "global"; then
    echo "Checking for older bits..."
    /opt/VirtualBox/vboxconfig.sh preremove fatal
    rc=$?
    if test "$rc" -eq 0; then
        echo "Installing new bits..."
        /opt/VirtualBox/vboxconfig.sh postinstall
    fi
    rc=$?
    if test "$rc" -eq 0; then
        echo "Completed Successfully!"
    else
        echo "Completed but with errors."
    fi
fi

/usr/sbin/installf -f $PKGINST

echo "Done."

# return 20 = requires reboot, 2 = partial failure, 0  = success
exit "$rc"

