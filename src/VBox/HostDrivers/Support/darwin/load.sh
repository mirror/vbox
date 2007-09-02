#!/bin/bash

#
# For development.
#
#  Copyright (C) 2006-2007 innotek GmbH
# 
#  This file is part of VirtualBox Open Source Edition (OSE), as
#  available from http://www.virtualbox.org. This file is free software;
#  you can redistribute it and/or modify it under the terms of the GNU
#  General Public License as published by the Free Software Foundation,
#  in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
#  distribution. VirtualBox OSE is distributed in the hope that it will
#  be useful, but WITHOUT ANY WARRANTY of any kind.


DIR="VBoxDrv.kext"

if [ ! -d "$DIR" ]; then
    echo "Cannot find $DIR or it's not a directory..."
    exit 1;
fi
if [ -n "$*" ]; then
  OPTS="$*"
else
  OPTS="-t"
fi

trap "sudo chown -R `whoami` $DIR; exit 1" INT
set -x
sudo chown -R root:wheel "$DIR"
sudo chmod -R o-rwx "$DIR"
sync
sudo kextload $OPTS "$DIR"
sudo chown -R `whoami` "$DIR"
sudo chmod 666 /dev/vboxdrv
