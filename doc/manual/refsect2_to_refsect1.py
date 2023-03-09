#!/usr/bin/env python3
import glob
import getopt
import os
import sys
import shutil
import logging

"""
    correct_references.py:
        This script scans all .dita files found in the as-argument-passed folder
        and creates a dictionary of topic id and file names. This is done since
        DITA expects href targets in the form of filename#topic-id. Then all the
        href targets found in man_VBoxManage... files are prefixed with files names.
"""

__copyright__ = \
"""
Copyright (C) 2009-2023 Oracle and/or its affiliates.

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

def replaceRefsect2(file_content):
    new_content = []
    """
        * Add a refsect1 before the first refsect2,
        * convert refsect2 to refsect1,
        * remove the </refsect1> which comes after a </refsect2>
    """
    first_refsect2 = False
    # remove lines that consist of new line only
    file_content = [line for line in file_content if line != "\n"]
    new_content.append(file_content[0])
    for current, next in  zip(file_content, file_content[1:]):

        if "<refsect2" in next:
            if not first_refsect2:
                first_refsect2 = True
                new_content.append("</refsect1>")
                new_content.append("\n")
            new_content.append(next.replace("<refsect2", "<refsect1"))
        elif "</refsect2>" in next:
            new_content.append("</refsect1>")
            new_content.append("\n")
        elif not ("</refsect1>" in next and "</refsect2>" in current):
            new_content.append(next)
    return new_content


def usage(arg):
    print('refsect2_to_refsect1.py -i <input_file> -o <output_file>')
    sys.exit(arg)

def main():
    input_file = ''
    output_file = ''
    try:
        opts, args = getopt.getopt(sys.argv[1:],"hi:o:")
    except getopt.GetoptError as err:
        print(err)
        usage(2)
    for opt, arg in opts:
        if opt == '-h':
            usage(0)
        elif opt in ("-i"):
            input_file = arg
        elif opt in ("-o"):
            output_file = arg

    if not input_file:
        logging.error('No input file is provided. Exiting')
        usage(2)
    if not output_file:
        logging.error('No output file is provided. Exiting')
        usage(2)

    file_handle = open(input_file, 'r', encoding="utf-8")
    file_content = file_handle.readlines()
    new_file_content = replaceRefsect2(file_content)
    file_handle.close()
    file_handle = open(output_file , 'w', encoding="utf-8")
    file_handle.write("".join(new_file_content))
    file_handle.close()

if __name__ == "__main__":
    main()
