#!/bin/sh
#
# Oracle VM VirtualBox
# VirtualBox linux uninstallation script

#
# Copyright (C) 2009-2015 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

. `dirname $0`/routines.sh

if [ -z "$ro_LOG_FILE" ]; then
    create_log "/var/log/vbox-uninstall.log"
fi

if [ -z "VBOX_NO_UNINSTALL_MESSAGE" ]; then
    info "Uninstalling VirtualBox"
    log "Uninstalling VirtualBox"
    log ""
fi

check_root

[ -z "$DKMS"       ]    && DKMS=`which dkms 2> /dev/null`
[ -z "$CONFIG_DIR" ]    && CONFIG_DIR="/etc/vbox"
[ -z "$CONFIG" ]        && CONFIG="vbox.cfg"
[ -z "$CONFIG_FILES" ]  && CONFIG_FILES="filelist"
[ -z "$DEFAULT_FILES" ] && DEFAULT_FILES=`pwd`/deffiles

# Find previous installation
if [ -r $CONFIG_DIR/$CONFIG ]; then
    . $CONFIG_DIR/$CONFIG
    PREV_INSTALLATION=$INSTALL_DIR
fi

# Remove previous installation
if [ "$PREV_INSTALLATION" = "" ]; then
    log "Unable to find a VirtualBox installation, giving up."
    abort "Couldn't find a VirtualBox installation to uninstall."
fi

# Stop the ballon control service
stop_init_script vboxballoonctrl-service
# Stop the autostart service
stop_init_script vboxautostart-service
# Stop the web service
stop_init_script vboxweb-service
# Do this check here after we terminated the web service
check_running
# Terminate VBoxNetDHCP if running
terminate_proc VBoxNetDHCP
# Terminate VBoxNetNAT if running
terminate_proc VBoxNetNAT
delrunlevel vboxballoonctrl-service > /dev/null 2>&1
remove_init_script vboxballoonctrl-service
delrunlevel vboxautostart-service > /dev/null 2>&1
remove_init_script vboxautostart-service
delrunlevel vboxweb-service > /dev/null 2>&1
remove_init_script vboxweb-service
# Stop kernel module and uninstall runlevel script
stop_init_script vboxdrv
delrunlevel vboxdrv > /dev/null 2>&1
remove_init_script vboxdrv
# Stop host networking and uninstall runlevel script (obsolete)
stop_init_script vboxnet
delrunlevel vboxnet > /dev/null 2>&1
remove_init_script vboxnet
# Remove kernel module installed
if [ -n "$DKMS" ]; then
    $DKMS remove -m vboxhost -v $INSTALL_VER --all > /dev/null 2>&1
fi
if [ -z "$VBOX_DONT_REMOVE_OLD_MODULES" ]; then
    find /lib/modules/`uname -r` -name "vboxdrv\.*" 2>/dev/null|xargs rm -f 2> /dev/null
    find /lib/modules/`uname -r` -name "vboxnetflt\.*" 2>/dev/null|xargs rm -f 2> /dev/null
    find /lib/modules/`uname -r` -name "vboxnetadp\.*" 2>/dev/null|xargs rm -f 2> /dev/null
    # Remove directories we have installed to in the past
    find /lib/modules/`uname -r` -name vbox\* 2>/dev/null|xargs rmdir -p 2> /dev/null
    find /lib/modules/`uname -r` -name misc\* 2>/dev/null|xargs rmdir -p 2> /dev/null
    rm -f /usr/src/vboxhost-$INSTALL_VER 2> /dev/null
    rm -f /usr/src/vboxdrv-$INSTALL_VER 2> /dev/null
    rm -f /usr/src/vboxnetflt-$INSTALL_VER 2> /dev/null
    rm -f /usr/src/vboxnetadp-$INSTALL_VER 2> /dev/null
fi

