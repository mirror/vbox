#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$

"""
Analyzer Experiment  1.
"""

__copyright__ = \
"""
Copyright (C) 2010-2022 Oracle and/or its affiliates.

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

The contents of this file may alternatively be used under the terms
of the Common Development and Distribution License Version 1.0
(CDDL), a copy of it is provided in the "COPYING.CDDL" file included
in the VirtualBox distribution, in which case the provisions of the
CDDL are applicable instead of those of the GPL.

You may elect to license modified versions of this file under the
terms and conditions of either the GPL or the CDDL or both.

SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
"""
__version__ = "$Revision$"


import os.path
import sys

# Only the main script needs to modify the path.
try:    __file__
except: __file__ = sys.argv[0];
g_ksValidationKitDir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)));
sys.path.append(g_ksValidationKitDir);

# Validation Kit imports.
from testanalysis import reader    ## @todo fix testanalysis/__init__.py.
from testanalysis import reporting
from testanalysis import diff


def usage():
    """ Display usage """
    print('usage: %s [options] <testresults.xml> [baseline.xml]' % (sys.argv[0]));
    print('')
    print('options:')
    print('  --filter <test-sub-string>')
    return 1;

def main(asArgs):
    """ C styl main(). """
    # Parse arguments
    sTestFile = None;
    sBaseFile = None;
    asFilters = [];
    iArg = 1;
    while iArg < len(asArgs):
        if asArgs[iArg] == '--filter':
            iArg += 1;
            asFilters.append(asArgs[iArg]);
        elif asArgs[iArg].startswith('--help'):
            return usage();
        elif asArgs[iArg].startswith('--'):
            print('syntax error: unknown option "%s"' % (asArgs[iArg]));
            return usage();
        elif sTestFile is None:
            sTestFile = asArgs[iArg];
        elif sBaseFile is None:
            sBaseFile = asArgs[iArg];
        else:
            print('syntax error: too many file names: %s' % (asArgs[iArg]))
            return usage();
        iArg += 1;

    # Down to business
    oTestTree = reader.parseTestResult(sTestFile);
    if oTestTree is None:
        return 1;
    oTestTree = oTestTree.filterTests(asFilters)

    if sBaseFile is not None:
        oBaseline = reader.parseTestResult(sBaseFile);
        if oBaseline is None:
            return 1;
        oTestTree = diff.baselineDiff(oTestTree, oBaseline);
        if oTestTree is None:
            return 1;

    reporting.produceTextReport(oTestTree);
    return 0;

if __name__ == '__main__':
    sys.exit(main(sys.argv));

