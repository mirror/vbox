#!/bin/sh
#
# @file
#
# Sun xVM VirtualBox
# X11 guest additions: dynamic display changes
#
# Copyright (C) 2008 Sun Microsystems, Inc.
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

#
# In order to keep things simple, we just call the X11 xrandr application
# when we receive notification of a display change instead of reimplementing
# it inside our Guest Additions client application.  This saves us among
# other things having to find ways to build and link the C code on systems
# which do not support RandR 1.2.  We wrap the call to xrandr in this script
# so that we can make any fiddly adjustments to the call which may be needed.
#

randrbin=xrandr
found=`which xrandr | grep "no xrandr"`
if test ! -z "$found"; then
    if test -f "/usr/X11/bin/xrandr"; then
        randrbin=/usr/X11/bin/xrandr
    else
        exit 1
    fi
fi

if test "$1" = "--test"; then
  version=`$randrbin -v 2>&1 | sed -n 's/[^0-9]*version 1\.\([0-9]*\)/\1/p'`
  expr "$version" '>=' 2 2>&1 > /dev/null
  exit
fi
$randrbin --output Virtual\ Output --preferred
xrefresh 2>&1 > /dev/null
