# Oracle VM VirtualBox
# VirtualBox installer shell routines
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

setup_normal_input_install_udev() {
    # The udev rule setup functions contain some aliases which are used for unit
    # testing purposes.  This function sets up the aliases to point to the real
    # commands for "normal" use.
    eval 'my_which() { which "$@" ; }'
    eval 'my_test() { test "$@" ; }'
    eval 'my_rm() { rm "$@" ; }'
}

# Needs to be done before the udev setup functions are called "normally" (i.e.
# not in a unit testing context).  Will be overridden later in a unit test.
setup_normal_input_install_udev

setup_test_input_install_udev() {
    # Set up unit testing environment for the "install_udev" function below.
    TEST_NAME="$1"          # used to identify the current test
    TEST_UDEV_VERSION="$2"  # udev version to simulate
    eval 'my_which() { echo test_udev ; }'
    eval 'my_test() { true ; }'
    eval 'my_rm() { case "$2" in "/etc/udev/rules.d/60-vboxdrv.rules") true ;; *) echo "rm: bad file name \"$2\"!"; false ;; esac ; }'
    eval 'test_udev() { echo "$TEST_UDEV_VERSION" ; }'
    DELETED_UDEV_FILE=""
}

cleanup_test_input_install_udev() {
    # Clean up after unit testing install_udev for a given input
    setup_normal_input_install_udev
    unset test_udev
    DELETED_UDEV_FILE=""
}

udev_write_vboxdrv() {
    VBOXDRV_GRP="$1"
    VBOXDRV_MODE="$2"

    echo "KERNEL==\"vboxdrv\", NAME=\"vboxdrv\", OWNER=\"root\", GROUP=\"$VBOXDRV_GRP\", MODE=\"$VBOXDRV_MODE\""
}

udev_write_usb() {
    INSTALLATION_DIR="$1"
    USB_GROUP="$2"

    echo "SUBSYSTEM==\"usb_device\", ACTION==\"add\", RUN+=\"$INSTALLATION_DIR/VBoxCreateUSBNode.sh \$major \$minor \$attr{bDeviceClass}${USB_GROUP}\""
    echo "SUBSYSTEM==\"usb\", ACTION==\"add\", ENV{DEVTYPE}==\"usb_device\", RUN+=\"$INSTALLATION_DIR/VBoxCreateUSBNode.sh \$major \$minor \$attr{bDeviceClass}${USB_GROUP}\""
    echo "SUBSYSTEM==\"usb_device\", ACTION==\"remove\", RUN+=\"$INSTALLATION_DIR/VBoxCreateUSBNode.sh --remove \$major \$minor\""
    echo "SUBSYSTEM==\"usb\", ACTION==\"remove\", ENV{DEVTYPE}==\"usb_device\", RUN+=\"$INSTALLATION_DIR/VBoxCreateUSBNode.sh --remove \$major \$minor\""
}

install_udev() {
    # install udev rule (disable with INSTALL_NO_UDEV=1 in /etc/default/virtualbox) for distribution packages
    VBOXDRV_GRP="$1"      # The group owning the vboxdrv device
    VBOXDRV_MODE="$2"     # The access mode for the vboxdrv device
    INSTALLATION_DIR="$3" # The directory VirtualBox is installed in
    USB_GROUP="$4"        # The group that has permission to access USB devices
    NO_INSTALL="$5"       # Set this to "1" to remove but not re-install rules

    # Extra space!
    case "$USB_GROUP" in ?*) USB_GROUP=" $USB_GROUP" ;; esac
    case "$NO_INSTALL" in
    "1") ;;
    *)
        if my_test -d /etc/udev/rules.d; then
            udev_call=""
            udev_app=`my_which udevadm 2> /dev/null`
            if [ $? -eq 0 ]; then
                udev_call="${udev_app} version 2> /dev/null"
            else
                udev_app=`my_which udevinfo 2> /dev/null`
                if [ $? -eq 0 ]; then
                    udev_call="${udev_app} -V 2> /dev/null"
                fi
            fi
            udev_fix=""
            if [ "${udev_call}" != "" ]; then
                udev_out=`${udev_call}`
                udev_ver=`expr "$udev_out" : '[^0-9]*\([0-9]*\)'`
                if [ "$udev_ver" = "" -o "$udev_ver" -lt 55 ]; then
                    udev_fix="1"
                fi
                udev_do_usb=""
                if [ "$udev_ver" -ge 59 ]; then
                    udev_do_usb="1"
                fi
            fi
            case "$udev_fix" in
            "1")
                udev_write_vboxdrv "$VBOXDRV_GRP" "$VBOXDRV_MODE" |
                    sed 's/\([^+=]*\)[+=]*\([^"]*"[^"]*"\)/\1=\2/g'
                ;;
            *)
                udev_write_vboxdrv "$VBOXDRV_GRP" "$VBOXDRV_MODE"
                case "$udev_do_usb" in "1")
                    udev_write_usb "$INSTALLATION_DIR" "$USB_GROUP" ;;
                esac
                ;;
            esac

        fi
        ;;
    esac
    # Remove old udev description file
    if my_test -f /etc/udev/rules.d/60-vboxdrv.rules; then
        my_rm -f /etc/udev/rules.d/60-vboxdrv.rules 2> /dev/null
    fi
}

