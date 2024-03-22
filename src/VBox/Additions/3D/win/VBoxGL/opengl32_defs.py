"""
Copyright (C) 2024 Oracle and/or its affiliates.

This file is part of VirtualBox base platform packages, as
available from https://www.virtualbox.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, in version 3 of the
License.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <https://www.gnu.org/licenses>.

SPDX-License-Identifier: GPL-3.0-only
"""

import sys

#
# Strip @d from function names:
# glAccum@8                            @7
# DrvCopyContext@12
#
def GenerateDef():

    # Read lines from input file
    in_file = open(sys.argv[1], "r")
    if not in_file:
        print("Error: couldn't open %s file!" % sys.argv[1])
        sys.exit()

    lines = in_file.readlines()

    in_file.close()

    # Write def file
    out_file = open(sys.argv[2], "w")
    if not out_file:
        print("Error: couldn't open %s file!" % sys.argv[2])
        sys.exit()

    out_file.write('EXPORTS\n')

    for line in lines:
        line = line.strip()
        if len(line) == 0:
            continue

        line_parts = line.split()
        if len(line_parts) == 0:
            continue

        name_parts = line_parts[0].split('@')
        if len(name_parts) != 2:
            continue

        out_line = name_parts[0]
        if len(line_parts) > 1:
            numSpaces = 30 - len(name_parts[0])
            out_line = out_line + ' '*numSpaces + line_parts[1]

        out_file.write('\t' + out_line + '\n')

    out_file.close()

GenerateDef()
