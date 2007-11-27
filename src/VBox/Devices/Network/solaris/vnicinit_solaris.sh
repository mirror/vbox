#!/bin/bash

# VirtualBox VNIC setup script for Solaris hosts with Crossbow.

if [ -z "$1" ]; then
    echo "Missing MAC address."
    echo
    echo "Usage: $0 macaddress [vnicname]"
    echo "       A new VNIC is created if no vnicname is provided."
    exit 1
fi

# Try obtain a physical NIC that is currently active
phys_nic=`/usr/sbin/dladm show-dev | /usr/bin/awk 'NF==7 && $3=="up" { print $1 }'`
if [ -z "$phys_nic" ]; then
    # Failed to get a currently active NIC, get the first available NIC.
    phys_nic=`/usr/sbin/dladm show-link | /usr/bin/nawk '/legacy/ {next} {print $1; exit}'`
    if [ -z "$phys_nic" ]; then
        # Failed to get any NICs!
        echo "Failed to get a physical NIC to bind to."
        exit 1
    fi
fi
vnic_id=0
vnic_name=""
mac=$1

# Create the VNIC if required
if [ -z "$2" ]; then
    # To use a specific physical NIC, replace $phys_nic with the name of the NIC.
    vnic_id=`/usr/lib/vna $phys_nic $mac`
    if [ $? != 0 ]; then
        exit 1
    fi
    vnic_name=vnic${vnic_id}
else
    vnic_name=$2
    vnic_id=${vnic_name##*[a-z]}
fi

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
exit $?

