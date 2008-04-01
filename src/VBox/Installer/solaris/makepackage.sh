#!/bin/sh
#
# innotek VirtualBox Solaris package creation script.
# Usage:
#       makespackage.sh $(PATH_TARGET)/install packagename $(BUILD_TARGET_ARCH)

if [ -z "$3" ]; then
    echo "Usage: $0 installdir packagename x86|amd64"
    exit 1
fi

cd "$1"
echo 'i pkginfo=./vbox.pkginfo' > prototype
echo 'i postinstall=./postinstall.sh' >> prototype
echo 'i preremove=./preremove.sh' >> prototype
echo 'i space=./vbox.space' >> prototype
echo 'e sed /etc/devlink.tab ? ? ?' >> prototype
find . -print | /usr/sfw/bin/ggrep -v -E 'prototype|makepackage.sh|vbox.pkginfo|postinstall.sh|preremove.sh|ReadMe.txt|vbox.space' | pkgproto >> prototype
/usr/bin/awk 'NF == 6 && $2 == "none" { $5 = "root"; $6 = "bin" } { print }' prototype > prototype2
/usr/bin/awk 'NF == 6 && $2 == "none" { $3 = "opt/VirtualBox/"$3"="$3 } { print }' prototype2 > prototype

# install the kernel module to the right place.
if test "$3" = "x86"; then
    /usr/bin/awk 'NF == 6 && $3 == "opt/VirtualBox/vboxdrv=vboxdrv" { $3 = "platform/i86pc/kernel/drv/vboxdrv=vboxdrv" } { print }' prototype > prototype2
else
    /usr/bin/awk 'NF == 6 && $3 == "opt/VirtualBox/vboxdrv=vboxdrv" { $3 = "platform/i86pc/kernel/drv/amd64/vboxdrv=vboxdrv" } { print }' prototype > prototype2
fi

/usr/bin/awk 'NF == 6 && $3 == "opt/VirtualBox/vboxdrv.conf=vboxdrv.conf" { $3 = "platform/i86pc/kernel/drv/vboxdrv.conf=vboxdrv.conf" } { print }' prototype2 > prototype

rm prototype2
pkgmk -o -r .
pkgtrans -s -o /var/spool/pkg `pwd`/$2 INNOvbox
/usr/sfw/bin/gtar zcvf $2.tar.gz $2 autoresponse ReadMe.txt
rm -rf /var/spool/pkg/INNOvbox

