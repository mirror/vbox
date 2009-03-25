#!/bin/sh
#
# @file
#
# Sun VirtualBox
# X11 guest additions: dynamic display changes
#
# Copyright (C) 2008-2009 Sun Microsystems, Inc.
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

xorgbin=Xorg
found=`which Xorg | grep "no Xorg"`
if test ! -z "$found"; then
    if test -f "/usr/X11/bin/Xorg"; then
        xorgbin=/usr/X11/bin/Xorg
    else
        exit 1
    fi
fi

randrbin=xrandr
found=`which xrandr | grep "no xrandr"`
if test ! -z "$found"; then
    if test -f "/usr/X11/bin/xrandr"; then
        randrbin=/usr/X11/bin/xrandr
    else
        exit 1
    fi
fi

refreshbin=xrefresh
found=`which xrefresh | grep "no xrefresh"`
if test ! -z "$found"; then
    if test -f "/usr/X11/bin/xrefresh"; then
        refreshbin=/usr/X11/bin/xrefresh
    else
        exit 1
    fi
fi

# If we were called with the --test parameter, we check whether the display
# we are running on is really using the VBox video driver (and RandR 1.2),
# and whether we are running on a buggy version of X.Org which might crash
# when we resize.
if test "$1" = "--test"; then
    # Check for buggy X.Org versions
    xout=`$xorgbin -version 2>&1`
    if echo "$xout" | grep "1\.4\.99\.90[12345]" > /dev/null
    then
        echo "Warning: the version of the X Window system on your guest has a known"
        echo "problem. Because of this, dynamic resizing and seamless mode will not work."
        echo
        exit 1
    fi
    # Check to see if the server is configured to use static modes only.
    for conf in "/etc/X11/xorg.conf-4" "/etc/X11/xorg.conf" "/etc/X11/.xorg.conf" "/etc/xorg.conf" \
                "/usr/etc/X11/xorg.conf-4" "/usr/etc/X11/xorg.conf" "/usr/lib/X11/xorg.conf-4" \
                "/usr/lib/X11/xorg.conf"
    do
        if [ -r $conf ]
        then
            if awk -v IN_SECTION=0 \
'tolower($0) ~ /^[ \t]*section/ { IN_SECTION=1 } '\
'tolower($0) ~ /^[ \t]*modes/ { if (IN_SECTION) { print "mode"; exit } } '\
'tolower($0) ~ /^[ \t]*option[ \t]+\"preferredmode\"/ { if (IN_SECTION) { print "mode"; exit } } '\
'tolower($0) ~ /endsection/ { IN_SECTION=0 }' \
                $conf 2>/dev/null | grep mode > /dev/null
            then
                echo "Disabling dynamic resizing as the X server is configured to only use static"
                echo "resolutions.  To fix this, edit the server configuration file, remove all"
                echo "\"Modes\" lines from the \"Screen\" section and any Option \"PreferredMode\""
                echo "lines from \"Monitor\" sections and restart the server."
                echo
                exit 1
            fi
        fi
    done
    # Don't write out error information in this case, as this script is also
    # called from the installer, and vboxvideo will not yet be in place yet in
    # that case.
    $randrbin 2> /dev/null | grep VBOX1 > /dev/null
    exit
fi

# Otherwise we just switch the guest display into its preferred resolution,
# which is the one corresponding to the last video mode hint sent by the host.
$randrbin --output VBOX1 --preferred
$refreshbin 2>&1 > /dev/null

