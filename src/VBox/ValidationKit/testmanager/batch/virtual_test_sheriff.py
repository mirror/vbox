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
from testmanager.core.testresults           import TestResultLogic, TestResultFileData;
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
        self.tReason                = None; # None or one of the ktReason_XXX constants.
        self.dReasonForResultId     = {};   # Reason assignments indexed by idTestResult.
        self.dCommentForResultId    = {};   # Comment assignments indexed by idTestResult.

    #
    # Reason.
    #

    def noteReason(self, tReason):
        """ Notes down a possible reason. """
        self.oSheriff.dprint('noteReason: %s -> %s' % (self.tReason, tReason,));
        self.tReason = tReason;
        return True;

    def noteReasonForId(self, tReason, idTestResult, sComment = None):
        """ Notes down a possible reason for a specific test result. """
        self.oSheriff.dprint('noteReasonForId: %u: %s -> %s%s'
                             % (idTestResult, self.dReasonForResultId.get(idTestResult, None), tReason,
                                (' (%s)' % (sComment,)) if sComment is not None else ''));
        self.dReasonForResultId[idTestResult] = tReason;
        if sComment is not None:
            self.dCommentForResultId[idTestResult] = sComment;
        return True;


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

    def getLogFile(self, oFile):
        """
        Tries to reads the given file as a utf-8 log file.
        oFile is a TestFileDataEx instance.
        Returns empty string if problems opening or reading the file.
        """
        sContent = '';
        (oFile, oSizeOrError, _) = self.oTestSet.openFile(oFile.sFile, 'rb');
        if oFile is not None:
            try:
                sContent = oFile.read(min(self.kcbMaxLogRead, oSizeOrError)).decode('utf-8', 'replace');
            except Exception as oXcpt:
                self.oSheriff.vprint('Error reading the "%s" log file: %s' % (oFile.sFile, oXcpt,))
        else:
            self.oSheriff.vprint('Error opening the "%s" log file: %s' % (oFile.sFile, oSizeOrError,));
        return sContent;


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
        self.asBsodReasons           = [];

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
    ktReason_Guru_Generic                              = ( 'Guru Meditations',  'Generic Guru Meditation' );
    ktReason_Guru_VERR_IEM_INSTR_NOT_IMPLEMENTED       = ( 'Guru Meditations',  'VERR_IEM_INSTR_NOT_IMPLEMENTED' );
    ktReason_Guru_VERR_IEM_ASPECT_NOT_IMPLEMENTED      = ( 'Guru Meditations',  'VERR_IEM_ASPECT_NOT_IMPLEMENTED' );
    ktReason_Guru_VERR_TRPM_DONT_PANIC                 = ( 'Guru Meditations',  'VERR_TRPM_DONT_PANIC' );
    ktReason_Guru_VINF_EM_TRIPLE_FAULT                 = ( 'Guru Meditations',  'VINF_EM_TRIPLE_FAULT' );
    ktReason_XPCOM_Exit_Minus_11                       = ( 'API / (XP)COM',     'exit -11' );
    ktReason_XPCOM_VBoxSVC_Hang                        = ( 'API / (XP)COM',     'VBoxSVC hang' );
    ktReason_XPCOM_VBoxSVC_Hang_Plus_Heap_Corruption   = ( 'API / (XP)COM',     'VBoxSVC hang + heap corruption' );
    ktReason_Unknown_Heap_Corruption                   = ( 'Unknown',           'Heap corruption' );
    ktReason_Unknown_Reboot_Loop                       = ( 'Unknown',           'Reboot loop' );
    ## @}

    ## BSOD category.
    ksBsodCategory    = 'BSOD';
    ## Special reason indicating that the flesh and blood sheriff has work to do.
    ksBsodAddNew      = 'Add new BSOD';

    ## Used for indica that we shouldn't report anything for this test result ID and
    ## consider promoting the previous error to test set level if it's the only one.
    ktHarmless = ( 'Probably', 'Caused by previous error' );


    def caseClosed(self, oCaseFile):
        """
        Reports the findings in the case and closes it.
        """
        #
        # Log it and create a dReasonForReasultId we can use below.
        #
        dCommentForResultId = oCaseFile.dCommentForResultId;
        if len(oCaseFile.dReasonForResultId) > 0:
            # Must weed out ktHarmless.
            dReasonForResultId = {};
            for idKey, tReason in oCaseFile.dReasonForResultId.items():
                if tReason is not self.ktHarmless:
                    dReasonForResultId[idKey] = tReason;
            if len(dReasonForResultId) == 0:
                self.vprint('TODO: Closing %s without a real reason, only %s.' % (oCaseFile.sName, oCaseFile.dReasonForResultId));
                return False;

            # Try promote to single reason.
            if len(dReasonForResultId) > 1:
                atValues = dReasonForResultId.values();
                if len(atValues) == atValues.count(atValues[0]):
                    self.dprint('Merged %d reasons to a single one: %s' % (len(atValues), atValues[0]));
                    dReasonForResultId = { oCaseFile.oTestSet.idTestResult: atValues[0], };
                    if len(dCommentForResultId) > 0:
                        dCommentForResultId = { oCaseFile.oTestSet.idTestResult: dCommentForResultId.values()[0], };
        elif oCaseFile.tReason is not None:
            dReasonForResultId = { oCaseFile.oTestSet.idTestResult: oCaseFile.tReason, };
        else:
            self.vprint('Closing %s without a reason - this should not happen!' % (oCaseFile.sName,));
            return False;

        self.vprint('Closing %s with following reason%s: %s'
                    % ( oCaseFile.sName, 's' if dReasonForResultId > 0 else '', dReasonForResultId, ));

        #
        # Add the test failure reason record(s).
        #
        for idTestResult, tReason in dReasonForResultId.items():
            oFailureReason = self.oFailureReasonLogic.cachedLookupByNameAndCategory(tReason[1], tReason[0]);
            if oFailureReason is not None:
                sComment = 'Set by $Revision$' # Handy for reverting later.
                if idTestResult in dCommentForResultId:
                    sComment += ': ' + dCommentForResultId[idTestResult];

                oAdd = TestResultFailureData();
                oAdd.initFromValues(idTestResult    = idTestResult,
                                    idFailureReason = oFailureReason.idFailureReason,
                                    uidAuthor       = self.uidSelf,
                                    idTestSet       = oCaseFile.oTestSet.idTestSet,
                                    sComment        = sComment,);
                if self.oConfig.fRealRun:
                    try:
                        self.oTestResultFailureLogic.addEntry(oAdd, self.uidSelf, fCommit = True);
                    except Exception as oXcpt:
                        self.eprint('caseClosed: Exception "%s" while adding reason %s for %s'
                                    % (oXcpt, oAdd, oCaseFile.sLongName,));
            else:
                self.eprint('caseClosed: Cannot locate failure reason: %s / %s' % ( tReason[0], tReason[1],));
        return True;

    #
    # Tools for assiting log parsing.
    #

    @staticmethod
    def matchFollowedByLines(sStr, off, asFollowingLines):
        """ Worker for isThisFollowedByTheseLines. """

        # Advance off to the end of the line.
        off = sStr.find('\n', off);
        if off < 0:
            return False;
        off += 1;

        # Match each string with the subsequent lines.
        for iLine, sLine in enumerate(asFollowingLines):
            offEnd = sStr.find('\n', off);
            if offEnd < 0:
                return  iLine + 1 == len(asFollowingLines) and sStr.find(sLine, off) < 0;
            if len(sLine) > 0 and sStr.find(sLine, off, offEnd) < 0:
                return False;

            # next line.
            off = offEnd + 1;

        return True;

    @staticmethod
    def isThisFollowedByTheseLines(sStr, sFirst, asFollowingLines):
        """
        Looks for a line contining sFirst which is then followed by lines
        with the strings in asFollowingLines.  (No newline chars anywhere!)
        Returns True / False.
        """
        off = sStr.find(sFirst, 0);
        while off >= 0:
            if VirtualTestSheriff.matchFollowedByLines(sStr, off, asFollowingLines):
                return True;
            off = sStr.find(sFirst, off + 1);
        return False;

    @staticmethod
    def findAndReturnResetOfLine(sHaystack, sNeedle):
        """
        Looks for sNeedle in sHaystack.
        Returns The text following the needle up to the end of the line.
        Returns None if not found.
        """
        off = sHaystack.find(sNeedle);
        if off < 0:
            return None;
        off += len(sNeedle)
        offEol = sHaystack.find('\n', off);
        if offEol < 0:
            offEol = len(sHaystack);
        return sHaystack[off:offEol]

    @staticmethod
    def findInAnyAndReturnResetOfLine(asHaystacks, sNeedle):
        """
        Looks for sNeedle in zeroe or more haystacks (asHaystack).
        Returns The text following the first needed found up to the end of the line.
        Returns None if not found.
        """
        for sHaystack in asHaystacks:
            sRet = VirtualTestSheriff.findAndReturnResetOfLine(sHaystack, sNeedle);
            if sRet is not None:
                return sRet;
        return None;


    #
    # The investigative units.
    #

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
        ( True,  'VERR_TRPM_DONT_PANIC',                            ktReason_Guru_VERR_TRPM_DONT_PANIC ),
        ( True,  'VINF_EM_TRIPLE_FAULT',                            ktReason_Guru_VINF_EM_TRIPLE_FAULT ),
    ];

    def investigateVMResult(self, oCaseFile, oFailedResult, sResultLog):
        """
        Investigates a failed VM run.
        """

        def investigateLogSet():
            """
            Investigates the current set of VM related logs.
            """
            self.dprint('investigateLogSet: %u chars result log, %u chars VM log, %u chars kernel log'
                        % ( len(sResultLog) if sResultLog is not None else 0,
                            len(sVMLog)     if sVMLog is not None else 0,
                            len(sKrnlLog)   if sKrnlLog is not None else 0), );
            #self.dprint('main.log<<<\n%s\n<<<\n' % (sResultLog,));
            #self.dprint('vbox.log<<<\n%s\n<<<\n' % (sVMLog,));
            #self.dprint('krnl.log<<<\n%s\n<<<\n' % (sKrnlLog,));

            # TODO: more

            #
            # Look for BSODs. Some stupid stupid inconsistencies in reason and log messages here, so don't try prettify this.
            #
            sDetails = self.findInAnyAndReturnResetOfLine([ sVMLog, sResultLog ],
                                                          'GIM: HyperV: Guest indicates a fatal condition! P0=');
            if sDetails is not None:
                # P0=%#RX64 P1=%#RX64 P2=%#RX64 P3=%#RX64 P4=%#RX64 "
                sKey = sDetails.split(' ', 1)[0];
                try:    sKey = '0x%08X' % (int(sKey, 16),);
                except: pass;
                if sKey in self.asBsodReasons or sKey.lower() in self.asBsodReasons:
                    tReason = ( self.ksBsodCategory, sKey );
                else:
                    self.dprint('BSOD "%s" not found in %s;' % (sKey, self.asBsodReasons));
                    tReason = ( self.ksBsodCategory, self.ksBsodAddNew );
                return oCaseFile.noteReasonForId(tReason, oFailedResult.idTestResult, sComment = sDetails.strip());

            #
            # Look for linux panic.
            #
            if sKrnlLog is not None:
                pass; ## @todo

            #
            # Loop thru the simple stuff.
            #
            fFoundSomething = False;
            for fStopOnHit, sNeedle, tReason in self.katSimpleMainAndVmLogReasons:
                if sResultLog.find(sNeedle) > 0 or sVMLog.find(sNeedle) > 0:
                    oCaseFile.noteReasonForId(tReason, oFailedResult.idTestResult);
                    if fStopOnHit:
                        return True;
                    fFoundSomething = True;

            #
            # Check for repeated reboots...
            #
            cResets = sVMLog.count('Changing the VM state from \'RUNNING\' to \'RESETTING\'');
            if cResets > 10:
                return oCaseFile.noteReasonForId(self.ktReason_Unknown_Reboot_Loop, oFailedResult.idTestResult,
                                                 sComment = 'Counted %s reboots' % (cResets,));

            return fFoundSomething;

        #
        # Check if we got any VM or/and kernel logs.  Treat them as sets in
        # case we run multiple VMs here.
        #
        sVMLog   = None;
        sKrnlLog = None;
        for oFile in oFailedResult.aoFiles:
            if oFile.sKind == TestResultFileData.ksKind_LogReleaseVm:
                if sVMLog is not None:
                    if investigateLogSet() is True:
                        return True;
                sKrnlLog = None;
                sVMLog   = oCaseFile.getLogFile(oFile);
            elif oFile.sKind == TestResultFileData.ksKind_LogGuestKernel:
                sKrnlLog = oCaseFile.getLogFile(oFile);
        if sVMLog is not None and investigateLogSet() is True:
            return True;

        return None;


    def isResultFromVMRun(self, oFailedResult, sResultLog):
        """
        Checks if this result and corresponding log snippet looks like a VM run.
        """

        # Look for startVmEx/ startVmAndConnectToTxsViaTcp and similar output in the log.
        if sResultLog.find(' startVm') > 0:
            return True;

        # Any other indicators? No?
        _ = oFailedResult;
        return False;


    def investigateVBoxVMTest(self, oCaseFile, fSingleVM):
        """
        Checks out a VBox VM test.

        This is generic investigation of a test running one or more VMs, like
        for example a smoke test or a guest installation test.

        The fSingleVM parameter is a hint, which probably won't come in useful.
        """
        _ = fSingleVM;

        #
        # Get a list of test result failures we should be looking into and the main log.
        #
        aoFailedResults = oCaseFile.oTree.getListOfFailures();
        sMainLog        = oCaseFile.getMainLog();

        #
        # There are a set of errors ending up on the top level result record.
        # Should deal with these first.
        #
        if len(aoFailedResults) == 1 and aoFailedResults[0] == oCaseFile.oTree:
            # Check if we've just got that XPCOM client smoke test shutdown issue.  This will currently always
            # be reported on the top result because vboxinstall.py doesn't add an error for it.  It is easy to
            # ignore other failures in the test if we're not a little bit careful here.
            if sMainLog.find('vboxinstaller: Exit code: -11 (') > 0:
                oCaseFile.noteReason(self.ktReason_XPCOM_Exit_Minus_11);
                return self.caseClosed(oCaseFile);

            # Hang after starting VBoxSVC (e.g. idTestSet=136307258)
            if self.isThisFollowedByTheseLines(sMainLog, 'oVBoxMgr=<vboxapi.VirtualBoxManager object at',
                                               (' Timeout: ', ' Attempting to abort child...',) ):
                if sMainLog.find('*** glibc detected *** /') > 0:
                    oCaseFile.noteReason(self.ktReason_XPCOM_VBoxSVC_Hang_Plus_Heap_Corruption);
                else:
                    oCaseFile.noteReason(self.ktReason_XPCOM_VBoxSVC_Hang);
                return self.caseClosed(oCaseFile);



            # Look for heap corruption without visible hang.
            if   sMainLog.find('*** glibc detected *** /') > 0 \
              or sMainLog.find("-1073740940"): # STATUS_HEAP_CORRUPTION / 0xc0000374
                oCaseFile.noteReason(self.ktReason_Unknown_Heap_Corruption);
                return self.caseClosed(oCaseFile);

        #
        # Go thru each failed result.
        #
        for oFailedResult in aoFailedResults:
            self.dprint('Looking at test result #%u - %s' % (oFailedResult.idTestResult, oFailedResult.getFullName(),));
            sResultLog = TestSetData.extractLogSectionElapsed(sMainLog, oFailedResult.tsCreated, oFailedResult.tsElapsed);
            if oFailedResult.sName == 'Installing VirtualBox':
                self.vprint('TODO: Installation failure');
            elif oFailedResult.sName == 'Uninstalling VirtualBox':
                self.vprint('TODO: Uninstallation failure');
            elif self.isResultFromVMRun(oFailedResult, sResultLog):
                self.investigateVMResult(oCaseFile, oFailedResult, sResultLog);
            elif sResultLog.find('The machine is not mutable (state is ') > 0:
                self.vprint('Ignorining "machine not mutable" error as it is probably due to an earlier problem');
                oCaseFile.noteReasonForId(self.ktHarmless, oFailedResult.idTestResult);
            else:
                self.vprint('TODO: Cannot place idTestResult=%u - %s' % (oFailedResult.idTestResult, oFailedResult.sName,));
                self.dprint('%s + %s <<\n%s\n<<' % (oFailedResult.tsCreated, oFailedResult.tsElapsed, sResultLog,));

        #
        # Report home and close the case if we got them all, otherwise log it.
        #
        if len(oCaseFile.dReasonForResultId) >= len(aoFailedResults):
            return self.caseClosed(oCaseFile);

        if len(oCaseFile.dReasonForResultId) > 0:
            self.vprint('TODO: Got %u out of %u - close, but no cigar. :-/'
                        % (len(oCaseFile.dReasonForResultId), len(aoFailedResults)));
        else:
            self.vprint('XXX: Could not figure out anything at all! :-(');
        return False;


    def reasoningFailures(self):
        """
        Guess the reason for failures.
        """
        #
        # Get a list of failed test sets without any assigned failure reason.
        #
        cGot = 0;
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
                fRc = self.investigateBadTestBox(oCaseFile);
            elif oCaseFile.isVBoxUnitTest():
                self.dprint('investigateVBoxUnitTest is taking over %s.' % (oCaseFile.sLongName,));
                fRc = self.investigateVBoxUnitTest(oCaseFile);
            elif oCaseFile.isVBoxInstallTest():
                self.dprint('investigateVBoxVMTest is taking over %s.' % (oCaseFile.sLongName,));
                fRc = self.investigateVBoxVMTest(oCaseFile, fSingleVM = True);
            elif oCaseFile.isVBoxSmokeTest():
                self.dprint('investigateVBoxVMTest is taking over %s.' % (oCaseFile.sLongName,));
                fRc = self.investigateVBoxVMTest(oCaseFile, fSingleVM = False);
            else:
                self.vprint('reasoningFailures: Unable to classify test set: %s' % (oCaseFile.sLongName,));
                fRc = False;
            cGot += fRc is True;
        self.vprint('reasoningFailures: Got %u out of %u' % (cGot, len(aoTestSets), ));
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
        self.asBsodReasons           = self.oFailureReasonLogic.fetchForSheriffByNamedCategory(self.ksBsodCategory);

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

