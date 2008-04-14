#!/bin/sh
#
# innotek VirtualBox Solaris Guest Additions package creation script.
# Usage:
#       makespackage.sh $(PATH_TARGET)/install packagename
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

if [ -z "$2" ]; then
    echo "Usage: $0 installdir packagename"
    exit 1
fi

cd "$1"
echo 'i pkginfo=./vboxguest.pkginfo' > prototype
echo 'i postinstall=./postinstall.sh' >> prototype
echo 'i preremove=./preremove.sh' >> prototype
echo 'e sed /etc/devlink.tab ? ? ?' >> prototype
find . -print | /usr/sfw/bin/ggrep -v -E 'prototype|makepackage.sh|vboxguest.pkginfo|postinstall.sh|preremove.sh' | pkgproto >> prototype

/usr/bin/awk 'NF == 6 && $2 == "none" { $5 = "root"; $6 = "bin" } { print }' prototype > prototype2
/usr/bin/awk 'NF == 6 && $2 == "none" { $3 = "opt/VirtualBoxAdditions/"$3"="$3 } { print }' prototype2 > prototype

# install the kernel module to the right place (for now only 32-bit guests)
/usr/bin/awk 'NF == 6 && $3 == "opt/VirtualBoxAdditions/vboxguest=vboxguest" { $3 = "platform/i86pc/kernel/drv/vboxguest=vboxguest" } { print }' prototype > prototype2
/usr/bin/awk 'NF == 6 && $3 == "opt/VirtualBoxAdditions/vboxguest.conf=vboxguest.conf" { $3 = "platform/i86pc/kernel/drv/vboxguest.conf=vboxguest.conf" } { print }' prototype2 > prototype

# install the vboxclient daemon
#/usr/bin/awk 'NF == 6 && $3 == "opt/VirtualBoxAdditions/1099.vboxclient=1099.vboxclient" { $3 = "usr/dt/config/Xsession.d/1099.vboxclient=1099.vboxclient" } { print }' prototype > prototype2

#install the timesync daemon
/usr/bin/awk 'NF == 6 && $3 == "opt/VirtualBoxAdditions/vboxservice.xml=vboxservice.xml" { $3 = "/var/svc/manifest/system/virtualbox/vboxservice.xml=vboxservice.xml" } { print }' prototype2 > prototype

rm prototype2
pkgmk -o -r .
pkgtrans -s -o /var/spool/pkg `pwd`/$2 INNOvboxguest
rm -rf /var/spool/pkg/INNOvboxguest

