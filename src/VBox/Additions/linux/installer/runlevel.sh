#! /bin/bash
# InnoTek VirtualBox
# Linux Additions add/delete init script
#
# Copyright (C) 2006 InnoTek Systemberatung GmbH
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License as published by the Free Software Foundation,
# in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
# distribution. VirtualBox OSE is distributed in the hope that it will
# be useful, but WITHOUT ANY WARRANTY of any kind.
#
# If you received this file as part of a commercial VirtualBox
# distribution, then only the terms of your commercial VirtualBox
# license agreement apply instead of the previous paragraph.

runlevels=35

system=unknown
if [ -f /etc/redhat-release ]; then
    system=redhat
    util=$(type -p /sbin/chkconfig)
elif [ -f /etc/SuSE-release ]; then
    system=suse
    util=$(type -p /sbin/chkconfig)
elif [ -f /etc/debian_version ]; then
    system=debian
    util=$(type -p update-rc.d)
elif [ -f /etc/gentoo-release ]; then
    system=gentoo
    util=$(type -p rc-update)
else
    echo "$0: Unknown system" 1>&2
fi

if [ -z $util ];then
    echo Could not find add/remove init scripts to a runlevel utility 1>&2
    echo This operation can not continue without it 1>&2
    exit 1
fi

fail() {
    echo "$1"
    exit 1
}


addrunlevel() {
    if [ $system == "redhat" ] || [ $system == "suse" ]; then
        $util --del $1 >&/dev/null

        if $util -v &>/dev/null; then
            $util --level $runlevels $1 on || {
                fail "Cannot add $1 to run levels: $runlevels"
            }
        else
            $util $1 $runlevels || {
                fail "Cannot add $1 to run levels: $runlevels"
            }
        fi
    elif [ $system == "debian" ]; then
        # Debian does not support dependencies currently -- use argument $2
        # for start sequence number and argument $3 for stop sequence number
        $util -f $1 remove >&/dev/null
        $util $1 defaults $2 $3 >&/dev/null
    elif [ $system == "gentoo" ]; then
        $util del $1 >&/dev/null
        $util add $1 default >&/dev/null
    fi
}


delrunlevel() {
    if [ $system == "redhat" ] || [ $system == "suse" ]; then
        if $util --list $1 >& /dev/null; then
            $util --del $1 >& /dev/null || {
                fail "Cannot delete $1 from runlevels"
            }
        fi
    elif [ $system == "debian" ]; then
        $util -f $1 remove >&/dev/null
    elif [ $system == "gentoo" ]; then
        $util del $1 >&/dev/null
    fi
}


usage() {
    echo "Usage: $0 {add|del} script"
    exit 1
}


# Test for second argument
test -z $2 && {
    usage
}

case "$1" in
add)
    addrunlevel $2 $3 $4
    ;;
del)
    delrunlevel $2
    ;;
*)
    usage
esac

exit
