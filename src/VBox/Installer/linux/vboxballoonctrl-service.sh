#!/bin/sh
#
# VirtualBox watchdog daemon init script.
#
# Copyright (C) 2006-2015 Oracle Corporation
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
# description: VirtualBox watchdog daemon
#
### BEGIN INIT INFO
# Provides:       vboxballoonctrl-service
# Required-Start: vboxdrv
# Required-Stop:  vboxdrv
# Default-Start:  2 3 4 5
# Default-Stop:   0 1 6
# Description:    VirtualBox watchdog daemon
### END INIT INFO

PATH=$PATH:/bin:/sbin:/usr/sbin

[ -f /lib/lsb/init-functions -a -f /etc/debian_release ] || NOLSB=yes
[ -f /etc/vbox/vbox.cfg ] && . /etc/vbox/vbox.cfg

if [ -n "$INSTALL_DIR" ]; then
    binary="$INSTALL_DIR/VBoxBalloonCtrl"
else
    binary="/usr/lib/virtualbox/VBoxBalloonCtrl"
fi

# silently exit if the package was uninstalled but not purged,
# applies to Debian packages only (but shouldn't hurt elsewhere)
[ ! -f /etc/debian_release -o -x $binary ] || exit 0

[ -r /etc/default/virtualbox ] && . /etc/default/virtualbox

system=unknown
if [ -f /etc/redhat-release ]; then
    system=redhat
    PIDFILE="/var/lock/subsys/vboxballoonctrl-service"
elif [ -f /etc/SuSE-release ]; then
    system=suse
    PIDFILE="/var/lock/subsys/vboxballoonctrl-service"
elif [ -f /etc/debian_version ]; then
    system=debian
    PIDFILE="/var/run/vboxballoonctrl-service"
elif [ -f /etc/gentoo-release ]; then
    system=gentoo
    PIDFILE="/var/run/vboxballoonctrl-service"
elif [ -f /etc/slackware-version ]; then
    system=slackware
    PIDFILE="/var/run/vboxballoonctrl-service"
elif [ -f /etc/lfs-release ]; then
    system=lfs
    PIDFILE="/var/run/vboxballoonctrl-service.pid"
else
    system=other
    if [ -d /var/run -a -w /var/run ]; then
        PIDFILE="/var/run/vboxballoonctrl-service"
    fi
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

check_single_user() {
    if [ -n "$2" ]; then
        fail_msg "VBOXWATCHDOG_USER must not contain multiple users!"
        exit 1
    fi
}

