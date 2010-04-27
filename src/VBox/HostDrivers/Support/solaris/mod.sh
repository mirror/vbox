#!/bin/sh
# $Id$
## @file
# Helper script for installing the solaris module in a development environment.
#

#
# Copyright (C) 2006-2009 Oracle Corporation
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

set -x
#
# Figure out the environment and locations.
#

# Sudo isn't native solaris, but it's very convenient...
if test -z "$SUDO" && test "`whoami`" != "root"; then
    SUDO=sudo
fi

script_dir=`dirname "$0"`
# src/VBox/HostDrivers/solaris/ residence:
script_dir=`cd "$script_dir/../../../../.." ; /bin/pwd`
## root residence:
#script_dir=`cd "$script_dir" ; /bin/pwd`

set -e
if test -z "$BUILD_TARGET"; then
    export BUILD_TARGET=solaris
fi
if test -z "$BUILD_TARGET_ARCH"; then
    export BUILD_TARGET_ARCH=x86
fi
if test -z "$BUILD_TYPE"; then
    export BUILD_TYPE=debug
fi

DIR=$script_dir/out/$BUILD_TARGET.$BUILD_TARGET_ARCH/$BUILD_TYPE/bin/

VBOXDRV_CONF_DIR=/platform/i86pc/kernel/drv
if test "$BUILD_TARGET_ARCH" = "amd64"; then
    VBOXDRV_DIR=$VBOXDRV_CONF_DIR/amd64
else
    VBOXDRV_DIR=$VBOXDRV_CONF_DIR
fi

#
# Do the job.
#
$SUDO cp $DIR/vboxdrv $VBOXDRV_DIR/vboxdrv
$SUDO cp $script_dir/src/VBox/HostDrivers/Support/solaris/vboxdrv.conf $VBOXDRV_CONF_DIR/vboxdrv.conf
old_id=`/usr/sbin/modinfo | /usr/xpg4/bin/grep vbox | cut -f 1 -d ' ' | sort -n -r `
if test -n "$old_id"; then
    echo "* unloading $old_id..."
    sync
    sync
    $SUDO /usr/sbin/modunload -i $old_id
#else
#    echo "* If it fails below, run: $SUDO add_drv -m'* 0666 root sys' vboxdrv"
fi
$SUDO /usr/sbin/rem_drv vboxdrv || echo "* ignored rem_drv failure..."
$SUDO /usr/sbin/add_drv vboxdrv

if /usr/xpg4/bin/grep  -q vboxdrv /etc/devlink.tab; then
    echo "* vboxdrv already present in /etc/devlink.tab"
else
    echo "* Adding vboxdrv to /etc/devlink.tab"
    $SUDO rm -f /tmp/devlink.tab.vboxdrv
    echo "" > /tmp/devlink.tab.vboxdrv
    echo '# vbox' >> /tmp/devlink.tab.vboxdrv
    echo 'type=ddi_pseudo;name=vboxdrv	\D' >> /tmp/devlink.tab.vboxdrv
    $SUDO /bin/sh -c 'cat /tmp/devlink.tab.vboxdrv >> /etc/devlink.tab'
fi

echo "* loading vboxdrv..."
sync
sync
$SUDO /usr/sbin/modload $VBOXDRV_DIR/vboxdrv
/usr/sbin/modinfo | /usr/xpg4/bin/grep vboxdrv
echo "* dmesg:"
dmesg | tail -20
if test ! -h /dev/vboxdrv; then
    $SUDO /usr/sbin/devfsadm -i vboxdrv
fi
ls -laL /dev/vboxdrv
