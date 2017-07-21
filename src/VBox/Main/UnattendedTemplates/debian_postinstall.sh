#!/bin/bash
## @file
# Post installation script template for debian-like distros.
#
# This script expects to be running w/o chroot.
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

#MY_DEBUG="yes"
MY_DEBUG=""
MY_TARGET="/target"
MY_LOGFILE="${MY_TARGET}/var/log/vboxpostinstall.log"
MY_EXITCODE=0

# Logs execution of a command.
log_command()
{
    echo "--------------------------------------------------" >> "${MY_LOGFILE}"
    echo "** Date:      `date -R`" >> "${MY_LOGFILE}"
    echo "** Executing: $*" >> "${MY_LOGFILE}"
    "$@" 2>&1 | tee -a "${MY_LOGFILE}"
    if [ "${PIPESTATUS[0]}" != "0" ]; then
        echo "exit code: ${PIPESTATUS[0]}"
        MY_EXITCODE=1;
    fi
}

log_command_in_target()
{
    #
    # We should be using in-target here, however we don't get any stderr output
    # from it because of log-output. We can get stdout by --pass-stdout, but
    # that's not helpful for failures.
    #
    # So, we try do the chroot prepping that in-target does at the start of the
    # script (see below) and just use chroot here.
    #
    # Also, GA installer and in-root/log-output doesn't seem to get along.
    #
    log_command chroot "${MY_TARGET}" "$@"
    # log_command in-target --pass-stdout "$@" # No stderr output... :-(
}


#
# Header.
#
echo "******************************************************************************" >> "${MY_LOGFILE}"
echo "** VirtualBox Unattended Guest Installation - Late installation actions" >> "${MY_LOGFILE}"
echo "** Date:    `date -R`" >> "${MY_LOGFILE}"
echo "** Started: $0 $*" >> "${MY_LOGFILE}"

#
# Setup the target jail ourselves since in-target steals all the output.
#
if [ -f /lib/chroot-setup.sh ]; then
    MY_HAVE_CHROOT_SETUP="yes"
    . /lib/chroot-setup.sh
    if chroot_setup; then
        echo "** chroot_setup: done" | tee -a "${MY_LOGFILE}"
    else
        echo "** chroot_setup: failed $?" | tee -a "${MY_LOGFILE}"
    fi
else
    MY_HAVE_CHROOT_SETUP=""
fi

#
# Debug
#
if [ "${MY_DEBUG}" = "yes" ]; then
    log_command id
    log_command df
    log_command mount
    log_command_in_target df
    log_command_in_target mount
    log_command_in_target ls -Rla /cdrom
    log_command_in_target ls -Rla /media
    log_command find /
    MY_EXITCODE=0
fi

# We want the ISO available inside the target jail.
if [ -f "${MY_TARGET}/cdrom/vboxpostinstall.sh" ]; then
    MY_UNMOUNT_TARGET_CDROM=
    echo "** binding cdrom into jail: already done" | tee -a "${MY_LOGFILE}"
else
    MY_UNMOUNT_TARGET_CDROM="yes"
    log_command mount -o bind /cdrom "${MY_TARGET}/cdrom"
    if [ -f "${MY_TARGET}/cdrom/vboxpostinstall.sh" ]; then
        echo "** binding cdrom into jail: success"  | tee -a "${MY_LOGFILE}"
    else
        echo "** binding cdrom into jail: failed"   | tee -a "${MY_LOGFILE}"
    fi
    if [ "${MY_DEBUG}" = "yes" ]; then
        log_command find "${MY_TARGET}/cdrom"
    fi
fi

#
# Packages needed for GAs.
#
echo '** Installing packages for building kernel modules...' | tee -a "${MY_LOGFILE}"
log_command_in_target apt-get -y install build-essential
log_command_in_target apt-get -y install linux-headers-$(uname -r)

#
# GAs
#
@@VBOX_COND_IS_INSTALLING_ADDITIONS@@
echo '** Installing VirtualBox Guest Additions...' | tee -a "${MY_LOGFILE}"
log_command_in_target /bin/bash /cdrom/vboxadditions/VBoxLinuxAdditions.run --nox11
log_command_in_target usermod -a -G vboxsf "@@VBOX_INSERT_USER_LOGIN@@"
@@VBOX_COND_END@@

#
# Testing.
#
@@VBOX_COND_IS_INSTALLING_TEST_EXEC_SERVICE@@
echo '** Installing Test Execution Service...' | tee -a "${MY_LOGFILE}"
log_command_in_target test "/cdrom/linux/@@VBOX_INSERT_OS_ARCH@@/TestExecService"
## @todo fix this
@@VBOX_COND_END@@

#
# Run user command.
#
@@VBOX_COND_HAS_POST_INSTALL_COMMAND@@
echo '** Running custom user command ...'      | tee -a "${MY_LOGFILE}"
log_command @@VBOX_INSERT_POST_INSTALL_COMMAND@@
@@VBOX_COND_END@@

#
# Unmount the cdrom if we bound it and clean up the chroot if we set it up.
#
if [ -n "${MY_UNMOUNT_TARGET_CDROM}" ]; then
    echo "** unbinding cdrom from jail..." | tee -a "${MY_LOGFILE}"
    log_command umount "${MY_TARGET}/cdrom"
fi
if [ -n "${MY_HAVE_CHROOT_SETUP}" ]; then
    if chroot_cleanup; then
        echo "** chroot_cleanup: done"      | tee -a "${MY_LOGFILE}"
    else
        echo "** chroot_cleanup: failed $?" | tee -a "${MY_LOGFILE}"
    fi
fi

#
# Footer.
#
echo "******************************************************************************" >> "${MY_LOGFILE}"
echo "** Date:            `date -R`" >> "${MY_LOGFILE}"
echo "** Final exit code: ${MY_EXITCODE}" >> "${MY_LOGFILE}"
echo "******************************************************************************" >> "${MY_LOGFILE}"

exit ${MY_EXITCODE}

