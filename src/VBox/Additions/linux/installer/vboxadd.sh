#! /bin/sh
# Sun VirtualBox
# Linux Additions kernel module init script ($Revision$)
#

#
# Copyright (C) 2006-2009 Sun Microsystems, Inc.
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
PACKAGE=VBoxGuestAdditions
BUILDVBOXGUEST=`/bin/ls /usr/src/vboxguest*/build_in_tmp 2>/dev/null|cut -d' ' -f1`
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

dev=/dev/vboxguest
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

running_vboxguest()
{
    lsmod | grep -q "vboxguest[^_-]"
}

running_vboxvfs()
{
    lsmod | grep -q "vboxvfs[^_-]"
}

start()
{
    begin "Starting the VirtualBox Guest Additions ";
    running_vboxguest || {
        rm -f $dev || {
            fail "Cannot remove $dev"
        }

        rm -f $userdev || {
            fail "Cannot remove $userdev"
        }

        modprobe vboxguest >/dev/null 2>&1 || {
            fail "modprobe vboxguest failed"
        }
        sleep .5
    }
    if [ ! -c $dev ]; then
        maj=`sed -n 's;\([0-9]\+\) vboxguest;\1;p' /proc/devices`
        if [ ! -z "$maj" ]; then
            min=0
        else
            min=`sed -n 's;\([0-9]\+\) vboxguest;\1;p' /proc/misc`
            if [ ! -z "$min" ]; then
                maj=10
            fi
        fi
        test -z "$maj" && {
            rmmod vboxguest 2>/dev/null
            fail "Cannot locate the VirtualBox device"
        }

        mknod -m 0664 $dev c $maj $min || {
            rmmod vboxguest 2>/dev/null
            fail "Cannot create device $dev with major $maj and minor $min"
        }
    fi
    chown $owner:$group $dev 2>/dev/null || {
        rm -f $dev 2>/dev/null
        rm -f $userdev 2>/dev/null
        rmmod vboxguest 2>/dev/null
        fail "Cannot change owner $owner:$group for device $dev"
    }

    if [ ! -c $userdev ]; then
        maj=10
        min=`sed -n 's;\([0-9]\+\) vboxuser;\1;p' /proc/misc`
        if [ ! -z "$min" ]; then
            mknod -m 0666 $userdev c $maj $min || {
                rm -f $dev 2>/dev/null
                rmmod vboxguest 2>/dev/null
                fail "Cannot create device $userdev with major $maj and minor $min"
            }
            chown $owner:$group $userdev 2>/dev/null || {
                rm -f $dev 2>/dev/null
                rm -f $userdev 2>/dev/null
                rmmod vboxguest 2>/dev/null
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
    if ! umount -a -t vboxsf 2>/dev/null; then
        fail "Cannot unmount vboxsf folders"
    fi
    if [ -n "$BUILDVBOXVFS" ]; then
        if running_vboxvfs; then
            rmmod vboxvfs 2>/dev/null || fail "Cannot unload module vboxvfs"
        fi
    fi
    if running_vboxguest; then
        rmmod vboxguest 2>/dev/null || fail "Cannot unload module vboxguest"
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

# setup_script
setup()
{
    # don't stop the old modules here -- they might be in use
    if find /lib/modules/`uname -r` -name "vboxvideo\.*" 2>/dev/null|grep -q vboxvideo; then
        begin "Removing old VirtualBox vboxvideo kernel module"
        find /lib/modules/`uname -r` -name "vboxvideo\.*" 2>/dev/null|xargs rm -f 2>/dev/null
        succ_msg
    fi
    if find /lib/modules/`uname -r` -name "vboxvfs\.*" 2>/dev/null|grep -q vboxvfs; then
        begin "Removing old VirtualBox vboxvfs kernel module"
        find /lib/modules/`uname -r` -name "vboxvfs\.*" 2>/dev/null|xargs rm -f 2>/dev/null
        succ_msg
    fi
    if find /lib/modules/`uname -r` -name "vboxguest\.*" 2>/dev/null|grep -q vboxguest; then
        begin "Removing old VirtualBox vboxguest kernel module"
        find /lib/modules/`uname -r` -name "vboxguest\.*" 2>/dev/null|xargs rm -f 2>/dev/null
        succ_msg
    fi
    begin "Building the VirtualBox Guest Additions kernel modules"
    if ! sh /usr/share/$PACKAGE/test/build_in_tmp \
        --no-dkms --no-print-directory > $LOG 2>&1; then
        fail "`printf "Your system does not seem to be set up to build kernel modules.\nLook at $LOG to find out what went wrong"`"
    fi
    echo
    if ! sh /usr/share/$PACKAGE/test_drm/build_in_tmp \
        --no-dkms --no-print-directory >> $LOG 2>&1; then
        printf "Your system does not seem to support OpenGL in the kernel (this requires\nLinux 2.6.27 or later).  The OpenGL support will not be built.\n"
        BUILDVBOXVIDEO=""
    fi
    begin "Building the main Guest Additions module"
    if ! $BUILDVBOXGUEST \
        --save-module-symvers /tmp/vboxguest-Module.symvers \
        --no-print-directory install >> $LOG 2>&1; then
        fail "Look at $LOG to find out what went wrong"
    fi
    succ_msg
    if [ -n "$BUILDVBOXVFS" ]; then
        begin "Building the shared folder support module"
        if ! $BUILDVBOXVFS \
            --use-module-symvers /tmp/vboxguest-Module.symvers \
            --no-print-directory install >> $LOG 2>&1; then
            fail "Look at $LOG to find out what went wrong"
        fi
        succ_msg
    fi
    if [ -n "$BUILDVBOXVIDEO" ]; then
        begin "Building the OpenGL support module"
        if ! $BUILDVBOXVIDEO \
            --use-module-symvers /tmp/vboxguest-Module.symvers \
            --no-print-directory install >> $LOG 2>&1; then
            fail "Look at $LOG to find out what went wrong"
        fi
        succ_msg
    fi
    depmod

    begin "Doing non-kernel setup of the Guest Additions"
    echo "Creating user for the Guest Additions." >> $LOG
    # This is the LSB version of useradd and should work on recent
    # distributions
    useradd -d /var/run/vboxadd -g 1 -r -s /bin/false vboxadd >/dev/null 2>&1
    # And for the others, we choose a UID ourselves
    useradd -d /var/run/vboxadd -g 1 -u 501 -o -s /bin/false vboxadd >/dev/null 2>&1

    # Create udev description file
    if [ -d /etc/udev/rules.d ]; then
        echo "Creating udev rule for the Guest Additions kernel module." >> $LOG
        udev_call=""
        udev_app=`which udevadm 2> /dev/null`
        if [ $? -eq 0 ]; then
            udev_call="${udev_app} version 2> /dev/null"
        else
            udev_app=`which udevinfo 2> /dev/null`
            if [ $? -eq 0 ]; then
                udev_call="${udev_app} -V 2> /dev/null"
            fi
        fi
        udev_fix="="
        if [ "${udev_call}" != "" ]; then
            udev_out=`${udev_call}`
            udev_ver=`expr "$udev_out" : '[^0-9]*\([0-9]*\)'`
            if [ "$udev_ver" = "" -o "$udev_ver" -lt 55 ]; then
               udev_fix=""
            fi
        fi
        ## @todo 60-vboxadd.rules -> 60-vboxguest.rules ?
        echo "KERNEL=${udev_fix}\"vboxguest\", NAME=\"vboxguest\", OWNER=\"vboxadd\", MODE=\"0660\"" > /etc/udev/rules.d/60-vboxadd.rules
        echo "KERNEL=${udev_fix}\"vboxuser\", NAME=\"vboxuser\", OWNER=\"vboxadd\", MODE=\"0666\"" >> /etc/udev/rules.d/60-vboxadd.rules
    fi

    # Put mount.vboxsf in the right place
    ln -sf /usr/lib/$PACKAGE/mount.vboxsf /sbin

    succ_msg
    start
    echo
    echo "You should reboot your guest to make sure the new modules are actually used"
}

# cleanup_script
cleanup()
{
    # Delete old versions of VBox modules.
    DKMS=`which dkms 2>/dev/null`
    if [ -n "$DKMS" ]; then
      echo "Attempt to remove old DKMS modules..."
      for mod in vboxadd vboxguest vboxvfs vboxvideo; do
        $DKMS status -m $mod | while read line; do
          if echo "$line" | grep -q added > /dev/null ||
             echo "$line" | grep -q built > /dev/null ||
             echo "$line" | grep -q installed > /dev/null; then
            version=`echo "$line" | sed "s/$mod,\([^,]*\)[,:].*/\1/;t;d"`
            echo "  removing module $mod version $version"
            $DKMS remove -m $mod -v $version --all 2>/dev/null
          fi
        done
      done
      echo "Done."
    fi

    # Remove old installed modules
    find /lib/modules -name vboxadd\* | xargs rm 2>/dev/null
    find /lib/modules -name vboxguest\* | xargs rm 2>/dev/null
    find /lib/modules -name vboxvfs\* | xargs rm 2>/dev/null
    find /lib/modules -name vboxvideo\* | xargs rm 2>/dev/null
    depmod

    # Remove old module sources
    rm -rf /usr/src/vboxadd-* /usr/src/vboxguest-* /usr/src/vboxvfs-* /usr/src/vboxvideo-*

    # Remove other files
    rm /sbin/mount.vboxsf 2>/dev/null
    rm /etc/udev/rules.d/60-vboxadd.rules 2>/dev/null
}

dmnstatus()
{
    if running_vboxguest; then
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
cleanup)
    cleanup
    ;;
status)
    dmnstatus
    ;;
*)
    echo "Usage: $0 {start|stop|restart|status}"
    exit 1
esac

exit
