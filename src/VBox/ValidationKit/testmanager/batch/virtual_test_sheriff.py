#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$
# pylint: disable=C0301

"""
Virtual Test Sheriff.

Duties:
    - Try to a assign failure reasons to recently failed tests.
    - Reboot or disable bad test boxes.

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

# Add Test Manager's modules path
g_ksTestManagerDir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))));
sys.path.append(g_ksTestManagerDir);

# Test Manager imports
from testmanager.core.db                    import TMDatabaseConnection;
from testmanager.core.build                 import BuildDataEx;
from testmanager.core.failurereason         import FailureReasonLogic;
from testmanager.core.testbox               import TestBoxLogic, TestBoxData;
from testmanager.core.testcase              import TestCaseDataEx;
from testmanager.core.testgroup             import TestGroupData;
from testmanager.core.testset               import TestSetLogic, TestSetData;
from testmanager.core.testresults           import TestResultLogic;
from testmanager.core.testresultfailures    import TestResultFailureLogic, TestResultFailureData;
from testmanager.core.useraccount           import UserAccountLogic;


class VirtualTestSheriffCaseFile(object):
    """
    A failure investigation case file.

    """


    ## Max log file we'll read into memory. (256 MB)
    kcbMaxLogRead = 0x10000000;


    def __init__(self, oSheriff, oTestSet, oTree, oBuild, oTestBox, oTestGroup, oTestCase):
        self.oSheriff       = oSheriff;
        self.oTestSet       = oTestSet;     # TestSetData
        self.oTree          = oTree;        # TestResultDataEx
        self.oBuild         = oBuild;       # BuildDataEx
        self.oTestBox       = oTestBox;     # TestBoxData
        self.oTestGroup     = oTestGroup;   # TestGroupData
        self.oTestCase      = oTestCase;    # TestCaseDataEx
        self.sMainLog       = '';           # The main log file.  Empty string if not accessible.

        # Generate a case file name.
        self.sName          = '#%u: %s' % (self.oTestSet.idTestSet, self.oTestCase.sName,)
        self.sLongName      = '#%u: "%s" on "%s" running %s %s (%s), "%s" by %s, using %s %s %s r%u'  \
                            % ( self.oTestSet.idTestSet,
                                self.oTestCase.sName,
                                self.oTestBox.sName,
                                self.oTestBox.sOs,
                                self.oTestBox.sOsVersion,
                                self.oTestBox.sCpuArch,
                                self.oTestBox.sCpuName,
                                self.oTestBox.sCpuVendor,
                                self.oBuild.oCat.sProduct,
                                self.oBuild.oCat.sBranch,
                                self.oBuild.oCat.sType,
                                self.oBuild.iRevision, );

        # Investigation notes.
        self.tReason            = None;     # None or one of the ktReason_XXX constants.
        self.dReasonForResultId = {};       # Reason assignments indexed by idTestResult.

    #
    # Reason.
    #

    def noteReason(self, tReason):
        """ Notes down a possible reason. """
        self.oSheriff.dprint('noteReason: %s -> %s' % (self.tReason, tReason,));
        self.tReason = tReason;

    def noteReasonForId(self, tReason, idTestResult):
        """ Notes down a possible reason for a specific test result. """
        self.oSheriff.dprint('noteReasonForId: %u: %s -> %s'
                             % (idTestResult, self.dReasonForResultId.get(idTestResult, None), tReason,));
        self.dReasonForResultId[idTestResult] = tReason;


    #
    # Test classification.
    #

    def isVBoxTest(self):
        """ Test classification: VirtualBox (using the build) """
        return self.oBuild.oCat.sProduct.lower() in [ 'virtualbox', 'vbox' ];

    def isVBoxUnitTest(self):
        """ Test case classification: The unit test doing all our testcase/*.cpp stuff. """
        return self.isVBoxTest() \
           and self.oTestCase.sName.lower() == 'unit tests';

    def isVBoxInstallTest(self):
        """ Test case classification: VirtualBox Guest installation test. """
        return self.isVBoxTest() \
           and self.oTestCase.sName.lower().startswith('install:');

    def isVBoxSmokeTest(self):
        """ Test case classification: Smoke test. """
        return self.isVBoxTest() \
           and self.oTestCase.sName.lower().startswith('smoketest');


    #
    # Utility methods.
    #

    def getMainLog(self):
        """
        Tries to reads the main log file since this will be the first source of information.
        """
        if len(self.sMainLog) > 0:
            return self.sMainLog;
        (oFile, oSizeOrError, _) = self.oTestSet.openFile('main.log', 'rb');
        if oFile is not None:
            try:
                self.sMainLog = oFile.read(min(self.kcbMaxLogRead, oSizeOrError)).decode('utf-8', 'replace');
            except Exception as oXcpt:
                self.oSheriff.vprint('Error reading main log file: %s' % (oXcpt,))
                self.sMainLog = '';
        else:
            self.oSheriff.vprint('Error opening main log file: %s' % (oSizeOrError,));
        return self.sMainLog;

    def isSingleTestFailure(self):
        """
        Figure out if this is a single test failing or if it's one of the
        more complicated ones.
        """
        if self.oTree.cErrors == 1:
            return True;
        if self.oTree.deepCountErrorContributers() <= 1:
            return True;
        return False;



class VirtualTestSheriff(object): # pylint: disable=R0903
    """
    Add build info into Test Manager database.
    """

    ## The user account for the virtual sheriff.
    ksLoginName = 'vsheriff';

    def __init__(self):
        """
        Parse command line.
        """
        self.oDb                     = None;
        self.tsNow                   = None;
        self.oTestResultLogic        = None;
        self.oTestSetLogic           = None;
        self.oFailureReasonLogic     = None;    # FailureReasonLogic;
        self.oTestResultFailureLogic = None;    # TestResultFailureLogic
        self.oLogin                  = None;
        self.uidSelf                 = -1;
        self.oLogFile                = None;

        oParser = OptionParser();
        oParser.add_option('--start-hours-ago', dest = 'cStartHoursAgo', metavar = '<hours>', default = 0, type = 'int',
                           help = 'When to start specified as hours relative to current time.  Defauls is right now.', );
        oParser.add_option('--hours-period', dest = 'cHoursBack', metavar = '<period-in-hours>', default = 2, type = 'int',
                           help = 'Work period specified in hours.  Defauls is 2 hours.');
        oParser.add_option('--real-run-back', dest = 'fRealRun', action = 'store_true', default = False,
                           help = 'Whether to commit the findings to the database. Default is a dry run.');
        oParser.add_option('-q', '--quiet', dest = 'fQuiet', action = 'store_true', default = False,
                           help = 'Quiet execution');
        oParser.add_option('-l', '--log', dest = 'sLogFile', metavar = '<logfile>', default = None,
                           help = 'Where to log messages.');
        oParser.add_option('--debug', dest = 'fDebug', action = 'store_true', default = False,
                           help = 'Enables debug mode.');

        (self.oConfig, _) = oParser.parse_args();

        if self.oConfig.sLogFile is not None and len(self.oConfig.sLogFile) > 0:
            self.oLogFile = open(self.oConfig.sLogFile, "a");
            self.oLogFile.write('VirtualTestSheriff: $Revision$ \n');


    def eprint(self, sText):
        """
        Prints error messages.
        Returns 1 (for exit code usage.)
        """
        print 'error: %s' % (sText,);
        if self.oLogFile is not None:
            self.oLogFile.write('error: %s\n' % (sText,));
        return 1;

    def dprint(self, sText):
        """
        Prints debug info.
        """
        if self.oConfig.fDebug:
            if not self.oConfig.fQuiet:
                print 'debug: %s' % (sText, );
            if self.oLogFile is not None:
                self.oLogFile.write('debug: %s\n' % (sText,));
        return 0;

    def vprint(self, sText):
        """
        Prints verbose info.
        """
        if not self.oConfig.fQuiet:
            print 'info: %s' % (sText,);
        if self.oLogFile is not None:
            self.oLogFile.write('info: %s\n' % (sText,));
        return 0;


    def selfCheck(self):
        """ Does some self checks, looking up things we expect to be in the database and such. """
        rcExit = 0;
        for sAttr in dir(self.__class__):
            if sAttr.startswith('ktReason_'):
                tReason = getattr(self.__class__, sAttr);
                oFailureReason = self.oFailureReasonLogic.cachedLookupByNameAndCategory(tReason[1], tReason[0]);
                if oFailureReason is None:
                    rcExit = self.eprint('Failured to find failure reason "%s" in category "%s" in the database!'
                                         % (tReason[1], tReason[0],));

        # Check the user account as well.
        if self.oLogin is None:
            oLogin = UserAccountLogic(self.oDb).tryFetchAccountByLoginName(VirtualTestSheriff.ksLoginName);
            if oLogin is None:
                rcExit = self.eprint('Cannot find my user account "%s"!' % (VirtualTestSheriff.ksLoginName,));
        return rcExit;



    def badTestBoxManagement(self):
        """
        Looks for bad test boxes and first tries once to reboot them then disables them.
        """
        rcExit = 0;

        #
        # We skip this entirely if we're running in the past and not in harmless debug mode.
        #
        if    self.oConfig.cStartHoursAgo != 0 \
          and (not self.oConfig.fDebug or self.oConfig.fRealRun):
            return rcExit;
        tsNow      = self.tsNow              if self.oConfig.fDebug else None;
        cHoursBack = self.oConfig.cHoursBack if self.oConfig.fDebug else 2;
        oTestBoxLogic = TestBoxLogic(self.oDb);

        #
        # Get list of bad test boxes for given period and check them out individually.
        #
        aidBadTestBoxes = self.oTestSetLogic.fetchBadTestBoxIds(cHoursBack = cHoursBack, tsNow = tsNow);
        for idTestBox in aidBadTestBoxes:
            # Skip if the testbox is already disabled or has a pending reboot command.
            try:
                oTestBox = TestBoxData().initFromDbWithId(self.oDb, idTestBox);
            except Exception as oXcpt:
                rcExit = self.eprint('Failed to get data for test box #%u in badTestBoxManagement: %s' % (idTestBox, oXcpt,));
                continue;
            if not oTestBox.fEnabled:
                self.dprint('badTestBoxManagement: Skipping test box #%u (%s) as it has been disabled already.'
                            % ( idTestBox, oTestBox.sName, ));
                continue;
            if oTestBox.enmPendingCmd != TestBoxData.ksTestBoxCmd_None:
                self.dprint('badTestBoxManagement: Skipping test box #%u (%s) as it has a command pending: %s'
                            % ( idTestBox, oTestBox.sName, oTestBox.enmPendingCmd));
                continue;

            # Get the most recent testsets for this box (descending on tsDone) and see how bad it is.
            aoSets  = self.oTestSetLogic.fetchSetsForTestBox(idTestBox, cHoursBack = cHoursBack, tsNow = tsNow);
            cOkay      = 0;
            cBad       = 0;
            iFirstOkay = len(aoSets);
            for iSet, oSet in enumerate(aoSets):
                if oSet.enmStatus == TestSetData.ksTestStatus_BadTestBox:
                    cBad += 1;
                else:
                    ## @todo maybe check the elapsed time here, it could still be a bad run.
                    cOkay += 1;
                    if iFirstOkay > iSet:
                        iFirstOkay = iSet;
                if iSet > 10:
                    break;

            # We react if there are two or more bad-testbox statuses at the head of the
            # history and at least three in the last 10 results.
            if iFirstOkay >= 2 and cBad > 2:
                if oTestBoxLogic.hasTestBoxRecentlyBeenRebooted(idTestBox, cHoursBack = cHoursBack, tsNow = tsNow):
                    self.vprint('Disabling testbox #%u (%s) - iFirstOkay=%u cBad=%u cOkay=%u'
                                % ( idTestBox, oTestBox.sName, iFirstOkay, cBad, cOkay));
                    if self.oConfig.fRealRun is True:
                        try:
                            oTestBoxLogic.disableTestBox(idTestBox, self.uidSelf, fCommit = True,
                                                         sComment = 'Automatically disabled (iFirstOkay=%u cBad=%u cOkay=%u)'
                                                                  % (iFirstOkay, cBad, cOkay),);
                        except Exception as oXcpt:
                            rcExit = self.eprint('Error disabling testbox #%u (%u): %s\n' % (idTestBox, oTestBox.sName, oXcpt,));
                else:
                    self.vprint('Rebooting testbox #%u (%s) - iFirstOkay=%u cBad=%u cOkay=%u'
                                % ( idTestBox, oTestBox.sName, iFirstOkay, cBad, cOkay));
                    if self.oConfig.fRealRun is True:
                        try:
                            oTestBoxLogic.rebootTestBox(idTestBox, self.uidSelf, fCommit = True,
                                                        sComment = 'Automatically rebooted (iFirstOkay=%u cBad=%u cOkay=%u)'
                                                                 % (iFirstOkay, cBad, cOkay),);
                        except Exception as oXcpt:
                            rcExit = self.eprint('Error rebooting testbox #%u (%u): %s\n' % (idTestBox, oTestBox.sName, oXcpt,));
            else:
                self.dprint('badTestBoxManagement: #%u (%s) looks ok:  iFirstOkay=%u cBad=%u cOkay=%u'
                            % ( idTestBox, oTestBox.sName, iFirstOkay, cBad, cOkay));
        return rcExit;


    ## @name Failure reasons we know.
    ## @{
    ktReason_Guru_Generic                              = ( 'Guru Meditations', 'Generic Guru Meditation' );
    ktReason_Guru_VERR_IEM_INSTR_NOT_IMPLEMENTED       = ( 'Guru Meditations', 'VERR_IEM_INSTR_NOT_IMPLEMENTED' );
    ktReason_Guru_VERR_IEM_ASPECT_NOT_IMPLEMENTED      = ( 'Guru Meditations', 'VERR_IEM_ASPECT_NOT_IMPLEMENTED' );
    ktReason_Guru_VINF_EM_TRIPLE_FAULT                 = ( 'Guru Meditations', 'VINF_EM_TRIPLE_FAULT' );
    ## @}

    def caseClosed(self, oCaseFile):
        """
        Reports the findings in the case and closes it.
        """
        #
        # Log it and create a dReasonForReasultId we can use below.
        #
        if len(oCaseFile.dReasonForResultId):
            self.vprint('Closing %s with following reasons: %s' % (oCaseFile.sName, oCaseFile.dReasonForResultId,));
            dReasonForReasultId = oCaseFile.dReasonForResultId;
        elif oCaseFile.tReason is not None:
            self.vprint('Closing %s with following reason: %s'  % (oCaseFile.sName, oCaseFile.tReason,));
            dReasonForReasultId = { oCaseFile.oTestSet.idTestResult: oCaseFile.tReason, };
        else:
            self.vprint('Closing %s without a reason ... weird!' % (oCaseFile.sName,));
            return False;

        #
        # Add the test failure reason record(s).
        #
        for idTestResult, tReason in dReasonForReasultId.items():
            oFailureReason = self.oFailureReasonLogic.cachedLookupByNameAndCategory(tReason[1], tReason[0]);
            if oFailureReason is not None:
                oAdd = TestResultFailureData();
                oAdd.initFromValues(idTestResult     = idTestResult,
                                    idFailureReason  = oFailureReason.idFailureReason,
                                    uidAuthor        = self.uidSelf,
                                    idTestSet        = oCaseFile.oTestSet.idTestSet,
                                    sComment         = 'Set by $Revision$',); # Handy for reverting later.
                if self.oConfig.fRealRun:
                    try:
                        self.oTestResultFailureLogic.addEntry(oAdd, self.uidSelf, fCommit = True);
                    except Exception as oXcpt:
                        self.eprint('caseClosed: Exception "%s" while adding reason %s for %s'
                                    % (oXcpt, oAdd, oCaseFile.sLongName,));
            else:
                self.eprint('caseClosed: Cannot locate failure reason: %s / %s' % ( tReason[0], tReason[1],));
        return True;


    def investigateBadTestBox(self, oCaseFile):
        """
        Checks out bad-testbox statuses.
        """
        _ = oCaseFile;
        return False;


    def investigateVBoxUnitTest(self, oCaseFile):
        """
        Checks out a VBox unittest problem.
        """
        _ = oCaseFile;

        #
        # As a first step we'll just fish out what failed here and report
        # the unit test case name as the "reason".  This can mostly be done
        # using the TestResultDataEx bits, however in case it timed out and
        # got killed we have to fish the test timing out from the logs.
        #

        #
        # Report lone failures on the testcase, multiple failures must be
        # reported directly on the failing test (will fix the test result
        # listing to collect all of them).
        #
        return False;


    ## Thing we search a main or VM log for to figure out why something went bust.
    katSimpleMainAndVmLogReasons = [
        # ( Whether to stop on hit, needle, reason tuple ),
        ( False, 'GuruMeditation',                                  ktReason_Guru_Generic ),
        ( True,  'VERR_IEM_INSTR_NOT_IMPLEMENTED',                  ktReason_Guru_VERR_IEM_INSTR_NOT_IMPLEMENTED ),
        ( True,  'VERR_IEM_ASPECT_NOT_IMPLEMENTED',                 ktReason_Guru_VERR_IEM_ASPECT_NOT_IMPLEMENTED ),
        ( True,  'VINF_EM_TRIPLE_FAULT',                            ktReason_Guru_VINF_EM_TRIPLE_FAULT ),
    ];

    def investigateVBoxVMTest(self, oCaseFile, fSingleVM):
        """
        Checks out a VBox VM test.

        This is generic investigation of a test running one or more VMs, like
        for example a smoke test or a guest installation test.

        The fSingleVM parameter is a hint, which probably won't come in useful.
        """
        _ = fSingleVM;

        #
        # Do some quick searches thru the main log to see if there is anything
        # immediately incriminating evidence there.
        #
        sMainLog = oCaseFile.getMainLog();
        for fStopOnHit, sNeedle, tReason in self.katSimpleMainAndVmLogReasons:
            if sMainLog.find(sNeedle) > 0:
                oCaseFile.noteReason(tReason);
                if fStopOnHit:
                    if oCaseFile.isSingleTestFailure():
                        return self.caseClosed(oCaseFile);
                    break;

        return False;


    def reasoningFailures(self):
        """
        Guess the reason for failures.
        """
        #
        # Get a list of failed test sets without any assigned failure reason.
        #
        aoTestSets = self.oTestSetLogic.fetchFailedSetsWithoutReason(cHoursBack = self.oConfig.cHoursBack, tsNow = self.tsNow);
        for oTestSet in aoTestSets:
            self.dprint('');
            self.dprint('reasoningFailures: Checking out test set #%u, status %s'  % ( oTestSet.idTestSet, oTestSet.enmStatus,))

            #
            # Open a case file and assign it to the right investigator.
            #
            (oTree, _ ) = self.oTestResultLogic.fetchResultTree(oTestSet.idTestSet);
            oBuild      = BuildDataEx().initFromDbWithId(       self.oDb, oTestSet.idBuild,       oTestSet.tsCreated);
            oTestBox    = TestBoxData().initFromDbWithGenId(    self.oDb, oTestSet.idGenTestBox);
            oTestGroup  = TestGroupData().initFromDbWithId(     self.oDb, oTestSet.idTestGroup,   oTestSet.tsCreated);
            oTestCase   = TestCaseDataEx().initFromDbWithGenId( self.oDb, oTestSet.idGenTestCase, oTestSet.tsConfig);

            oCaseFile = VirtualTestSheriffCaseFile(self, oTestSet, oTree, oBuild, oTestBox, oTestGroup, oTestCase);

            if oTestSet.enmStatus == TestSetData.ksTestStatus_BadTestBox:
                self.dprint('investigateBadTestBox is taking over %s.' % (oCaseFile.sLongName,));
                self.investigateBadTestBox(oCaseFile);
            elif oCaseFile.isVBoxUnitTest():
                self.dprint('investigateVBoxUnitTest is taking over %s.' % (oCaseFile.sLongName,));
                self.investigateVBoxUnitTest(oCaseFile);
            elif oCaseFile.isVBoxInstallTest():
                self.dprint('investigateVBoxVMTest is taking over %s.' % (oCaseFile.sLongName,));
                self.investigateVBoxVMTest(oCaseFile, fSingleVM = True);
            elif oCaseFile.isVBoxSmokeTest():
                self.dprint('investigateVBoxVMTest is taking over %s.' % (oCaseFile.sLongName,));
                self.investigateVBoxVMTest(oCaseFile, fSingleVM = False);
            else:
                self.vprint('reasoningFailures: Unable to classify test set: %s' % (oCaseFile.sLongName,));
        return 0;


    def main(self):
        """
        The 'main' function.
        Return exit code (0, 1, etc).
        """
        # Database stuff.
        self.oDb                     = TMDatabaseConnection()
        self.oTestResultLogic        = TestResultLogic(self.oDb);
        self.oTestSetLogic           = TestSetLogic(self.oDb);
        self.oFailureReasonLogic     = FailureReasonLogic(self.oDb);
        self.oTestResultFailureLogic = TestResultFailureLogic(self.oDb);

        # Get a fix on our 'now' before we do anything..
        self.oDb.execute('SELECT CURRENT_TIMESTAMP - interval \'%s hours\'', (self.oConfig.cStartHoursAgo,));
        self.tsNow = self.oDb.fetchOne();

        # If we're suppost to commit anything we need to get our user ID.
        rcExit = 0;
        if self.oConfig.fRealRun:
            self.oLogin = UserAccountLogic(self.oDb).tryFetchAccountByLoginName(VirtualTestSheriff.ksLoginName);
            if self.oLogin is None:
                rcExit = self.eprint('Cannot find my user account "%s"!' % (VirtualTestSheriff.ksLoginName,));
            else:
                self.uidSelf = self.oLogin.uid;

        # Do the stuff.
        if rcExit == 0:
            rcExit  = self.selfCheck();
        if rcExit == 0:
            rcExit  = self.badTestBoxManagement();
            rcExit2 = self.reasoningFailures();
            if rcExit == 0:
                rcExit = rcExit2;

        # Cleanup.
        self.oFailureReasonLogic     = None;
        self.oTestResultFailureLogic = None;
        self.oTestSetLogic           = None;
        self.oTestResultLogic        = None;
        self.oDb.close();
        self.oDb = None;
        if self.oLogFile is not None:
            self.oLogFile.close();
            self.oLogFile = None;
        return rcExit;

if __name__ == '__main__':
    sys.exit(VirtualTestSheriff().main());

