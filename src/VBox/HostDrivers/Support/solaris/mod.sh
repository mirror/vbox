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

if test "$BUILD_TARGET_ARCH" = "amd64"; then
    VBOXDRV_INS=/usr/kernel/drv/amd64/vboxdrv
else
    VBOXDRV_INS=/usr/kernel/drv/vboxdrv
fi
sudo cp $DIR/vboxdrv.o $VBOXDRV_INS
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
sudo /usr/sbin/modload $VBOXDRV_INS
/usr/sbin/modinfo | grep vboxdrv
echo "* dmesg:"
dmesg | tail -20
sudo chmod a+rw /devices/pseudo/vboxdrv*