start() {
    if ! test -f $PIDFILE; then
        [ -z "$VBOXWATCHDOG_USER" -a -z "$VBOXBALLOONCTRL_USER" ] && exit 0
        [ -z "$VBOXWATCHDOG_USER" ] && VBOXWATCHDOG_USER="$VBOXBALLOONCTRL_USER"
        begin_msg "Starting VirtualBox watchdog service";
        check_single_user $VBOXWATCHDOG_USER
        vboxdrvrunning || {
            fail_msg "VirtualBox kernel module not loaded!"
            exit 0
        }
        # Handle legacy parameters, do not add any further ones unless absolutely necessary.
        [ -z "$VBOXWATCHDOG_BALLOON_INTERVAL" -a -n "$VBOXBALLOONCTRL_INTERVAL" ]           && VBOXWATCHDOG_BALLOON_INTERVAL="$VBOXBALLOONCTRL_INTERVAL"
        [ -z "$VBOXWATCHDOG_BALLOON_INCREMENT" -a -n "$VBOXBALLOONCTRL_INCREMENT" ]         && VBOXWATCHDOG_BALLOON_INCREMENT="$VBOXBALLOONCTRL_INCREMENT"
        [ -z "$VBOXWATCHDOG_BALLOON_DECREMENT" -a -n "$VBOXBALLOONCTRL_DECREMENT" ]         && VBOXWATCHDOG_BALLOON_DECREMENT="$VBOXBALLOONCTRL_DECREMENT"
        [ -z "$VBOXWATCHDOG_BALLOON_LOWERLIMIT" -a -n "$VBOXBALLOONCTRL_LOWERLIMIT" ]       && VBOXWATCHDOG_BALLOON_LOWERLIMIT="$VBOXBALLOONCTRL_LOWERLIMIT"
        [ -z "$VBOXWATCHDOG_BALLOON_SAFETYMARGIN" -a -n "$VBOXBALLOONCTRL_SAFETYMARGIN" ]   && VBOXWATCHDOG_BALLOON_SAFETYMARGIN="$VBOXBALLOONCTRL_SAFETYMARGIN"
        [ -z "$VBOXWATCHDOG_ROTATE" -a -n "$VBOXBALLOONCTRL_ROTATE" ]           && VBOXWATCHDOG_ROTATE="$VBOXBALLOONCTRL_ROTATE"
        [ -z "$VBOXWATCHDOG_LOGSIZE" -a -n "$VBOXBALLOONCTRL_LOGSIZE" ]         && VBOXWATCHDOG_LOGSIZE="$VBOXBALLOONCTRL_LOGSIZE"
        [ -z "$VBOXWATCHDOG_LOGINTERVAL" -a -n "$VBOXBALLOONCTRL_LOGINTERVAL" ] && VBOXWATCHDOG_LOGINTERVAL="$VBOXBALLOONCTRL_LOGINTERVAL"

        PARAMS="--background"
        [ -n "$VBOXWATCHDOG_BALLOON_INTERVAL" ]     && PARAMS="$PARAMS --balloon-interval \"$VBOXWATCHDOG_BALLOON_INTERVAL\""
        [ -n "$VBOXWATCHDOG_BALLOON_INCREMENT" ]    && PARAMS="$PARAMS --balloon-inc \"$VBOXWATCHDOG_BALLOON_INCREMENT\""
        [ -n "$VBOXWATCHDOG_BALLOON_DECREMENT" ]    && PARAMS="$PARAMS --balloon-dec \"$VBOXWATCHDOG_BALLOON_DECREMENT\""
        [ -n "$VBOXWATCHDOG_BALLOON_LOWERLIMIT" ]   && PARAMS="$PARAMS --balloon-lower-limit \"$VBOXWATCHDOG_BALLOON_LOWERLIMIT\""
        [ -n "$VBOXWATCHDOG_BALLOON_SAFETYMARGIN" ] && PARAMS="$PARAMS --balloon-safety-margin \"$VBOXWATCHDOG_BALLOON_SAFETYMARGIN\""
        [ -n "$VBOXWATCHDOG_ROTATE" ]       && PARAMS="$PARAMS -R \"$VBOXWATCHDOG_ROTATE\""
        [ -n "$VBOXWATCHDOG_LOGSIZE" ]      && PARAMS="$PARAMS -S \"$VBOXWATCHDOG_LOGSIZE\""
        [ -n "$VBOXWATCHDOG_LOGINTERVAL" ]  && PARAMS="$PARAMS -I \"$VBOXWATCHDOG_LOGINTERVAL\""
        # prevent inheriting this setting to VBoxSVC
        unset VBOX_RELEASE_LOG_DEST
        start_daemon $VBOXWATCHDOG_USER $binary $PARAMS > /dev/null 2>&1
        # ugly: wait until the final process has forked
        sleep .1
        PID=`pidof $binary 2>/dev/null`
        if [ -n "$PID" ]; then
            echo "$PID" > $PIDFILE
            RETVAL=0
            succ_msg
        else
            RETVAL=1
            fail_msg
        fi
    fi
    return $RETVAL
}

stop() {
    if test -f $PIDFILE; then
        begin_msg "Stopping VirtualBox watchdog service";
        killproc $binary
        RETVAL=$?
        if ! pidof $binary > /dev/null 2>&1; then
            rm -f $PIDFILE
            succ_msg
        else
            fail_msg
        fi
    fi
    return $RETVAL
}

restart() {
    stop && start
}

status() {
    echo -n "Checking for VBox watchdog service"
    if [ -f $PIDFILE ]; then
        echo " ...running"
    else
        echo " ...not running"
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
force-reload)
    restart
    ;;
status)
    status
    ;;
setup)
    ;;
cleanup)
    ;;
*)
    echo "Usage: $0 {start|stop|restart|status}"
    exit 1
esac

exit $RETVAL
