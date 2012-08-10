#!/bin/sh
#
# VirtualBox Uninstaller Script.
#
# Copyright (C) 2007-2010 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

# Override any funny stuff from the user.
export PATH="/bin:/usr/bin:/sbin:/usr/sbin:$PATH"

#
# Display a simple welcome message first.
#
echo ""
echo "Welcome to the VirtualBox uninstaller script."
echo ""

#
# Check for arguments and display
#
my_default_prompt=0
if test "$#" != "0"; then
    if test "$#" != "1" -o "$1" != "--unattended"; then
        echo "Error: Unknown argument(s): $*"
        echo ""
        echo "Usage: uninstall.sh [--unattended]"
        echo ""
        echo "If the '--unattended' option is not given, you will be prompted"
        echo "for a Yes/No before doing the actual uninstallation."
        echo ""
        exit 4;
    fi
    my_default_prompt="Yes"
fi

#
# Collect directories and files to remove.
# Note: Do NOT attempt adding directories or filenames with spaces!
#
my_directories=""
my_files=""

# Users files first
test -f "${HOME}/Library/LaunchAgents/org.virtualbox.vboxwebsrv.plist"  && my_files="$my_files ${HOME}/Library/LaunchAgents/org.virtualbox.vboxwebsrv.plist"

test -d /Library/StartupItems/VirtualBox/          && my_directories="$my_directories /Library/StartupItems/VirtualBox/"
test -d /Library/Receipts/VBoxStartupItems.pkg/    && my_directories="$my_directories /Library/Receipts/VBoxStartupItems.pkg/"

test -d /Library/Extensions/VBoxDrv.kext/          && my_directories="$my_directories /Library/Extensions/VBoxDrv.kext/"
test -d /Library/Extensions/VBoxUSB.kext/          && my_directories="$my_directories /Library/Extensions/VBoxUSB.kext/"
test -d /Library/Extensions/VBoxNetFlt.kext/       && my_directories="$my_directories /Library/Extensions/VBoxNetFlt.kext/"
test -d /Library/Extensions/VBoxNetAdp.kext/       && my_directories="$my_directories /Library/Extensions/VBoxNetAdp.kext/"
# Tiger support is obsolete, but we leave it here for a clean removing of older
# VirtualBox versions
test -d /Library/Extensions/VBoxDrvTiger.kext/     && my_directories="$my_directories /Library/Extensions/VBoxDrvTiger.kext/" 
test -d /Library/Extensions/VBoxUSBTiger.kext/     && my_directories="$my_directories /Library/Extensions/VBoxUSBTiger.kext/"
test -d /Library/Receipts/VBoxKEXTs.pkg/           && my_directories="$my_directories /Library/Receipts/VBoxKEXTs.pkg/"

test -f /usr/bin/VirtualBox                        && my_files="$my_files /usr/bin/VirtualBox"
test -f /usr/bin/VBoxManage                        && my_files="$my_files /usr/bin/VBoxManage"
test -f /usr/bin/VBoxVRDP                          && my_files="$my_files /usr/bin/VBoxVRDP"
test -f /usr/bin/VBoxHeadless                      && my_files="$my_files /usr/bin/VBoxHeadless"
test -f /usr/bin/vboxwebsrv                        && my_files="$my_files /usr/bin/vboxwebsrv"
test -f /usr/bin/VBoxBalloonCtrl                   && my_files="$my_files /usr/bin/VBoxBalloonCtrl"
test -f /usr/bin/VBoxAutostart                     && my_files="$my_files /usr/bin/VBoxAutostart"
test -f /usr/bin/vbox-img                          && my_files="$my_files /usr/bin/vbox-img"
test -d /Library/Receipts/VirtualBoxCLI.pkg/       && my_directories="$my_directories /Library/Receipts/VirtualBoxCLI.pkg/"

test -d /Applications/VirtualBox.app/              && my_directories="$my_directories /Applications/VirtualBox.app/"
test -d /Library/Receipts/VirtualBox.pkg/          && my_directories="$my_directories /Library/Receipts/VirtualBox.pkg/"

# legacy
test -d /Library/Receipts/VBoxDrv.pkg/             && my_directories="$my_directories /Library/Receipts/VBoxDrv.pkg/"
test -d /Library/Receipts/VBoxUSB.pkg/             && my_directories="$my_directories /Library/Receipts/VBoxUSB.pkg/"

# python stuff
python_versions="2.3 2.5 2.6"
for p in $python_versions; do
    test -f /Library/Python/$p/site-packages/vboxapi/VirtualBox_constants.py  && my_files="$my_files /Library/Python/$p/site-packages/vboxapi/VirtualBox_constants.py"
    test -f /Library/Python/$p/site-packages/vboxapi/VirtualBox_constants.pyc && my_files="$my_files /Library/Python/$p/site-packages/vboxapi/VirtualBox_constants.pyc"
    test -f /Library/Python/$p/site-packages/vboxapi/__init__.py              && my_files="$my_files /Library/Python/$p/site-packages/vboxapi/__init__.py"
    test -f /Library/Python/$p/site-packages/vboxapi/__init__.pyc             && my_files="$my_files /Library/Python/$p/site-packages/vboxapi/__init__.pyc"
    test -f /Library/Python/$p/site-packages/vboxapi-1.0-py$p.egg-info        && my_files="$my_files /Library/Python/$p/site-packages/vboxapi-1.0-py$p.egg-info"
    test -d /Library/Python/$p/site-packages/vboxapi/                         && my_directories="$my_directories /Library/Python/$p/site-packages/vboxapi/"
