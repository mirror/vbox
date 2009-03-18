#! /bin/sh
# Sun xVM VirtualBox
# Linux Additions kernel module init script
#
# Copyright (C) 2006-2007 Sun Microsystems, Inc.
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


# chkconfig: 35 30 70
# description: VirtualBox Linux Additions kernel modules
#
### BEGIN INIT INFO
# Provides:       vboxadd
# Required-Start:
# Required-Stop:
# Default-Start:  2 3 4 5
# Default-Stop:   0 1 6
# Description:    VirtualBox Linux Additions kernel modules
### END INIT INFO

PATH=$PATH:/bin:/sbin:/usr/sbin
BUILDVBOXADD=`/bin/ls /usr/src/vboxadd*/build_in_tmp 2>/dev/null|cut -d' ' -f1`
BUILDVBOXVFS=`/bin/ls /usr/src/vboxvfs*/build_in_tmp 2>/dev/null|cut -d' ' -f1`
BUILDVBOXVIDEO=`/bin/ls /usr/src/vboxvideo*/build_in_tmp 2>/dev/null|cut -d' ' -f1`
LOG="/var/log/vboxadd-install.log"

if [ -f /etc/arch-release ]; then
    system=arch
elif [ -f /etc/redhat-release ]; then
    system=redhat
elif [ -f /etc/SuSE-release ]; then
    system=suse
elif [ -f /etc/gentoo-release ]; then
    system=gentoo
elif [ -f /etc/lfs-release -a -d /etc/rc.d/init.d ]; then
    system=lfs
else
    system=other
fi

if [ "$system" = "arch" ]; then
    USECOLOR=yes
    . /etc/rc.d/functions
    fail_msg() {
        stat_fail
    }

    succ_msg() {
        stat_done
    }

    begin() {
        stat_busy "$1"
    }
fi

if [ "$system" = "redhat" ]; then
    . /etc/init.d/functions
    fail_msg() {
        echo_failure
        echo
    }
    succ_msg() {
        echo_success
        echo
    }
    begin() {
        echo -n "$1"
    }
fi

if [ "$system" = "suse" ]; then
    . /etc/rc.status
    fail_msg() {
        rc_failed 1
        rc_status -v
    }
    succ_msg() {
        rc_reset
        rc_status -v
    }
    begin() {
        echo -n "$1"
    }
fi

if [ "$system" = "gentoo" ]; then
    if [ -f /sbin/functions.sh ]; then
        . /sbin/functions.sh
    elif [ -f /etc/init.d/functions.sh ]; then
        . /etc/init.d/functions.sh
    fi
    fail_msg() {
        eend 1
    }
    succ_msg() {
        eend $?
    }
    begin() {
        ebegin $1
    }
    if [ "`which $0`" = "/sbin/rc" ]; then
        shift
    fi
fi

if [ "$system" = "lfs" ]; then
    . /etc/rc.d/init.d/functions
    fail_msg() {
        echo_failure
    }
    succ_msg() {
        echo_ok
    }
    begin() {
        echo $1
    }
fi

if [ "$system" = "other" ]; then
    fail_msg() {
        echo " ...fail!"
    }
    succ_msg() {
        echo " ...done."
    }
    begin() {
        echo -n $1
    }
fi

dev=/dev/vboxadd
userdev=/dev/vboxuser
owner=vboxadd
group=1

fail()
{
    if [ "$system" = "gentoo" ]; then
        eerror $1
        exit 1
    fi
    fail_msg
    echo "($1)"
    exit 1
}

running_vboxadd()
{
    lsmod | grep -q "vboxadd[^_-]"
}

running_vboxvfs()
{
    lsmod | grep -q "vboxvfs[^_-]"
}

