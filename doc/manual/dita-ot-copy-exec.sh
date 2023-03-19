#!/usr/bin/env kmk_ash
# $Id$
## @file
# Helper Script for copying the DITA-OT toolkit from $1 to $2,
# run the following command line, nuke $2.
#
# DITA-OT-1.8.5 updates around 40 files everytime it is started in an manner
# that will cause race conditions when starting multiple dost.jar instances
# concurrently.  So, we work around that by running each job of a unqiue and
# temporary copy.  Extra effort is taken to using hardlinking when possible
# and to reduce the number of files and directory we copy.
#

#
# Copyright (C) 2023 Oracle and/or its affiliates.
#
# This file is part of VirtualBox base platform packages, as
# available from https://www.virtualbox.org.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation, in version 3 of the
# License.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <https://www.gnu.org/licenses>.
#
# SPDX-License-Identifier: GPL-3.0-only
#

#
# Some constants.
#
MY_INSTALL="kmk_install"
MY_MKDIR="kmk_mkdir"
MY_RM="kmk_rm"
MY_SED="kmk_sed"
MY_CP="kmk_cp"

# Debug
MY_DEBUG_INSTALL="-v"
MY_DEBUG_INSTALL=""

MY_DEBUG_MKDIR="-v"
MY_DEBUG_MKDIR=""

MY_DEBUG_RM="-v"
MY_DEBUG_RM=""

#
# Pop off the source and target directories from the input arguments.
# We're not trying to be very userfriendly here, just reasonably safe.
#
if test $# -lt 5; then
    echo "syntax error: too few arguments" 1>&2;
    exit 2;
fi
MY_SRCDIR="$1"
MY_DSTDIR="$2"
if test "$3" != "--"; then
    echo "syntax error: Expected '--' as the 3rd parameter, got: $3" 1>&2;
    exit 2;
fi
shift 3
if ! test -d "${MY_SRCDIR}"; then
    echo "error: Source directory does not exists or is not a directory: ${MY_SRCDIR}" 1>&2;
    exit 1;
fi
if test -e "${MY_DSTDIR}"; then
    echo "error: Destination directory already exists: ${MY_DSTDIR}" 1>&2;
    exit 1;
fi

#
# Helper.
#
CopyTree()
{
    MY_SRCTREE="$1"
    MY_DSTTREE="$2"

    # Gather files and directories.
    cd "${MY_SRCTREE}"
    MY_DIRS=""
    MY_DIRS_PAIRS=""
    MY_FILES=""
    for MY_FILE in *;
    do
        if test -d "${MY_SRCTREE}/${MY_FILE}"; then
            case "${MY_SRCTREE}/${MY_FILE}" in
                *\ *)
                    echo "Unexpected space in dir/subdir: ${MY_SRCTREE}/${MY_FILE}" 1>&2;
                    exit 1
                    ;;
            esac
            MY_DIRS="${MY_DIRS} ${MY_FILE}"
            MY_DIRS_PAIRS="${MY_DIRS_PAIRS} ${MY_SRCTREE}/${MY_FILE} ${MY_DSTTREE}/${MY_FILE}"
        elif test -f "${MY_SRCTREE}/${MY_FILE}"; then
            case "${MY_SRCTREE}/${MY_FILE}" in
                *\ *)
                    echo "Unexpected space in dir/filename: ${MY_SRCTREE}/${MY_FILE}" 1>&2;
                    exit 1
                    ;;

                *org.dita.dost.platform/plugin.properties)
                    ;;
                *_template.xml)
                    MY_FILES="${MY_FILES} ${MY_FILE}"
                    MY_TEMPLATED_FILES="${MY_TEMPLATED_FILES} ${MY_DSTTREE}/${MY_FILE%_template.xml}.xml"
                    ;;
                *_template.xsl)
                    MY_FILES="${MY_FILES} ${MY_FILE}"
                    MY_TEMPLATED_FILES="${MY_TEMPLATED_FILES} ${MY_DSTTREE}/${MY_FILE%_template.xsl}.xsl"
                    ;;
                *)
                    MY_FILES="${MY_FILES} ${MY_FILE}"
                    ;;
            esac
        else
            echo "Unexpected file type: ${MY_SRCTREE}/${MY_FILE}" 1>&2;
            exit 1
        fi
    done

    # Install all the files in one go.
    if test -n "${MY_FILES}"; then
        "${MY_INSTALL}" ${MY_DEBUG_INSTALL} --hard-link-files-when-possible -m644 -- ${MY_FILES} "${MY_DSTTREE}/"
    fi

    # Create all subdirs and recurse into each.
    if test -n "${MY_DIRS}"; then
        cd "${MY_DSTTREE}"
        "${MY_MKDIR}" -p ${MY_DEBUG_MKDIR} -- ${MY_DIRS}

        # Now transfer MY_DIRS_PAIRS to the argument list, so they're preseved across recursive calls.
        # (the local command/keyword is busted in current binaries, though fixed in kBuild svn.)
        set -- ${MY_DIRS_PAIRS};
        while test "$#" -ge 2;
        do
            CopyTree "${1}" "${2}"
            shift  2
        done
    fi
}

#
# Copy the files over.
#
set -e
MY_SAVED_CWD=`pwd`
"${MY_MKDIR}" -p ${MY_DEBUG_MKDIR} -- "${MY_DSTDIR}"

# Selected root files.
"${MY_INSTALL}" ${MY_DEBUG_INSTALL} --hard-link-files-when-possible -m644 -- \
    "${MY_SRCDIR}/build_template.xml" \
    "${MY_SRCDIR}/catalog-dita.txt" \
    "${MY_SRCDIR}/catalog-dita_template.xml" \
    "${MY_SRCDIR}/integrator.properties" \
    "${MY_SRCDIR}/integrator.xml" \
    "${MY_DSTDIR}/"

# Selected directory trees.
MY_TEMPLATED_FILES=""
for MY_SUBDIR in css dtd lib plugins resource schema tools xsl;
do
    "${MY_MKDIR}" -p ${MY_DEBUG_MKDIR} -- "${MY_DSTDIR}/${MY_SUBDIR}"
    CopyTree "${MY_SRCDIR}/${MY_SUBDIR}" "${MY_DSTDIR}/${MY_SUBDIR}"
done

# Now remove all files that has a _template.xml/xsl variant, just to ensure
# that if hardlinked the copy in the tree won't be modified.
if test -n "${MY_TEMPLATED_FILES}"; then
    "${MY_RM}" -f ${MY_DEBUG_RM} -- ${MY_TEMPLATED_FILES}
fi

cd "${MY_SAVED_CWD}"
set +e

#
# Execute the command.
#
"$@"
MY_EXITCODE=$?

#
# Cleanup and exit.
#
#"${MY_RM}" -Rf ${MY_DEBUG_RM} -- "${MY_DSTDIR}"
exit ${MY_EXITCODE}

