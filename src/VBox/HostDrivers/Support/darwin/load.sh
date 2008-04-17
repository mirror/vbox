#!/bin/bash
## @file
# For development.
#

#
# Copyright (C) 2006-2007 innotek GmbH
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

DRVNAME="VBoxDrv.kext"
BUNDLE="org.virtualbox.kext.VBoxDrv"

DIR=`dirname "$0"`
DIR=`cd "$DIR" && pwd`
DIR="$DIR/$DRVNAME"
if [ ! -d "$DIR" ]; then
    echo "Cannot find $DIR or it's not a directory..."
    exit 1;
fi
if [ -n "$*" ]; then
  OPTS="$*"
else
  OPTS="-t"
fi

trap "sudo chown -R `whoami` $DIR; exit 1" INT

# Try unload any existing instance first.
LOADED=`kextstat -b $BUNDLE -l`
if test -n "$LOADED"; then
    echo "load.sh: Unloading $BUNDLE..."
    sudo kextunload -v 6 -b $BUNDLE
    LOADED=`kextstat -b $BUNDLE -l`
    if test -n "$LOADED"; then
        echo "load.sh: failed to unload $BUNDLE, see above..."
        exit 1;
    fi
    echo "load.sh: Successfully unloaded $BUNDLE"
fi

set -e
# On smbfs, this might succeed just fine but make no actual changes,
# so we might have to temporarily copy the driver to a local directory.
sudo chown -R root:wheel "$DIR"
OWNER=`stat -f "%u" "$DIR"`
if test "$OWNER" -ne 0; then
    TMP_DIR=/tmp/loaddrv.tmp
    echo "load.sh: chown didn't work on $DIR, using temp location $TMP_DIR/$DRVNAME"

    # clean up first (no sudo rm)
    if test -e "$TMP_DIR"; then
        sudo chown -R `whoami` "$TMP_DIR"
        rm -Rf "$TMP_DIR"
    fi

    # make a copy and switch over DIR
    mkdir -p "$TMP_DIR/"
    cp -Rp "$DIR" "$TMP_DIR/"
    DIR="$TMP_DIR/$DRVNAME"

    # retry
    sudo chown -R root:wheel "$DIR"
fi
sudo chmod -R o-rwx "$DIR"
sync
echo "load.sh: loading $DIR..."
sudo kextload $OPTS "$DIR"
sync
sudo chown -R `whoami` "$DIR"
#sudo chmod 666 /dev/vboxdrv
kextstat | grep org.virtualbox.kext

