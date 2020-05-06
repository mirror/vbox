#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$

"""
VirtualBox Validation Kit - Additions Basics #1.
"""

__copyright__ = \
"""
Copyright (C) 2010-2020 Oracle Corporation

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
import os;
import sys;
import uuid;

# Only the main script needs to modify the path.
try:    __file__
except: __file__ = sys.argv[0];
g_ksValidationKitDir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))));
sys.path.append(g_ksValidationKitDir);

# Validation Kit imports.
from testdriver import reporter;
from testdriver import base;
from testdriver import vbox;
from testdriver import vboxcon;

# Sub-test driver imports.
sys.path.append(os.path.dirname(os.path.abspath(__file__))); # For sub-test drivers.
from tdAddGuestCtrl import SubTstDrvAddGuestCtrl;
from tdAddSharedFolders1 import SubTstDrvAddSharedFolders1;



class tdAddBasicConsoleCallbacks(vbox.ConsoleEventHandlerBase):
    """
    For catching the Guest Additions change state events.
    """
    def __init__(self, dArgs):
        oTstDrv  = dArgs['oTstDrv'];
        oVBoxMgr = dArgs['oVBoxMgr']; _ = oVBoxMgr;
        oGuest   = dArgs['oGuest'];

        vbox.ConsoleEventHandlerBase.__init__(self, dArgs, 'tdAddBasic1');
        self.oTstDrv  = oTstDrv;
        self.oGuest   = oGuest;

    def handleEvent(self, oEvt):
        try:
            oEvtBase = self.oVBoxMgr.queryInterface(oEvt, 'IEvent');
            eType = oEvtBase.type;
        except:
            reporter.logXcpt();
            return None;
        if eType == vboxcon.VBoxEventType_OnAdditionsStateChanged:
            return self.onAdditionsStateChanged();
        return None;

    def onAdditionsStateChanged(self):
        reporter.log('onAdditionsStateChange');
        self.oTstDrv.fGAStatusCallbackFired = True;
        self.oTstDrv.iGAStatusCallbackRunlevel = self.oGuest.additionsRunLevel;
        self.oVBoxMgr.interruptWaitEvents();
        return None;

class tdAddBasic1(vbox.TestDriver):                                         # pylint: disable=too-many-instance-attributes
    """
    Additions Basics #1.
    """
    ## @todo
    # - More of the settings stuff can be and need to be generalized!
    #

    def __init__(self):
        vbox.TestDriver.__init__(self);
        self.oTestVmSet  = self.oTestVmManager.getSmokeVmSet('nat');
        self.asTestsDef  = ['install', 'guestprops', 'stdguestprops', 'guestcontrol', 'sharedfolders'];
        self.asTests     = self.asTestsDef;
        self.asRsrcs     = None
        # The file we're going to use as a beacon to wait if the Guest Additions CD-ROM is ready.
        self.sFileCdWait = '';

        self.addSubTestDriver(SubTstDrvAddGuestCtrl(self));
        self.addSubTestDriver(SubTstDrvAddSharedFolders1(self));

        self.fGAStatusCallbackFired    = False;
        self.iGAStatusCallbackRunlevel = 0;

    #
    # Overridden methods.
    #
    def showUsage(self):
        rc = vbox.TestDriver.showUsage(self);
        reporter.log('');
        reporter.log('tdAddBasic1 Options:');
        reporter.log('  --tests        <s1[:s2[:]]>');
        reporter.log('      Default: %s  (all)' % (':'.join(self.asTestsDef)));
        reporter.log('  --quick');
        reporter.log('      Same as --virt-modes hwvirt --cpu-counts 1.');
        return rc;

    def parseOption(self, asArgs, iArg):                                  # pylint: disable=too-many-branches,too-many-statements
        if asArgs[iArg] == '--tests':
            iArg += 1;
            if iArg >= len(asArgs): raise base.InvalidOption('The "--tests" takes a colon separated list of tests');
            self.asTests = asArgs[iArg].split(':');
            for s in self.asTests:
                if s not in self.asTestsDef:
                    raise base.InvalidOption('The "--tests" value "%s" is not valid; valid values are: %s'
                                             % (s, ' '.join(self.asTestsDef),));

        elif asArgs[iArg] == '--quick':
            self.parseOption(['--virt-modes', 'hwvirt'], 0);
            self.parseOption(['--cpu-counts', '1'], 0);

        else:
            return vbox.TestDriver.parseOption(self, asArgs, iArg);
        return iArg + 1;

    def getResourceSet(self):
        if self.asRsrcs is None:
            self.asRsrcs = []
            for oSubTstDrv in self.aoSubTstDrvs:
                self.asRsrcs.extend(oSubTstDrv.asRsrcs)
            self.asRsrcs.extend(self.oTestVmSet.getResourceSet())
        return self.asRsrcs

    def actionConfig(self):
        if not self.importVBoxApi(): # So we can use the constant below.
            return False;

        eNic0AttachType = vboxcon.NetworkAttachmentType_NAT;
        sGaIso = self.getGuestAdditionsIso();

        # On 6.0 we merge the GAs with the ValidationKit so we can get at FsPerf.
        # Note! Not possible to do a dboule import as both images an '/OS2' dir.
        #       So, using same dir as with unattended VISOs for the valkit.
        if self.fpApiVer >= 6.0 and 'sharedfolders' in self.asTests:
            sGaViso = os.path.join(self.sScratchPath, 'AdditionsAndValKit.viso');
            ## @todo encode as bash cmd line:
            sVisoContent = '--iprt-iso-maker-file-marker-bourne-sh %s ' \
                           '--import-iso \'%s\' ' \
                           '--push-iso \'%s\' ' \
                           '/vboxvalidationkit=/ ' \
                           '--pop ' \
                          % (uuid.uuid4(), sGaIso, self.sVBoxValidationKitIso);
            reporter.log2('Using VISO combining GAs and ValKit "%s": %s' % (sGaViso, sVisoContent));
            oGaViso = open(sGaViso, 'w');
            oGaViso.write(sVisoContent);
            oGaViso.close();
            sGaIso = sGaViso;

        return self.oTestVmSet.actionConfig(self, eNic0AttachType = eNic0AttachType, sDvdImage = sGaIso);

    def actionExecute(self):
        return self.oTestVmSet.actionExecute(self, self.testOneCfg);


    #
    # Test execution helpers.
    #

    def testOneCfg(self, oVM, oTestVm):
        """
        Runs the specified VM thru the tests.

        Returns a success indicator on the general test execution. This is not
        the actual test result.
        """
        fRc = False;

        if oTestVm.isWindows():
            self.sFileCdWait = 'VBoxWindowsAdditions.exe';
        elif oTestVm.isLinux():
            self.sFileCdWait = 'VBoxLinuxAdditions.run';

        self.logVmInfo(oVM);
        oSession, oTxsSession = self.startVmAndConnectToTxsViaTcp(oTestVm.sVmName, fCdWait = True,
                                                                  cMsCdWait = 5 * 60 * 1000,
                                                                  sFileCdWait = self.sFileCdWait);
        if oSession is not None:
            self.addTask(oTxsSession);
            # Do the testing.
            fSkip = 'install' not in self.asTests;
            reporter.testStart('Install');
            if not fSkip:
                fRc, oTxsSession = self.testInstallAdditions(oSession, oTxsSession, oTestVm);
            reporter.testDone(fSkip);

            if  not fSkip \
            and not fRc:
                reporter.log('Skipping following tests as Guest Additions were not installed successfully');
            else:
                fSkip = 'guestprops' not in self.asTests;
                reporter.testStart('Guest Properties');
                if not fSkip:
                    fRc = self.testGuestProperties(oSession, oTxsSession, oTestVm) and fRc;
                reporter.testDone(fSkip);

                fSkip = 'guestcontrol' not in self.asTests;
                reporter.testStart('Guest Control');
                if not fSkip:
                    fRc, oTxsSession = self.aoSubTstDrvs[0].testIt(oTestVm, oSession, oTxsSession);
                reporter.testDone(fSkip);

                fSkip = 'sharedfolders' not in self.asTests and self.fpApiVer >= 6.0;
                reporter.testStart('Shared Folders');
                if not fSkip:
                    fRc, oTxsSession = self.aoSubTstDrvs[1].testIt(oTestVm, oSession, oTxsSession);
                reporter.testDone(fSkip or fRc is None);

            ## @todo Save and restore test.

            ## @todo Reset tests.

            ## @todo Final test: Uninstallation.

            # Cleanup.
            self.removeTask(oTxsSession);
            self.terminateVmBySession(oSession)
        return fRc;

    def waitForGuestAdditionsRunLevel(self, oSession, oGuest, cMsTimeout, iRunLevel):
        """
        Waits for the Guest Additions to reach a specific run level.

        Returns success status.
        """
        # No need to wait as we already reached the run level?
        if iRunLevel == oGuest.additionsRunLevel:
            reporter.log('Already reached run level %s' % iRunLevel);
            return True;

        reporter.log('Waiting for Guest Additions to reach run level %s ...' % iRunLevel);

        oConsoleCallbacks = oSession.registerDerivedEventHandler(tdAddBasicConsoleCallbacks, \
                                                                 {'oTstDrv':self, 'oGuest':oGuest, });
        fRc = False;
        if oConsoleCallbacks is not None:
            # Wait for 5 minutes max.
            tsStart = base.timestampMilli();
            while base.timestampMilli() - tsStart < cMsTimeout:
                oTask = self.waitForTasks(1000);
                if oTask is not None:
                    break;
                if self.fGAStatusCallbackFired:
                    reporter.log('Reached new run level %s' % iRunLevel);
                    if iRunLevel == self.iGAStatusCallbackRunlevel:
                        fRc = True;
                        break;
                    self.fGAStatusCallbackFired = False;
            if not fRc:
                reporter.testFailure('Guest Additions status did not change to required level');

            # cleanup.
            oConsoleCallbacks.unregister();

        reporter.log('Waiting for Guest Additions to reach run level %s ended with %s' % (iRunLevel, fRc));
        return fRc;

    def testInstallAdditions(self, oSession, oTxsSession, oTestVm):
        """
        Tests installing the guest additions
        """
        if oTestVm.isWindows():
            (fRc, oTxsSession) = self.testWindowsInstallAdditions(oSession, oTxsSession, oTestVm);
        elif oTestVm.isLinux():
            (fRc, oTxsSession) = self.testLinuxInstallAdditions(oSession, oTxsSession, oTestVm);
        else:
            reporter.error('Guest Additions installation not implemented for %s yet! (%s)' %
                           (oTestVm.sKind, oTestVm.sVmName,));
            fRc = False;

        #
        # Verify installation of Guest Additions using commmon bits.
        #
        if fRc:
            #
            # Check if the additions are operational.
            #
            try:    oGuest = oSession.o.console.guest;
            except:
                reporter.errorXcpt('Getting IGuest failed.');
                return (False, oTxsSession);

            #
            # Wait for the GAs to come up.
            #
            fRc = self.waitForGuestAdditionsRunLevel(oSession, oGuest, 5 * 60 * 1000, vboxcon.AdditionsRunLevelType_Userland);
            if not fRc:
                return (False, oTxsSession);

            # Check the additionsVersion attribute. It must not be empty.
            reporter.testStart('IGuest::additionsVersion');
            fRc = self.testIGuest_additionsVersion(oGuest);
            reporter.testDone();

            reporter.testStart('IGuest::additionsRunLevel');
            self.testIGuest_additionsRunLevel(oGuest, oTestVm);
            reporter.testDone();

            ## @todo test IAdditionsFacilities.

        return (fRc, oTxsSession);

    def testWindowsInstallAdditions(self, oSession, oTxsSession, oTestVm):
        """
        Installs the Windows guest additions using the test execution service.
        Since this involves rebooting the guest, we will have to create a new TXS session.
        """

        sUser     = oTestVm.getTestUser();
        ## @todo Do we need to get/use the test user's password as well here somehow?

        #
        # Install the public signing key.
        #
        if oTestVm.sKind not in ('WindowsNT4', 'Windows2000', 'WindowsXP', 'Windows2003'):
            fRc = self.txsRunTest(oTxsSession, 'VBoxCertUtil.exe', 1 * 60 * 1000, '${CDROM}/cert/VBoxCertUtil.exe',
                                  ('${CDROM}/cert/VBoxCertUtil.exe', 'add-trusted-publisher', '${CDROM}/cert/vbox-sha1.cer'),
                                  sAsUser = sUser, fCheckSessionStatus = True);
            if not fRc:
                reporter.error('Error installing SHA1 certificate');
            else:
                fRc = self.txsRunTest(oTxsSession, 'VBoxCertUtil.exe', 1 * 60 * 1000, '${CDROM}/cert/VBoxCertUtil.exe',
                                      ('${CDROM}/cert/VBoxCertUtil.exe', 'add-trusted-publisher',
                                       '${CDROM}/cert/vbox-sha256.cer'),
                                      sAsUser = sUser, fCheckSessionStatus = True);
                if not fRc:
                    reporter.error('Error installing SHA256 certificate');

        #
        # Delete relevant log files.
        #
        # Note! On some guests the files in question still can be locked by the OS, so ignore
        #       deletion errors from the guest side (e.g. sharing violations) and just continue.
        #
        asLogFiles = [];
        fHaveSetupApiDevLog = False;
        if oTestVm.sKind in ('WindowsNT4',):
            sWinDir = 'C:/WinNT/';
        else:
            sWinDir = 'C:/Windows/';
            asLogFiles = [sWinDir + 'setupapi.log', sWinDir + 'setupact.log', sWinDir + 'setuperr.log'];

            # Apply The SetupAPI logging level so that we also get the (most verbose) setupapi.dev.log file.
            ## @todo !!! HACK ALERT !!! Add the value directly into the testing source image. Later.
            fHaveSetupApiDevLog = self.txsRunTest(oTxsSession, 'Enabling setupapi.dev.log', 30 * 1000,
                                                  'c:\\Windows\\System32\\reg.exe',
                                                  ('c:\\Windows\\System32\\reg.exe', 'add',
                                                   '"HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Setup"',
                                                   '/v', 'LogLevel', '/t', 'REG_DWORD', '/d', '0xFF'),
                                                   sAsUser = sUser, fCheckSessionStatus = True);

        for sFile in asLogFiles:
            self.txsRmFile(oSession, oTxsSession, sFile, 10 * 1000, fIgnoreErrors = True);

        #
        # The actual install.
        # Enable installing the optional auto-logon modules (VBoxGINA/VBoxCredProv).
        # Also tell the installer to produce the appropriate log files.
        #
        fRc = self.txsRunTest(oTxsSession, 'VBoxWindowsAdditions.exe', 5 * 60 * 1000, '${CDROM}/VBoxWindowsAdditions.exe',
                              ('${CDROM}/VBoxWindowsAdditions.exe', '/S', '/l', '/with_autologon'),
                              sAsUser = sUser, fCheckSessionStatus = True);

        #
        # Reboot the VM and reconnect the TXS session.
        #
        if not fRc \
        or oTxsSession.getResult() is False:
            reporter.error('Error installing Windows Guest Additions (installer returned with exit code)')
        else:
            (fRc, oTxsSession) = self.txsRebootAndReconnectViaTcp(oSession, oTxsSession, cMsTimeout = 15 * 60 * 1000,
                                                                  cMsCdWait = 15 * 60 * 1000);
            if fRc is True:
                # Add the Windows Guest Additions installer files to the files we want to download
                # from the guest.
                sGuestAddsDir = 'C:/Program Files/Oracle/VirtualBox Guest Additions/';
                asLogFiles.append(sGuestAddsDir + 'install.log');
                # Note: There won't be a install_ui.log because of the silent installation.
                asLogFiles.append(sGuestAddsDir + 'install_drivers.log');
                asLogFiles.append('C:/Windows/setupapi.log');

                # Note: setupapi.dev.log only is available since Windows 2000.
                if fHaveSetupApiDevLog:
                    asLogFiles.append('C:/Windows/setupapi.dev.log');

                #
                # Download log files.
                # Ignore errors as all files above might not be present (or in different locations)
                # on different Windows guests.
                #
                self.txsDownloadFiles(oSession, oTxsSession, asLogFiles, fIgnoreErrors = True);

        return (fRc, oTxsSession);

    def getAdditionsInstallerResult(self, oTxsSession):
        """
        Extracts the Guest Additions installer exit code from a run before.
        Assumes that nothing else has been run on the same TXS session in the meantime.
        """
        iRc = 0;
        (_, sOpcode, abPayload) = oTxsSession.getLastReply();
        if sOpcode.startswith('PROC NOK '): # Extract process rc
            iRc = abPayload[0]; # ASSUMES 8-bit rc for now.
        ## @todo Parse more statuses here.
        return iRc;

    def testLinuxInstallAdditions(self, oSession, oTxsSession, oTestVm):
        _ = oSession;
        _ = oTestVm;

        fRc = False;

        #
        # The actual install.
        # Also tell the installer to produce the appropriate log files.
        #
        # Make sure to add "--nox11" to the makeself wrapper in order to not getting any blocking
        # xterm window spawned.
        fRc = self.txsRunTest(oTxsSession, 'VBoxLinuxAdditions.run', 30 * 60 * 1000,
                              '/bin/sh', ('/bin/sh', '${CDROM}/VBoxLinuxAdditions.run', '--nox11'));
        if not fRc:
            iRc = self.getAdditionsInstallerResult(oTxsSession);
            # Check for rc == 0 just for completeness.
            if iRc in (0, 2): # Can happen if the GA installer has detected older VBox kernel modules running and needs a reboot.
                reporter.log('Guest has old(er) VBox kernel modules still running; requires a reboot');
                fRc = True;

            if not fRc:
                reporter.error('Installing Linux Additions failed (isSuccess=%s, lastReply=%s, see log file for details)'
                               % (oTxsSession.isSuccess(), oTxsSession.getLastReply()));

        #
        # Download log files.
        # Ignore errors as all files above might not be present for whatever reason.
        #
        asLogFile = [];
        asLogFile.append('/var/log/vboxadd-install.log');
        self.txsDownloadFiles(oSession, oTxsSession, asLogFile, fIgnoreErrors = True);

        # Do the final reboot to get the just installed Guest Additions up and running.
        if fRc:
            reporter.testStart('Rebooting guest w/ updated Guest Additions active');
            (fRc, oTxsSession) = self.txsRebootAndReconnectViaTcp(oSession, oTxsSession, cMsTimeout = 15 * 60 * 1000,
                                                                  cMsCdWait = 15 * 60 * 1000);
            if fRc:
                pass
            else:
                reporter.testFailure('Rebooting and reconnecting to TXS service failed');
            reporter.testDone();

        return (fRc, oTxsSession);

    def testIGuest_additionsVersion(self, oGuest):
        """
        Returns False if no version string could be obtained, otherwise True
        even though errors are logged.
        """
        try:
            sVer = oGuest.additionsVersion;
        except:
            reporter.errorXcpt('Getting the additions version failed.');
            return False;
        reporter.log('IGuest::additionsVersion="%s"' % (sVer,));

        if sVer.strip() == '':
            reporter.error('IGuest::additionsVersion is empty.');
            return False;

        if sVer != sVer.strip():
            reporter.error('IGuest::additionsVersion is contains spaces: "%s".' % (sVer,));

        asBits = sVer.split('.');
        if len(asBits) < 3:
            reporter.error('IGuest::additionsVersion does not contain at least tree dot separated fields: "%s" (%d).'
                           % (sVer, len(asBits)));

        ## @todo verify the format.
        return True;

    def testIGuest_additionsRunLevel(self, oGuest, oTestVm):
        """
        Do run level tests.
        """
        if oTestVm.isLoggedOntoDesktop():
            eExpectedRunLevel = vboxcon.AdditionsRunLevelType_Desktop;
        else:
            eExpectedRunLevel = vboxcon.AdditionsRunLevelType_Userland;

        ## @todo Insert wait for the desired run level.
        try:
            iLevel = oGuest.additionsRunLevel;
        except:
            reporter.errorXcpt('Getting the additions run level failed.');
            return False;
        reporter.log('IGuest::additionsRunLevel=%s' % (iLevel,));

        if iLevel != eExpectedRunLevel:
            pass; ## @todo We really need that wait!!
            #reporter.error('Expected runlevel %d, found %d instead' % (eExpectedRunLevel, iLevel));
        return True;


    def testGuestProperties(self, oSession, oTxsSession, oTestVm):
        """
        Test guest properties.
        """
        _ = oSession; _ = oTxsSession; _ = oTestVm;
        return True;

if __name__ == '__main__':
    sys.exit(tdAddBasic1().main(sys.argv));
