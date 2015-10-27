#!/bin/sh
## @file
# For development, builds and loads all the host drivers.
#

#
# Copyright (C) 2010-2015 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

TARGET=`readlink -e -- "${0}"` || exit 1 # The GNU-specific way.
MY_DIR="${TARGET%/[!/]*}"

if test -f src/vboxdrv.ko; then
    echo "Cleaning build folder."
    make -C "${MY_DIR}/src" clean > /dev/null
fi
if test ${#} -eq 0; then
    sudo "${MY_DIR}/vboxdrv.sh" start
elif test ${#} = 1 && test "x${1}" = x-u; then
    sudo "${MY_DIR}/vboxdrv.sh" cleanup
else
    echo "Usage: loadall.sh [-u]"
    exit 1
fi