setup_normal_input_install_create_usb_node() {
    # install_create_usb_node_for_sysfs contain some aliases for unit testing
    # purposes.  This function sets up the aliases to point to the real
    # commands for "normal" use.
    eval 'my_test() { test "$@" ; }'
    eval 'my_cat() { cat "$@" ; }'
    eval 'my_sh() { sh "$@" ; }'
}

# Assume we will be calling the function normally, not for unit testing
setup_normal_input_install_create_usb_node

# Add a unit test here if/when needed following the same pattern as
# setup_test_input_install_udev.

install_create_usb_node_for_sysfs() {
    # Create a usb device node for a given sysfs path
    path="$1"           # sysfs path for the device
    usb_createnode="$2" # Path to the USB device node creation script
    usb_group="$3"      # The group to give ownership of the node to
    if my_test -r "${path}/dev"; then
        dev="`my_cat "${path}/dev" 2> /dev/null`"
        major="`expr "$dev" : '\(.*\):' 2> /dev/null`"
        minor="`expr "$dev" : '.*:\(.*\)' 2> /dev/null`"
        class="`my_cat ${path}/bDeviceClass 2> /dev/null`"
        my_sh "${usb_createnode}" "$major" "$minor" "$class" "${usb_group}" \
              2>/dev/null
    fi
}

setup_normal_input_install_device_node_setup() {
    # install_device_node_setup contain some aliases for unit testing
    # purposes.  This function sets up the aliases to point to the real
    # values for "normal" use.
    udev_rule_file=/etc/udev/rules.d/10-vboxdrv.rules # Set this to /dev/null
                                                      # for unit testing
    sysfs_usb_devices="/sys/bus/usb/devices/*"
    eval 'do_install_udev() { install_udev "$@"; }'
    eval 'do_install_create_usb_node_for_sysfs() { \
                          install_create_usb_node_for_sysfs "$@" ; }'
}

# Assume we will be calling the function normally, not for unit testing
setup_normal_input_install_device_node_setup

setup_test_input_install_device_node_setup() {
    # Set up unit testing environment for the "install_udev" function below.
    test_drv_grp="$1"  # The expected vboxdrv group
    test_drv_mode="$2" # The expected vboxdrv mode
    test_inst_dir="$3" # The expected installation directory
    test_usb_grp="$4"  # The expected USB node group
    udev_rule_file=/dev/null
    sysfs_usb_devices=test_sysfs_path
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

cleanup_test_input_install_device_node_setup() {
    # Clean up unit testing environment for the "install_udev" function below.
    # Nothing to do.
    true
}

install_device_node_setup() {
    # Install udev rules and create device nodes for usb access
    VBOXDRV_GRP="$1"      # The group that should own /dev/vboxdrv
    VBOXDRV_MODE="$2"     # The mode to be used for /dev/vboxdrv
    INSTALLATION_DIR="$3" # The directory VirtualBox is installed in
    USB_GROUP="$4"        # The group that should own the /dev/vboxusb device
                          # nodes unless INSTALL_NO_GROUP=1 in
                          # /etc/default/virtualbox.  Optional.
    usb_createnode="$INSTALLATION_DIR/VBoxCreateUSBNode.sh"
    # install udev rule (disable with INSTALL_NO_UDEV=1 in
    # /etc/default/virtualbox)
    if [ "$INSTALL_NO_GROUP" != "1" ]; then
        usb_group=$USB_GROUP
        vboxdrv_group=$VBOXDRV_GRP
    else
        usb_group=root
        vboxdrv_group=root
    fi
    do_install_udev "${vboxdrv_group}" "$VBOXDRV_MODE" "$INSTALLATION_DIR" \
                    "${usb_group}" "$INSTALL_NO_UDEV" > ${udev_rule_file}
    # Build our device tree
    for i in ${sysfs_usb_devices}; do  # This line intentionally without quotes.
        do_install_create_usb_node_for_sysfs "$i" "${usb_createnode}" \
                                             "${usb_group}"
    done
}
