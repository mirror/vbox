#!/bin/bash
## @file
# For development.
#

#
# Copyright (C) 2006-2022 Oracle Corporation
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

SCRIPT_NAME="loadusb"
XNU_VERSION=`LC_ALL=C uname -r | LC_ALL=C cut -d . -f 1`

DRVNAME="VBoxUSB.kext"
BUNDLE="org.virtualbox.kext.VBoxUSB"

DEP_DRVNAME="VBoxDrv.kext"
DEP_BUNDLE="org.virtualbox.kext.VBoxDrv"


DIR=`dirname "$0"`
DIR=`cd "$DIR" && pwd`
DEP_DIR="$DIR/$DEP_DRVNAME"
DIR="$DIR/$DRVNAME"
if [ ! -d "$DIR" ]; then
    echo "Cannot find $DIR or it's not a directory..."
    exit 1;
fi
if [ ! -d "$DEP_DIR" ]; then
    echo "Cannot find $DEP_DIR or it's not a directory... (dependency)"
    exit 1;
fi
if [ -n "$*" ]; then
  OPTS="$*"
else
  OPTS="-t"
fi

trap "sudo chown -R `whoami` $DIR $DEP_DIR; exit 1" INT

# Try unload any existing instance first.
LOADED=`kextstat -b $BUNDLE -l`
if test -n "$LOADED"; then
    echo "${SCRIPT_NAME}.sh: Unloading $BUNDLE..."
    sudo kextunload -v 6 -b $BUNDLE
    LOADED=`kextstat -b $BUNDLE -l`
    if test -n "$LOADED"; then
        echo "${SCRIPT_NAME}.sh: failed to unload $BUNDLE, see above..."
        exit 1;
    fi
    echo "${SCRIPT_NAME}.sh: Successfully unloaded $BUNDLE"
fi

# We are now done since VBoxUSB.kext is no longer used.
