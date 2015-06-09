# Oracle VM VirtualBox
# VirtualBox installer shell routines
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

## @todo Make this file into a script in the VirtualBox lib directory once
#        enough code has been made shared between the different installers.

# This is used for unit testing and will be reset after the file is sourced for
# test runs.
unset EXTERN

udev_write_vboxdrv() {
    VBOXDRV_GRP="$1"
    VBOXDRV_MODE="$2"

    echo "KERNEL==\"vboxdrv\", NAME=\"vboxdrv\", OWNER=\"root\", GROUP=\"$VBOXDRV_GRP\", MODE=\"$VBOXDRV_MODE\""
    echo "KERNEL==\"vboxdrvu\", NAME=\"vboxdrvu\", OWNER=\"root\", GROUP=\"root\", MODE=\"0666\""
    echo "KERNEL==\"vboxnetctl\", NAME=\"vboxnetctl\", OWNER=\"root\", GROUP=\"$VBOXDRV_GRP\", MODE=\"$VBOXDRV_MODE\""
}

udev_write_usb() {
    INSTALLATION_DIR="$1"
    USB_GROUP="$2"

    echo "SUBSYSTEM==\"usb_device\", ACTION==\"add\", RUN+=\"$INSTALLATION_DIR/VBoxCreateUSBNode.sh \$major \$minor \$attr{bDeviceClass}${USB_GROUP}\""
    echo "SUBSYSTEM==\"usb\", ACTION==\"add\", ENV{DEVTYPE}==\"usb_device\", RUN+=\"$INSTALLATION_DIR/VBoxCreateUSBNode.sh \$major \$minor \$attr{bDeviceClass}${USB_GROUP}\""
    echo "SUBSYSTEM==\"usb_device\", ACTION==\"remove\", RUN+=\"$INSTALLATION_DIR/VBoxCreateUSBNode.sh --remove \$major \$minor\""
    echo "SUBSYSTEM==\"usb\", ACTION==\"remove\", ENV{DEVTYPE}==\"usb_device\", RUN+=\"$INSTALLATION_DIR/VBoxCreateUSBNode.sh --remove \$major \$minor\""
}

generate_udev_rule() {
    VBOXDRV_GRP="$1"      # The group owning the vboxdrv device
    VBOXDRV_MODE="$2"     # The access mode for the vboxdrv device
    INSTALLATION_DIR="$3" # The directory VirtualBox is installed in
    USB_GROUP="$4"        # The group that has permission to access USB devices
    NO_INSTALL="$5"       # Set this to "1" to remove but not re-install rules
    UDEV_STRING="$6"      # The output of the udev version command

    # Extra space!
    case "$USB_GROUP" in ?*) USB_GROUP=" $USB_GROUP" ;; esac
    case "$NO_INSTALL" in
    "1") ;;
    *)
        udev_ver=`expr "$UDEV_STRING" : '[^0-9]*\([0-9]*\)'`
        udev_fix=""
        test "$udev_ver" = "" -o "$udev_ver" -lt 55 &&
            udev_fix="1"
        udev_do_usb=""
        test "$udev_ver" -ge 59 &&
            udev_do_usb="1"
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
        ;;
    esac
}

install_udev() {
    # install udev rule (disable with INSTALL_NO_UDEV=1 in /etc/default/virtualbox) for distribution packages
    VBOXDRV_GRP="$1"      # The group owning the vboxdrv device
    VBOXDRV_MODE="$2"     # The access mode for the vboxdrv device
    INSTALLATION_DIR="$3" # The directory VirtualBox is installed in
    USB_GROUP="$4"        # The group that has permission to access USB devices
    NO_INSTALL="$5"       # Set this to "1" to remove but not re-install rules

    if test -d /etc/udev/rules.d; then
        udev_out="`udevadm version 2>/dev/null ||  udevinfo -V 2>/dev/null`"
        generate_udev_rule "$VBOXDRV_GRP" "$VBOXDRV_MODE" "$INSTALLATION_DIR" \
                           "$USB_GROUP" "$NO_INSTALL" "$udev_out"
    fi
    # Remove old udev description file
    rm -f /etc/udev/rules.d/10-vboxdrv.rules 2> /dev/null
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
udev_rule_file=/etc/udev/rules.d/60-vboxdrv.rules # Set this to /dev/null
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

set_selinux_permissions() {
    # XXX SELinux: allow text relocation entries
    INSTALLATION_DIR="$1"  # Where the VirtualBox binaries are installed to
    SHARE_DIR="$2"         # Where shared bits are installed to
    if [ -x /usr/bin/chcon ]; then
        chcon -t texrel_shlib_t "$INSTALLATION_DIR"/*VBox* > /dev/null 2>&1
        chcon -t texrel_shlib_t "$INSTALLATION_DIR"/VBoxAuth.so \
            > /dev/null 2>&1
        chcon -t texrel_shlib_t "$INSTALLATION_DIR"/VirtualBox.so \
            > /dev/null 2>&1
        chcon -t texrel_shlib_t "$INSTALLATION_DIR"/components/VBox*.so \
            > /dev/null 2>&1
        chcon -t java_exec_t    "$INSTALLATION_DIR"/VirtualBox > /dev/null 2>&1
        chcon -t java_exec_t    "$INSTALLATION_DIR"/VBoxSDL > /dev/null 2>&1
        chcon -t java_exec_t    "$INSTALLATION_DIR"/VBoxHeadless \
            > /dev/null 2>&1
        chcon -t java_exec_t    "$INSTALLATION_DIR"/VBoxNetDHCP \
            > /dev/null 2>&1
        chcon -t java_exec_t    "$INSTALLATION_DIR"/VBoxNetNAT \
            > /dev/null 2>&1
        chcon -t java_exec_t    "$INSTALLATION_DIR"/VBoxExtPackHelperApp \
            > /dev/null 2>&1
        chcon -t java_exec_t    "$INSTALLATION_DIR"/vboxwebsrv > /dev/null 2>&1
        chcon -t java_exec_t    "$INSTALLATION_DIR"/webtest > /dev/null 2>&1
        chcon -t bin_t          "$SHARE_DIR"/src/vboxhost/build_in_tmp \
             > /dev/null 2>&1
    fi
}
