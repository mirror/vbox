#!/bin/bash

#
# For development.
#

DIR="VBoxDrv.kext"

if [ ! -d "$DIR" ]; then
    echo "Cannot find $DIR or it's not a directory..."
    exit 1;
fi

trap "sudo chown -R `whoami` $DIR; exit 1" INT
set -x
sudo chown -R root:wheel "$DIR"
sudo chmod -R o-rwx "$DIR"
sync ; sleep 1; sync
sudo kextload -t "$DIR"
sudo chown -R `whoami` "$DIR"



