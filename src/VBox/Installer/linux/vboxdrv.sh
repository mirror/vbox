#! /bin/sh
# innotek VirtualBox
# Linux kernel module init script

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

# chkconfig: 35 30 60
# description: VirtualBox Linux kernel module
#
### BEGIN INIT INFO
# Provides:       vboxdrv
# Required-Start: $syslog
# Required-Stop:
# Default-Start:  3 5
# Default-Stop:
# Description:    VirtualBox Linux kernel module
### END INIT INFO

PATH=$PATH:/bin:/sbin:/usr/sbin
CONFIG="/etc/vbox/vbox.cfg"

if [ -f /etc/redhat-release ]; then
    system=redhat
elif [ -f /etc/SuSE-release ]; then
    system=suse
elif [ -f /etc/gentoo-release ]; then
    system=gentoo
else
    system=other
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
    . /sbin/functions.sh
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


kdir=/lib/modules/`uname -r`/misc
dev=/dev/vboxdrv
modname=vboxdrv
groupname=vboxusers

fail() {
    if [ "$system" = "gentoo" ]; then
        eerror $1
        exit 1
    fi
    fail_msg
    echo "($1)"
    exit 1
}

running() {
    lsmod | grep -q "$modname[^_-]"
}

start() {
    begin "Starting VirtualBox kernel module "
    test -f "$kdir/$modname.o" -o -f "$kdir/$modname.ko" || {
        fail "Kernel module not found"
    }
    running || {
        rm -f $dev || {
            fail "Cannot remove $dev"
        }

        modprobe $modname > /dev/null 2>&1 || {
            fail "modprobe $modname failed"
        }

        sleep 1
    }
    if [ ! -c $dev ]; then
        maj=`sed -n 's;\([0-9]\+\) vboxdrv;\1;p' /proc/devices`
        if [ ! -z "$maj" ]; then
            min=0
        else
            min=`sed -n 's;\([0-9]\+\) vboxdrv;\1;p' /proc/misc`
            if [ ! -z "$min" ]; then
                maj=10
            fi
        fi
        test -z "$maj" && {
            rmmod $modname
            fail "Cannot locate the VirtualBox device"
        }

        mknod -m 0660 $dev c $maj $min || {
            rmmod $modname
            fail "Cannot create device $dev with major $maj and minor $min"
        }
    fi

    chown :$groupname $dev || {
        rmmod $modname
        fail "Cannot change owner $groupname for device $dev"
    }

    succ_msg
    return 0
}

stop() {
    begin "Stopping VirtualBox kernel module "
    if running; then
        rmmod $modname || fail "Cannot unload module $modname"
        rm -f $dev || fail "Cannot unlink $dev"
    fi
    succ_msg
    return 0
}

restart() {
    stop && start
    return 0
}

setup() {
    . "$CONFIG"
    stop
    begin "Recompiling VirtualBox kernel module "
    if ! $INSTALL_DIR/src/build_in_tmp install > /var/log/vbox-install.log 2>&1; then
        fail "Look at /var/log/vbox-install.log to find out what went wrong"
    fi
    succ_msg
    start
}

dmnstatus() {
    if running; then
        echo "VirtualBox kernel module is loaded."
    else
        echo "VirtualBox kernel module is not loaded."
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
    echo "Usage: $0 {start|stop|restart|status|setup}"
    exit 1
esac

exit
