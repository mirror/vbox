#!/bin/sh
# innotek VirtualBox
# VirtualBox startup script for Solaris
#
# Copyright (C) 2007-2008 innotek GmbH
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

INSTALL_DIR="/opt/VirtualBox"
APP=`which $0`
APP=`basename $APP`
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
  VBoxQtconfig)
    # qtconfig requires setting LD_LIBRARY_PATH
    QT_DIR=$INSTALL_DIR/qtgcc/lib
    if test "$LD_LIBRARY_PATH"; then
        LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$QT_DIR"
    else
        LD_LIBRARY_PATH="$QT_DIR"
    fi
    export LD_LIBRARY_PATH
    exec "$INSTALL_DIR/qtgcc/bin/qtconfig" "$@"
    ;;
  *)
    echo "Unknown application - $APP"
  ;;
esac

