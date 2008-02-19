#! /bin/sh
# innotek VirtualBox
# Linux kernel module init script

#
# Copyright (C) 2006-2007 innotek GmbH
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

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
. "$CONFIG"
VBOXMANAGE="$INSTALL_DIR/VBoxManage"

if [ -f /etc/redhat-release ]; then
    system=redhat
elif [ -f /etc/SuSE-release ]; then
    system=suse
elif [ -f /etc/gentoo-release ]; then
    system=gentoo
else
    system=other
fi

if [ -r /etc/default/virtualbox ]; then
  . /etc/default/virtualbox
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
    find /lib/modules/`uname -r` -name "vboxdrv\.*" 2>/dev/null|grep -q vboxdrv || {
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
            rmmod $modname 2>/dev/null
            fail "Cannot locate the VirtualBox device"
        }

        mknod -m 0660 $dev c $maj $min 2>/dev/null || {
            rmmod $modname 2>/dev/null
            fail "Cannot create device $dev with major $maj and minor $min"
        }
    fi

    chown :$groupname $dev 2>/dev/null || {
        rmmod $modname 2>/dev/null
        fail "Cannot change owner $groupname for device $dev"
    }

    succ_msg
    return 0
}

stop() {
    begin "Stopping VirtualBox kernel module "
    if running; then
        rmmod $modname 2>/dev/null || fail "Cannot unload module $modname"
        rm -f $dev || fail "Cannot unlink $dev"
    fi
    succ_msg
    return 0
}

# enter the following variables in /etc/default/virtualbox:
#   SHUTDOWN_USERS="foo bar"  
#     check for running VMs of user foo and user bar
#   SHUTDOWN=poweroff
#   SHUTDOWN=acpibutton
#   SHUTDOWN=savestate
#     select one of these shutdown methods for running VMs
stop_vms() {
    wait=0
    for i in $SHUTDOWN_USERS; do
        # don't create the ipcd directory with wrong permissions!
        if [ -d /tmp/.vbox-$i-ipc ]; then
            export VBOX_IPC_SOCKETID="$i"
            VMS=`$VBOXMANAGE -nologo list runningvms 2>/dev/null`
            if [ -n "$VMS" ]; then
                if [ "$SHUTDOWN" = "poweroff" ]; then
                    begin "Powering off remaining VMs "
                    for v in $VMS; do
                        $VBOXMANAGE -nologo controlvm $v poweroff
                    done
                    succ_msg
                elif [ "$SHUTDOWN" = "acpibutton" ]; then
                    begin "Sending ACPI power button event to remaining VMs "
                    for v in $VMS; do
                        $VBOXMANAGE -nologo controlvm $v acpipowerbutton
                        wait=15
                    done
                    succ_msg
                elif [ "$SHUTDOWN" = "savestate" ]; then
                    begin "Saving state of remaining VMs "
                    for v in $VMS; do
                        $VBOXMANAGE -nologo controlvm $v savestate
                    done
                    succ_msg
                fi
            fi
        fi
    done
    # wait for some seconds when doing ACPI shutdown
    if [ "$wait" -ne 0 ]; then
        log_daemon_msg "Waiting for $wait seconds for VM shutdown"
        sleep $wait
        log_end_msg
    fi
}

restart() {
    stop && start
    return 0
}

setup() {
    stop
    if find /lib/modules/`uname -r` -name "vboxdrv\.*" 2>/dev/null|grep -q vboxdrv; then
      begin "Removing old VirtualBox kernel module "
      find /lib/modules/`uname -r` -name "vboxdrv\.*" 2>/dev/null|xargs rm -f 2>/dev/null
      succ_msg
    fi
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
        for i in $SHUTDOWN_USERS; do
            # don't create the ipcd directory with wrong permissions!
            if [ -d /tmp/.vbox-$i-ipc ]; then
                export VBOX_IPC_SOCKETID="$i"
                VMS=`$VBOXMANAGE -nologo list runningvms 2>/dev/null`
                if [ -n "$VMS" ]; then
                    echo "The following VMs are currently running:"
                    for v in $VMS; do
                       echo "  $v"
                    done
                fi
            fi
        done
    else
        echo "VirtualBox kernel module is not loaded."
    fi
}

case "$1" in
start)
    start
    ;;
stop)
    stop_vms
    stop
    ;;
stop_vms)
    stop_vms
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
    echo "Usage: $0 {start|stop|stop_vms|restart|status|setup}"
    exit 1
esac

exit
