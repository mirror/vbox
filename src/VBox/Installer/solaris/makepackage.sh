#!/bin/sh
set -e
# Sun xVM VirtualBox
# VirtualBox Solaris package creation script.
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

#
# Usage:
#       makespackage.sh $(PATH_TARGET)/install packagename $(BUILD_TARGET_ARCH)

if [ -z "$3" ]; then
    echo "Usage: $0 installdir packagename x86|amd64"
    echo "-- packagename must not have any extension (e.g. VirtualBox-SunOS-amd64-r28899)"
    exit 1
fi

VBOX_PKGFILE=$2.pkg
VBOX_ARCHIVE=$2.tar.gz
VBOX_PKGNAME=SUNWvbox

VBOX_GGREP=/usr/sfw/bin/ggrep
VBOX_AWK=/usr/bin/awk
VBOX_GTAR=/usr/sfw/bin/gtar

# check for GNU grep we use which might not ship with all Solaris
if test ! -f "$VBOX_GGREP" && test ! -h "$VBOX_GGREP"; then
    echo "## GNU grep not found in $VBOX_GGREP."
    exit 1
fi

# check for GNU tar we use which might not ship with all Solaris
if test ! -f "$VBOX_GTAR" && test ! -h "$VBOX_GTAR"; then
    echo "## GNU tar not found in $VBOX_GTAR."
    exit 1
fi


# prepare file list
cd "$1"
echo 'i pkginfo=./vbox.pkginfo' > prototype
echo 'i postinstall=./postinstall.sh' >> prototype
echo 'i preremove=./preremove.sh' >> prototype
echo 'i space=./vbox.space' >> prototype
echo 'e sed /etc/devlink.tab ? ? ?' >> prototype
find . -print | $VBOX_GGREP -v -E 'prototype|makepackage.sh|vbox.pkginfo|postinstall.sh|preremove.sh|ReadMe.txt|vbox.space' | pkgproto >> prototype

# don't grok for the sed class files
$VBOX_AWK 'NF == 6 && $2 == "none" { $5 = "root"; $6 = "bin" } { print }' prototype > prototype2
$VBOX_AWK 'NF == 6 && $2 == "none" { $3 = "opt/VirtualBox/"$3"="$3 } { print }' prototype2 > prototype

# install the kernel module to the right place.
if test "$3" = "x86"; then
    $VBOX_AWK 'NF == 6 && $3 == "opt/VirtualBox/vboxdrv=vboxdrv" { $3 = "platform/i86pc/kernel/drv/vboxdrv=vboxdrv" } { print }' prototype > prototype2
else
    $VBOX_AWK 'NF == 6 && $3 == "opt/VirtualBox/vboxdrv=vboxdrv" { $3 = "platform/i86pc/kernel/drv/amd64/vboxdrv=vboxdrv" } { print }' prototype > prototype2
fi

$VBOX_AWK 'NF == 6 && $3 == "opt/VirtualBox/vboxdrv.conf=vboxdrv.conf" { $3 = "platform/i86pc/kernel/drv/vboxdrv.conf=vboxdrv.conf" } { print }' prototype2 > prototype

rm prototype2

# explicitly set timestamp to shutup warning
VBOXPKG_TIMESTAMP=vbox`date '+%Y%m%d%H%M%S'`

# create the package instance
pkgmk -p $VBOXPKG_TIMESTAMP -o -r .
if test $? -ne 0; then
    exit 1
fi

# translate into package datastream
pkgtrans -s -o /var/spool/pkg "`pwd`/$VBOX_PKGFILE" "$VBOX_PKGNAME"
if test $? -ne 0; then
    exit 1
fi

$VBOX_GTAR zcvf "$VBOX_ARCHIVE" "$VBOX_PKGFILE" autoresponse ReadMe.txt

if test  $? -eq 0; then
    echo "## Packaging and transfer completed successfully!"
fi
rm -rf "/var/spool/pkg/$VBOX_PKGNAME"
exit $?

