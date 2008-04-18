#!/bin/sh
# innotek VirtualBox
# VirtualBox preremove script for Solaris.
#
# Copyright (C) 2007-2008 Sun Microsystems, Inc.
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

echo "Sun xVM VirtualBox - preremove script"
echo "This script will unload the VirtualBox kernel module..."

# vboxdrv.sh would've been installed, we just need to call it.
/opt/VirtualBox/vboxdrv.sh stop

# remove /dev/vboxdrv
currentzone=`zonename`
if test "$currentzone" = "global"; then
    rm -f /dev/vboxdrv
fi

echo "Done."

