#!/bin/sh
# Sun VirtualBox
# VirtualBox preremove script for VBI kernel package on Solaris hosts.
#
# Copyright (C) 2008-2009 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

echo "Sun VirtualBox Kernel Package - preremove script"
echo "This script will unload the VirtualBox kernel interface module..."

# check for vbi and force unload it
vbi_mod_id=`/usr/sbin/modinfo | grep vbi | cut -f 1 -d ' ' `
if test -n "$vbi_mod_id"; then
    /usr/sbin/modunload -i $vbi_mod_id
fi

echo "Done."

