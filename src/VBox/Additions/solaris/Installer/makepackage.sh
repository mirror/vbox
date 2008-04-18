#!/bin/sh
set -e
# innotek VirtualBox
# VirtualBox Solaris Guest Additions package creation script.
#
# Copyright (C) 2008 innotek GmbH
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

#
# Usage:
#       makespackage.sh $(PATH_TARGET)/install packagename

if test -z "$2"; then
    echo "Usage: $0 installdir packagename"
    exit 1
fi

MY_PKGNAME=SUNWvboxguest
MY_GREP=/usr/sfw/bin/ggrep
MY_AWK=/usr/bin/awk

# check for GNU grep we use which might not ship with all Solaris
if test ! -f "$MY_GREP" || test ! -h "$MY_GREP"; then
    echo "## GNU grep not found in $MY_GREP."
    exit 1
fi

# prepare file list.
cd "$1"
echo 'i pkginfo=./vboxguest.pkginfo' > prototype
echo 'i postinstall=./postinstall.sh' >> prototype
echo 'i preremove=./preremove.sh' >> prototype
echo 'i space=./vboxguest.space' >> prototype
echo 'e sed /etc/devlink.tab ? ? ?' >> prototype
find . -print | /usr/sfw/bin/ggrep -v -E 'prototype|makepackage.sh|vboxguest.pkginfo|postinstall.sh|preremove.sh|vboxguest.space' | pkgproto >> prototype

# don't grok for the sed class files
$MY_AWK 'NF == 6 && $2 == "none" { $5 = "root"; $6 = "bin" } { print }' prototype > prototype2
$MY_AWK 'NF == 6 && $2 == "none" { $3 = "opt/VirtualBoxAdditions/"$3"="$3 } { print }' prototype2 > prototype

# install the kernel module to the right place (for now only 32-bit guests)
$MY_AWK 'NF == 6 && $3 == "opt/VirtualBoxAdditions/vboxguest=vboxguest" { $3 = "platform/i86pc/kernel/drv/vboxguest=vboxguest" } { print }' prototype > prototype2
$MY_AWK 'NF == 6 && $3 == "opt/VirtualBoxAdditions/vboxguest.conf=vboxguest.conf" { $3 = "platform/i86pc/kernel/drv/vboxguest.conf=vboxguest.conf" } { print }' prototype2 > prototype

# install the timesync SMF service
$MY_AWK 'NF == 6 && $3 == "opt/VirtualBoxAdditions/vboxservice.xml=vboxservice.xml" { $3 = "/var/svc/manifest/system/virtualbox/vboxservice.xml=vboxservice.xml" } { print }' prototype2 > prototype

rm prototype2

# explicitly set timestamp to shutup warning
VBOXPKG_TIMESTAMP=vbox`date '+%Y%m%d%H%M%S'`

# create the package instance
pkgmk -p $VBOXPKG_TIMESTAMP -o -r .
if test $? -ne 0; then
    exit 1
fi

# translate into package datastream (errors are sent to stderr)
pkgtrans -s -o /var/spool/pkg `pwd`/$2 "$MY_PKGNAME"
if test $? -ne 0; then
    exit 1
fi

rm -rf "/var/spool/pkg/$MY_PKGNAME"
exit $?

