#!/bin/bash

# VirtualBox VNIC setup script for Solaris hosts with Crossbow.
# usage: ./vnicinit.sh vnicname
#
# format of VNIC interface names MUST be like [name][number]
# example: vnic1, vnic2, vnic900 etc.

if [ -z "$1" ]; then
    echo "Missing VNIC interface name."
    echo
    echo "Usage: $0 vnicname"
    exit 1
fi

vnic_name=`echo $1 | /usr/xpg4/bin/tr -s [:upper:] [:lower:]`
vnic_id=${vnic_name##*[a-z]}
vnic_name=$1
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

/sbin/ifconfig $vnic_name plumb
/sbin/ifconfig $vnic_name $host_ip destination $guest_ip netmask $netmask up

# Output the VNIC name though not used by VirtualBox
echo "$vnic_name"
exit $?

