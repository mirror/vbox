# $Id$
# Shell script include file
## @file
# Shell script routines which are likely to be useful for different scripts
#

#
# Copyright (C) 2009-2011 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

# Deal with differing "which" semantics
mywhich() {
    which "$1" 2>/dev/null | grep -v "no $1"
}

# Get the name and execute switch for a useful terminal emulator
#
# Sets $gxtpath to the emulator path or empty
# Sets $gxttitle to the "title" switch for that emulator
# Sets $gxtexec to the "execute" switch for that emulator
# May clobber $gtx*
# Calls mywhich
getxterm() {
    # gnome-terminal uses -e differently to other emulators
    for gxti in "gnome-terminal --title -x" "konsole --title -e" "xterm -T -e"; do
        set $gxti
        gxtpath="`mywhich $1`"
        case "$gxtpath" in ?*)
            gxttitle=$2
            gxtexec=$3
            return
            ;;
        esac
    done
}

# Quotes its argument by inserting '\' in front of every character save
# for 'A-Za-z0-9/'.  Prints the result to stdout.
quotify() {
    echo "$1" | sed -e 's/\([^a-zA-Z0-9/]\)/\\\1/g'
}
