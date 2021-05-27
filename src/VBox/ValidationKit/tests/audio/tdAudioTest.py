# -*- coding: utf-8 -*-
# $Id$

"""
AudioTest test driver which invokes the AudioTest (VKAT) binary to
perform the actual audio tests.

The generated test set archive on the guest will be downloaded by TXS
to the host for later audio comparison / verification.
"""

__copyright__ = \
"""
Copyright (C) 2021 Oracle Corporation

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
import subprocess
import time
import uuid

# Validation Kit imports.
from testdriver import reporter
from testdriver import base
from testdriver import vbox
from testdriver import vboxcon
from testdriver import vboxtestvms

# pylint: disable=unnecessary-semicolon

class tdAudioTest(vbox.TestDriver):

    def __init__(self):
        vbox.TestDriver.__init__(self);

    def showUsage(self):
        return vbox.TestDriver.showUsage(self);

    def actionConfig(self):
        return True

    def actionExecute(self):
        if self.sVMname is None:
            return self.oTestVmSet.actionExecute(self, self.testOneVmConfig);
        return self.actionExecuteOnRunnigVM();

    def testOneVmConfig(self, oVM, oTestVm):

        fRc = False;

        self.logVmInfo(oVM);

        fSkip = False;
        if  oTestVm.isWindows() \
        and oTestVm.sKind in ('WindowsNT4', 'Windows2000'): # Too old for DirectSound and WASAPI backends.
            fSkip = True;
        elif oTestVm.isLinux():
            pass;
        else: # Implement others first.
            fSkip = True;

        if not fSkip:
            reporter.testStart('Waiting for TXS');
            sPathAutoTestExe = '${CDROM}/vboxvalidationkit/${OS/ARCH}/vkat${EXESUFF}';
            oSession, oTxsSession = self.startVmAndConnectToTxsViaTcp(oTestVm.sVmName,
                                                                    fCdWait = True,
                                                                    cMsTimeout = 3 * 60 * 1000,
                                                                    sFileCdWait = self.sFileCdWait);
            reporter.testDone();
            if oSession is not None and oTxsSession is not None:
                self.addTask(oTxsSession);

                sPathTemp      = self.getGuestTempDir(oTestVm);
                sPathAudioOut  = oTestVm.pathJoin(sPathTemp, 'vkat-out');
                sPathAudioTemp = oTestVm.pathJoin(sPathTemp, 'vkat-temp');
                reporter.log("Audio test temp path is '%s'" % (sPathAudioOut));
                reporter.log("Audio test output path is '%s'" % (sPathAudioTemp));
                sTag           = uuid.uuid4();
                reporter.log("Audio test tag is %s'" % (sTag));

                reporter.testStart('Running vkat (Validation Kit Audio Test)');
                fRc = self.txsRunTest(oTxsSession, 'vkat', 5 * 60 * 1000,
                                      self.getGuestSystemShell(oTestVm),
                                      (self.getGuestSystemShell(oTestVm),
                                       sPathAutoTestExe, '-vvv', 'test', '--tag ' + sTag,
                                       '--tempdir ' + sPathAudioTemp, '--outdir ' + sPathAudioOut));
                reporter.testDone()

                if fRc:
                    sFileAudioTestArchive = oTestVm.pathJoin(sPathTemp, 'vkat-%s.tar.gz' % (sTag));
                    fRc = self.txsDownloadFiles(oSession, oTxsSession,
                                                [( sFileAudioTestArchive ), ], fIgnoreErrors = False);
                    if fRc:
                        ## @todo Add "AudioTest verify" here.
                        pass;

            if oSession is not None:
                self.removeTask(oTxsSession);
                self.terminateVmBySession(oSession);

            return fRc;

        reporter.log('Audio testing skipped, not implemented/available for that OS yet.');
        return True;

    #
    # Test execution helpers.
    #
    def testOneCfg(self, oVM, oTestVm):
        """
        Runs the specified VM thru the tests.

        Returns a success indicator on the general test execution. This is not
        the actual test result.
        """

        self.logVmInfo(oVM);

        fRc = True;
        oSession, oTxsSession = self.startVmAndConnectToTxsViaTcp(oTestVm.sVmName, fCdWait = False);
        reporter.log("TxsSession: %s" % (oTxsSession,));
        if oSession is not None:
            self.addTask(oTxsSession);

            fRc, oTxsSession = self.aoSubTstDrvs[0].testIt(oTestVm, oSession, oTxsSession);

            # Cleanup.
            self.removeTask(oTxsSession);
            if not self.aoSubTstDrvs[0].oDebug.fNoExit:
                self.terminateVmBySession(oSession);
        else:
            fRc = False;
        return fRc;

    def onExit(self, iRc):
        if self.aoSubTstDrvs[0].oDebug.fNoExit:
            return True
        return vbox.TestDriver.onExit(self, iRc);

if __name__ == '__main__':
    sys.exit(tdAudioTest().main(sys.argv))
