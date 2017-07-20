#!/bin/bash
## @file
# Post installation script template for redhat-like distros.
#
# This script expects to be running chroot'ed into /target.
#

#
# Copyright (C) 2017 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

MY_LOGFILE="/var/log/vboxpostinstall.log"
MY_EXITCODE=0

# Logs execution of a command.
log_command()
{
    echo "Executing: $*" >> "${MY_LOGFILE}"
    "$@" 2>&1 | tee -a "${MY_LOGFILE}"
    if [ "${PIPESTATUS[0]}" != "0" ]; then
        echo "exit code: ${PIPESTATUS[0]}"
        MY_EXITCODE=1;
    fi
}


echo "Started: $*" >> "${MY_LOGFILE}"
echo "Date:    `date -R`" >> "${MY_LOGFILE}"

#echo ''
#echo 'Installing packages for building kernel modules...'
#log_command apt-get -y install build-essential
#log_command apt-get -y install linux-headers-$(uname -r)
#

@@VBOX_COND_IS_INSTALLING_ADDITIONS@@
echo ''
echo 'Installing VirtualBox Guest Additions...'
## @todo fix this
log_command /bin/bash /cdrom/VBoxAdditions/VBoxLinuxAdditions.run
log_command usermod -a -G vboxsf "@@VBOX_INSERT_USER_LOGIN@@"
@@VBOX_COND_END@@

@@VBOX_COND_IS_INSTALLING_TEST_EXEC_SERVICE@@
echo ''
echo 'Installing Test Execution Service...'
## @todo fix this
@@VBOX_COND_END@@

echo "Final exit code: ${MY_EXITCODE}" >> "${MY_LOGFILE}"
exit ${MY_EXITCODE}

