#!/bin/sh
#
# VirtualBox autostart service init script.
#
# Copyright (C) 2012-2015 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

# chkconfig: 345 35 65
# description: VirtualBox autostart service
#
### BEGIN INIT INFO
# Provides:       vboxautostart-service
# Required-Start: vboxdrv
# Required-Stop:  vboxdrv
# Default-Start:  2 3 4 5
# Default-Stop:   0 1 6
# Description:    VirtualBox autostart service
### END INIT INFO

PATH=$PATH:/bin:/sbin:/usr/sbin

[ -f /etc/debian_release -a -f /lib/lsb/init-functions ] || NOLSB=yes
[ -f /etc/vbox/vbox.cfg ] && . /etc/vbox/vbox.cfg

if [ -n "$INSTALL_DIR" ]; then
    binary="$INSTALL_DIR/VBoxAutostart"
else
    binary="/usr/lib/virtualbox/VBoxAutostart"
fi

# silently exit if the package was uninstalled but not purged,
# applies to Debian packages only (but shouldn't hurt elsewhere)
[ ! -f /etc/debian_release -o -x $binary ] || exit 0

[ -r /etc/default/virtualbox ] && . /etc/default/virtualbox

system=unknown
if [ -f /etc/redhat-release ]; then
    system=redhat
elif [ -f /etc/SuSE-release ]; then
    system=suse
elif [ -f /etc/debian_version ]; then
    system=debian
elif [ -f /etc/gentoo-release ]; then
    system=gentoo
elif [ -f /etc/slackware-version ]; then
    system=slackware
elif [ -f /etc/lfs-release ]; then
    system=lfs
else
    system=other
fi

if [ -z "$NOLSB" ]; then
    . /lib/lsb/init-functions
    fail_msg() {
        echo ""
        log_failure_msg "$1"
    }
    succ_msg() {
        log_success_msg " done."
    }
    begin_msg() {
        log_daemon_msg "$@"
    }
fi

if [ "$system" = "redhat" ]; then
    . /etc/init.d/functions
    if [ -n "$NOLSB" ]; then
        start_daemon() {
            usr="$1"
            shift
            daemon --user $usr $@
        }
        fail_msg() {
            echo_failure
            echo
        }
        succ_msg() {
            echo_success
            echo
        }
        begin_msg() {
            echo -n "$1"
        }
    fi
fi

if [ "$system" = "suse" ]; then
    . /etc/rc.status
    start_daemon() {
        usr="$1"
        shift
        su - $usr -c "$*"
    }
    if [ -n "$NOLSB" ]; then
        fail_msg() {
            rc_failed 1
            rc_status -v
        }
        succ_msg() {
            rc_reset
            rc_status -v
        }
        begin_msg() {
            echo -n "$1"
        }
    fi
fi

if [ "$system" = "debian" ]; then
    start_daemon() {
        usr="$1"
        shift
        bin="$1"
        shift
        start-stop-daemon --background --chuid $usr --start --exec $bin -- $@
    }
    killproc() {
        start-stop-daemon --stop --exec $@
    }
    if [ -n "$NOLSB" ]; then
        fail_msg() {
            echo " ...fail!"
        }
        succ_msg() {
            echo " ...done."
        }
        begin_msg() {
            echo -n "$1"
       }
    fi
fi

if [ "$system" = "gentoo" ]; then
    if [ -f /sbin/functions.sh ]; then
        . /sbin/functions.sh
    elif [ -f /etc/init.d/functions.sh ]; then
        . /etc/init.d/functions.sh
    fi
    start_daemon() {
        usr="$1"
        shift
        bin="$1"
        shift
        start-stop-daemon --background --chuid $usr --start --exec $bin -- $@
    }
    killproc() {
        start-stop-daemon --stop --exec $@
    }
    if [ -n "$NOLSB" ]; then
        fail_msg() {
            echo " ...fail!"
        }
        succ_msg() {
            echo " ...done."
        }
        begin_msg() {
            echo -n "$1"
        }
        if [ "`which $0`" = "/sbin/rc" ]; then
            shift
        fi
    fi
fi

if [ "$system" = "slackware" ]; then
    killproc() {
        killall $1
        rm -f $PIDFILE
    }
    if [ -n "$NOLSB" ]; then
        fail_msg() {
            echo " ...fail!"
        }
        succ_msg() {
            echo " ...done."
        }
        begin_msg() {
            echo -n "$1"
        }
    fi
    start_daemon() {
        usr="$1"
        shift
        su - $usr -c "$*"
    }
fi

if [ "$system" = "lfs" ]; then
    . /etc/rc.d/init.d/functions
    if [ -n "$NOLSB" ]; then
        fail_msg() {
            echo_failure
        }
        succ_msg() {
            echo_ok
        }
        begin_msg() {
            echo $1
        }
    fi
    start_daemon() {
        usr="$1"
        shift
        su - $usr -c "$*"
    }
    status() {
        statusproc $1
    }
fi

if [ "$system" = "other" ]; then
    if [ -n "$NOLSB" ]; then
        fail_msg() {
            echo " ...fail!"
        }
        succ_msg() {
            echo " ...done."
        }
        begin_msg() {
            echo -n "$1"
        }
    fi
fi

vboxdrvrunning() {
    lsmod | grep -q "vboxdrv[^_-]"
}

start() {
    [ -z "$VBOXAUTOSTART_DB" ] && exit 0
    [ -z "$VBOXAUTOSTART_CONFIG" ] && exit 0
    begin_msg "Starting VirtualBox VMs configured for autostart";
    vboxdrvrunning || {
        fail_msg "VirtualBox kernel module not loaded!"
        exit 0
    }
    PARAMS="--background --start --config $VBOXAUTOSTART_CONFIG"

    # prevent inheriting this setting to VBoxSVC
    unset VBOX_RELEASE_LOG_DEST

    for user in `ls $VBOXAUTOSTART_DB/*.start`
    do
        start_daemon `basename $user | sed -ne "s/\(.*\).start/\1/p"` $binary $PARAMS > /dev/null 2>&1
    done

    return $RETVAL
}

stop() {
    [ -z "$VBOXAUTOSTART_DB" ] && exit 0
    [ -z "$VBOXAUTOSTART_CONFIG" ] && exit 0

    PARAMS="--stop --config $VBOXAUTOSTART_CONFIG"

    # prevent inheriting this setting to VBoxSVC
    unset VBOX_RELEASE_LOG_DEST

    for user in `ls $VBOXAUTOSTART_DB/*.stop`
    do
        start_daemon `basename $user | sed -ne "s/\(.*\).stop/\1/p"` $binary $PARAMS > /dev/null 2>&1
    done

    return $RETVAL
}

case "$1" in
start)
    start
    ;;
stop)
    stop
    ;;
*)
    echo "Usage: $0 {start|stop}"
    exit 1
esac

exit $RETVAL
