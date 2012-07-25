#!/bin/sh

#
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
# Wrapper for the per user autostart daemon. Gets a list of all users
# and starts the VMs.
#

function vboxStartAllUserVms()
{
    # Go through the list and filter out all users without a shell and a
    # non existing home.
    for user in `dscl . -list /Users`
    do
        HOMEDIR=`dscl . -read /Users/${user} | grep NFSHomeDirectory | sed 's/NFSHomeDirectory: //g'`
        USERSHELL=`dscl . -read /Users/${user} | grep UserShell | sed 's/UserShell: //g'`

        # Check for known home directories and shells for daemons
        if [[   "${HOMEDIR}" == "/var/empty" || "${HOMEDIR}" == "/dev/null"
             || "${USERSHELL}" == "/usr/bin/false" || "${USERSHELL}" == "/dev/null" || "${USERSHELL}" == "/usr/sbin/uucico" ]]
        then
            continue
        fi

        # Start the daemon
        su ${user} -c '/Applications/VirtualBox.app/Contents/MacOS/VBoxAutostart --start --background --config /etc/vbox/autostart.cfg'

    done
}

case $1 in
    --start) vboxStartAllUserVms;;
          *) echo "Unknown option ${1}";;
esac

