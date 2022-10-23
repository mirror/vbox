#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$

"""
Analyzer CLI.
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
from analysis import reader    ## @todo fix testanalysis/__init__.py.
from analysis import reporting
#from analysis import diff


def usage():
    """ Display usage """
    print('usage: %s [options] <test1.xml/txt> <test2.xml/txt> [..] [-- <baseline1.file> [baseline2.file] [..]]'
          % (sys.argv[0]));
    print('')
    print('options:')
    print('  --filter <test-sub-string>')
    return 1;

class ResultCollection(object):
    """
    One or more test runs that should be merged before comparison.
    """

    def __init__(self, sName):
        self.sName       = sName;
        self.aoTestTrees = []   # type: [Test]
        self.asTestFiles = []   # type: [str] - runs parallel to aoTestTrees
        self.oDistilled  = None # type: Test

    def append(self, sFilename):
        """
        Loads sFilename and appends the result.
        Returns True on success, False on failure.
        """
        oTestTree = reader.parseTestResult(sFilename);
        if oTestTree:
            self.aoTestTrees.append(oTestTree);
            self.asTestFiles.append(sFilename);
            return True;
        return False;

    def isEmpty(self):
        """ Checks if the result is empty. """
        return len(self.aoTestTrees) == 0;

    def filterTests(self, asFilters):
        """
        Filters all the test trees using asFilters.
        """
        for oTestTree in self.aoTestTrees:
            oTestTree.filterTests(asFilters);
        return self;

    def distill(self, sMethod, fDropLoners = False):
        """
        Distills the set of test results into a single one by the given method.

        Valid sMethod values:
            - 'best': Pick the best result for each test and value among all the test runs.
            - 'avg':  Calculate the average value among all the test runs.

        When fDropLoners is True, tests and values that only appear in a single test run
        will be discarded.  When False (the default), the lone result will be used.
        """
        assert sMethod in ['best', 'avg'];
        assert not self.oDistilled;

        # If empty, nothing to do.
        if self.isEmpty():
            return None;

        # If there is only a single tree, make a deep copy of it.
        if len(self.aoTestTrees) == 1:
            oDistilled = self.aoTestTrees[0].clone();
        else:

            # Since we don't know if the test runs are all from the same test, we create
            # dummy root tests for each run and use these are the start for the distillation.
            aoDummyInputTests = [];
            for oRun in self.aoTestTrees:
                oDummy = reader.Test();
                oDummy.aoChildren = [oRun,];
                aoDummyInputTests.append(oDummy);

            # Similarly, we end up with a "dummy" root test for the result.
            oDistilled = reader.Test();
            oDistilled.distill(aoDummyInputTests, sMethod, fDropLoners);

            # We can drop this if there is only a single child, i.e. if all runs are for
            # the same test.
            if len(oDistilled.aoChildren) == 1:
                oDistilled = oDistilled.aoChildren[0];

        self.oDistilled = oDistilled;
        return oDistilled;





def main(asArgs):
    """ C style main(). """
    #
    # Parse arguments
    #
    oCurCollection      = ResultCollection('#0');
    aoCollections       = [ oCurCollection, ];
    iBaseline           = 0;
    sDistillationMethod = 'best';
    fBrief              = True;
    cPctPrecision       = 2;
    asFilters           = [];

    iArg = 1;
    while iArg < len(asArgs):
        #print("dbg: iArg=%s '%s'" % (iArg, asArgs[iArg],));
        if asArgs[iArg].startswith('--help'):
            return usage();
        if asArgs[iArg] == '--filter':
            iArg += 1;
            asFilters.append(asArgs[iArg]);
        elif asArgs[iArg] == '--best':
            sDistillationMethod = 'best';
        elif asArgs[iArg] in ('--avg', '--average'):
            sDistillationMethod = 'avg';
        elif asArgs[iArg] == '--brief':
            fBrief = True;
        elif asArgs[iArg] == '--verbose':
            fBrief = False;
        elif asArgs[iArg] in ('--pct', '--pct-precision'):
            iArg += 1;
            cPctPrecision = int(asArgs[iArg]);
        elif asArgs[iArg] in ('--base', '--baseline'):
            iArg += 1;
            iBaseline = int(asArgs[iArg]);
        # '--' starts a new collection.  If current one is empty, drop it.
        elif asArgs[iArg] == '--':
            print("dbg: new collection");
            #if oCurCollection.isEmpty():
            #    del aoCollections[-1];
            oCurCollection = ResultCollection("#%s" % (len(aoCollections),));
            aoCollections.append(oCurCollection);
        # Name the current result collection.
        elif asArgs[iArg] == '--name':
            iArg += 1;
            oCurCollection.sName = asArgs[iArg];
        # Read in a file and add it to the current data set.
        else:
            if not oCurCollection.append(asArgs[iArg]):
                return 1;
        iArg += 1;

    #
    # Post argument parsing processing.
    #

    # Drop the last collection if empty.
    if oCurCollection.isEmpty():
        del aoCollections[-1];
    if not aoCollections:
        print("error: No input files given!");
        return 1;

    # Check the baseline value and mark the column as such.
    if iBaseline < 0 or iBaseline > len(aoCollections):
        print("error: specified baseline is out of range: %s, valid range 0 <= baseline < %s"
              % (iBaseline, len(aoCollections),));
        return 1;
    aoCollections[iBaseline].sName += ' (baseline)';

    #
    # Apply filtering before distilling each collection into a single result
    # tree and comparing them to the first one.
    #
    if asFilters:
        for oCollection in aoCollections:
            oCollection.filterTests(asFilters);

    for oCollection in aoCollections:
        oCollection.distill(sDistillationMethod);

    #
    # Produce the report.
    #
    oTable = reporting.RunTable(iBaseline, fBrief, cPctPrecision);
    oTable.populateFromRuns([oCollection.oDistilled for oCollection in aoCollections],
                            [oCollection.sName      for oCollection in aoCollections]);
    print('\n'.join(oTable.formatAsText()));
    return 0;

if __name__ == '__main__':
    sys.exit(main(sys.argv));

