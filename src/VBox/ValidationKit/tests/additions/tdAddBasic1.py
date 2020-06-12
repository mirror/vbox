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
if sys.version_info[0] >= 3:
    from io       import StringIO as StringIO;      # pylint: disable=import-error,no-name-in-module,useless-import-alias
else:
    from StringIO import StringIO as StringIO;      # pylint: disable=import-error,no-name-in-module,useless-import-alias

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
        # Path pointing to the Guest Additions on the (V)ISO file.
        self.sPathGaISO  = '';

        self.addSubTestDriver(SubTstDrvAddGuestCtrl(self));
        self.addSubTestDriver(SubTstDrvAddSharedFolders1(self));

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
        #
        # Note1: Not possible to do a double import as both images an '/OS2' dir.
        #        So, using same dir as with unattended VISOs for the valkit.
        #
        # Note2: We need to make sure that we don't change the location of the
        #        ValidationKit bits of the combined VISO, as this will break TXS' (TestExecService)
        #        automatic updating mechanism (uses hardcoded paths, e.g. "{CDROM}/linux/amd64/TestExecService").
        #
        ## @todo Find a solution for testing the automatic Guest Additions updates, which also looks at {CDROM}s root.
        if self.fpApiVer >= 6.0 and 'sharedfolders' in self.asTests:
            sGaViso = os.path.join(self.sScratchPath, 'AdditionsAndValKit.viso');
            ## @todo encode as bash cmd line:
            sVisoContent = '--iprt-iso-maker-file-marker-bourne-sh %s ' \
                           '--import-iso \'%s\' ' \
                           '--push-iso \'%s\' ' \
                           '/vboxadditions=/ ' \
                           '--pop ' \
                          % (uuid.uuid4(), self.sVBoxValidationKitIso, sGaIso);
            reporter.log2('Using VISO combining ValKit and GAs "%s": %s' % (sVisoContent, sGaViso));
            oGaViso = open(sGaViso, 'w');
            oGaViso.write(sVisoContent);
            oGaViso.close();
            sGaIso = sGaViso;

            self.sPathGaISO = '${CDROM}/vboxadditions';
        else:
            self.sPathGaISO = '${CDROM}';

        reporter.log2('Path to Guest Additions on ISO is "%s"' % self.sPathGaISO);

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
        # HACK ALERT! HORRIBLE MESS THAT SHOULDN'T BE HERE!
        aasLogFiles = [ ];
        if oTestVm.isLinux():
            reporter.testStart('Enabling udev logging ...');
            oSession, oTxsSession = self.startVmAndConnectToTxsViaTcp(oTestVm.sVmName, fCdWait = False);
            reporter.testDone();
            if oTxsSession:
                oTxsSession.syncExec("sed", ("sed", "-i", "'s/.*udev_log.*/udev_log=\"debug\"/'", "/etc/udev/udev.conf"),
                                     fIgnoreErrors = True);

                sUDevMonitorLog = '/tmp/udev_monitor.log';
                aasLogFiles.append((sUDevMonitorLog, 'guest-udev_monitor-%s.log' % (oTestVm.sVmName,),));

                reporter.testStart('Enabling udev monitoring ...');
                sUdevSvc = StringIO();
                sUdevSvc.write('[Unit]\n');
                sUdevSvc.write('Description=udev Monitoring\n');
                sUdevSvc.write('DefaultDependencies=no\n');
                sUdevSvc.write('Wants=systemd-udevd.service\n');
                sUdevSvc.write('After=systemd-udevd-control.socket systemd-udevd-kernel.socket\n');
                sUdevSvc.write('Before=sysinit.target systemd-udev-trigger.service\n');
                sUdevSvc.write('[Service]\n');
                sUdevSvc.write('Type=simple\n');
                sUdevSvc.write('ExecStart=/usr/bin/sh -c "/usr/sbin/udevadm monitor --udev --env > ' + sUDevMonitorLog + '\n');
                sUdevSvc.write('[Install]\n');
                sUdevSvc.write('WantedBy=sysinit.target');
                oTxsSession.syncUploadString(sUdevSvc.getvalue(), '/etc/systemd/system/systemd-udev-monitor.service', 0o644,
                                             fIgnoreErrors = True);
                oTxsSession.syncExec("systemctl", ("systemctl", "enable", "systemd-udev-monitor.service"), fIgnoreErrors = True);
                reporter.testDone();
        # HACK ALERT - END.

        fRc = False;

        self.logVmInfo(oVM);

        if oTestVm.isWindows():
            self.sFileCdWait = ('%s/VBoxWindowsAdditions.exe' % (self.sPathGaISO,));
        elif oTestVm.isLinux():
            self.sFileCdWait = ('%s/VBoxLinuxAdditions.run' % (self.sPathGaISO,));

        reporter.log2('Waiting for TXS + CD: %s' % (self.sFileCdWait,));

        reporter.testStart('Waiting for TXS + CD');
        if oTestVm.isLinux():
            fRc, oTxsSession = self.txsRebootAndReconnectViaTcp(oSession, oTxsSession, fCdWait = True,
                                                                cMsCdWait = 5 * 60 * 1000,
                                                                sFileCdWait = self.sFileCdWait);
        else:
            oSession, oTxsSession = self.startVmAndConnectToTxsViaTcp(oTestVm.sVmName, fCdWait = True,
                                                                      cMsCdWait = 5 * 60 * 1000,
                                                                      sFileCdWait = self.sFileCdWait);
        reporter.testDone();

        # More HACK ALERT stuff.
        if aasLogFiles and oTxsSession:
            self.txsDownloadFiles(oSession, oTxsSession, aasLogFiles, fIgnoreErrors = True);

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

            # Wait for the GAs to come up.
            reporter.testStart('IGuest::additionsRunLevel');
            fRc = self.testIGuest_additionsRunLevel(oSession, oTestVm, oGuest);
            reporter.testDone();

            # Check the additionsVersion attribute. It must not be empty.
            reporter.testStart('IGuest::additionsVersion');
            fRc = self.testIGuest_additionsVersion(oGuest) and fRc;
            reporter.testDone();

            # Check Guest Additions facilities
            reporter.testStart('IGuest::getFacilityStatus');
            fRc = self.testIGuest_getFacilityStatus(oTestVm, oGuest) and fRc;
            reporter.testDone();

            # Do a bit of diagnosis on error.
            if not fRc:
                if oTestVm.isLinux():
                    reporter.log('Boot log:');
                    sCmdJournalCtl = oTestVm.pathJoin(self.getGuestSystemDir(oTestVm), 'journalctl');
                    oTxsSession.syncExec(sCmdJournalCtl, (sCmdJournalCtl, '-b'), fIgnoreErrors = True);
                    reporter.log('Loaded processes:');
                    sCmdPs = oTestVm.pathJoin(self.getGuestSystemDir(oTestVm), 'ps');
                    oTxsSession.syncExec(sCmdPs, (sCmdPs, '-a', '-u', '-x'), fIgnoreErrors = True);
                    reporter.log('Kernel messages:');
                    sCmdDmesg = oTestVm.pathJoin(self.getGuestSystemDir(oTestVm), 'dmesg');
                    oTxsSession.syncExec(sCmdDmesg, (sCmdDmesg), fIgnoreErrors = True);
                    reporter.log('Loaded modules:');
                    sCmdLsMod = oTestVm.pathJoin(self.getGuestSystemAdminDir(oTestVm), 'lsmod');
                    oTxsSession.syncExec(sCmdLsMod, (sCmdLsMod), fIgnoreErrors = True);
                elif oTestVm.isWindows() or oTestVm.isOS2():
                    sShell    = self.getGuestSystemShell(oTestVm);
                    sShellOpt = '/C' if oTestVm.isWindows() or oTestVm.isOS2() else '-c';
                    reporter.log('Loaded processes:');
                    oTxsSession.syncExec(sShell, (sShell, sShellOpt, "tasklist.exe", "/FO", "CSV"), fIgnoreErrors = True);
                    reporter.log('Listing autostart entries:');
                    oTxsSession.syncExec(sShell, (sShell, sShellOpt, "wmic.exe", "startup", "get"), fIgnoreErrors = True);
                    reporter.log('Listing autostart entries:');
                    oTxsSession.syncExec(sShell, (sShell, sShellOpt, "dir",
                                                  oTestVm.pathJoin(self.getGuestSystemDir(oTestVm), 'VBox*')),
                                         fIgnoreErrors = True);
                    reporter.log('Downloading logs ...');
                    self.txsDownloadFiles(oSession, oTxsSession,
                              [ ( self.getGuestVBoxTrayClientLogFile(oTestVm),
                                  'ga-vboxtrayclient-%s.log' % (oTestVm.sVmName,),),
                                ( "C:\\Documents and Settings\\All Users\\Application Data\\Microsoft\\Dr Watson\\drwtsn32.log",
                                  'ga-drwatson-%s.log' % (oTestVm.sVmName,), ),
                              ],
                                fIgnoreErrors = True);

        return (fRc, oTxsSession);

    def getGuestVBoxTrayClientLogFile(self, oTestVm):
        """ Gets the path on the guest for the (release) log file of VBoxTray / VBoxClient. """
        if oTestVm.isWindows():
            return oTestVm.pathJoin(self.getGuestTempDir(oTestVm), 'VBoxTray.log');

        return oTestVm.pathJoin(self.getGuestTempDir(oTestVm), 'VBoxClient.log');

    def setGuestEnvVar(self, oSession, oTxsSession, oTestVm, sName, sValue):
        """ Sets a system-wide environment variable on the guest. Only supports Windows guests so far. """
        _ = oSession;
        if oTestVm.isWindows():
            sPathRegExe   = oTestVm.pathJoin(self.getGuestSystemDir(oTestVm), 'reg.exe');
            self.txsRunTest(oTxsSession, ('Set env var \"%s\"' % (sName,)),
                            30 * 1000, sPathRegExe,
                            (sPathRegExe, 'add',
                             '"HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"', '/v',
                             sName, '/t', 'REG_EXPAND_SZ', '/d', sValue, '/f'));

    def testWindowsInstallAdditions(self, oSession, oTxsSession, oTestVm):
        """
        Installs the Windows guest additions using the test execution service.
        Since this involves rebooting the guest, we will have to create a new TXS session.
        """

        # Set system-wide env vars to enable release logging on some applications.
        self.setGuestEnvVar(oSession, oTxsSession, oTestVm, 'VBOXTRAY_RELEASE_LOG', 'all.e.l.l2.l3.f');
        self.setGuestEnvVar(oSession, oTxsSession, oTestVm, 'VBOXTRAY_RELEASE_LOG_FLAGS', 'time thread group append');
        self.setGuestEnvVar(oSession, oTxsSession, oTestVm, 'VBOXTRAY_RELEASE_LOG_DEST',
                            ('file=%s' % (self.getGuestVBoxTrayClientLogFile(oTestVm),)));

        #
        # Install the public signing key.
        #
        if oTestVm.sKind not in ('WindowsNT4', 'Windows2000', 'WindowsXP', 'Windows2003'):
            fRc = self.txsRunTest(oTxsSession, 'VBoxCertUtil.exe', 1 * 60 * 1000,
                                   '%s/cert/VBoxCertUtil.exe' % self.sPathGaISO,
                                  ('%s/cert/VBoxCertUtil.exe' % self.sPathGaISO, 'add-trusted-publisher',
                                   '%s/cert/vbox-sha1.cer'    % self.sPathGaISO),
                                  fCheckSessionStatus = True);
            if not fRc:
                reporter.error('Error installing SHA1 certificate');
            else:
                fRc = self.txsRunTest(oTxsSession, 'VBoxCertUtil.exe', 1 * 60 * 1000,
                                       '%s/cert/VBoxCertUtil.exe' % self.sPathGaISO,
                                      ('%s/cert/VBoxCertUtil.exe' % self.sPathGaISO, 'add-trusted-publisher',
                                       '%s/cert/vbox-sha256.cer'  % self.sPathGaISO), fCheckSessionStatus = True);
                if not fRc:
                    reporter.error('Error installing SHA256 certificate');

        #
        # Delete relevant log files.
        #
        # Note! On some guests the files in question still can be locked by the OS, so ignore
        #       deletion errors from the guest side (e.g. sharing violations) and just continue.
        #
        sWinDir = self.getGuestWinDir(oTestVm);
        aasLogFiles = [
            ( oTestVm.pathJoin(sWinDir, 'setupapi.log'), 'ga-setupapi-%s.log' % (oTestVm.sVmName,), ),
            ( oTestVm.pathJoin(sWinDir, 'setupact.log'), 'ga-setupact-%s.log' % (oTestVm.sVmName,), ),
            ( oTestVm.pathJoin(sWinDir, 'setuperr.log'), 'ga-setuperr-%s.log' % (oTestVm.sVmName,), ),
        ];

        # Apply The SetupAPI logging level so that we also get the (most verbose) setupapi.dev.log file.
        ## @todo !!! HACK ALERT !!! Add the value directly into the testing source image. Later.
        sRegExe = oTestVm.pathJoin(self.getGuestSystemDir(oTestVm), 'reg.exe');
        fHaveSetupApiDevLog = self.txsRunTest(oTxsSession, 'Enabling setupapi.dev.log', 30 * 1000,
                                              sRegExe,
                                              (sRegExe, 'add',
                                               '"HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Setup"',
                                               '/v', 'LogLevel', '/t', 'REG_DWORD', '/d', '0xFF'),
                                               fCheckSessionStatus = True);

        for sGstFile, _ in aasLogFiles:
            self.txsRmFile(oSession, oTxsSession, sGstFile, 10 * 1000, fIgnoreErrors = True);

        #
        # The actual install.
        # Enable installing the optional auto-logon modules (VBoxGINA/VBoxCredProv).
        # Also tell the installer to produce the appropriate log files.
        #
        fRc = self.txsRunTest(oTxsSession, 'VBoxWindowsAdditions.exe', 5 * 60 * 1000,
                               '%s/VBoxWindowsAdditions.exe' % self.sPathGaISO,
                              ('%s/VBoxWindowsAdditions.exe' % self.sPathGaISO, '/S', '/l', '/with_autologon'),
                              fCheckSessionStatus = True);

        # Add the Windows Guest Additions installer files to the files we want to download
        # from the guest. Note: There won't be a install_ui.log because of the silent installation.
        sGuestAddsDir = 'C:\\Program Files\\Oracle\\VirtualBox Guest Additions\\';
        aasLogFiles.append((sGuestAddsDir + 'install.log',           'ga-install-%s.log' % (oTestVm.sVmName,),));
        aasLogFiles.append((sGuestAddsDir + 'install_drivers.log',   'ga-install_drivers-%s.log' % (oTestVm.sVmName,),));
        aasLogFiles.append(('C:\\Windows\\setupapi.log',             'ga-setupapi-%s.log' % (oTestVm.sVmName,),));

        # Note: setupapi.dev.log only is available since Windows 2000.
        if fHaveSetupApiDevLog:
            aasLogFiles.append(('C:\\Windows\\setupapi.dev.log',     'ga-setupapi.dev-%s.log' % (oTestVm.sVmName,),));

        #
        # Download log files.
        # Ignore errors as all files above might not be present (or in different locations)
        # on different Windows guests.
        #
        self.txsDownloadFiles(oSession, oTxsSession, aasLogFiles, fIgnoreErrors = True);

        #
        # Reboot the VM and reconnect the TXS session.
        #
        if fRc:
            reporter.testStart('Rebooting guest w/ updated Guest Additions active');
            (fRc, oTxsSession) = self.txsRebootAndReconnectViaTcp(oSession, oTxsSession, cMsTimeout = 15 * 60 * 1000);
            if fRc:
                pass;
            else:
                reporter.testFailure('Rebooting and reconnecting to TXS service failed');
            reporter.testDone();
        else:
            reporter.error('Error installing Windows Guest Additions (installer returned with exit code <> 0)')

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
        #
        # The actual install.
        # Also tell the installer to produce the appropriate log files.
        #
        # Make sure to add "--nox11" to the makeself wrapper in order to not getting any blocking
        # xterm window spawned.
        fRc = self.txsRunTest(oTxsSession, 'VBoxLinuxAdditions.run', 30 * 60 * 1000,
                              self.getGuestSystemShell(oTestVm),
                              (self.getGuestSystemShell(oTestVm), '%s/VBoxLinuxAdditions.run' % self.sPathGaISO, '--nox11'));
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
        self.txsDownloadFiles(oSession, oTxsSession,
                              [('/var/log/vboxadd-install.log', 'vboxadd-install-%s.log' % oTestVm.sVmName), ],
                              fIgnoreErrors = True);

        # Do the final reboot to get the just installed Guest Additions up and running.
        if fRc:
            reporter.testStart('Rebooting guest w/ updated Guest Additions active');
            (fRc, oTxsSession) = self.txsRebootAndReconnectViaTcp(oSession, oTxsSession, cMsTimeout = 15 * 60 * 1000);
            if fRc:
                pass
            else:
                reporter.testFailure('Rebooting and reconnecting to TXS service failed');
            reporter.testDone();

        return (fRc, oTxsSession);

    def testIGuest_additionsRunLevel(self, oSession, oTestVm, oGuest):
        """
        Do run level tests.
        """

        _ = oGuest;

        if oTestVm.isWindows():
            if oTestVm.isLoggedOntoDesktop():
                eExpectedRunLevel = vboxcon.AdditionsRunLevelType_Desktop;
            else:
                eExpectedRunLevel = vboxcon.AdditionsRunLevelType_Userland;
        else:
            ## @todo VBoxClient does not have facility statuses implemented yet.
            eExpectedRunLevel = vboxcon.AdditionsRunLevelType_Userland;

        return self.waitForGAs(oSession, aenmWaitForRunLevels = [ eExpectedRunLevel ]);

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

    def checkFacilityStatus(self, oGuest, eFacilityType, sDesc, fMustSucceed = True):
        """
        Prints the current status of a Guest Additions facility.

        Return success status.
        """

        fRc = True;

        try:
            eStatus, tsLastUpdatedMs = oGuest.getFacilityStatus(eFacilityType);
        except:
            if fMustSucceed:
                reporter.errorXcpt('Getting facility status for "%s" failed' % (sDesc,));
                fRc = False;
        else:
            if eStatus == vboxcon.AdditionsFacilityStatus_Inactive:
                sStatus = "INACTIVE";
            elif eStatus == vboxcon.AdditionsFacilityStatus_Paused:
                sStatus = "PAUSED";
            elif eStatus == vboxcon.AdditionsFacilityStatus_PreInit:
                sStatus = "PREINIT";
            elif eStatus == vboxcon.AdditionsFacilityStatus_Init:
                sStatus = "INIT";
            elif eStatus == vboxcon.AdditionsFacilityStatus_Active:
                sStatus = "ACTIVE";
            elif eStatus == vboxcon.AdditionsFacilityStatus_Terminating:
                sStatus = "TERMINATING";
                fRc = not fMustSucceed;
            elif eStatus == vboxcon.AdditionsFacilityStatus_Terminated:
                sStatus = "TERMINATED";
                fRc = not fMustSucceed;
            elif eStatus == vboxcon.AdditionsFacilityStatus_Failed:
                sStatus = "FAILED";
                fRc = not fMustSucceed;
            elif eStatus == vboxcon.AdditionsFacilityStatus_Unknown:
                sStatus = "UNKNOWN";
                fRc = not fMustSucceed;
            else:
                sStatus = "???";
                fRc = not fMustSucceed;

        reporter.log('Guest Additions facility "%s": %s (last updated: %sms)' % (sDesc, sStatus, str(tsLastUpdatedMs)));
        if      fMustSucceed \
        and not fRc:
            reporter.error('Guest Additions facility "%s" did not report expected status (is "%s")' % (sDesc, sStatus));

        return fRc;

    def testIGuest_getFacilityStatus(self, oTestVm, oGuest):
        """
        Checks Guest Additions facilities for their status.

        Returns success status.
        """

        reporter.testStart('Status VBoxGuest Driver');
        fRc = self.checkFacilityStatus(oGuest, vboxcon.AdditionsFacilityType_VBoxGuestDriver, "VBoxGuest Driver");
        reporter.testDone();

        reporter.testStart('Status VBoxService');
        fRc = self.checkFacilityStatus(oGuest, vboxcon.AdditionsFacilityType_VBoxService,     "VBoxService") and fRc;
        reporter.testDone();

        if oTestVm.isWindows():
            if oTestVm.isLoggedOntoDesktop():
                ## @todo VBoxClient does not have facility statuses implemented yet.
                reporter.testStart('Status VBoxTray / VBoxClient');
                fRc = self.checkFacilityStatus(oGuest, vboxcon.AdditionsFacilityType_VBoxTrayClient,
                                               "VBoxTray / VBoxClient") and fRc;
                reporter.testDone();
        ## @todo Add more.

        return fRc;

    def testGuestProperties(self, oSession, oTxsSession, oTestVm):
        """
        Test guest properties.
        """
        _ = oSession; _ = oTxsSession; _ = oTestVm;
        return True;

if __name__ == '__main__':
    sys.exit(tdAddBasic1().main(sys.argv));
