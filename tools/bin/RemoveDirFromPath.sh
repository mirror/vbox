# !kmk_ash
# $Id$
## @file
# Bash function for removing a directory from the PATH.
#
# This is used by tools/env.sh but cannot be a part of it because kmk_ash
# freaks out when it sees the bash-specific substitutions.
#
# Assumes KBUILD_HOST is set.
#

#
# Copyright (C) 2020-2022 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

##
# Modifies the PATH variable by removing $1.
#
# @param 1     The PATH separator (":" or ";").
# @param 2     The directory to remove from the path.
RemoveDirFromPath()
{
    MY_SEP=$1
    MY_DIR=$2
    if test "${KBUILD_HOST}" = "win"; then
        MY_DIR="$(cygpath -u "${MY_DIR}")"
    fi

    # This should work at least back to bash 2.0 if the info page is anything to go by.
    PATH="${MY_SEP}${PATH}${MY_SEP}"                        # Make sure there are separators at both ends.
    PATH="${PATH//${MY_SEP}${MY_DIR}${MY_SEP}/${MY_SEP}}"   # Remove all (=first '/') ${MY_DIR} instance.
    PATH="${PATH#${MY_SEP}}"                                # Remove the leading separator we added above.
    PATH="${PATH%${MY_SEP}}"                                # Remove the trailing separator we added above.
}

