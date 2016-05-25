#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$
# pylint: disable=C0301

"""
Utility for dumping the last X days of data.
"""

__copyright__ = \
"""
Copyright (C) 2012-2016 Oracle Corporation

This file is part of VirtualBox Open Source Edition (OSE), as
available from http://www.virtualbox.org. This file is free software;
you can redistribute it and/or modify it under the terms of the GNU
General Public License (GPL) as published by the Free Software
Foundation, in version 2 as it comes in the "COPYING" file of the
VirtualBox OSE distribution. VirtualBox OSE is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

The contents of this file may alternatively be used under the terms
of the Common Development and Distribution License Version 1.0
(CDDL) only, as it comes in the "COPYING.CDDL" file of the
VirtualBox OSE distribution, in which case the provisions of the
CDDL are applicable instead of those of the GPL.

You may elect to license modified versions of this file under the
terms and conditions of either the GPL or the CDDL or both.
"""
__version__ = "$Revision$"

# Standard python imports
import sys;
import os;
from optparse import OptionParser;
import xml.etree.ElementTree as ET;

# Add Test Manager's modules path
g_ksTestManagerDir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))));
sys.path.append(g_ksTestManagerDir);

# Test Manager imports
from testmanager.core.db            import TMDatabaseConnection;
from common                         import utils;


