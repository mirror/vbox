#!/bin/bash
## @file
# For development, builds and loads all the host drivers.
#

#
# Copyright (C) 2010-2012 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

MY_DIR=`cd "${0}/.." && cmd /c cd | kmk_sed -e 's,\\\\,/,g' `
if [ ! -d "${MY_DIR}" ]; then
    echo "Cannot find ${MY_DIR} or it's not a directory..."
    exit 1;
fi
echo MY_DIR=$MY_DIR

set -e
cd "$MY_DIR"
set +e


#
# Query the status of the drivers.
#
for drv in VBoxNetAdp VBoxNetFlt VBoxUSBMon VBoxUSB VBoxDrv;
do
    if sc query $drv > /dev/null; then
        STATE=`sc query $drv \
               | kmk_sed -e '/^ *STATE /!d' -e 's/^[^:]*: [0-9 ]*//' \
                         -e 's/  */ /g' \
                         -e 's/^ *//g' \
                         -e 's/ *$//g' \
              `
        echo "load.sh: $drv - $STATE"
    else
        echo "load.sh: $drv - not configured, probably."
    fi
done

set -e
set -x

#
# Invoke the uninstallers.
#
for uninst in NetAdpUninstall.exe NetFltUninstall.exe USBUninstall.exe SUPUninstall.exe;
do
    if test -f ${MY_DIR}/$uninst; then
        ${MY_DIR}/$uninst
    fi
done

#
# Invoke the installers.
#
for inst in SUPInstall.exe USBInstall.exe NetFltInstall.exe ; #NetAdpInstall.exe; - busted
do
    if test -f ${MY_DIR}/$inst; then
        ${MY_DIR}/$inst
    fi
done

echo "load.sh: Successfully installed all drivers"
exit 0

