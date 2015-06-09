#!/bin/sh
#
# Oracle VM VirtualBox
# VirtualBox linux installation script unit test

#
# Copyright (C) 2007-2015 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

#include installer-common.sh

echo Testing udev rule generation

setup_test_install_udev

TEST_UDEV_VERSION=55

udev_55_rules=`cat <<UDEV_END
KERNEL=="vboxdrv", NAME="vboxdrv", OWNER="root", GROUP="vboxusers", MODE="0660"
SUBSYSTEM=="usb_device", ACTION=="add", RUN="/opt/VirtualBox/VBoxCreateUSBNode.sh $major $minor $attr{bDeviceClass}"
SUBSYSTEM=="usb", ACTION=="add", ENV{DEVTYPE}=="usb_device", RUN="/opt/VirtualBox/VBoxCreateUSBNode.sh $major $minor $attr{bDeviceClass}"
SUBSYSTEM=="usb_device", ACTION=="remove", RUN="/opt/VirtualBox/VBoxCreateUSBNode.sh --remove $major $minor"
SUBSYSTEM=="usb", ACTION=="remove", ENV{DEVTYPE}=="usb_device", RUN="/opt/VirtualBox/VBoxCreateUSBNode.sh --remove $major $minor"
UDEV_END

install_udev_output="`install_udev_run`"
case "$install_udev_output" in
    "$udev_55_rules") ;;
    *)
        echo "Bad output for udev version 55.  Expected:"
        echo "$udev_55_rules"
        echo "Actual:"
        echo "$install_udev_output"
        ;;
esac

TEST_UDEV_VERSION=54

udev_54_rules=`cat <<UDEV_END
KERNEL="vboxdrv", NAME="vboxdrv", OWNER="root", GROUP="vboxusers", MODE="0660"
SUBSYSTEM="usb_device", ACTION="add", RUN="/opt/VirtualBox/VBoxCreateUSBNode.sh $major $minor $attr{bDeviceClass}"
SUBSYSTEM="usb", ACTION="add", ENV{DEVTYPE}="usb_device", RUN="/opt/VirtualBox/VBoxCreateUSBNode.sh $major $minor $attr{bDeviceClass}"
SUBSYSTEM="usb_device", ACTION="remove", RUN="/opt/VirtualBox/VBoxCreateUSBNode.sh --remove $major $minor"
SUBSYSTEM="usb", ACTION="remove", ENV{DEVTYPE}="usb_device", RUN="/opt/VirtualBox/VBoxCreateUSBNode.sh --remove $major $minor"
UDEV_END

install_udev_output="`install_udev_run`"
case "$install_udev_output" in
    "$udev_54_rules") ;;
    *)
        echo "Bad output for udev version 54.  Expected:"
        echo "$udev_54_rules"
        echo "Actual:"
        echo "$install_udev_output"
        ;;
esac

echo Done.