done

#
# Collect KEXTs to remove.
# Note that the unload order is significant.
#
my_kexts=""
for kext in org.virtualbox.kext.VBoxUSB org.virtualbox.kext.VBoxNetFlt org.virtualbox.kext.VBoxNetAdp org.virtualbox.kext.VBoxDrv; do
    if /usr/sbin/kextstat -b $kext -l | grep -q $kext; then
        my_kexts="$my_kexts $kext"
    fi
done

#
# Did we find anything to uninstall?
#
if test -z "$my_directories"  -a  -z "$my_files"   -a  -z "$my_kexts"; then
    echo "No VirtualBox files, directories or KEXTs to uninstall."
    echo "Done."
    exit 0;
fi

#
# Look for running VirtualBox processes and warn the user
# if something is running. Since deleting the files of
# running processes isn't fatal as such, we will leave it
# to the user to choose whether to continue or not.
#
# Note! comm isn't supported on Tiger, so we make -c to do the stripping.
#
my_processes="`ps -axco 'pid uid command' | grep -wEe '(VirtualBox|VirtualBoxVM|VBoxManage|VBoxHeadless|vboxwebsrv|VBoxXPCOMIPCD|VBoxSVC|VBoxNetDHCP)' | grep -vw grep | grep -vw VirtualBox_Uninstall.tool | tr '\n' '\a'`";
if test -n "$my_processes"; then
    echo 'Warning! Found the following active VirtualBox processes:'
    echo "$my_processes" | tr '\a' '\n'
    echo ""
    echo "We recommend that you quit all VirtualBox processes before"
    echo "uninstalling the product."
    echo ""
    if test "$my_default_prompt" != "Yes"; then
        echo "Do you wish to continue none the less (Yes/No)?"
        read my_answer
        if test "$my_answer" != "Yes"  -a  "$my_answer" != "YES"  -a  "$my_answer" != "yes"; then
            echo "Aborting uninstall. (answer: '$my_answer')".
            exit 2;
        fi
        echo ""
        my_answer=""
    fi
fi

#
# Display the files and directories that will be removed
# and get the user's consent before continuing.
#
if test -n "$my_files"  -o  -n "$my_directories"; then
    echo "The following files and directories (bundles) will be removed:"
    for file in $my_files;       do echo "    $file"; done
    for dir  in $my_directories; do echo "    $dir"; done
fi
if test -n "$my_kexts"; then
echo "And the following KEXTs will be unloaded:"
    for kext in $my_kexts;       do echo "    $kext"; done
fi
echo ""

if test "$my_default_prompt" != "Yes"; then
    echo "Do you wish to uninstall VirtualBox (Yes/No)?"
    read my_answer
    if test "$my_answer" != "Yes"  -a  "$my_answer" != "YES"  -a  "$my_answer" != "yes"; then
        echo "Aborting uninstall. (answer: '$my_answer')".
        exit 2;
    fi
    echo ""
fi

#
# Unregister has to be done before the files are removed.
#
LSREGISTER=/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/LaunchServices.framework/Versions/A/Support/lsregister
if [ -e ${LSREGISTER} ]; then
    ${LSREGISTER} -u /Applications/VirtualBox.app > /dev/null
    ${LSREGISTER} -u /Applications/VirtualBox.app/Contents/Resources/vmstarter.app > /dev/null
fi

#
# Display the sudo usage instructions and execute the command.
#
echo "The uninstallation processes requires administrative privileges"
echo "because some of the installed files cannot be removed by a normal"
echo "user. You may be prompted for your password now..."
echo ""

if test -n "$my_files"  -o  -n "$my_directories"; then
    /usr/bin/sudo -p "Please enter %u's password:" /bin/rm -Rf $my_files $my_directories
    my_rc=$?
    if test "$my_rc" -ne 0; then
        echo "An error occurred durning 'sudo rm', there should be a message above. (rc=$my_rc)"
        test -x /usr/bin/sudo || echo "warning: Cannot find /usr/bin/sudo or it's not an executable."
        test -x /bin/rm       || echo "warning: Cannot find /bin/rm or it's not an executable"
        echo ""
        echo "The uninstall failed. Please retry."
        exit 1;
    fi
fi

my_rc=0
for kext in $my_kexts; do
    echo unloading $kext
    /usr/bin/sudo -p "Please enter %u's password (unloading $kext):" /sbin/kextunload -m $kext
    my_rc2=$?
    if test "$my_rc2" -ne 0; then
        echo "An error occurred durning 'sudo /sbin/kextunload -m $kext', there should be a message above. (rc=$my_rc2)"
        test -x /usr/bin/sudo    || echo "warning: Cannot find /usr/bin/sudo or it's not an executable."
        test -x /sbin/kextunload || echo "warning: Cannot find /sbin/kextunload or it's not an executable"
        my_rc=$my_rc2
    fi
done
if test "$my_rc" -eq 0; then
    echo "Successfully uninstalled VirtualBox."
else
    echo "Failed to unload on or more KEXTs, please reboot the machine to complete the uninstall."
fi
echo "Done."
exit 0;

