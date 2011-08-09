#!/bin/sh
#
# Oracle VM VirtualBox
# VirtualBox linux installation script unit test

#
# Copyright (C) 2007-2011 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

#include installer-utils.sh

CERRS=0

echo "Testing udev rule generation"

setup_test_input_install_udev ".run, udev-59" 59

udev_59_rules=`cat <<'UDEV_END'
KERNEL=="vboxdrv", NAME="vboxdrv", OWNER="root", GROUP="vboxusers", MODE="0660"
SUBSYSTEM=="usb_device", ACTION=="add", RUN+="/opt/VirtualBox/VBoxCreateUSBNode.sh $major $minor $attr{bDeviceClass}"
SUBSYSTEM=="usb", ACTION=="add", ENV{DEVTYPE}=="usb_device", RUN+="/opt/VirtualBox/VBoxCreateUSBNode.sh $major $minor $attr{bDeviceClass}"
SUBSYSTEM=="usb_device", ACTION=="remove", RUN+="/opt/VirtualBox/VBoxCreateUSBNode.sh --remove $major $minor"
SUBSYSTEM=="usb", ACTION=="remove", ENV{DEVTYPE}=="usb_device", RUN+="/opt/VirtualBox/VBoxCreateUSBNode.sh --remove $major $minor"
UDEV_END`

install_udev_output="`install_udev vboxusers 0660 /opt/VirtualBox`"
case "$install_udev_output" in
    "$udev_59_rules") ;;
    *)
        echo "Bad output for udev version 59.  Expected:"
        echo "$udev_59_rules"
        echo "Actual:"
        echo "$install_udev_output"
        CERRS="`expr "$CERRS" + 1`"
        ;;
esac

cleanup_test_input_install_udev

setup_test_input_install_udev ".run, udev-55" 55

udev_55_rules=`cat <<'UDEV_END'
KERNEL=="vboxdrv", NAME="vboxdrv", OWNER="root", GROUP="vboxusers", MODE="0660"
UDEV_END`

install_udev_output="`install_udev vboxusers 0660 /opt/VirtualBox`"
case "$install_udev_output" in
    "$udev_55_rules") ;;
    *)
        echo "Bad output for udev version 55.  Expected:"
        echo "$udev_55_rules"
        echo "Actual:"
        echo "$install_udev_output"
        CERRS="`expr "$CERRS" + 1`"
        ;;
esac

cleanup_test_input_install_udev

setup_test_input_install_udev ".run, udev-54" 54

udev_54_rules=`cat <<'UDEV_END'
KERNEL="vboxdrv", NAME="vboxdrv", OWNER="root", GROUP="root", MODE="0600"
UDEV_END`

install_udev_output="`install_udev root 0600 /usr/lib/virtualbox`"
case "$install_udev_output" in
    "$udev_54_rules") ;;
    *)
        echo "Bad output for udev version 54.  Expected:"
        echo "$udev_54_rules"
        echo "Actual:"
        echo "$install_udev_output"
        CERRS="`expr "$CERRS" + 1`"
        ;;
esac

cleanup_test_input_install_udev

echo "Testing device node setup"

unset INSTALL_NO_GROUP
unset INSTALL_NO_UDEV
setup_test_input_install_device_node_setup vboxusers 0660 /opt/VirtualBox \
                                           vboxusb

command="install_device_node_setup vboxusers 0660 /opt/VirtualBox vboxusb"
err="`${command} 2>&1`"
test -n "${err}" && {
    echo "${command} failed."
    echo "Error: ${err}"
    CERRS="`expr "$CERRS" + 1`"
}

cleanup_test_input_install_device_node_setup

INSTALL_NO_GROUP=1
unset INSTALL_NO_UDEV
setup_test_input_install_device_node_setup root 0660 /opt/VirtualBox root

command="install_device_node_setup vboxusers 0660 /opt/VirtualBox vboxusb"
err="`${command} 2>&1`"
test -n "${err}" && {
    echo "${command} failed."
    echo "Error: ${err}"
    CERRS="`expr "$CERRS" + 1`"
}

cleanup_test_input_install_device_node_setup

unset INSTALL_NO_GROUP
INSTALL_NO_UDEV=1
setup_test_input_install_device_node_setup vboxusers 0660 /opt/VirtualBox \
                                           vboxusb

command="install_device_node_setup vboxusers 0660 /opt/VirtualBox vboxusb"
err="`${command} 2>&1`"
test -n "${err}" && {
    echo "${command} failed."
    echo "Error: ${err}"
    CERRS="`expr "$CERRS" + 1`"
}

cleanup_test_input_install_device_node_setup

echo "Done.  Error count $CERRS."
