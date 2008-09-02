#!/bin/sh
# Sun xVM VirtualBox
# VirtualBox preremove script for VBI kernel package on Solaris hosts.
#
# Copyright (C) 2008 Sun Microsystems, Inc.
#
# Sun Microsystems, Inc. confidential
# All rights reserved
#

echo "Sun xVM VirtualBox Kernel Package - preremove script"
echo "This script will unload the VirtualBox kernel interface module..."

# check for vbi and force unload it
vbi_mod_id=`/usr/sbin/modinfo | grep vbi | cut -f 1 -d ' ' `
if test -n "$vbi_mod_id"; then
    /usr/sbin/modunload -i $vbi_mod_id
fi

echo "Done."

