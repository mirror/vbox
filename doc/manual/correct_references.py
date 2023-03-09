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

"""
key is the id, value is the name of the containing file.
"""
xref_dictionary = {}

""""
    =================================================================
    Cross reference utility functions to add file name to target name.
    =================================================================
"""
# Search for hrefs and return a tuple where 0th element is target and 1st element is its index in @p line
def extractHrefTarget(line, start = 0):
    target = ""
    pointer0 = line.find("href=\"", start)
    if pointer0 == -1:
        return (target, -1)
    pointer0 += len("href=\"")
    pointer1 = line.find("\"", pointer0)
    if pointer1 == -1:
        return (target, -1)
    return (line[pointer0:pointer1], pointer0)

# Return list of href targets contained in @p line
def extractHrefList(line):
    targets = []
    index = 0
    while index < len(line):
        ttarget = extractHrefTarget(line, index)
        index = ttarget[1]
        if index == -1:
            break
        targets.append(ttarget[0])
    return targets

def correctHrefTargets(file_content):
    for idx, line in enumerate(file_content):
        if "href" in line:
            targets = extractHrefList(line)
            newline = line
            for target in targets:
                if target in xref_dictionary and os.path.basename != xref_dictionary[target]:
                    old = "\"" + target + "\""
                    new = "\"" + xref_dictionary[target] + "#" + target + "\""
                    newline = newline.replace(old, new)
            file_content[idx] = newline

def createXrefIdDictionary(ditafolder):
    xref_dictionary.clear()
    dita_files = glob.glob(ditafolder + "/*.dita")
    for file in dita_files:
        file_content = open(file, 'r', encoding="utf-8")
        for line in file_content:
            if "reference" in line or "topic" in line or "section" in line:
                start = line.find("id=\"")
                if start != -1:
                    start += len("id=\"")
                    end = line.find("\"", start)
                    if end != -1:
                        id = line[start:end]
                        if len(id) > 0:
                            if id not in xref_dictionary:
                                xref_dictionary[id] = os.path.basename(file)
                                print("%s %s" % (id, os.path.basename(file)))
                            else:
                                logging.warning('Non unique topic/section id %s in file %s. This is already found in %s'
                                    % (id, os.path.basename(file), os.path.basename(xref_dictionary[id])))

def usage(arg):
    print('correct_references.py -d <ditafolder>')
    sys.exit(arg)

def main():
    ditafolder = ''
    try:
        opts, args = getopt.getopt(sys.argv[1:],"hd:")
    except getopt.GetoptError as err:
        print(err)
        usage(2)
    for opt, arg in opts:
        if opt == '-h':
            usage(0)
        elif opt in ("-d"):
            ditafolder = arg

    if not ditafolder:
        logging.error('No folder is provided. Exiting')
        usage(2)
    createXrefIdDictionary(ditafolder)

    vboxmanage_dita_files = glob.glob(ditafolder + "/man_V*dita")
    for file in vboxmanage_dita_files:
        print(file)
        file_handle = open(file, 'r', encoding="utf-8")
        file_content = file_handle.readlines()
        correctHrefTargets(file_content)
        file_handle.close()
        file_handle = open(file, 'w', encoding="utf-8")
        file_handle.write("".join(file_content))
        file_handle.close()
if __name__ == "__main__":
    main()
