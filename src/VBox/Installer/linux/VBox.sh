#!/bin/sh
#
# Sun xVM VirtualBox
#
# Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

PATH="/usr/bin:/bin:/usr/sbin:/sbin"
CONFIG="/etc/vbox/vbox.cfg"

if [ ! -r "$CONFIG" ]; then
    echo "Could not find VirtualBox installation. Please reinstall."
    exit 1
fi

. "$CONFIG"

# Note: This script must not fail if the module was not successfully installed
#       because the user might not want to run a VM but only change VM params!

if [ "$1" = "shutdown" ]; then
    SHUTDOWN="true"
elif ! lsmod|grep -q vboxdrv; then
    cat << EOF
WARNING: The vboxdrv kernel module is not loaded. Either there is no module
         available for the current kernel (`uname -r`) or it failed to
         load. Please recompile the kernel module and install it by

           sudo /etc/init.d/vboxdrv setup

         You will not be able to start VMs until this problem is fixed.
EOF
elif [ ! -c /dev/vboxdrv ]; then
    cat << EOF
WARNING: The character device /dev/vboxdrv does not exist. Try

           sudo /etc/init.d/vboxdrv restart

         and if that is not successful, try to re-install the package.

	 You will not be able to start VMs until this problem is fixed.
EOF
fi

if [ -f /etc/vbox/module_not_compiled ]; then
    cat << EOF
WARNING: The compilation of the vboxdrv.ko kernel module failed during the
         installation for some reason. Starting a VM will not be possible.
         Please consult the User Manual for build instructions.
EOF
fi

SERVER_PID=`ps -U \`whoami\` | grep VBoxSVC | awk '{ print $1 }'`
if [ -z "$SERVER_PID" ]; then
    # Server not running yet/anymore, cleanup socket path.
    # See IPC_GetDefaultSocketPath()!
    if [ -n "$LOGNAME" ]; then
        rm -rf /tmp/.vbox-$LOGNAME-ipc > /dev/null 2>&1
    else
        rm -rf /tmp/.vbox-$USER-ipc > /dev/null 2>&1
    fi
fi

if [ "$SHUTDOWN" = "true" ]; then
    if [ -n "$SERVER_PID" ]; then
        kill -TERM $SERVER_PID
        sleep 2
    fi
    exit 0
fi

APP=`which $0`
APP=`basename $APP`
APP=${APP##/*/}
case "$APP" in
  VirtualBox)
    exec "$INSTALL_DIR/VirtualBox" "$@"
    ;;
  VBoxManage)
    exec "$INSTALL_DIR/VBoxManage" "$@"
    ;;
  VBoxSDL)
    exec "$INSTALL_DIR/VBoxSDL" "$@"
    ;;
  VBoxVRDP)
    exec "$INSTALL_DIR/VBoxHeadless" "$@"
    ;;
  VBoxHeadless)
    exec "$INSTALL_DIR/VBoxHeadless" "$@"
    ;;
  vboxwebsrv)
    exec "$INSTALL_DIR/vboxwebsrv" "$@"
    ;;
  *)
    echo "Unknown application - $APP"
    ;;
esac
