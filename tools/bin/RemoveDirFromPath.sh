# !kmk_ash
# $Id$
## @file
# Shell (bash + kmk_ash) function for removing a directory from the PATH.
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
    # Parameters.
    local MY_SEP="$1"
    local MY_DIR="$2"
    if test "${KBUILD_HOST}" = "win"; then
        MY_DIR="$(cygpath -u "${MY_DIR}")"
    fi

    # Set the PATH components as script argument.
    local MY_OLD_IFS="${IFS}"
    IFS="${MY_SEP}"
    set -- ${PATH}
    IFS="${MY_OLD_IFS}"

    # Iterate the components and rebuild the path.
    PATH=""
    local MY_SEP_PREV=""
    local MY_COMPONENT
    for MY_COMPONENT
    do
        if test "${MY_COMPONENT}" != "${MY_DIR}"; then
            PATH="${PATH}${MY_SEP_PREV}${MY_COMPONENT}"
            MY_SEP_PREV="${MY_SEP}" # Helps not eliminating empty entries.
        fi
    done
}

