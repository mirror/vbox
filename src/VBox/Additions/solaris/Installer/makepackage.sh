#!/bin/sh
# Sun xVM VirtualBox
# VirtualBox Solaris Guest Additions package creation script.
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
# Usage:
#       makespackage.sh $(PATH_TARGET)/install packagename $(KBUILD_TARGET_ARCH)

if test -z "$3"; then
    echo "Usage: $0 installdir packagename x86|amd64"
    exit 1
fi

MY_PKGNAME=SUNWvboxguest
MY_GGREP=/usr/sfw/bin/ggrep
MY_AWK=/usr/bin/awk

# check for GNU grep we use which might not ship with all Solaris
if test ! -f "$MY_GGREP" && test ! -h "$MY_GGREP"; then
    echo "## GNU grep not found in $MY_GGREP."
    exit 1
fi

# bail out on non-zero exit status
set -e

# prepare file list
cd "$1"
echo 'i pkginfo=./vboxguest.pkginfo' > prototype
echo 'i postinstall=./postinstall.sh' >> prototype
echo 'i preremove=./preremove.sh' >> prototype
echo 'i space=./vboxguest.space' >> prototype
if test -f "./vboxguest.copyright"; then
    echo 'i copyright=./vboxguest.copyright' >> prototype
fi
echo 'e sed /etc/devlink.tab ? ? ?' >> prototype
find . -print | $MY_GGREP -v -E 'prototype|makepackage.sh|vboxguest.pkginfo|postinstall.sh|preremove.sh|vboxguest.space|vboxguest.copyright' | pkgproto >> prototype

# don't grok for the sed class files
$MY_AWK 'NF == 6 && $2 == "none" { $5 = "root"; $6 = "bin" } { print }' prototype > prototype2
$MY_AWK 'NF == 6 && $2 == "none" { $3 = "opt/VirtualBoxAdditions/"$3"="$3 } { print }' prototype2 > prototype

# install the kernel module to the right place
if test "$3" = "x86"; then
    $MY_AWK 'NF == 6 && $3 == "opt/VirtualBoxAdditions/vboxguest=vboxguest" { $3 = "platform/i86pc/kernel/drv/vboxguest=vboxguest"; $6 = "sys" } { print }' prototype > prototype2
else
    $MY_AWK 'NF == 6 && $3 == "opt/VirtualBoxAdditions/vboxguest=vboxguest" { $3 = "platform/i86pc/kernel/drv/amd64/vboxguest=vboxguest"; $6 = "sys" } { print }' prototype > prototype2
fi
$MY_AWK 'NF == 6 && $3 == "opt/VirtualBoxAdditions/vboxguest.conf=vboxguest.conf" { $3 = "platform/i86pc/kernel/drv/vboxguest.conf=vboxguest.conf" } { print }' prototype2 > prototype

# install the timesync SMF service
$MY_AWK 'NF == 6 && $3 == "opt/VirtualBoxAdditions/vboxservice.xml=vboxservice.xml" { $3 = "/var/svc/manifest/system/virtualbox/vboxservice.xml=vboxservice.xml" } { print }' prototype2 > prototype

rm prototype2

# explicitly set timestamp to shutup warning
VBOXPKG_TIMESTAMP=vboxguest`date '+%Y%m%d%H%M%S'`

# create the package instance
pkgmk -p $VBOXPKG_TIMESTAMP -o -r .

# translate into package datastream
pkgtrans -s -o /var/spool/pkg `pwd`/$2 "$MY_PKGNAME"

rm -rf "/var/spool/pkg/$MY_PKGNAME"
exit $?

