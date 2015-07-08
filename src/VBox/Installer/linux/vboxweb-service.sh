#!/bin/sh
#
# VirtualBox web service API daemon init script.
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
# description: VirtualBox web service API
#
### BEGIN INIT INFO
# Provides:       vboxweb-service
# Required-Start: vboxdrv
# Required-Stop:  vboxdrv
# Default-Start:  2 3 4 5
# Default-Stop:   0 1 6
# Description:    VirtualBox web service API
### END INIT INFO

PATH=$PATH:/bin:/sbin:/usr/sbin

[ -f /etc/debian_release -a -f /lib/lsb/init-functions ] || NOLSB=yes
[ -f /etc/vbox/vbox.cfg ] && . /etc/vbox/vbox.cfg

if [ -n "$INSTALL_DIR" ]; then
    binary="$INSTALL_DIR/vboxwebsrv"
    vboxmanage="$INSTALL_DIR/VBoxManage"
else
    binary="/usr/lib/virtualbox/vboxwebsrv"
    vboxmanage="/usr/lib/virtualbox/VBoxManage"
fi

# silently exit if the package was uninstalled but not purged,
# applies to Debian packages only (but shouldn't hurt elsewhere)
[ ! -f /etc/debian_release -o -x $binary ] || exit 0

[ -r /etc/default/virtualbox ] && . /etc/default/virtualbox

system=unknown
if [ -f /etc/redhat-release ]; then
    system=redhat
    PIDFILE="/var/lock/subsys/vboxweb-service"
elif [ -f /etc/SuSE-release ]; then
    system=suse
    PIDFILE="/var/lock/subsys/vboxweb-service"
elif [ -f /etc/debian_version ]; then
    system=debian
    PIDFILE="/var/run/vboxweb-service"
elif [ -f /etc/gentoo-release ]; then
    system=gentoo
    PIDFILE="/var/run/vboxweb-service"
elif [ -f /etc/slackware-version ]; then
    system=slackware
    PIDFILE="/var/run/vboxweb-service"
elif [ -f /etc/lfs-release ]; then
    system=lfs
    PIDFILE="/var/run/vboxweb-service.pid"
else
    system=other
    if [ -d /var/run -a -w /var/run ]; then
        PIDFILE="/var/run/vboxweb-service"
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
        fail_msg "VBOXWEB_USER must not contain multiple users!"
        exit 1
    fi
}

start() {
    if ! test -f $PIDFILE; then
        [ -z "$VBOXWEB_USER" ] && exit 0
        begin_msg "Starting VirtualBox web service";
        check_single_user $VBOXWEB_USER
        vboxdrvrunning || {
            fail_msg "VirtualBox kernel module not loaded!"
            exit 0
        }
        PARAMS="--background"
        [ -n "$VBOXWEB_HOST" ]           && PARAMS="$PARAMS -H $VBOXWEB_HOST"
        [ -n "$VBOXWEB_PORT" ]           && PARAMS="$PARAMS -p $VBOXWEB_PORT"
        [ -n "$VBOXWEB_SSL_KEYFILE" ]    && PARAMS="$PARAMS -s -K $VBOXWEB_SSL_KEYFILE"
        [ -n "$VBOXWEB_SSL_PASSWORDFILE" ] && PARAMS="$PARAMS -a $VBOXWEB_SSL_PASSWORDFILE"
        [ -n "$VBOXWEB_SSL_CACERT" ]     && PARAMS="$PARAMS -c $VBOXWEB_SSL_CACERT"
        [ -n "$VBOXWEB_SSL_CAPATH" ]     && PARAMS="$PARAMS -C $VBOXWEB_SSL_CAPATH"
        [ -n "$VBOXWEB_SSL_DHFILE" ]     && PARAMS="$PARAMS -D $VBOXWEB_SSL_DHFILE"
        [ -n "$VBOXWEB_SSL_RANDFILE" ]   && PARAMS="$PARAMS -r $VBOXWEB_SSL_RANDFILE"
        [ -n "$VBOXWEB_TIMEOUT" ]        && PARAMS="$PARAMS -t $VBOXWEB_TIMEOUT"
        [ -n "$VBOXWEB_CHECK_INTERVAL" ] && PARAMS="$PARAMS -i $VBOXWEB_CHECK_INTERVAL"
        [ -n "$VBOXWEB_THREADS" ]        && PARAMS="$PARAMS -T $VBOXWEB_THREADS"
        [ -n "$VBOXWEB_KEEPALIVE" ]      && PARAMS="$PARAMS -k $VBOXWEB_KEEPALIVE"
        [ -n "$VBOXWEB_AUTHENTICATION" ] && PARAMS="$PARAMS -A $VBOXWEB_AUTHENTICATION"
        [ -n "$VBOXWEB_LOGFILE" ]        && PARAMS="$PARAMS -F $VBOXWEB_LOGFILE"
        [ -n "$VBOXWEB_ROTATE" ]         && PARAMS="$PARAMS -R $VBOXWEB_ROTATE"
        [ -n "$VBOXWEB_LOGSIZE" ]        && PARAMS="$PARAMS -S $VBOXWEB_LOGSIZE"
        [ -n "$VBOXWEB_LOGINTERVAL" ]    && PARAMS="$PARAMS -I $VBOXWEB_LOGINTERVAL"
        # set authentication method + password hash
        if [ -n "$VBOXWEB_AUTH_LIBRARY" ]; then
            su - "$VBOXWEB_USER" -c "$vboxmanage setproperty websrvauthlibrary \"$VBOXWEB_AUTH_LIBRARY\""
            if [ $? -ne 0 ]; then
                fail_msg "Error $? setting webservice authentication library to $VBOXWEB_AUTH_LIBRARY"
            fi
        fi
        if [ -n "$VBOXWEB_AUTH_PWHASH" ]; then
            su - "$VBOXWEB_USER" -c "$vboxmanage setextradata global \"VBoxAuthSimple/users/$VBOXWEB_USER\" \"$VBOXWEB_AUTH_PWHASH\""
            if [ $? -ne 0 ]; then
                fail_msg "Error $? setting webservice password hash"
            fi
        fi
        # prevent inheriting this setting to VBoxSVC
        unset VBOX_RELEASE_LOG_DEST
        start_daemon $VBOXWEB_USER $binary $PARAMS > /dev/null 2>&1
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
        begin_msg "Stopping VirtualBox web service";
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
    echo -n "Checking for VBox Web Service"
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
