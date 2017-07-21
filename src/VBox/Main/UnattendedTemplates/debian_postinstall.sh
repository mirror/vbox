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
    MY_TMP_EXITCODE="${PIPESTATUS[0]}"
    if [ "${MY_TMP_EXITCODE}" != "0" ]; then
        if [ "${MY_TMP_EXITCODE}" != "${MY_IGNORE_EXITCODE}" ]; then
            echo "** exit code: ${MY_TMP_EXITCODE}" | tee -a "${MY_LOGFILE}"
            MY_EXITCODE=1;
        else
            echo "** exit code: ${MY_TMP_EXITCODE} (ignored)" | tee -a "${MY_LOGFILE}"
        fi
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
# We want the ISO available inside the target jail.
#
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
# Debug
#
if [ "${MY_DEBUG}" = "yes" ]; then
    log_command id
    log_command df
    log_command mount
    log_command_in_target df
    log_command_in_target mount
    log_command find /
    MY_EXITCODE=0
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
MY_IGNORE_EXITCODE=2  # returned if modules already loaded and reboot required.
log_command_in_target /bin/bash /cdrom/vboxadditions/VBoxLinuxAdditions.run --nox11
MY_IGNORE_EXITCODE=
log_command_in_target usermod -a -G vboxsf "@@VBOX_INSERT_USER_LOGIN@@"
@@VBOX_COND_END@@

#
# Test Execution Service.
#
@@VBOX_COND_IS_INSTALLING_TEST_EXEC_SERVICE@@
echo '** Installing Test Execution Service...' | tee -a "${MY_LOGFILE}"
log_command_in_target test "/cdrom/vboxvalidationkit/linux/@@VBOX_INSERT_OS_ARCH@@/TestExecService"
log_command mkdir -p "${MY_TARGET}/root/validationkit" "${MY_TARGET}/target/cdrom"
log_command cp -R /cdrom/vboxvalidationkit/* "${MY_TARGET}/root/validationkit/"
log_command chmod -R u+rw,a+xr "${MY_TARGET}/root/validationkit/"

# systemd service config:
MY_UNIT_PATH="${MY_TARGET}/lib/systemd/system"
test -d "${MY_TARGET}/usr/lib/systemd/system" && MY_UNIT_PATH="${MY_TARGET}/usr/lib/systemd/system"
if [ -d "${MY_UNIT_PATH}" ]; then
    if [  -f "${MY_TARGET}/linux/vboxtxs.service" ]; then ## REMOVE AS SOON AS r117117 IS READY
        log_command cp "${MY_TARGET}/linux/vboxtxs.service" "${MY_UNIT_PATH}/vboxtxs.service"
    else                                                  ## REMOVE AS SOON AS r117117 IS READY
        cat > "${MY_UNIT_PATH}/vboxtxs.service" <<EOF
[Unit]
Description=VirtualBox Test Execution Service
SourcePath=/root/validationkit/linux/vboxtxs

[Service]
Type=forking
Restart=no
TimeoutSec=5min
IgnoreSIGPIPE=no
KillMode=process
GuessMainPID=no
RemainAfterExit=yes
ExecStart=/root/validationkit/linux/vboxtxs start
ExecStop=/root/validationkit/linux/vboxtxs stop

[Install]
WantedBy=multi-user.target
EOF
    fi
    log_command chmod 755 "${MY_UNIT_PATH}/vboxtxs.service"
    log_command_in_target systemctl -q enable vboxtxs

# Not systemd.  Add support for upstart later...
else
    echo "** error: No systemd unit dir found.  Using upstart or something?" | tee -a "${MY_LOGFILE}"
fi

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

