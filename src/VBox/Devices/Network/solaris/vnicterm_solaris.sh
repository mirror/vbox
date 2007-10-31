#!/bin/bash

# VirtualBox VNIC terminate script for Solaris hosts with Crossbow.
# usage: ./vnicterm.sh vnicname
#
# format of VNIC interface names MUST be like [name][number]
# example: vnic1, vnic2, vnic900 etc.

if [ -z "$1" ]; then
    echo "Missing VNIC interface name."
    echo
    echo "Usage: $0 vnicname"
    exit 1
fi

/sbin/ifconfig $1 unplumb
exit $?

