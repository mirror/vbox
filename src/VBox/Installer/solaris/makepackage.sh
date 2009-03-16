#!/bin/sh
## @file
# Sun xVM VirtualBox
# VirtualBox package creation script, Solaris hosts.
#

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
#       makespackage.sh [--hardened] $(PATH_TARGET)/install packagename {$(KBUILD_TARGET_ARCH)|neutral} $(VBOX_SVN_REV) [VBIPackageName]


# Parse options.
HARDENED=""
while test $# -ge 1;
do
    case "$1" in
        --hardened)
            HARDENED=1
            ;;
    *)
        break
        ;;
    esac
    shift
done

if [ -z "$4" ]; then
    echo "Usage: $0 installdir packagename x86|amd64 svnrev [VBIPackage]"
    echo "-- packagename must not have any extension (e.g. VirtualBox-SunOS-amd64-r28899)"
    exit 1
fi

VBOX_INSTALLED_DIR=$1
VBOX_PKGFILE=$2.pkg
VBOX_ARCHIVE=$2.tar.gz
# VBOX_PKG_ARCH is currently unused.
VBOX_PKG_ARCH=$3
VBOX_SVN_REV=$4

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

# bail out on non-zero exit status
set -e

# Fixup filelist using awk, the parameters must be in awk syntax
# params: filename condition action
filelist_fixup()
{
  "$VBOX_AWK" 'NF == 6 && '"$2"' { '"$3"' } { print }' "$1" > "tmp-$1"
  mv -f "tmp-$1" "$1"
}

hardlink_fixup()
{
  "$VBOX_AWK" 'NF == 3 && $1=="l" && '"$2"' { '"$3"' } { print }' "$1" > "tmp-$1"
  mv -f "tmp-$1" "$1"
}

symlink_fixup()
{
  "$VBOX_AWK" 'NF == 3 && $1=="s" && '"$2"' { '"$3"' } { print }' "$1" > "tmp-$1"
  mv -f "tmp-$1" "$1"
}


# prepare file list
cd "$VBOX_INSTALLED_DIR"
echo 'i pkginfo=./vbox.pkginfo' > prototype
echo 'i postinstall=./postinstall.sh' >> prototype
echo 'i preremove=./preremove.sh' >> prototype
echo 'i space=./vbox.space' >> prototype
if test -f "./vbox.copyright"; then
    echo 'i copyright=./vbox.copyright' >> prototype
fi

# Relative hardlinks
ln -f ./VBoxISAExec $VBOX_INSTALLED_DIR/VBoxManage
ln -f ./VBoxISAExec $VBOX_INSTALLED_DIR/VBoxSDL
ln -f ./VBoxISAExec $VBOX_INSTALLED_DIR/vboxwebsrv
ln -f ./VBoxISAExec $VBOX_INSTALLED_DIR/webtest
ln -f ./VBoxISAExec $VBOX_INSTALLED_DIR/VBoxZoneAccess
if test -f $VBOX_INSTALLED_DIR/amd64/VirtualBox || test -f $VBOX_INSTALLED_DIR/i386/VirtualBox; then
    ln -f ./VBoxISAExec $VBOX_INSTALLED_DIR/VirtualBox
fi
if test -f $VBOX_INSTALLED_DIR/amd64/VBoxBFE || test -f $VBOX_INSTALLED_DIR/i386/VBoxBFE; then
    ln -f ./VBoxISAExec $VBOX_INSTALLED_DIR/VBoxBFE
fi
if test -f $VBOX_INSTALLED_DIR/amd64/VBoxHeadless || test -f $VBOX_INSTALLED_DIR/i386/VBoxHeadless; then
    ln -f ./VBoxISAExec $VBOX_INSTALLED_DIR/VBoxHeadless
    ln -fs ./VBoxHeadless $VBOX_INSTALLED_DIR/VBoxVRDP
fi

find . -print | $VBOX_GGREP -v -E 'prototype|makepackage.sh|vbox.pkginfo|postinstall.sh|preremove.sh|ReadMe.txt|vbox.space|vbox.copyright|VirtualBoxKern' | pkgproto >> prototype

