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
    TEST_UDEV_VERSION="$1"  # udev version to simulate
    eval 'my_which() { which "$@" ; }'
    eval 'my_test() { test "$@" ; }'
    eval 'my_rm() { rm "$@" ; }'
}

setup_normal_input_install_udev

setup_test_input_install_udev() {
    TEST_NAME="$1"          # used to identify the current test
    TEST_UDEV_VERSION="$2"  # udev version to simulate
    eval 'my_which() { echo test_udev ; }'
    eval 'my_test() { true ; }'
    eval 'my_rm() { case "$2" in "/etc/udev/rules.d/60-vboxdrv.rules") true ;; *) echo "rm: bad file name \"$2\"!"; false ;; esac ; }'
    eval 'test_udev() { echo "$TEST_UDEV_VERSION" ; }'
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

cleanup_test_input_install_udev() {
    setup_normal_input_install_udev
    unset test_udev
    DELETED_UDEV_FILE=""
}
