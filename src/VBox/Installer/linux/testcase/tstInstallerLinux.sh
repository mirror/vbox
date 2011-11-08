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

#include installer-common.sh

CERRS=0

echo "Testing udev rule generation"

extern_test_input_install_udev() {
    command="$1"
    shift
    case "$command" in
        "which") my_which "$@";;
        "test")  my_test "$@";;
        "rm")    my_rm "$@";;
        *) echo Unknown command $command >&2; exit 1;;
    esac        
}

setup_test_input_install_udev() {
    # Set up unit testing environment for the "install_udev" function below.
    TEST_NAME="$1"          # used to identify the current test
    TEST_UDEV_VERSION="$2"  # udev version to simulate
    EXTERN=extern_test_input_install_udev
    eval 'my_which() { echo test_udev ; }'
    eval 'my_test() { true ; }'
    eval 'my_rm() { case "$2" in "/etc/udev/rules.d/60-vboxdrv.rules") true ;; *) echo "rm: bad file name \"$2\"!"; false ;; esac ; }'
    eval 'test_udev() { echo "$TEST_UDEV_VERSION" ; }'
    DELETED_UDEV_FILE=""
}

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

echo "Testing device node setup"

extern_test_input_install_device_node_setup() {
    command="$1"
    shift
    case "$command" in
        "install_udev")
            do_install_udev "$@";;
        "install_create_usb_node_for_sysfs")
            do_install_create_usb_node_for_sysfs "$@";;
        *)
            echo Unknown command $command >&2; exit 1;;
    esac        
}

setup_test_input_install_device_node_setup() {
    # Set up unit testing environment for the "install_udev" function below.
    test_drv_grp="$1"  # The expected vboxdrv group
    test_drv_mode="$2" # The expected vboxdrv mode
    test_inst_dir="$3" # The expected installation directory
    test_usb_grp="$4"  # The expected USB node group
    udev_rule_file=/dev/null
    sysfs_usb_devices=test_sysfs_path
    EXTERN=extern_test_input_install_device_node_setup
    eval 'do_install_udev() {    test    "$1" = "${test_drv_grp}" \
                                      -a "$2" = "${test_drv_mode}" \
                                      -a "$3" = "${test_inst_dir}" \
                                      -a "$4" = "${test_usb_grp}" \
                                      -a "$5" = "${INSTALL_NO_UDEV}" \
                              || echo "do_install_udev: bad parameters: $@" >&2 ; }'
    eval 'do_install_create_usb_node_for_sysfs() { \
                       test    "$1" = "${sysfs_usb_devices}" \
                            -a "$2" = "${test_inst_dir}/VBoxCreateUSBNode.sh" \
                            -a "$3" = "${test_usb_grp}" \
                    || echo "do_install_create_usb_node_for_sysfs: \
bad parameters: $@" >&2 ; }'
}

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

echo "Done.  Error count $CERRS."