# don't grok for the class files
filelist_fixup prototype '$2 == "none"'                                                                 '$5 = "root"; $6 = "bin"'
filelist_fixup prototype '$2 == "none"'                                                                 '$3 = "opt/VirtualBox/"$3"="$3'
hardlink_fixup prototype '$2 == "none"'                                                                 '$3 = "opt/VirtualBox/"$3'
symlink_fixup  prototype '$2 == "none"'                                                                 '$3 = "opt/VirtualBox/"$3'

# install the kernel modules to the right place.
filelist_fixup prototype '$3 == "opt/VirtualBox/i386/vboxdrv=i386/vboxdrv"'                             '$3 = "platform/i86pc/kernel/drv/vboxdrv=i386/vboxdrv"; $6 = "sys"'
filelist_fixup prototype '$3 == "opt/VirtualBox/amd64/vboxdrv=amd64/vboxdrv"'                           '$3 = "platform/i86pc/kernel/drv/amd64/vboxdrv=amd64/vboxdrv"; $6 = "sys"'

# NetFilter vboxflt
filelist_fixup prototype '$3 == "opt/VirtualBox/i386/vboxflt=i386/vboxflt"'                             '$3 = "platform/i86pc/kernel/drv/vboxflt=i386/vboxflt"; $6 = "sys"'
filelist_fixup prototype '$3 == "opt/VirtualBox/amd64/vboxflt=amd64/vboxflt"'                           '$3 = "platform/i86pc/kernel/drv/amd64/vboxflt=amd64/vboxflt"; $6 = "sys"'

# NetAdapter vboxnet
filelist_fixup prototype '$3 == "opt/VirtualBox/i386/vboxnet=i386/vboxnet"'                             '$3 = "platform/i86pc/kernel/drv/vboxnet=i386/vboxnet"; $6 = "sys"'
filelist_fixup prototype '$3 == "opt/VirtualBox/amd64/vboxnet=amd64/vboxnet"'                           '$3 = "platform/i86pc/kernel/drv/amd64/vboxnet=amd64/vboxnet"; $6 = "sys"'

# USB vboxusbmon
filelist_fixup prototype '$3 == "opt/VirtualBox/i386/vboxusbmon=i386/vboxusbmon"'                       '$3 = "platform/i86pc/kernel/drv/vboxusbmon=i386/vboxusbmon"; $6 = "sys"'
filelist_fixup prototype '$3 == "opt/VirtualBox/amd64/vboxusbmon=amd64/vboxusbmon"'                     '$3 = "platform/i86pc/kernel/drv/amd64/vboxusbmon=amd64/vboxusbmon"; $6 = "sys"'

# All the driver conf files
filelist_fixup prototype '$3 == "opt/VirtualBox/vboxdrv.conf=vboxdrv.conf"'                             '$3 = "platform/i86pc/kernel/drv/vboxdrv.conf=vboxdrv.conf"'
filelist_fixup prototype '$3 == "opt/VirtualBox/vboxflt.conf=vboxflt.conf"'                             '$3 = "platform/i86pc/kernel/drv/vboxflt.conf=vboxflt.conf"'
filelist_fixup prototype '$3 == "opt/VirtualBox/vboxnet.conf=vboxnet.conf"'                             '$3 = "platform/i86pc/kernel/drv/vboxnet.conf=vboxnet.conf"'
filelist_fixup prototype '$3 == "opt/VirtualBox/vboxusbmon.conf=vboxusbmon.conf"'                       '$3 = "platform/i86pc/kernel/drv/vboxusbmon.conf=vboxusbmon.conf"'

# hardening requires some executables to be marked setuid.
if test -n "$HARDENED"; then
    $VBOX_AWK 'NF == 6 \
        && (    $3 == "opt/VirtualBox/amd64/VirtualBox=amd64/VirtualBox" \
            ||  $3 == "opt/VirtualBox/amd64/VirtualBox3=amd64/VirtualBox3" \
            ||  $3 == "opt/VirtualBox/amd64/VBoxHeadless=amd64/VBoxHeadless" \
            ||  $3 == "opt/VirtualBox/amd64/VBoxSDL=amd64/VBoxSDL" \
            ||  $3 == "opt/VirtualBox/amd64/VBoxBFE=amd64/VBoxBFE" \
            ||  $3 == "opt/VirtualBox/amd64/VBoxNetDHCP=amd64/VBoxNetDHCP" \
            ||  $3 == "opt/VirtualBox/i386/VirtualBox=i386/VirtualBox" \
            ||  $3 == "opt/VirtualBox/i386/VirtualBox3=i386/VirtualBox3" \
            ||  $3 == "opt/VirtualBox/i386/VBoxHeadless=i386/VBoxHeadless" \
            ||  $3 == "opt/VirtualBox/i386/VBoxSDL=i386/VBoxSDL" \
            ||  $3 == "opt/VirtualBox/i386/VBoxBFE=i386/VBoxBFE" \
            ||  $3 == "opt/VirtualBox/i386/VBoxNetDHCP=i386/VBoxNetDHCP" \
            ) \
       { $4 = "4755" } { print }' prototype > prototype2
    mv -f prototype2 prototype
