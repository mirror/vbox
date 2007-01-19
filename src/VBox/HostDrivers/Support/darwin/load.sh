#!/bin/bash

#
# For development.
#

DIR="VBoxDrv.kext"

if [ ! -d "$DIR" ]; then
    echo "Cannot find $DIR or it's not a directory..."
    exit 1;
fi

sudo chown -R root:wheel "$DIR"
sudo chmod -R o-rwx "$DIR"
sudo kextload -t -v 6 "$DIR"
sudo chown -R `whoami` "$DIR"