class PartialDbDump(object): # pylint: disable=R0903
    """
    Dumps or loads the last X days of database data.

    This is a useful tool when hacking on the test manager locally.  You can get
    a small sample from the last few days from the production test manager server
    without spending hours dumping, downloading, and loading the whole database
    (because it is gigantic).

    """

    def __init__(self):
        """
        Parse command line.
        """

        oParser = OptionParser()
        oParser.add_option('-q', '--quiet', dest = 'fQuiet', action = 'store_true',
                           help = 'Quiet execution');
        oParser.add_option('-b', '--base-filename', dest = 'sBaseFilename', metavar = '<base-filename>',
                           default = '/tmp/testmanager-partial-dump',
                           help = 'Absolute base filename (dash + table name + sql appended).');
        oParser.add_option('--days-to-dump', dest = 'cDays', metavar = '<days>', type = 'int', default = 14,
                           help = 'How many days to dump (counting backward from current date).');
        oParser.add_option('--load-dump-into-database', dest = 'fLoadDumpIntoDatabase', action = 'store_true',
                           default = False, help = 'For loading instead of dumping.');

        (self.oConfig, _) = oParser.parse_args();


    ##
    # Tables dumped in full because they're either needed in full or they normally
    # aren't large enough to bother reducing.
    kasTablesToDumpInFull = [
        'Users',
        'BuildBlacklist',
        'BuildCategories',
        'BuildSources',
        'FailureCategories',
        'FailureReasons',
        'GlobalResources',
        'Testcases',
        'TestcaseArgs',
        'TestcaseDeps',
        'TestcaseGlobalRsrcDeps',
        'TestGroups',
        'TestGroupMembers',
        'SchedGroupMembers',            # ?
        'SchedQueues',
        'Builds',                       # ??
        'SystemLog',                    # ?
        'VcsRevisions',                 # ?
        'TestResultStrTab',             # 36K rows, never mind complicated then.
        'GlobalResourceStatuses',       # ?
        'TestBoxStatuses',              # ?
    ];

    ##
    # Tables where we only dump partial info (the TestResult* tables are rather
    # gigantic).
    kasTablesToPartiallyDump = [
        'TestBoxes',                    # 2016-05-25: ca.  641 MB
        'TestSets',                     # 2016-05-25: ca.  525 MB
        'TestResults',                  # 2016-05-25: ca.   13 GB
        'TestResultFiles',              # 2016-05-25: ca.   87 MB
        'TestResultMsgs',               # 2016-05-25: ca.   29 MB
        'TestResultValues',             # 2016-05-25: ca. 3728 MB
        'TestResultFailures',
    ];

    def _doDump(self, oDb):
        """ Does the dumping of the database. """

        oDb.begin();

        # Dumping full tables is simple.
        for sTable in self.kasTablesToDumpInFull:
            sFile = self.oConfig.sBaseFilename + '-' + sTable + '.pgtxt';
            print 'Dumping %s into "%s"...' % (sTable, sFile,);
            oDb.execute('COPY ' + sTable + ' TO %s WITH (FORMAT TEXT)', (sFile,));
            cRows = oDb.getRowCount();
            print '... %s rows.' % (cRows,);

        # Figure out how far back we need to go.
        oDb.execute('SELECT CURRENT_TIMESTAMP - INTERVAL \'%s days\'' % (self.oConfig.cDays,));
        tsEffective = oDb.fetchOne()[0];
        oDb.execute('SELECT CURRENT_TIMESTAMP - INTERVAL \'%s days\'' % (self.oConfig.cDays + 2,));
        tsEffectiveSafe = oDb.fetchOne()[0];
        print 'Going back to:     %s (safe: %s)' % (tsEffective, tsEffectiveSafe);

        # We dump test boxes back to the safe timestamp because the test sets may
        # use slightly dated test box references and we don't wish to have dangling
        # references when loading.
        for sTable in [ 'TestBoxes', ]:
            sFile = self.oConfig.sBaseFilename + '-' + sTable + '.pgtxt';
            print 'Dumping %s into "%s"...' % (sTable, sFile,);
            oDb.execute('COPY (SELECT * FROM ' + sTable + ' WHERE tsExpire >= %s) TO %s WITH (FORMAT TEXT)',
                        (tsEffectiveSafe, sFile,));
            cRows = oDb.getRowCount();
            print '... %s rows.' % (cRows,);

        # The test results needs to start with test sets and then dump everything
        # releated to them.  So, figure the lowest (oldest) test set ID we'll be
        # dumping first.
        oDb.execute('SELECT idTestSet FROM TestSets WHERE tsCreated >= %s', (tsEffective, ));
        idFirstTestSet = 0;
        if oDb.getRowCount() > 0:
            idFirstTestSet = oDb.fetchOne()[0];
        print 'First test set ID: %s' % (idFirstTestSet,);

        # Tables with idTestSet member.
        for sTable in [ 'TestSets', 'TestResults', 'TestResultValues' ]:
            sFile = self.oConfig.sBaseFilename + '-' + sTable + '.pgtxt';
            print 'Dumping %s into "%s"...' % (sTable, sFile,);
            oDb.execute('COPY (SELECT * FROM ' + sTable + ' WHERE idTestSet >= %s) TO %s WITH (FORMAT TEXT)',
                        (idFirstTestSet, sFile,));
            cRows = oDb.getRowCount();
            print '... %s rows.' % (cRows,);

        # Tables where we have to go via TestResult.
        for sTable in [ 'TestResultFiles', 'TestResultMsgs', 'TestResultFailures' ]:
            sFile = self.oConfig.sBaseFilename + '-' + sTable + '.pgtxt';
            print 'Dumping %s into "%s"...' % (sTable, sFile,);
            oDb.execute('COPY (SELECT it.*\n'
                        '      FROM ' + sTable + ' it, TestResults tr\n'
                        '      WHERE  tr.idTestSet >= %s AND it.idTestResult = tr.idTestResult )\n'
                        '  TO %s WITH (FORMAT TEXT)',
                        (idFirstTestSet, sFile,));
            cRows = oDb.getRowCount();
            print '... %s rows.' % (cRows,);

        return 0;

    def _doLoad(self, oDb):
        """ Does the loading of the dumped data into the database. """
        return 0;

    def main(self):
        """
        Main function.
        """
        oDb = TMDatabaseConnection();

        if self.oConfig.fLoadDumpIntoDatabase is not True:
            rc = self._doDump(oDb);
        else:
            rc = self._doLoad(oDb);

        oDb.close();
        return 0;

if __name__ == '__main__':
    sys.exit(PartialDbDump().main());


