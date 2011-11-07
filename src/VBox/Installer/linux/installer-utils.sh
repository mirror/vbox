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

# This is used for unit testing and will be reset after the file is sourced for
# test runs.
unset EXTERN

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
    # To unit test, set $EXTERN to point to a function simulating external
    # commands: test; which; rm.  See the code for usage.
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
        if $EXTERN test -d /etc/udev/rules.d; then
            udev_call=""
            udev_app=`$EXTERN which udevadm 2> /dev/null`
            if [ $? -eq 0 ]; then
                udev_call="${udev_app} version 2> /dev/null"
            else
                udev_app=`$EXTERN which udevinfo 2> /dev/null`
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
    if $EXTERN test -f /etc/udev/rules.d/60-vboxdrv.rules; then
        $EXTERN rm -f /etc/udev/rules.d/60-vboxdrv.rules 2> /dev/null
    fi
}

# Add a unit test if/when needed following the same pattern as for
# install_udev.

install_create_usb_node_for_sysfs() {
    # Create a usb device node for a given sysfs path
    path="$1"           # sysfs path for the device
    usb_createnode="$2" # Path to the USB device node creation script
    usb_group="$3"      # The group to give ownership of the node to
    if $EXTERN test -r "${path}/dev"; then
        dev="`$EXTERN cat "${path}/dev" 2> /dev/null`"
        major="`expr "$dev" : '\(.*\):' 2> /dev/null`"
        minor="`expr "$dev" : '.*:\(.*\)' 2> /dev/null`"
        class="`$EXTERN cat ${path}/bDeviceClass 2> /dev/null`"
        $EXTERN sh "${usb_createnode}" "$major" "$minor" "$class" \
              "${usb_group}" 2>/dev/null
    fi
}

# install_device_node_setup contains some aliases for unit testing purposes.  # Set them to their normal values here.
udev_rule_file=/etc/udev/rules.d/10-vboxdrv.rules # Set this to /dev/null
                                                  # for unit testing
sysfs_usb_devices="/sys/bus/usb/devices/*"

install_device_node_setup() {
    # Install udev rules and create device nodes for usb access
    # To unit test, set $EXTERN to point to a function simulating these
    # functions (defined further up in this file): install_udev;
    # install_create_usb_node_for_sysfs.  See the code for usage.
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
    $EXTERN install_udev "${vboxdrv_group}" "$VBOXDRV_MODE" \
                         "$INSTALLATION_DIR" "${usb_group}" \
                         "$INSTALL_NO_UDEV" > ${udev_rule_file}
    # Build our device tree
    for i in ${sysfs_usb_devices}; do  # This line intentionally without quotes.
        $EXTERN install_create_usb_node_for_sysfs "$i" "${usb_createnode}" \
                                                  "${usb_group}"
    done
}
