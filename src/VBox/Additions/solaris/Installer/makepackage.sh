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
#       makespackage.sh $(PATH_TARGET)/install packagename svnrev

if test -z "$3"; then
    echo "Usage: $0 installdir packagename svnrev"
    exit 1
fi

VBOX_INSTALLED_DIR=$1
VBOX_PKGFILENAME=$2
VBOX_SVN_REV=$3

VBOX_PKGNAME=SUNWvboxguest
VBOX_AWK=/usr/bin/awk
VBOX_GGREP=/usr/sfw/bin/ggrep
VBOX_AWK=/usr/bin/awk

# check for GNU grep we use which might not ship with all Solaris
if test ! -f "$VBOX_GGREP" && test ! -h "$VBOX_GGREP"; then
    echo "## GNU grep not found in $VBOX_GGREP."
    exit 1
fi

# bail out on non-zero exit status
set -e

# Fixup filelist using awk, the parameters must be in awk syntax
# params: filename condition action
filelist_fixup()
{
    "$VBOX_AWK" 'NF == 6 && '"$2"' { '"$3"' } { print }' "$1" > "tmp-$1"
    mv -f "tmp-$1" "$1"
}

# prepare file list
cd "$VBOX_INSTALLED_DIR"
echo 'i pkginfo=./vboxguest.pkginfo' > prototype
echo 'i postinstall=./postinstall.sh' >> prototype
echo 'i preremove=./preremove.sh' >> prototype
echo 'i space=./vboxguest.space' >> prototype
if test -f "./vboxguest.copyright"; then
    echo 'i copyright=./vboxguest.copyright' >> prototype
fi
find . -print | $VBOX_GGREP -v -E 'prototype|makepackage.sh|vboxguest.pkginfo|postinstall.sh|preremove.sh|vboxguest.space|vboxguest.copyright' | pkgproto >> prototype

# don't grok for the class files
filelist_fixup prototype '$2 == "none"'                                                     '$5 = "root"; $6 = "bin"'
filelist_fixup prototype '$2 == "none"'                                                     '$3 = "opt/VirtualBoxAdditions/"$3"="$3'

# VBoxService requires suid
filelist_fixup prototype '$3 == "opt/VirtualBoxAdditions/VBoxService=VBoxService"'              '$4 = "4755"'
filelist_fixup prototype '$3 == "opt/VirtualBoxAdditions/amd64/VBoxService=amd64/VBoxService"'  '$4 = "4755"'

# 32-bit vboxguest
filelist_fixup prototype '$3 == "opt/VirtualBoxAdditions/vboxguest=vboxguest"'              '$3 = "usr/kernel/drv/vboxguest=vboxguest"; $6="sys"'

# 64-bit vboxguest
filelist_fixup prototype '$3 == "opt/VirtualBoxAdditions/amd64/vboxguest=amd64/vboxguest"'  '$3 = "usr/kernel/drv/amd64/vboxguest=amd64/vboxguest"; $6="sys"'

# vboxguest module config file
filelist_fixup prototype '$3 == "opt/VirtualBoxAdditions/vboxguest.conf=vboxguest.conf"'    '$3 = "usr/kernel/drv/vboxguest.conf=vboxguest.conf"'

# vboxfsmount binary (always 32-bit on combined package)
filelist_fixup prototype '$3 == "opt/VirtualBoxAdditions/vboxfsmount=vboxfsmount"'         '$3 = "etc/fs/vboxfs/mount=vboxfsmount"; $6="sys"'

# this is required for amd64-specific package where we do not build 32-bit binaries
filelist_fixup prototype '$3 == "opt/VirtualBoxAdditions/amd64/vboxfsmount=vboxfsmount"'   '$3 = "etc/fs/vboxfs/mount=amd64/vboxfsmount"; $6="sys"'


filelist_fixup prototype '$3 == "opt/VirtualBoxAdditions/vboxservice.xml=vboxservice.xml"'  '$3 = "var/svc/manifest/system/virtualbox/vboxservice.xml=vboxservice.xml"'
echo " --- start of prototype  ---" 
cat prototype
echo " --- end of prototype --- "

# explicitly set timestamp to shutup warning
VBOXPKG_TIMESTAMP=vboxguest`date '+%Y%m%d%H%M%S'`_r$VBOX_SVN_REV

# create the package instance
pkgmk -p $VBOXPKG_TIMESTAMP -o -r .

# translate into package datastream
pkgtrans -s -o /var/spool/pkg `pwd`/$VBOX_PKGFILENAME "$VBOX_PKGNAME"

rm -rf "/var/spool/pkg/$VBOX_PKGNAME"
exit $?

