#!/bin/sh
set -x
#
# Figure out the environment and locations.
#

# Sudo isn't native solaris, but it's very convenient...
if test -z "$SUDO" && test "`whoami`" != "root"; then
    SUDO=sudo
fi

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

VBOXDRV_CONF_DIR=/platform/i86pc/kernel/drv
if test "$BUILD_TARGET_ARCH" = "amd64"; then
    VBOXDRV_DIR=$VBOXDRV_CONF_DIR/amd64
else
    VBOXDRV_DIR=$VBOXDRV_CONF_DIR
fi

#
# Do the job.
#
$SUDO cp $DIR/vboxdrv $VBOXDRV_DIR/vboxdrv
$SUDO cp $script_dir/src/VBox/HostDrivers/Support/solaris/vboxdrv.conf $VBOXDRV_CONF_DIR/vboxdrv.conf
old_id=`/usr/sbin/modinfo | grep vbox | cut -f 1 -d ' ' `
if test -n "$old_id"; then
    echo "* unloading $old_id..."
    sync
    sync
    $SUDO /usr/sbin/modunload -i $old_id
else
    echo "* If it fails below, run: $SUDO add_drv -m'* 0666 root sys' vboxdrv"
fi
if grep -q vboxdrv /etc/devlink.tab; then
    echo "* vboxdrv already present in /etc/devlink.tab"
else 
    echo "* Adding vboxdrv to /etc/devlink.tab"
    $SUDO rm -f /tmp/devlink.tab.vboxdrv
    echo "" > /tmp/devlink.tab.vboxdrv
    echo '# vbox' >> /tmp/devlink.tab.vboxdrv
    echo 'type=ddi_pseudo;name=vboxdrv	\D' >> /tmp/devlink.tab.vboxdrv
    $SUDO /bin/sh -c 'cat /tmp/devlink.tab.vboxdrv >> /etc/devlink.tab'
fi

echo "* loading vboxdrv..."
sync
sync
$SUDO /usr/sbin/modload $VBOXDRV_DIR/vboxdrv
/usr/sbin/modinfo | grep vboxdrv
echo "* dmesg:"
dmesg | tail -20
if test ! -h /dev/vboxdrv; then
    $SUDO /usr/sbin/devfsadm -i vboxdrv 
fi

