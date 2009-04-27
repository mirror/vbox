#!/sbin/sh
# $Id$

# Copyright (C) 2008 Sun Microsystems, Inc.
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

#
# smf-vboxwebsrv method
#
# Argument is the method name (start, stop, ...)

. /lib/svc/share/smf_include.sh

VW_OPT="$1"
VW_EXIT=0

case $VW_OPT in
    start)
        if [ ! -x /opt/VirtualBox/vboxwebsrv ]; then
            echo "ERROR: /opt/VirtualBox/vboxwebsrv does not exist."
            return $SMF_EXIT_ERR_CONFIG
        fi

        if [ ! -f /opt/VirtualBox/vboxwebsrv ]; then
            echo "ERROR: /opt/VirtualBox/vboxwebsrv does not exist."
            return $SMF_EXIT_ERR_CONFIG
        fi

        # Get svc configuration
        VW_USER=`/usr/bin/svcprop -p config/user $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VW_USER=
        VW_HOST=`/usr/bin/svcprop -p config/host $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VW_HOST=
        VW_PORT=`/usr/bin/svcprop -p config/port $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VW_PORT=
        VW_TIMEOUT=`/usr/bin/svcprop -p config/timeout $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VW_TIMEOUT=
        VW_CHECK_INTERVAL=`/usr/bin/svcprop -p config/checkinterval $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VW_CHECK_INTERVAL=

        # Provide sensible defaults
        [ -z "$VW_USER" ] && VW_USER=root
        [ -z "$VW_HOST" ] && VW_HOST=localhost
        [ -z "$VW_PORT" -o "$VW_PORT" -eq 0 ] && VW_PORT=18083
        [ -z "$VW_TIMEOUT" ] && VW_TIMEOUT=20
        [ -z "$VW_CHECK_INTERVAL" ] && VW_CHECK_INTERVAL=5
        su - "$VW_USER" -c "/opt/VirtualBox/vboxwebsrv --background --host \"$VW_HOST\" --port \"$VW_PORT\" --timeout \"$VW_TIMEOUT\" --check-interval \"$VW_CHECK_INTERVAL\""

        VW_EXIT=$?
        if [ $VW_EXIT != 0 ]; then
            echo "vboxwebsrv failed with $VW_EXIT."
            VW_EXIT=1
        fi

        # Bump per-process semaphore limit of VBoxSVC
        PRCTLBIN=`which prctl`
        if test ! -f "$PRTCLBIN"; then
            # Wait for VBoxSVC to spawn
            TRIES=0
            while test $TRIES -le 3; do
                VBOXSVC_PID=`ps -eo pid,fname | grep VBoxSVC | grep -v grep | cut -f 1 -d " "`
                if test $VBOXSVC_PID -ge 0; then
                    $PRCTLBIN -r -n project.max-sem-ids -v 1024 $VBOXSVC_PID
                    if test $? -eq 0; then
                        echo "Successfully bumped VBoxSVC (pid $VBOXSVC_PID) semaphore id limit to 1024."
                    else
                        echo "Failed to bump VBoxSVC (pid $VBOXSVC_PID) semaphore id limit."
                    fi
                    break
                else
                    sleep 1
                fi
                TRIES=`expr $TRIES + 1`
            done
            if test $TRIES -eq 3; then
                echo "Stopped waiting for VBoxSVC to spawn..."
                echo "Failed to bump VBoxSVC process' semaphore id limit."
            fi
        else
            echo "Failed to find prctl to bump VBoxSVC semaphore id limit."
            echo "As a result, not more than 99 VMs can be started."
        fi
    ;;
    stop)
        # Kill service contract
        smf_kill_contract $2 TERM 1
    ;;
    *)
        VW_EXIT=$SMF_EXIT_ERR_CONFIG
    ;;
esac

exit $VW_EXIT
