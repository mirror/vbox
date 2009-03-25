#!/bin/sh
# Sun VirtualBox
# VirtualBox startup script, Solaris hosts.
#
# Copyright (C) 2007-2009 Sun Microsystems, Inc.
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

CURRENT_ISA=`isainfo -k`
if test "$CURRENT_ISA" = "amd64"; then
    INSTALL_DIR="/opt/VirtualBox/amd64"
else
    INSTALL_DIR="/opt/VirtualBox/i386"
fi
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
    exec "$INSTALL_DIR/VBoxQtconfig" "$@"
  ;;
  VBoxBFE)
    exec "$INSTALL_DIR/VBoxBFE" "$@"
  ;;
  *)
    echo "Unknown application - $APP"
  ;;
esac