# Remove symlinks
rm -f \
  /usr/bin/VirtualBox \
  /usr/bin/VBoxManage \
  /usr/bin/VBoxSDL \
  /usr/bin/VBoxVRDP \
  /usr/bin/VBoxHeadless \
  /usr/bin/VBoxDTrace \
  /usr/bin/VBoxBalloonCtrl \
  /usr/bin/VBoxAutostart \
  /usr/bin/VBoxNetDHCP \
  /usr/bin/VBoxNetNAT \
  /usr/bin/vboxwebsrv \
  /usr/bin/vbox-img \
  /usr/bin/VBoxAddIF \
  /usr/bin/VBoxDeleteIf \
  /usr/bin/VBoxTunctl \
  /usr/bin/virtualbox \
  /usr/share/pixmaps/VBox.png \
  /usr/share/pixmaps/virtualbox.png \
  /usr/share/applications/virtualbox.desktop \
  /usr/share/mime/packages/virtualbox.xml \
  /usr/bin/rdesktop-vrdp \
  /usr/bin/virtualbox \
  /usr/bin/vboxmanage \
  /usr/bin/vboxsdl \
  /usr/bin/vboxheadless \
  /usr/bin/vboxdtrace \
  $PREV_INSTALLATION/components/VBoxVMM.so \
  $PREV_INSTALLATION/components/VBoxREM.so \
  $PREV_INSTALLATION/components/VBoxRT.so \
  $PREV_INSTALLATION/components/VBoxDDU.so \
  $PREV_INSTALLATION/components/VBoxXPCOM.so \
  2> /dev/null

# Remove udev description file
if [ -f /etc/udev/rules.d/60-vboxdrv.rules ]; then
    rm -f /etc/udev/rules.d/60-vboxdrv.rules 2> /dev/null
fi
if [ -f /etc/udev/rules.d/10-vboxdrv.rules ]; then
    rm -f /etc/udev/rules.d/10-vboxdrv.rules 2> /dev/null
fi

# Remove our USB device tree
rm -rf /dev/vboxusb 2> /dev/null

cwd=`pwd`
if [ -f $PREV_INSTALLATION/src/Makefile ]; then
    cd $PREV_INSTALLATION/src
    make clean > /dev/null 2>&1
fi
if [ -f $PREV_INSTALLATION/src/vboxdrv/Makefile ]; then
    cd $PREV_INSTALLATION/src/vboxdrv
    make clean > /dev/null 2>&1
fi
if [ -f $PREV_INSTALLATION/src/vboxnetflt/Makefile ]; then
    cd $PREV_INSTALLATION/src/vboxnetflt
    make clean > /dev/null 2>&1
fi
if [ -f $PREV_INSTALLATION/src/vboxnetadp/Makefile ]; then
    cd $PREV_INSTALLATION/src/vboxnetadp
    make clean > /dev/null 2>&1
fi
cd $PREV_INSTALLATION
if [ -r $CONFIG_DIR/$CONFIG_FILES ]; then
    rm -f `cat $CONFIG_DIR/$CONFIG_FILES` 2> /dev/null
elif [ -n "$DEFAULT_FILES" -a -r "$DEFAULT_FILES" ]; then
    DEFAULT_FILE_NAMES=""
    . $DEFAULT_FILES
    for i in "$DEFAULT_FILE_NAMES"; do
        rm -f $i 2> /dev/null
    done
fi
for file in `find $PREV_INSTALLATION 2> /dev/null`; do
    rmdir -p $file 2> /dev/null
done
cd $cwd
mkdir -p $PREV_INSTALLATION 2> /dev/null # The above actually removes the current directory and parents!
rmdir $PREV_INSTALLATION 2> /dev/null
rm -r $CONFIG_DIR/$CONFIG 2> /dev/null

if [ -z "$VBOX_NO_UNINSTALL_MESSAGE" ]; then
    rm -r $CONFIG_DIR/$CONFIG_FILES 2> /dev/null
    rmdir $CONFIG_DIR 2> /dev/null
    [ -n "$INSTALL_REV" ] && INSTALL_REV=" r$INSTALL_REV"
    info "VirtualBox $INSTALL_VER$INSTALL_REV has been removed successfully."
    log "Successfully $INSTALL_VER$INSTALL_REV removed VirtualBox."
fi
update-mime-database /usr/share/mime >/dev/null 2>&1