start()
{
    begin "Starting VirtualBox Additions ";
    running_vboxadd || {
        rm -f $dev || {
            fail "Cannot remove $dev"
        }

        rm -f $userdev || {
            fail "Cannot remove $userdev"
        }

        modprobe vboxadd >/dev/null 2>&1 || {
            fail "modprobe vboxadd failed"
        }
        sleep .5
    }
    if [ ! -c $dev ]; then
        maj=`sed -n 's;\([0-9]\+\) vboxadd;\1;p' /proc/devices`
        if [ ! -z "$maj" ]; then
            min=0
        else
            min=`sed -n 's;\([0-9]\+\) vboxadd;\1;p' /proc/misc`
            if [ ! -z "$min" ]; then
                maj=10
            fi
        fi
        test -z "$maj" && {
            rmmod vboxadd 2>/dev/null
            fail "Cannot locate the VirtualBox device"
        }

        mknod -m 0664 $dev c $maj $min || {
            rmmod vboxadd 2>/dev/null
            fail "Cannot create device $dev with major $maj and minor $min"
        }
    fi
    chown $owner:$group $dev 2>/dev/null || {
        rm -f $dev 2>/dev/null
        rm -f $userdev 2>/dev/null
        rmmod vboxadd 2>/dev/null
        fail "Cannot change owner $owner:$group for device $dev"
    }

    if [ ! -c $userdev ]; then
        maj=10
        min=`sed -n 's;\([0-9]\+\) vboxuser;\1;p' /proc/misc`
        if [ ! -z "$min" ]; then
            mknod -m 0666 $userdev c $maj $min || {
                rm -f $dev 2>/dev/null
                rmmod vboxadd 2>/dev/null
                fail "Cannot create device $userdev with major $maj and minor $min"
            }
            chown $owner:$group $userdev 2>/dev/null || {
                rm -f $dev 2>/dev/null
                rm -f $userdev 2>/dev/null
                rmmod vboxadd 2>/dev/null
                fail "Cannot change owner $owner:$group for device $userdev"
            }
        fi
    fi

    if [ -n "$BUILDVBOXVFS" ]; then
        running_vboxvfs || {
            modprobe vboxvfs > /dev/null 2>&1 || {
                if dmesg | grep "vboxConnect failed" > /dev/null 2>&1; then
                    fail_msg
                    echo "Unable to start shared folders support.  Make sure that your VirtualBox build"
                    echo "supports this feature."
                    exit 1
                fi
                fail "modprobe vboxvfs failed"
            }
        }
    fi

    # Mount all shared folders from /etc/fstab. Normally this is done by some
    # other startup script but this requires the vboxdrv kernel module loaded.
    mount -a -t vboxsf

    succ_msg
    return 0
}

stop()
{
    begin "Stopping VirtualBox Additions ";
    if !umount -a -t vboxsf 2>/dev/null; then
        fail "Cannot unmount vboxsf folders"
    fi
    if [ -n "$BUILDVBOXVFS" ]; then
        if running_vboxvfs; then
            rmmod vboxvfs 2>/dev/null || fail "Cannot unload module vboxvfs"
        fi
    fi
    if running_vboxadd; then
        rmmod vboxadd 2>/dev/null || fail "Cannot unload module vboxadd"
        rm -f $userdev || fail "Cannot unlink $userdev"
        rm -f $dev || fail "Cannot unlink $dev"
    fi
    succ_msg
    return 0
}

restart()
{
    stop && start
    return 0
}

setup()
{
    # don't stop the old modules here -- they might be in use
    if find /lib/modules/`uname -r` -name "vboxvfs\.*" 2>/dev/null|grep -q vboxvfs; then
        begin "Removing old VirtualBox vboxvfs kernel module"
        find /lib/modules/`uname -r` -name "vboxvfs\.*" 2>/dev/null|xargs rm -f 2>/dev/null
        succ_msg
    fi  
    if find /lib/modules/`uname -r` -name "vboxadd\.*" 2>/dev/null|grep -q vboxadd; then
        begin "Removing old VirtualBox vboxadd kernel module"
        find /lib/modules/`uname -r` -name "vboxadd\.*" 2>/dev/null|xargs rm -f 2>/dev/null
        succ_msg
    fi
    begin "Recompiling VirtualBox kernel modules"
    if ! $BUILDVBOXADD \
        --save-module-symvers /tmp/vboxadd-Module.symvers \
        --no-print-directory install > $LOG 2>&1; then
        fail "Look at $LOG to find out what went wrong"
    fi
    if [ -n "$BUILDVBOXVFS" ]; then
        if ! $BUILDVBOXVFS \
            --use-module-symvers /tmp/vboxadd-Module.symvers \
            --no-print-directory install >> $LOG 2>&1; then
            fail "Look at $LOG to find out what went wrong"
        fi
    fi
    if [ -n "$BUILDVBOXVIDEO" ]; then
        if ! $BUILDVBOXVIDEO \
            --use-module-symvers /tmp/vboxadd-Module.symvers \
            --no-print-directory install >> $LOG 2>&1; then
            fail "Look at $LOG to find out what went wrong"
        fi
    fi
    succ_msg
    start
    echo
    echo "You should reboot your guest to make sure the new modules are actually used"
}

dmnstatus()
{
    if running_vboxadd; then
        echo "The VirtualBox Additions are currently running."
    else
        echo "The VirtualBox Additions are not currently running."
    fi
}

case "$1" in
start)
    start
    ;;
stop)
    stop
    ;;
restart)
    restart
    ;;
setup)
    setup
    ;;
status)
    dmnstatus
    ;;
*)
    echo "Usage: $0 {start|stop|restart|status}"
    exit 1
esac

exit
