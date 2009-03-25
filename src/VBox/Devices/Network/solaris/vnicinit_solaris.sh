#!/bin/bash
# Sun VirtualBox
# VirtualBox VNIC setup script for Solaris hosts with Crossbow.
#
# Copyright (C) 2007-2009 Sun Microsystems, Inc.
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
    # snv <= 82 is to handled differently (dladm format changes at 83+)
    snv_str=`uname -v`
    snv_num=${snv_str##*[a-z][_]}

    # Try obtain one that's currently active (82 dladm show-link doesn't indicate status; just use show-dev atm)
    if [ $snv_num -le 82 ]; then
        phys_nic=`/usr/sbin/dladm show-dev -p | /usr/bin/awk 'NF==4 && $2=="link=up" { print $1 }'`
    elif [ $snv_num -le 95 ]; then
        phys_field=`/usr/sbin/dladm show-link -p | /usr/bin/awk 'NF==5 && $4=="STATE=\"up\"" { print $1 }'`
        eval $phys_field
        phys_nic="$LINK"
    else
        phys_field=`/usr/sbin/dladm show-link -p -o link,state | /usr/bin/awk 'BEGIN{FS=":"} /up/ {print $1}'`
        eval $phys_field
        phys_nic="$LINK"
    fi

    if [ -z "$phys_nic" ]; then
        # Failed to get a currently active NIC, get the first available link.
        if [ $snv_num -le 82 ]; then
            phys_nic=`/usr/sbin/dladm show-link -p | /usr/bin/nawk '/legacy/ {next} {print $1; exit}'`
        elif [ $snv_num -le 95 ]; then
            phys_field=`/usr/sbin/dladm show-link -p | /usr/bin/awk 'NF==5 && $2=="CLASS=\"phys\"" { print $1 }'`
            eval $phys_field
            phys_nic="$LINK"
        else
            phys_field=`/usr/sbin/dladm show-link -p -o link,class | /usr/bin/awk 'BEGIN{FS=":"} /up/ {print $1}'`
            eval $phys_field
            phys_nic="$LINK"
        fi
        if [ -z "$phys_nic" ]; then
            # Failed to get any NICs!
            echo "Failed to get a physical NIC to bind to."
            exit 1
        fi
    fi

    # To use a specific physical NIC, replace $phys_nic with the name of the NIC.
    vnic_name=`/usr/lib/vna $phys_nic $mac`
    if [ $? != 0 ]; then
        echo "vna failed to bind VNIC."
        exit 1
    fi

    # The vna utility can return the id/name depending on the distro
    case "$vnic_name" in
        vnic[0-9]*)
            vnic_name="$vnic_name"
            ;;
        *)
            vnic_name="vnic$vnic_name"
            ;;
    esac
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

