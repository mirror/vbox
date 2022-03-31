#!/usr/bin/env kmk_ash

#
# Copyright (C) 2022 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

# Get parameters.
     CP="$1"
     MV="$2"
    SED="$3"
 OUTDIR="$4"
 SRCDIR="$5"
   FILE="$6"

# Globals.
set -e
LC_ALL=C
export LC_ALL
SRCFILE="${SRCDIR}/tstIEMAImplData${FILE}.cpp"
OUTFILE="${OUTDIR}/tstIEMAImplData${FILE}.cpp"

# Copy the file and deal with empty file.
if test -f "${SRCFILE}"; then
    "${CP}" -f -- "${SRCFILE}" "${OUTFILE}.tmp"
else
    > "${OUTFILE}.tmp"
fi
if ! test -s "${OUTFILE}.tmp"; then
    echo '#include "tstIEMAImpl.h"' >> "${OUTFILE}.tmp"
fi
echo '' >> "${OUTFILE}.tmp"

# Stub empty test arrays.
"${SED}" -n \
    -e 's/TSTIEM_DECLARE_TEST_ARRAY[(]'"${FILE}"', *\([^,]*\), *\([^ ][^ ]*\) *[)];/\1\n\2/p' \
    "${SRCDIR}/tstIEMAImpl.h" \
| \
while IFS= read -r a_Type && IFS= read -r a_Instr;
do
    if "${SED}" -n -e "/ const g_cTests_${a_Instr} /q1" "${OUTFILE}.tmp"; then
        echo "TSTIEM_DEFINE_EMPTY_TEST_ARRAY(${a_Type}, ${a_Instr});" >> "${OUTFILE}.tmp"
    fi
done

# Put the file in place.
"${MV}" -f -- "${OUTFILE}.tmp" "${OUTFILE}"