fi

# VBoxUSBHelper needs to be marked setuid root.
if test -f $VBOX_INSTALLED_DIR/amd64/VBoxUSBHelper || test -f $VBOX_INSTALLED_DIR/i386/VBoxUSBHelper; then
    $VBOX_AWK 'NF == 6 \
        && (    $3 == "opt/VirtualBox/amd64/VBoxUSBHelper=amd64/VBoxUSBHelper" \
            ||  $3 == "opt/VirtualBox/i386/VBoxUSBHelper=i386/VBoxUSBHelper" \
            ) \
       { $4 = "4755" } { print }' prototype > prototype2
    mv -f prototype2 prototype
fi

# VBoxNetAdpCtl needs to be marked setuid root.
if test -f $VBOX_INSTALLED_DIR/amd64/VBoxNetAdpCtl || test -f $VBOX_INSTALLED_DIR/i386/VBoxNetAdpCtl; then
    $VBOX_AWK 'NF == 6 \
        && (    $3 == "opt/VirtualBox/amd64/VBoxNetAdpCtl=amd64/VBoxNetAdpCtl" \
            ||  $3 == "opt/VirtualBox/i386/VBoxNetAdpCtl=i386/VBoxNetAdpCtl" \
            ) \
       { $4 = "4755" } { print }' prototype > prototype2
    mv -f prototype2 prototype
fi


# desktop links and icons
filelist_fixup prototype '$3 == "opt/VirtualBox/virtualbox.desktop=virtualbox.desktop"'                 '$3 = "usr/share/applications/virtualbox.desktop=virtualbox.desktop"'
filelist_fixup prototype '$3 == "opt/VirtualBox/VBox.png=VBox.png"'                                     '$3 = "usr/share/pixmaps/VBox.png=VBox.png"'

# zoneaccess SMF manifest
filelist_fixup prototype '$3 == "opt/VirtualBox/virtualbox-zoneaccess.xml=virtualbox-zoneaccess.xml"'   '$3 = "var/svc/manifest/application/virtualbox/zoneaccess.xml=virtualbox-zoneaccess.xml"'

# webservice SMF manifest
filelist_fixup prototype '$3 == "opt/VirtualBox/virtualbox-webservice.xml=virtualbox-webservice.xml"'   '$3 = "var/svc/manifest/application/virtualbox/webservice.xml=virtualbox-webservice.xml"'

# webservice SMF start/stop script
filelist_fixup prototype '$3 == "opt/VirtualBox/smf-vboxwebsrv.sh=smf-vboxwebsrv.sh"'                   '$3 = "opt/VirtualBox/smf-vboxwebsrv=smf-vboxwebsrv.sh"'

echo " --- start of prototype  ---"
cat prototype
echo " --- end of prototype --- "

# explicitly set timestamp to shutup warning
VBOXPKG_TIMESTAMP=vbox`date '+%Y%m%d%H%M%S'`_r$VBOX_SVN_REV

# create the package instance
pkgmk -p $VBOXPKG_TIMESTAMP -o -r .

# translate into package datastream
pkgtrans -s -o /var/spool/pkg "`pwd`/$VBOX_PKGFILE" "$VBOX_PKGNAME"

# $5 if exist would contain the path to the VBI package to include in the .tar.gz
if test -f "$5"; then
    $VBOX_GTAR zcvf "$VBOX_ARCHIVE" "$VBOX_PKGFILE" "$5" autoresponse ReadMe.txt
else
    $VBOX_GTAR zcvf "$VBOX_ARCHIVE" "$VBOX_PKGFILE" autoresponse ReadMe.txt
fi

echo "## Packaging and transfer completed successfully!"
rm -rf "/var/spool/pkg/$VBOX_PKGNAME"

exit $?

