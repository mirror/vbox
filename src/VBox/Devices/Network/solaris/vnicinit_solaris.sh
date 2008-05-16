#!/bin/bash
# Sun xVM VirtualBox
# VirtualBox VNIC setup script for Solaris hosts with Crossbow.
#
# Copyright (C) 2007 Sun Microsystems, Inc.
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#
# Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
# Clara, CA 95054 USA or visit http://www.sun.com if you need
# additional information or have any questions.
#

if [ -z "$1" ]; then
    echo "Missing MAC address."
    echo
    echo "Usage: $0 macaddress [vnicname]"
    echo "       A new VNIC is created if no vnicname is provided."
    exit 1
fi

mac=$1
# Create the VNIC if required
if [ -z "$2" ]; then
    # Try obtain a physical NIC that is currently active
    phys_nic=`/usr/sbin/dladm show-dev | /usr/bin/awk 'NF==7 && $3=="up" { print $1 }'`
    if [ -z "$phys_nic" ]; then
        # Try obtain a physical NIC that is currently active
        phys_nic=`/usr/sbin/dladm show-dev | /usr/bin/awk 'NF==4 && $2=="up" { print $1 }'`
        # Failed to get a currently active NIC, get the first available NIC.
        phys_nic=`/usr/sbin/dladm show-link | /usr/bin/nawk '/legacy/ {next} {print $1; exit}'`
        if [ -z "$phys_nic" ]; then
            # Failed to get any NICs!
            echo "Failed to get a physical NIC to bind to."
            exit 1
        fi
    fi

    # To use a specific physical NIC, replace $phys_nic with the name of the NIC.
    vnic_name="vnic"`/usr/lib/vna $phys_nic $mac`
    if [ $? != 0 ]; then
        exit 1
    fi
else
    vnic_name=$2
fi
vnic_id=${vnic_name##*[a-z]}

if [ ${vnic_id} -lt 10 ]; then
    host_ip="192.168.1.10${vnic_id}"
    guest_ip="192.168.1.20${vnic_id}"
elif [ ${vnic_id} -lt 256 ]; then
    host_ip="192.168.1.${vnic_id}"
    guest_ip="192.168.2.${vnic_id}"
elif [ ${vnic_id} -gt 899 ]; then
    let "temp_id = $vnic_id % 900"
    host_ip="192.168.1.10${temp_id}"
    guest_ip="192.168.1.20${temp_id}"
else
    # VNIC ID is probably off the scale!
    host_ip="192.168.1.10"
    guest_ip="192.168.1.20"
fi

netmask="255.255.255.0"

if [ -z "$2" ]; then
    /sbin/ifconfig $vnic_name plumb
    /sbin/ifconfig $vnic_name $host_ip destination $guest_ip netmask $netmask up
#else
#   Do existing VNIC configuration here if needed...
fi

echo "$vnic_name"
exit 0

