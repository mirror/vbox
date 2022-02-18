#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$

"""
VirtualBox Validation Kit - Unit Tests.
"""

__copyright__ = \
"""
Copyright (C) 2010-2022 Oracle Corporation

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


# Standard Python imports.
import os
import sys
import re


# Only the main script needs to modify the path.
try:    __file__
except: __file__ = sys.argv[0];
g_ksValidationKitDir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.append(g_ksValidationKitDir)

# Validation Kit imports.
from common     import utils;
from testdriver import reporter;
from testdriver import vbox;
from testdriver import vboxcon;


class tdUnitTest1(vbox.TestDriver):
    """
    Unit Tests.
    """

    ## The temporary exclude list.
    ## @note This shall be empty before we release 4.3!
    kdTestCasesBuggyPerOs = {
        'darwin': {
            'testcase/tstX86-1': '',                    # 'FSTP M32R, ST0' fails; no idea why.
        },
        'linux': {
            'testcase/tstRTFileAio': '',                # See xTracker #8035.
        },
        'linux.amd64': {
            'testcase/tstLdr-4': '',        # failed: Failed to get bits for '/home/vbox/test/tmp/bin/testcase/tstLdrObjR0.r0'/0,
                                                        # rc=VERR_SYMBOL_VALUE_TOO_BIG. aborting test
        },
        'solaris': {
            'testcase/tstIntNet-1': '',                 # Fails opening rge0, probably a generic issue figuring which nic to use.
            'testcase/tstIprtList': '',                 # Crashes in the multithreaded test, I think.
            'testcase/tstRTCritSect': '',               # Fairness/whatever issue here.
            'testcase/tstRTR0MemUserKernelDriver': '',  # Failes when kernel to kernel buffers.
            'testcase/tstRTSemRW': '',                  # line 338: RTSemRWReleaseRead(hSemRW): got VERR_ACCESS_DENIED
            'testcase/tstRTStrAlloc': '',               # VERR_NO_STR_MEMORY!
            'testcase/tstRTFileQuerySize-1': '',        # VERR_DEV_IO_ERROR on /dev/null!
        },
        'solaris.amd64': {
            'testcase/tstLdr-4': '',        # failed: Failed to get bits for '/home/vbox/test/tmp/bin/testcase/tstLdrObjR0.r0'/0,
                                                        # rc=VERR_SYMBOL_VALUE_TOO_BIG. aborting test
        },
        'win': {
            'testcase/tstFile': '',                     # ??
            'testcase/tstIntNet-1': '',                 # possibly same issue as solaris.
            'testcase/tstMouseImpl': '',                # STATUS_ACCESS_VIOLATION
            'testcase/tstRTR0ThreadPreemptionDriver': '', # ??
            'testcase/tstRTPath': '<4.3.51r89894',
            'testcase/tstRTPipe': '',                   # ??
            'testcase/tstRTR0MemUserKernelDriver': '',  # ??
            'testcase/tstRTR0SemMutexDriver': '',       # ??
            'testcase/tstRTStrAlloc': '',               # ??
            'testcase/tstRTStrFormat': '',              # ??
            'testcase/tstRTSystemQueryOsInfo': '',      # ??
            'testcase/tstRTTemp': '',                   # ??
            'testcase/tstRTTime': '',                   # ??
            'testcase/tstTime-2': '',                   # Total time differs too much! ... delta=-10859859
            'testcase/tstTime-4': '',                   # Needs to be converted to DLL; ditto for tstTime-2.
            'testcase/tstUtf8': '',                     # ??
            'testcase/tstVMMR0CallHost-2': '',          # STATUS_STACK_OVERFLOW
            'testcase/tstX86-1': '',                    # Fails on win.x86.
            'tscpasswd': '',                            # ??
            'tstVMREQ': '',                             # ?? Same as darwin.x86?
        },
        'win.x86': {
            'testcase/tstRTR0TimerDriver': '',          # See xTracker #8041.
        }
    };

    kdTestCasesBuggy = {
        'testcase/tstGuestPropSvc': '',     # GET_NOTIFICATION fails on testboxlin5.de.oracle.com and others.
        'testcase/tstRTProcCreateEx': '',   # Seen failing on wei01-b6ka-9.de.oracle.com.
        'testcase/tstTimer': '',            # Sometimes fails on linux, not important atm.
        'testcase/tstGIP-2': '',            # 2015-09-10: Fails regularly. E.g. TestSetID 2744205 (testboxsh2),
                                            #             2743961 (wei01-b6kc-6). The responsible engineer should reenable
                                            #             it once it has been fixed.
    };

    ## The permanent exclude list.
    # @note Stripped of extensions!
    kdTestCasesBlackList = {
        'testcase/tstClipboardX11Smoke': '',            # (Old naming, deprecated) Needs X, not available on all test boxes.
        'testcase/tstClipboardGH-X11Smoke': '',         # (New name) Ditto.
        'tstClipboardQt': '',                           # Is interactive and needs Qt, needed for Qt clipboard bugfixing.
        'testcase/tstClipboardQt': '',                  # In case it moves here.
        'testcase/tstFileLock': '',
        'testcase/tstDisasm-2': '',                     # without parameters it will disassembler 1GB starting from 0
        'testcase/tstFileAppendWin-1': '',
        'testcase/tstDir': '',                          # useless without parameters
        'testcase/tstDir-2': '',                        # useless without parameters
        'testcase/tstGlobalConfig': '',
        'testcase/tstHostHardwareLinux': '',            # must be killed with CTRL-C
        'testcase/tstHttp': '',                         # Talks to outside servers.
        'testcase/tstRTHttp': '',                       # parameters required
        'testcase/tstLdr-2': '',                        # parameters required
        'testcase/tstLdr-3': '',                        # parameters required
        'testcase/tstLdr': '',                          # parameters required
        'testcase/tstLdrLoad': '',                      # parameters required
        'testcase/tstMove': '',                         # parameters required
        'testcase/tstRTR0Timer': '',                    # loads 'tstRTR0Timer.r0'
        'testcase/tstRTR0ThreadDriver': '',             # loads 'tstRTR0Thread.r0'
        'testcase/tstRunTestcases': '',                 # that's a script like this one
        'testcase/tstRTReqPool': '',                    # fails sometimes, testcase buggy
        'testcase/tstRTS3': '',                         # parameters required
        'testcase/tstSDL': '',                          # graphics test
        'testcase/tstSupLoadModule': '',                # Needs parameters and vboxdrv access. Covered elsewhere.
        'testcase/tstSeamlessX11': '',                  # graphics test
        'testcase/tstTime-3': '',                       # parameters required
        'testcase/tstVBoxControl': '',                  # works only inside a guest
        'testcase/tstVDCopy': '',                       # parameters required
        'testcase/tstVDFill': '',                       # parameters required
        'tstAnimate': '',                               # parameters required
        'testcase/tstAPI': '',                          # user interaction required
        'tstCollector': '',                             # takes forever
        'testcase/tstHeadless': '',                     # parameters required
        'tstHeadless': '',                              # parameters required
        'tstMicroRC': '',                               # required by tstMicro
        'tstVBoxDbg': '',                               # interactive test
        'testcase/tstTestServMgr': '',                  # some strange xpcom18a4 test, does not work
        'tstTestServMgr': '',                           # some strange xpcom18a4 test, does not work
        'tstPDMAsyncCompletion': '',                    # parameters required
        'testcase/tstXptDump': '',                      # parameters required
        'tstXptDump': '',                               # parameters required
        'testcase/tstnsIFileEnumerator': '',            # some strange xpcom18a4 test, does not work
        'tstnsIFileEnumerator': '',                     # some strange xpcom18a4 test, does not work
        'testcase/tstSimpleTypeLib': '',                # parameters required
        'tstSimpleTypeLib': '',                         # parameters required
        'testcase/tstTestAtoms': '',                    # additional test file (words.txt) required
        'tstTestAtoms': '',                             # additional test file (words.txt) required
        'testcase/tstXptLink': '',                      # parameters required
        'tstXptLink': '',                               # parameters required
        'tstXPCOMCGlue': '',                            # user interaction required
        'testcase/tstXPCOMCGlue': '',                   # user interaction required
        'testcase/tstCAPIGlue': '',                     # user interaction required
        'testcase/tstTestCallTemplates': '',            # some strange xpcom18a4 test, segfaults
        'tstTestCallTemplates': '',                     # some strange xpcom18a4 test, segfaults
        'testcase/tstRTFilesystem': '',                 # parameters required
        'testcase/tstRTDvm': '',                        # parameters required
        'tstSSLCertDownloads': '',                      # Obsolete.
        # later
        'testcase/tstIntNetR0': '',                     # RTSPINLOCK_FLAGS_INTERRUPT_SAFE == RTSPINLOCK_FLAGS_INTERRUPT_UNSAFE
        # slow stuff
        'testcase/tstAvl': '',                          # SLOW!
        'testcase/tstRTAvl': '',                        # SLOW! (new name)
        'testcase/tstVD': '',                           # 8GB fixed-sized vmdk
        # failed or hang
        'testcase/tstCryptoPkcs7Verify': '',            # hang
        'tstOVF': '',                                   # hang (only ancient version, now in new place)
        'testcase/tstOVF': '',                          # Creates mess when fails, needs to be run in a separate test.
        'testcase/tstRTLockValidator': '',              # Lock validation is not enabled for critical sections
        'testcase/tstGuestControlSvc': '',              # failed: line 288: testHost(&svcTable):
                                                        # expected VINF_SUCCESS, got VERR_NOT_FOUND
        'testcase/tstRTMemEf': '',                      # failed w/o error message
        'testcase/tstSupSem': '',                       # failed: SRE Timeout Accuracy (ms) : FAILED (1 errors)
        'testcase/tstCryptoPkcs7Sign': '',              # failed: 29330:
                                                        # error:02001002:lib(2):func(1):reason(2):NA:0:fopen('server.pem': '','r')
        'testcase/tstCompressionBenchmark': '',         # failed: error: RTZipBlockCompress failed
                                                        # for 'RTZipBlock/LZJB' (#4): VERR_NOT_SUPPORTED
        'tstPDMAsyncCompletionStress': '',              # VERR_INVALID_PARAMETER (cbSize = 0)
        'tstMicro': '',                                 # doesn't work on solaris, fix later if we care.
        'tstVMM-HwAccm': '',                            # failed: Only checked AMD-V on linux
        'tstVMM-HM': '',                                # failed: Only checked AMD-V on linux
        'tstVMMFork': '',                               # failed: xtracker 6171
        'tstTestFactory': '',                           # some strange xpcom18a4 test, does not work
        'testcase/tstRTSemXRoads': '',                  # sporadically failed: Traffic - 8 threads per direction, 10 sec :
                                                        # FAILED (8 errors)
        'tstVBoxAPILinux': '',                          # creates VirtualBox directories for root user because of sudo
                                                        # (should be in vbox)
        'testcase/tstVMStructDTrace': '',               # This is a D-script generator.
        'tstVMStructRC': '',                            # This is a C-code generator.
        'tstDeviceStructSizeRC': '',                    # This is a C-code generator.
        'testcase/tstTSC': '',                          # Doesn't test anything and might fail with HT or/and too many cores.
        'testcase/tstOpenUSBDev': '',                   # Not a useful testcase.
        'testcase/tstX86-1': '',                        # Really more guest side.
        'testcase/tstX86-FpuSaveRestore': '',           # Experiments, could be useful for the guest not the host.
        'tstAsmStructsRC': '',                          # Testcase run during build time (fails to find libstdc++.so.6 on some
                                                        # Solaris testboxes).
    };

    # Suffix exclude list.
    kasSuffixBlackList = [
        '.r0',
        '.gc',
        '.debug',
        '.rel',
        '.sys',
        '.ko',
        '.o',
        '.obj',
        '.lib',
        '.a',
        '.so',
        '.dll',
        '.dylib',
        '.tmp',
        '.log',
        '.py',
        '.pyc',
        '.pyo',
        '.pdb',
        '.dSYM',
        '.sym',
        '.template',
        '.expected',
        '.expect',
    ];

    # White list, which contains tests considered to be safe to execute,
    # even on remote targets (guests).
    kdTestCasesWhiteList = {
        'testcase/tstFile': '',
        'testcase/tstFileLock': '',
        'testcase/tstRTLocalIpc': '',
        'testcase/tstRTPathQueryInfo': '',
        'testcase/tstRTPipe': '',
        'testcase/tstRTProcCreateEx': '',
        'testcase/tstRTProcCreatePrf': '',
        'testcase/tstRTProcIsRunningByName': '',
        'testcase/tstRTProcQueryUsername': '',
        'testcase/tstRTProcWait': '',
        'testcase/tstTime-2': '',
        'testcase/tstTime-3': '',
        'testcase/tstTime-4': '',
        'testcase/tstTimer': '',
        'testcase/tstThread-1': '',
        'testcase/tstUtf8': '',
    }

    # Test dependency list -- libraries.
    # Needed in order to execute testcases on remote targets which don't have a VBox installation present.
    kdTestCaseDepsLibs = [
        "VBoxRT"
    ];

    ## The exclude list.
    # @note Stripped extensions!
    kasHardened = [
        "testcase/tstIntNet-1",
        "testcase/tstR0ThreadPreemptionDriver", # VBox 4.3
        "testcase/tstRTR0ThreadPreemptionDriver",
        "testcase/tstRTR0MemUserKernelDriver",
        "testcase/tstRTR0SemMutexDriver",
        "testcase/tstRTR0TimerDriver",
        "testcase/tstRTR0ThreadDriver",
        'testcase/tstRTR0DbgKrnlInfoDriver',
        "tstInt",
        "tstVMM",
        "tstVMMFork",
        "tstVMREQ",
        'testcase/tstCFGM',
        'testcase/tstContiguous',
        'testcase/tstGetPagingMode',
        'testcase/tstGIP-2',
        'testcase/tstInit',
        'testcase/tstLow',
        'testcase/tstMMHyperHeap',
        'testcase/tstPage',
        'testcase/tstPin',
        'testcase/tstRTTime',   'testcase/tstTime',   # GIP test case.
        'testcase/tstRTTime-2', 'testcase/tstTime-2', # GIP test case.
        'testcase/tstRTTime-4', 'testcase/tstTime-4', # GIP test case.
        'testcase/tstSSM',
        'testcase/tstSupSem-Zombie',
    ]

    ## Argument lists
    kdArguments = {
        'testcase/tstbntest':       [ '-out', os.devnull, ], # Very noisy.
    };


    ## Status code translations.
    ## @{
    kdExitCodeNames = {
        0:              'RTEXITCODE_SUCCESS',
        1:              'RTEXITCODE_FAILURE',
        2:              'RTEXITCODE_SYNTAX',
        3:              'RTEXITCODE_INIT',
        4:              'RTEXITCODE_SKIPPED',
    };
    kdExitCodeNamesWin = {
        -1073741515:    'STATUS_DLL_NOT_FOUND',
        -1073741512:    'STATUS_ORDINAL_NOT_FOUND',
        -1073741511:    'STATUS_ENTRYPOINT_NOT_FOUND',
        -1073741502:    'STATUS_DLL_INIT_FAILED',
        -1073741500:    'STATUS_UNHANDLED_EXCEPTION',
        -1073741499:    'STATUS_APP_INIT_FAILURE',
        -1073741819:    'STATUS_ACCESS_VIOLATION',
        -1073741571:    'STATUS_STACK_OVERFLOW',
    };
    ## @}

    def __init__(self):
        """
        Reinitialize child class instance.
        """
        vbox.TestDriver.__init__(self);

        # We need to set a default test VM set here -- otherwise the test
        # driver base class won't let us use the "--test-vms" switch.
        #
        # See the "--local" switch in self.parseOption().
        self.oTestVmSet = self.oTestVmManager.getSmokeVmSet('nat');

        # Selected NIC attachment.
        self.sNicAttachment = '';

        # Session handling stuff.
        # Only needed for remote tests executed by TxS.
        self.oSession    = None;
        self.oTxsSession = None;

        self.sVBoxInstallRoot = None;

        self.cSkipped   = 0;
        self.cPassed    = 0;
        self.cFailed    = 0;

        # The source directory where our unit tests live.
        # This most likely is our out/ or some staging directory and
        # also acts the source for copying over the testcases to a remote target.
        self.sUnitTestsPathSrc = None;

        # Array of environment variables with NAME=VAL entries
        # to be applied for testcases.
        #
        # This is also needed for testcases which are being executed remotely.
        self.asEnv = [];

        # The destination directory our unit tests live when being
        # copied over to a remote target (via TxS).
        self.sUnitTestsPathDst = None;

        self.sExeSuff   = '.exe' if utils.getHostOs() in [ 'win', 'dos', 'os2' ] else '';

        self.aiVBoxVer  = (4, 3, 0, 0);

        # For testing testcase logic.
        self.fDryRun        = False;
        self.fOnlyWhiteList = False;


    def _detectPaths(self):
        """
        Internal worker for actionVerify and actionExecute that detects paths.

        This sets sVBoxInstallRoot and sUnitTestsPathBase and returns True/False.
        """

        #
        # We need a VBox install (/ build) to test.
        #
        if False is True: ## @todo r=andy WTF?
            if not self.importVBoxApi():
                reporter.error('Unabled to import the VBox Python API.')
                return False
        else:
            self._detectBuild();
            if self.oBuild is None:
                reporter.error('Unabled to detect the VBox build.');
                return False;

        #
        # Where are the files installed?
        # Solaris requires special handling because of it's multi arch subdirs.
        #
        self.sVBoxInstallRoot = self.oBuild.sInstallPath
        if not self.oBuild.isDevBuild() and utils.getHostOs() == 'solaris':
            sArchDir = utils.getHostArch();
            if sArchDir == 'x86': sArchDir = 'i386';
            self.sVBoxInstallRoot = os.path.join(self.sVBoxInstallRoot, sArchDir);

        # Add the installation root to the PATH on windows so we can get DLLs from it.
        if utils.getHostOs() == 'win':
            sPathName = 'PATH';
            if not sPathName in os.environ:
                sPathName = 'Path';
            sPath = os.environ.get(sPathName, '.');
            if sPath and sPath[-1] != ';':
                sPath += ';';
            os.environ[sPathName] = sPath + self.sVBoxInstallRoot + ';';

        #
        # The unittests are generally not installed, so look for them.
        #
        sBinOrDist = 'dist' if utils.getHostOs() in [ 'darwin', ] else 'bin';
        asCandidates = [
            self.oBuild.sInstallPath,
            os.path.join(self.sScratchPath, utils.getHostOsDotArch(), self.oBuild.sType, sBinOrDist),
            os.path.join(self.sScratchPath, utils.getHostOsDotArch(), 'release', sBinOrDist),
            os.path.join(self.sScratchPath, utils.getHostOsDotArch(), 'debug',   sBinOrDist),
            os.path.join(self.sScratchPath, utils.getHostOsDotArch(), 'strict',  sBinOrDist),
            os.path.join(self.sScratchPath, utils.getHostOsDotArch(), 'dbgopt',  sBinOrDist),
            os.path.join(self.sScratchPath, utils.getHostOsDotArch(), 'profile', sBinOrDist),
            os.path.join(self.sScratchPath, sBinOrDist + '.' + utils.getHostArch()),
            os.path.join(self.sScratchPath, sBinOrDist, utils.getHostArch()),
            os.path.join(self.sScratchPath, sBinOrDist),
        ];
        if utils.getHostOs() == 'darwin':
            for i in range(1, len(asCandidates)):
                asCandidates[i] = os.path.join(asCandidates[i], 'VirtualBox.app', 'Contents', 'MacOS');

        for sCandidat in asCandidates:
            if os.path.exists(os.path.join(sCandidat, 'testcase', 'tstVMStructSize' + self.sExeSuff)):
                self.sUnitTestsPathSrc = sCandidat;
                return True;

        reporter.error('Unable to find unit test dir. Candidates: %s' % (asCandidates,))
        return False;

    #
    # Overridden methods.
    #

    def showUsage(self):
        """
        Shows the testdriver usage.
        """
        fRc = vbox.TestDriver.showUsage(self);
        reporter.log('');
        reporter.log('Unit Test #1 options:');
        reporter.log('  --dryrun');
        reporter.log('      Performs a dryrun (no tests being executed).');
        reporter.log('  --local');
        reporter.log('      Runs unit tests locally (this host).');
        reporter.log('  --only-whitelist');
        reporter.log('      Only processes the white list.');
        reporter.log('  --quick');
        reporter.log('      Very selective testing.');
        return fRc;

    def parseOption(self, asArgs, iArg):
        """
        Parses the testdriver arguments from the command line.
        """
        if asArgs[iArg] == '--dryrun':
            self.fDryRun = True;
        elif asArgs[iArg] == '--local':
            # Disable the test VM set and only run stuff locally.
            self.oTestVmSet = None;
        elif asArgs[iArg] == '--only-whitelist':
            self.fOnlyWhiteList = True;
        elif asArgs[iArg] == '--quick':
            self.fOnlyWhiteList = True;
        else:
            return vbox.TestDriver.parseOption(self, asArgs, iArg);
        return iArg + 1;

    def actionVerify(self):
        if not self._detectPaths():
            return False;

        if self.oTestVmSet:
            return vbox.TestDriver.actionVerify(self);

        return True;

    def actionConfig(self):
        # Make sure vboxapi has been imported so we can use the constants.
        if not self.importVBoxApi():
            return False;

        # Do the configuring.
        if self.oTestVmSet:
            if   self.sNicAttachment == 'nat':     eNic0AttachType = vboxcon.NetworkAttachmentType_NAT;
            elif self.sNicAttachment == 'bridged': eNic0AttachType = vboxcon.NetworkAttachmentType_Bridged;
            else:                                  eNic0AttachType = None;

            # Make sure to mount the Validation Kit .ISO so that TxS has the chance
            # to update itself.
            #
            # This is necessary as a lot of our test VMs nowadays have a very old TxS
            # installed which don't understand commands like uploading files to the guest.
            # Uploading files is needed for this test driver, however.
            #
            ## @todo Get rid of this as soon as we create test VMs in a descriptive (automated) manner.
            return self.oTestVmSet.actionConfig(self, eNic0AttachType = eNic0AttachType, \
                                                sDvdImage = self.sVBoxValidationKitIso);

        return True;

    def actionExecute(self):
        if self.sUnitTestsPathSrc is None and not self._detectPaths():
            return False;

        if not self.sUnitTestsPathDst:
            self.sUnitTestsPathDst = self.sScratchPath;
        reporter.log('Unit test destination path is "%s"\n' % self.sUnitTestsPathDst);

        if self.oTestVmSet: # Run on a test VM (guest).
            fRc = self.oTestVmSet.actionExecute(self, self.testOneVmConfig);
        else: # Run locally (host).
            self._figureVersion();
            self._makeEnvironmentChanges();
            fRc = self._testRunUnitTests(None);

        return fRc;

    #
    # Test execution helpers.
    #

    def _testRunUnitTests(self, oTestVm):
        """
        Main function to execute all unit tests.
        """
        self._testRunUnitTestsSet(r'^tst*', 'testcase');
        self._testRunUnitTestsSet(r'^tst*', '.');

        fRc = self.cFailed == 0;

        reporter.log('');
        reporter.log('********************');
        reporter.log('Target: %s' % (oTestVm.sVmName if oTestVm else 'local'));
        reporter.log('********************');
        reporter.log('***  PASSED: %d' % self.cPassed);
        reporter.log('***  FAILED: %d' % self.cFailed);
        reporter.log('*** SKIPPED: %d' % self.cSkipped);
        reporter.log('***   TOTAL: %d' % (self.cPassed + self.cFailed + self.cSkipped));

        return fRc;


    def testOneVmConfig(self, oVM, oTestVm):
        """
        Runs the specified VM thru test #1.
        """

        # Simple test.
        self.logVmInfo(oVM);
        # Try waiting for a bit longer (5 minutes) until the CD is available to avoid running into timeouts.
        self.oSession, self.oTxsSession = self.startVmAndConnectToTxsViaTcp(oTestVm.sVmName, \
                                                                            fCdWait = True, \
                                                                            cMsCdWait = 5 * 60 * 1000);
        if self.oSession is not None:
            self.addTask(self.oTxsSession);

            # Determine the unit tests destination path.
            self.sUnitTestsPathDst = oTestVm.pathJoin(self.getGuestTempDir(oTestVm), 'testUnitTests');

            # Run the unit tests.
            self._testRunUnitTests(oTestVm);

            # Cleanup.
            self.removeTask(self.oTxsSession);
            self.terminateVmBySession(self.oSession);
            return True;

        return False;

    #
    # Test execution helpers.
    #

    def _figureVersion(self):
        """ Tries to figure which VBox version this is, setting self.aiVBoxVer. """
        try:
            sVer = utils.processOutputChecked(['VBoxManage', '--version'])

            sVer = sVer.strip();
            sVer = re.sub(r'_BETA.*r', '.', sVer);
            sVer = re.sub(r'_ALPHA.*r', '.', sVer);
            sVer = re.sub(r'_RC.*r', '.', sVer);
            sVer = re.sub('_SPB', '', sVer)
            sVer = sVer.replace('r', '.');

            self.aiVBoxVer = [int(sComp) for sComp in sVer.split('.')];

            reporter.log('VBox version: %s' % (self.aiVBoxVer,));
        except:
            reporter.logXcpt();
            return False;
        return True;

    def _compareVersion(self, aiVer):
        """
        Compares the give version string with the vbox version string,
        returning a result similar to C strcmp().  aiVer is on the right side.
        """
        cComponents = min(len(self.aiVBoxVer), len(aiVer));
        for i in range(cComponents):
            if self.aiVBoxVer[i] < aiVer[i]:
                return -1;
            if self.aiVBoxVer[i] > aiVer[i]:
                return 1;
        return len(self.aiVBoxVer) - len(aiVer);

    def _isExcluded(self, sTest, dExclList):
        """ Checks if the testcase is excluded or not. """
        if sTest in dExclList:
            sFullExpr = dExclList[sTest].replace(' ', '').strip();
            if sFullExpr == '':
                return True;

            # Consider each exclusion expression. These are generally ranges,
            # either open ended or closed: "<4.3.51r12345", ">=4.3.0 && <=4.3.4".
            asExprs = sFullExpr.split(';');
            for sExpr in asExprs:

                # Split it on the and operator and process each sub expression.
                fResult = True;
                for sSubExpr in sExpr.split('&&'):
                    # Split out the comparison operator and the version value.
                    if sSubExpr.startswith('<=') or sSubExpr.startswith('>='):
                        sOp = sSubExpr[:2];
                        sValue = sSubExpr[2:];
                    elif sSubExpr.startswith('<') or sSubExpr.startswith('>') or sSubExpr.startswith('='):
                        sOp = sSubExpr[:1];
                        sValue = sSubExpr[1:];
                    else:
                        sOp = sValue = '';

                    # Convert the version value, making sure we've got a valid one.
                    try:    aiValue = [int(sComp) for sComp in sValue.replace('r', '.').split('.')];
                    except: aiValue = ();
                    if not aiValue or len(aiValue) > 4:
                        reporter.error('Invalid exclusion expression for %s: "%s" [%s]' % (sTest, sSubExpr, dExclList[sTest]));
                        return True;

                    # Do the compare.
                    iCmp = self._compareVersion(aiValue);
                    if sOp == '>=' and iCmp < 0:
                        fResult = False;
                    elif sOp == '>' and iCmp <= 0:
                        fResult = False;
                    elif sOp == '<' and iCmp >= 0:
                        fResult = False;
                    elif sOp == '>=' and iCmp < 0:
                        fResult = False;
                    reporter.log2('iCmp=%s; %s %s %s -> %s' % (iCmp, self.aiVBoxVer, sOp, aiValue, fResult));

                # Did the expression match?
                if fResult:
                    return True;

        return False;

    def _sudoExecuteSync(self, asArgs):
        """
        Executes a sudo child process synchronously.
        Returns True if the process executed successfully and returned 0,
        otherwise False is returned.
        """
        reporter.log('Executing [sudo]: %s' % (asArgs, ));
        if self.oTestVmSet:
            iRc = -1; ## @todo Not used remotely yet.
        else:
            try:
                iRc = utils.sudoProcessCall(asArgs, shell = False, close_fds = False);
            except:
                reporter.errorXcpt();
                return False;
            reporter.log('Exit code [sudo]: %s (%s)' % (iRc, asArgs));
        return iRc == 0;

    def _hardenedPathExists(self, sPath):
        """
        Creates the directory specified sPath (including parents).
        """
        reporter.log('_hardenedPathExists: %s' % (sPath,));
        fRc = False;
        if self.oTestVmSet:
            fRc = self.txsIsDir(self.oSession, self.oTxsSession, sPath, fIgnoreErrors = True);
            if not fRc:
                fRc = self.txsIsFile(self.oSession, self.oTxsSession, sPath, fIgnoreErrors = True);
        else:
            fRc = os.path.exists(sPath);
        return fRc;

    def _hardenedMkDir(self, sPath):
        """
        Creates the directory specified sPath (including parents).
        """
        reporter.log('_hardenedMkDir: %s' % (sPath,));
        fRc = True;
        if self.oTestVmSet:
            fRc = self.txsMkDirPath(self.oSession, self.oTxsSession, sPath, fMode = 0o755);
        else:
            if utils.getHostOs() in [ 'win', 'os2' ]:
                os.makedirs(sPath, 0o755);
            else:
                fRc = self._sudoExecuteSync(['/bin/mkdir', '-p', '-m', '0755', sPath]);
        if fRc is not True:
            raise Exception('Failed to create dir "%s".' % (sPath,));
        return True;

    def _hardenedCopyFile(self, sSrc, sDst, iMode):
        """
        Copies a file.
        """
        reporter.log('_hardenedCopyFile: %s -> %s (mode: %o)' % (sSrc, sDst, iMode,));
        fRc = True;
        if self.oTestVmSet:
            fRc = self.txsUploadFile(self.oSession, self.oTxsSession, sSrc, sDst);
            if fRc:
                self.oTxsSession.syncChMod(sDst, 0o755);
        else:
            if utils.getHostOs() in [ 'win', 'os2' ]:
                utils.copyFileSimple(sSrc, sDst);
                os.chmod(sDst, iMode);
            else:
                fRc = self._sudoExecuteSync(['/bin/cp', sSrc, sDst]);
                if fRc:
                    fRc = self._sudoExecuteSync(['/bin/chmod', '%o' % (iMode,), sDst]);
                    if fRc is not True:
                        raise Exception('Failed to chmod "%s".' % (sDst,));
        if fRc is not True:
            raise Exception('Failed to copy "%s" to "%s".' % (sSrc, sDst,));
        return True;

    def _hardenedDeleteFile(self, sPath):
        """
        Deletes a file.
        """
        reporter.log('_hardenedDeleteFile: %s' % (sPath,));
        fRc = True;
        if self.oTestVmSet:
            if self.txsIsFile(self.oSession, self.oTxsSession, sPath):
                fRc = self.txsRmFile(self.oSession, self.oTxsSession, sPath);
        else:
            if os.path.exists(sPath):
                if utils.getHostOs() in [ 'win', 'os2' ]:
                    os.remove(sPath);
                else:
                    fRc = self._sudoExecuteSync(['/bin/rm', sPath]);
        if fRc is not True:
            raise Exception('Failed to remove "%s".' % (sPath,));
        return True;

    def _hardenedRemoveDir(self, sPath):
        """
        Removes a directory.
        """
        reporter.log('_hardenedRemoveDir: %s' % (sPath,));
        fRc = True;
        if self.oTestVmSet:
            if self.txsIsDir(self.oSession, self.oTxsSession, sPath):
                fRc = self.txsRmDir(self.oSession, self.oTxsSession, sPath);
        else:
            if os.path.exists(sPath):
                if utils.getHostOs() in [ 'win', 'os2' ]:
                    os.rmdir(sPath);
                else:
                    fRc = self._sudoExecuteSync(['/bin/rmdir', sPath]);
        if fRc is not True:
            raise Exception('Failed to remove "%s".' % (sPath,));
        return True;

    def _envSet(self, sName, sValue):
        if self.oTestVmSet:
            # For remote execution we cache the environment block and pass it
            # right when the process execution happens.
            self.asEnv.append([ sName, sValue ]);
        else:
            os.environ[sName] = sValue;
        return True;

    def _executeTestCase(self, sName, sFullPath, sTestCaseSubDir, oDevNull): # pylint: disable=too-many-locals,too-many-statements
        """
        Executes a test case.
        """

        fSkipped = False;

        #
        # If hardening is enabled, some test cases and their dependencies
        # needs to be copied to and execute from the sVBoxInstallRoot
        # directory in order to work. They also have to be executed as
        # root, i.e. via sudo.
        #
        fHardened       = False;
        fToRemote       = False;
        fCopyDeps       = False;
        asFilesToRemove = []; # Stuff to clean up.
        asDirsToRemove  = []; # Ditto.

        if  sName in self.kasHardened \
        and self.sUnitTestsPathSrc != self.sVBoxInstallRoot:
            fHardened = True;

        if self.oTestVmSet:
            fToRemote = True;
            fCopyDeps = True;

        if fHardened \
        or fToRemote:
            if fToRemote:
                sDstDir = os.path.join(self.sUnitTestsPathDst, sTestCaseSubDir);
            else:
                sDstDir = os.path.join(self.sVBoxInstallRoot, sTestCaseSubDir);
            if not self._hardenedPathExists(sDstDir):
                self._hardenedMkDir(sDstDir);
                asDirsToRemove.append(sDstDir);

            sDst = os.path.join(sDstDir, os.path.basename(sFullPath));
            self._hardenedCopyFile(sFullPath, sDst, 0o755);
            asFilesToRemove.append(sDst);

            # Copy required dependencies to destination.
            if fCopyDeps:
                for sLib in self.kdTestCaseDepsLibs:
                    for sSuff in [ '.dll', '.so', '.dylib' ]:
                        sSrc = os.path.join(self.sVBoxInstallRoot, sLib + sSuff);
                        if os.path.exists(sSrc):
                            sDst = os.path.join(sDstDir, os.path.basename(sSrc));
                            self._hardenedCopyFile(sSrc, sDst, 0o644);
                            asFilesToRemove.append(sDst);

            # Copy any associated .dll/.so/.dylib.
            for sSuff in [ '.dll', '.so', '.dylib' ]:
                sSrc = os.path.splitext(sFullPath)[0] + sSuff;
                if os.path.exists(sSrc):
                    sDst = os.path.join(sDstDir, os.path.basename(sSrc));
                    self._hardenedCopyFile(sSrc, sDst, 0o644);
                    asFilesToRemove.append(sDst);

            # Copy any associated .r0, .rc and .gc modules.
            offDriver = sFullPath.rfind('Driver')
            if offDriver > 0:
                for sSuff in [ '.r0', 'RC.rc', 'RC.gc' ]:
                    sSrc = sFullPath[:offDriver] + sSuff;
                    if os.path.exists(sSrc):
                        sDst = os.path.join(sDstDir, os.path.basename(sSrc));
                        self._hardenedCopyFile(sSrc, sDst, 0o644);
                        asFilesToRemove.append(sDst);

            sFullPath = os.path.join(sDstDir, os.path.basename(sFullPath));

        #
        # Set up arguments and environment.
        #
        asArgs = [sFullPath,]
        if sName in self.kdArguments:
            asArgs.extend(self.kdArguments[sName]);

        sXmlFile = os.path.join(self.sUnitTestsPathDst, 'result.xml');

        self._envSet('IPRT_TEST_OMIT_TOP_TEST', '1');
        self._envSet('IPRT_TEST_FILE', sXmlFile);

        if self._hardenedPathExists(sXmlFile):
            try:    os.unlink(sXmlFile);
            except: self._hardenedDeleteFile(sXmlFile);

        #
        # Execute the test case.
        #
        # Windows is confusing output.  Trying a few things to get rid of this.
        # First, flush both stderr and stdout before running the child.  Second,
        # assign the child stderr to stdout.  If this doesn't help, we'll have
        # to capture the child output.
        #
        reporter.log('*** Executing %s%s...' % (asArgs, ' [hardened]' if fHardened else ''));
        try:    sys.stdout.flush();
        except: pass;
        try:    sys.stderr.flush();
        except: pass;
        if not self.fDryRun:
            if fToRemote:
                fRc = self.txsRunTest(self.oTxsSession, sName, 30 * 60 * 1000, asArgs[0], asArgs, self.asEnv, \
                                      fCheckSessionStatus = True);
                if fRc:
                    iRc = 0;
                else:
                    (_, sOpcode, abPayload) = self.oTxsSession.getLastReply();
                    if sOpcode.startswith('PROC NOK '): # Extract process rc.
                        iRc = abPayload[0]; # ASSUMES 8-bit rc for now.
                        if iRc == 0: # Might happen if the testcase misses some dependencies. Set it to -42 then.
                            iRc = -42;
                    else:
                        iRc = -1; ## @todo
            else:
                try:
                    if fHardened:
                        oChild = utils.sudoProcessPopen(asArgs, stdin = oDevNull, stdout = sys.stdout, stderr = sys.stdout);
                    else:
                        oChild = utils.processPopenSafe(asArgs, stdin = oDevNull, stdout = sys.stdout, stderr = sys.stdout);
                except:
                    if sName in [ 'tstAsmStructsRC',    # 32-bit, may fail to start on 64-bit linux. Just ignore.
                                ]:
                        reporter.logXcpt();
                        fSkipped = True;
                    else:
                        reporter.errorXcpt();
                    iRc    = 1023;
                    oChild = None;

                if oChild is not None:
                    self.pidFileAdd(oChild.pid, sName, fSudo = fHardened);
                    iRc = oChild.wait();
                    self.pidFileRemove(oChild.pid);
        else:
            iRc = 0;

        #
        # Clean up
        #
        for sPath in asFilesToRemove:
            self._hardenedDeleteFile(sPath);
        for sPath in asDirsToRemove:
            self._hardenedRemoveDir(sPath);

        #
        # Report.
        #
        if os.path.exists(sXmlFile):
            reporter.addSubXmlFile(sXmlFile);
            if fHardened:
                self._hardenedDeleteFile(sXmlFile);
            else:
                os.unlink(sXmlFile);

        if iRc == 0:
            reporter.log('*** %s: exit code %d' % (sFullPath, iRc));
            self.cPassed += 1;

        elif iRc == 4: # RTEXITCODE_SKIPPED
            reporter.log('*** %s: exit code %d (RTEXITCODE_SKIPPED)' % (sFullPath, iRc));
            fSkipped = True;
            self.cSkipped += 1;

        elif fSkipped:
            reporter.log('*** %s: exit code %d (Skipped)' % (sFullPath, iRc));
            self.cSkipped += 1;

        else:
            sName = self.kdExitCodeNames.get(iRc, '');
            if iRc in self.kdExitCodeNamesWin and utils.getHostOs() == 'win':
                sName = self.kdExitCodeNamesWin[iRc];
            if sName != '':
                sName = ' (%s)' % (sName);

            if iRc != 1:
                reporter.testFailure('Exit status: %d%s' % (iRc, sName));
                reporter.log(  '!*! %s: exit code %d%s' % (sFullPath, iRc, sName));
            else:
                reporter.error('!*! %s: exit code %d%s' % (sFullPath, iRc, sName));
            self.cFailed += 1;

        return fSkipped;

    def _testRunUnitTestsSet(self, sTestCasePattern, sTestCaseSubDir):
        """
        Run subset of the unit tests set.
        """

        # Open /dev/null for use as stdin further down.
        try:
            oDevNull = open(os.path.devnull, 'w+');
        except:
            oDevNull = None;

        # Determin the host OS specific exclusion lists.
        dTestCasesBuggyForHostOs = self.kdTestCasesBuggyPerOs.get(utils.getHostOs(), []);
        dTestCasesBuggyForHostOs.update(self.kdTestCasesBuggyPerOs.get(utils.getHostOsDotArch(), []));

        ## @todo Add filtering for more specific OSes (like OL server, doesn't have X installed) by adding a separate
        #        black list + using utils.getHostOsVersion().

        #
        # Process the file list and run everything looking like a testcase.
        #
        for sFilename in sorted(os.listdir(os.path.join(self.sUnitTestsPathSrc, sTestCaseSubDir))):
            # Separate base and suffix and morph the base into something we
            # can use for reporting and array lookups.
            sName, sSuffix = os.path.splitext(sFilename);
            if sTestCaseSubDir != '.':
                sName = sTestCaseSubDir + '/' + sName;

            # Process white list first, if set.
            if  self.fOnlyWhiteList \
            and sName not in self.kdTestCasesWhiteList:
                reporter.log2('"%s" is not in white list, skipping.' % (sFilename,));
                continue;

            # Basic exclusion.
            if   not re.match(sTestCasePattern, sFilename) \
              or sSuffix in self.kasSuffixBlackList:
                reporter.log2('"%s" is not a test case.' % (sFilename,));
                continue;

            # Check if the testcase is black listed or buggy before executing it.
            if self._isExcluded(sName, self.kdTestCasesBlackList):
                # (No testStart/Done or accounting here!)
                reporter.log('%s: SKIPPED (blacklisted)' % (sName,));

            elif self._isExcluded(sName, self.kdTestCasesBuggy):
                reporter.testStart(sName);
                reporter.log('%s: Skipping, buggy in general.' % (sName,));
                reporter.testDone(fSkipped = True);
                self.cSkipped += 1;

            elif self._isExcluded(sName, dTestCasesBuggyForHostOs):
                reporter.testStart(sName);
                reporter.log('%s: Skipping, buggy on %s.' % (sName, utils.getHostOs(),));
                reporter.testDone(fSkipped = True);
                self.cSkipped += 1;

            else:
                sFullPath = os.path.normpath(os.path.join(self.sUnitTestsPathSrc, os.path.join(sTestCaseSubDir, sFilename)));
                reporter.testStart(sName);
                try:
                    fSkipped = self._executeTestCase(sName, sFullPath, sTestCaseSubDir, oDevNull);
                except:
                    reporter.errorXcpt('!*!');
                    self.cFailed += 1;
                    fSkipped = False;
                reporter.testDone(fSkipped);



if __name__ == '__main__':
    sys.exit(tdUnitTest1().main(sys.argv))
