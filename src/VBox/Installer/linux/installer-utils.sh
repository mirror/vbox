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

my_which() {
    which "$@"
}

my_rm() {
    rm "$@"
}

setup_test_install_udev() {
    eval 'my_which() { echo test_udev ; }'
    eval 'my_rm() { true ; }'
    eval 'test_udev() { echo "$TEST_UDEV_VERSION" ; }'
}

install_udev_run() {
    # install udev rule (disable with INSTALL_NO_UDEV=1 in /etc/default/virtualbox) for distribution packages
    VBOXDRV_GRP="$1"
    VBOXDRV_MODE="$2"
    INSTALLATION_DIR="$3"
    if [ -d /etc/udev/rules.d ]; then
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
        udev_fix="="
        if [ "${udev_call}" != "" ]; then
            udev_out=`${udev_call}`
            udev_ver=`expr "$udev_out" : '[^0-9]*\([0-9]*\)'`
            if [ "$udev_ver" = "" -o "$udev_ver" -lt 55 ]; then
                udev_fix=""
            fi
        fi
        # Write udev rules
        echo "KERNEL=${udev_fix}\"vboxdrv\", NAME=\"vboxdrv\", OWNER=\"root\", GROUP=\"$VBOXDRV_GRP\", MODE=\"$VBOXDRV_MODE\""
        echo "SUBSYSTEM=${udev_fix}\"usb_device\", ACTION=${udev_fix}\"add\", RUN=\"$INSTALLATION_DIR/VBoxCreateUSBNode.sh \$major \$minor \$attr{bDeviceClass}\""
        echo "SUBSYSTEM=${udev_fix}\"usb\", ACTION=${udev_fix}\"add\", ENV{DEVTYPE}=${udev_fix}\"usb_device\", RUN=\"$INSTALLATION_DIR/VBoxCreateUSBNode.sh \$major \$minor \$attr{bDeviceClass}\""
        echo "SUBSYSTEM=${udev_fix}\"usb_device\", ACTION=${udev_fix}\"remove\", RUN=\"$INSTALLATION_DIR/VBoxCreateUSBNode.sh --remove \$major \$minor\""
        echo "SUBSYSTEM=${udev_fix}\"usb\", ACTION=${udev_fix}\"remove\", ENV{DEVTYPE}=${udev_fix}\"usb_device\", RUN=\"$INSTALLATION_DIR/VBoxCreateUSBNode.sh --remove \$major \$minor\""
    fi
    # Remove old udev description file
    if [ -f /etc/udev/rules.d/60-vboxdrv.rules ]; then
        my_rm -f /etc/udev/rules.d/60-vboxdrv.rules 2> /dev/null
    fi
}

teardown_test_install_udev() {
    true
}

install_udev_package() {
    # install udev rule (disable with INSTALL_NO_UDEV=1 in /etc/default/virtualbox) for distribution packages
    usb_group=$1
    if [ -d /etc/udev/rules.d -a "$INSTALL_NO_UDEV" != "1" ]; then
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
        udev_fix="="
        if [ "${udev_call}" != "" ]; then
            udev_out=`${udev_call}`
            udev_ver=`expr "$udev_out" : '[^0-9]*\([0-9]*\)'`
            if [ "$udev_ver" = "" -o "$udev_ver" -lt 55 ]; then
                udev_fix=""
            fi
        fi
        usb_createnode="/usr/share/virtualbox/VBoxCreateUSBNode.sh"
        echo "KERNEL=${udev_fix}\"vboxdrv\", NAME=\"vboxdrv\", OWNER=\"root\", GROUP=\"root\", MODE=\"0600\""
        echo "SUBSYSTEM=${udev_fix}\"usb_device\", ACTION=${udev_fix}\"add\", RUN=\"${usb_createnode} \$major \$minor \$attr{bDeviceClass} ${usb_group}\""
        echo "SUBSYSTEM=${udev_fix}\"usb\", ACTION=${udev_fix}\"add\", ENV{DEVTYPE}=${udev_fix}\"usb_device\", RUN=\"${usb_createnode} \$major \$minor \$attr{bDeviceClass} ${usb_group}\""
        echo "SUBSYSTEM=${udev_fix}\"usb_device\", ACTION=${udev_fix}\"remove\", RUN=\"${usb_createnode} --remove \$major \$minor\""
        echo "SUBSYSTEM=${udev_fix}\"usb\", ACTION=${udev_fix}\"remove\", ENV{DEVTYPE}=${udev_fix}\"usb_device\", RUN=\"${usb_createnode} --remove \$major \$minor\""
    fi
    # Remove old udev description file
    if [ -f /etc/udev/rules.d/60-vboxdrv.rules ]; then
        my_rm -f /etc/udev/rules.d/60-vboxdrv.rules 2> /dev/null
    fi
}
