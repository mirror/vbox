#!/bin/bash

# VirtualBox TAP setup script for Solaris hosts.
# usage: ./tapinit.sh tapname
#
# format of TAP interface names MUST be like [name][number]
# example: tap0, tap1, tap2 etc.

if [ -z "$1" ]; then
    echo "Missing TAP interface name."
    echo
    echo "Usage: $0 tapifname"
    exit 1
fi

tap_name=`echo $1 | /usr/xpg4/bin/tr -s [:upper:] [:lower:]`
ppa=${tap_name##*[a-z]}
tap_name=$1
host_ip="192.168.1.10${ppa}"
guest_ip="192.168.1.20${ppa}"
netmask="255.255.255.0"

/sbin/ifconfig $tap_name $host_ip destination $guest_ip netmask $netmask
/sbin/ifconfig $tap_name up

# Output the TAP interface though not used by VirtualBox
echo "$tap_name"

exit $?

