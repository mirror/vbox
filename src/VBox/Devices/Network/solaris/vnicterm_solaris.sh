#!/bin/bash
# innotek VirtualBox
# Copyright (C) 2007 innotek GmbH
#
# VirtualBox VNIC terminate script for Solaris hosts with Crossbow.
#

if [ -z "$1" ]; then
    echo "Missing VNIC interface name."
    echo
    echo "Usage: $0 vnicname"
    exit 1
fi

/sbin/ifconfig $1 unplumb
vnic_id=${1##*[a-z]}
/usr/lib/vna ${vnic_id}
exit $?

