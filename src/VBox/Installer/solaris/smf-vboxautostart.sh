#!/sbin/sh
# $Id$

# Copyright (C) 2012 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

#
# smf-vboxautostart method
#
# Argument is the method name (start, stop, ...)

. /lib/svc/share/smf_include.sh

VW_OPT="$1"
VW_EXIT=0

case $VW_OPT in
    start)
        if [ ! -x /opt/VirtualBox/VBoxAutostart ]; then
            echo "ERROR: /opt/VirtualBox/VBoxAutostart does not exist."
            return $SMF_EXIT_ERR_CONFIG
        fi

        if [ ! -f /opt/VirtualBox/VBoxAutostart ]; then
            echo "ERROR: /opt/VirtualBox/VBoxAutostart does not exist."
            return $SMF_EXIT_ERR_CONFIG
        fi

        # Get svc configuration
        VW_CONFIG=`/usr/bin/svcprop -p config/config $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VW_CONFIG=
        VW_ROTATE=`/usr/bin/svcprop -p config/logrotate $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VW_ROTATE=
        VW_LOGSIZE=`/usr/bin/svcprop -p config/logsize $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VW_LOGSIZE=
        VW_LOGINTERVAL=`/usr/bin/svcprop -p config/loginterval $SMF_FMRI 2>/dev/null`
        [ $? != 0 ] && VW_LOGINTERVAL=

        # Provide sensible defaults
        [ -z "$VW_CONFIG" ] && VW_CONFIG=/etc/vbox/autostart.cfg
        [ -z "$VW_ROTATE" ] && VW_ROTATE=10
        [ -z "$VW_LOGSIZE" ] && VW_LOGSIZE=104857600
        [ -z "$VW_LOGINTERVAL" ] && VW_LOGINTERVAL=86400

        # Get all users
        for VW_USER in `logins -g staff`
        do
            exec su - "$VW_USER" -c "/opt/VirtualBox/VBoxAutostart --background --start --config \"$VW_CONFIG\" --logrotate \"$VW_ROTATE\" --logsize \"$VW_LOGSIZE\" --loginterval \"$VW_LOGINTERVAL\""

            VW_EXIT=$?
            if [ $VW_EXIT != 0 ]; then
                echo "VBoxAutostart failed with $VW_EXIT."
                VW_EXIT=1
                break
            fi
        done
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
