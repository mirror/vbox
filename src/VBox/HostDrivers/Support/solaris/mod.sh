#!/bin/sh



script_dir=`dirname "$0"`
# src/VBox/HostDrivers/solaris/ residence:
script_dir=`cd "$script_dir/../../../../.." ; /bin/pwd`
## root residence:
#script_dir=`cd "$script_dir" ; /bin/pwd`

set -e
if test -z "$BUILD_TARGET"; then
    export BUILD_TARGET=solaris
fi
if test -z "$BUILD_TARGET_ARCH"; then
    export BUILD_TARGET_ARCH=x86
fi
if test -z "$BUILD_TYPE"; then
    export BUILD_TYPE=debug
fi

DIR=$script_dir/out/$BUILD_TARGET.$BUILD_TARGET_ARCH/$BUILD_TYPE/bin/

sudo cp $DIR/vboxdrv.o /usr/kernel/drv/vboxdrv
sudo cp $script_dir/src/VBox/HostDrivers/Support/solaris/vboxdrv.conf /usr/kernel/drv/vboxdrv.conf
old_id=`/usr/sbin/modinfo | grep vbox | cut -f 1 -d ' ' `
if test -n "$old_id"; then
    echo "* unloading $old_id..."
    sync
    sync
    sudo /usr/sbin/modunload -i $old_id
else
    echo "* If it fails below, run: sudo add_drv vboxdrv"
fi
echo "* loading vboxdrv..."
sync
sync
sudo /usr/sbin/modload /usr/kernel/drv/vboxdrv
/usr/sbin/modinfo | grep vboxdrv
echo "* dmesg:"
dmesg | tail -20
sudo chmod a+rw /devices/pseudo/vboxdrv*

