#!/bin/sh
## @file
# Start the Guest Additions X11 Client
#

#
# Copyright (C) 2007-2011 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

# It can happen that pidfiles from a sudo session can land in the user's
# home directory and prevent new ones from being created.  This is not really
# our fault, but the user may not quite appreciate that...
for i in $HOME/.vboxclient-*.pid; do
  if test -r $i && ! ps -e | grep `cat $i`; then
    rm -f $i
  fi
done

# Check whether the display we are running on running a known buggy version
# of X.Org which might crash when we resize.
no_display=
xorgbin=Xorg
found=`which Xorg | grep "no Xorg"`
if test ! -z "$found"; then
    if test -f "/usr/X11/bin/Xorg"; then
        xorgbin=/usr/X11/bin/Xorg
    else
        exit 1
    fi
fi
xout=`$xorgbin -version 2>&1`
if echo "$xout" | grep "1\.4\.99\.90[12345]" > /dev/null
then
    no_display=1
fi

/usr/bin/VBoxClient --clipboard
/usr/bin/VBoxClient --checkhostversion
test -z "$no_display" &&
    /usr/bin/VBoxClient --display
test -z "$no_display" &&
    /usr/bin/VBoxClient --seamless
