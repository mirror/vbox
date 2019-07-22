#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$

"""
VirtualBox Validation Kit - Guest OS unattended installation tests.
"""

__copyright__ = \
"""
Copyright (C) 2010-2019 Oracle Corporation

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
import copy;
import os;
import sys;


# Only the main script needs to modify the path.
try:    __file__
except: __file__ = sys.argv[0]
g_ksValidationKitDir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.append(g_ksValidationKitDir)

# Validation Kit imports.
from testdriver import vbox;
from testdriver import base;
from testdriver import reporter;
from testdriver import vboxcon;
from testdriver import vboxtestvms;

# Sub-test driver imports.
sys.path.append(os.path.join(g_ksValidationKitDir, 'tests', 'additions'));
from tdAddGuestCtrl      import SubTstDrvAddGuestCtrl;
from tdAddSharedFolders1 import SubTstDrvAddSharedFolders1;


class UnattendedVm(vboxtestvms.BaseTestVm):
    """ Unattended Installation test VM. """

    ## @name VM option flags (OR together).
    ## @{
    kfUbuntuAvx2Crash       = 0x0001; ##< Disables AVX2 as ubuntu 16.04 think it means AVX512 is available and compiz crashes.
    kfNoGAs                 = 0x0002; ##< No guest additions installation possible.
    kfIdeIrqDelay           = 0x1000;
    kfUbuntuNewAmdBug       = 0x2000;
    kfNoWin81Paravirt       = 0x4000;
    ## @}

    ## kfUbuntuAvx2Crash: Extra data that disables AVX2.
    kasUbuntuAvx2Crash = [ '/CPUM/IsaExts/AVX2:0', ];

    ## IRQ delay extra data config for win2k VMs.
    kasIdeIrqDelay     = [ 'VBoxInternal/Devices/piix3ide/0/Config/IRQDelay:1', ];

    def __init__(self, oSet, sVmName, sKind, sInstallIso, fFlags = 0):
        vboxtestvms.BaseTestVm.__init__(self, sVmName, oSet = oSet, sKind = sKind,
                                        fRandomPvPModeCrap = (fFlags & self.kfNoWin81Paravirt) == 0);
        self.sInstallIso            = sInstallIso;
        self.fInstVmFlags           = fFlags;

        # Adjustments over the defaults.
        self.iOptRamAdjust          = 0;
        self.fOptIoApic             = None;
        self.fOptPae                = None;
        self.fOptInstallAdditions   = False;
        self.asOptExtraData         = [];
        if fFlags & self.kfUbuntuAvx2Crash:
            self.asOptExtraData    += self.kasUbuntuAvx2Crash;
        if fFlags & self.kfIdeIrqDelay:
            self.asOptExtraData    += self.kasIdeIrqDelay;

    def _unattendedConfigure(self, oIUnattended, oTestDrv): # type: (Any, vbox.TestDriver) -> bool
        """
        Configures the unattended install object.

        The ISO attribute has been set and detectIsoOS has been done, the rest of the
        setup is done here.

        Returns True on success, False w/ errors logged on failure.
        """

        #
        # Make it install the TXS.
        #
        try:    oIUnattended.installTestExecService = True;
        except: return reporter.errorXcpt();
        try:    oIUnattended.validationKitIsoPath   = oTestDrv.sVBoxValidationKitIso;
        except: return reporter.errorXcpt();
        oTestDrv.processPendingEvents();

        #
        # Install GAs?
        #
        if self.fOptInstallAdditions:
            if (self.fInstVmFlags & self.kfNoGAs) == 0:
                try:    oIUnattended.installGuestAdditions = True;
                except: return reporter.errorXcpt();
                try:    oIUnattended.additionsIsoPath      = oTestDrv.getGuestAdditionsIso();
                except: return reporter.errorXcpt();
                oTestDrv.processPendingEvents();
            else:
                reporter.log("Warning! Ignoring request to install Guest Additions as kfNoGAs is set!");

        return True;

    def _unattendedDoIt(self, oIUnattended, oVM, oTestDrv): # type: (Any, Any, vbox.TestDriver) -> bool
        """
        Does the unattended installation preparing, media construction and VM reconfiguration.

        Returns True on success, False w/ errors logged on failure.
        """

        # Associate oVM with the installer:
        try:
            oIUnattended.machine = oVM;
        except:
            return reporter.errorXcpt();
        oTestDrv.processPendingEvents();

        # Prepare and log it:
        try:
            oIUnattended.prepare();
        except:
            return reporter.errorXcpt("IUnattended.prepare failed");
        oTestDrv.processPendingEvents();

        reporter.log('IUnattended attributes after prepare():');
        self._unattendedLogIt(oIUnattended, oTestDrv);

        # Create media:
        try:
            oIUnattended.constructMedia();
        except:
            return reporter.errorXcpt("IUnattended.constructMedia failed");
        oTestDrv.processPendingEvents();

        # Reconfigure the VM:
        try:
            oIUnattended.reconfigureVM();
        except:
            return reporter.errorXcpt("IUnattended.reconfigureVM failed");
        oTestDrv.processPendingEvents();

        return True;

    def _unattendedLogIt(self, oIUnattended, oTestDrv):
        """
        Logs the attributes of the unattended installation object.
        """
        fRc = True;
        asAttribs = ( 'isoPath', 'user', 'password', 'fullUserName', 'productKey', 'additionsIsoPath', 'installGuestAdditions',
                      'validationKitIsoPath', 'installTestExecService', 'timeZone', 'locale', 'language', 'country', 'proxy',
                      'packageSelectionAdjustments', 'hostname', 'auxiliaryBasePath', 'imageIndex', 'machine',
                      'scriptTemplatePath', 'postInstallScriptTemplatePath', 'postInstallCommand',
                      'extraInstallKernelParameters', 'detectedOSTypeId', 'detectedOSVersion', 'detectedOSLanguages',
                      'detectedOSFlavor', 'detectedOSHints', );
        for sAttrib in asAttribs:
            try:
                oValue = getattr(oIUnattended, sAttrib);
            except:
                fRc = reporter.errorXcpt('sAttrib=%s' % sAttrib);
            else:
                reporter.log('%s: %s' % (sAttrib.rjust(32), oValue,));
                oTestDrv.processPendingEvents();
        return fRc;


    #
    # Overriden methods.
    #

    def getResourceSet(self):
        if not os.path.isabs(self.sInstallIso):
            return [self.sInstallIso,];
        return [];

    def _createVmDoIt(self, oTestDrv, eNic0AttachType, sDvdImage):
        #
        # Use HostOnly networking for ubuntu and debian VMs to prevent them from
        # downloading updates and doing database updates during installation.
        # We want predicable results.
        #
        if     eNic0AttachType is None \
          and self.isLinux() \
           and (   'ubuntu' in self.sKind.lower()
                or 'debian' in self.sKind.lower() ):
            eNic0AttachType = vboxcon.NetworkAttachmentType_HostOnly;

        return vboxtestvms.BaseTestVm._createVmDoIt(self, oTestDrv, eNic0AttachType, sDvdImage);


    def _createVmPost(self, oTestDrv, oVM, eNic0AttachType, sDvdImage):
        #
        # Adjust the ram, I/O APIC and stuff.
        #

        oSession = oTestDrv.openSession(oVM);
        if oSession is None:
            return None;

        fRc = True;

        ## Set proper boot order - IUnattended::reconfigureVM does this, doesn't it?
        #fRc = fRc and oSession.setBootOrder(1, vboxcon.DeviceType_HardDisk)
        #fRc = fRc and oSession.setBootOrder(2, vboxcon.DeviceType_DVD)

        # Adjust memory if requested.
        if self.iOptRamAdjust != 0:
            try:    cMbRam = oSession.o.machine.memorySize;
            except: fRc    = reporter.errorXcpt();
            else:
                fRc = oSession.setRamSize(cMbRam + self.iOptRamAdjust) and fRc;

        # I/O APIC:
        if self.fOptIoApic is not None:
            fRc = oSession.enableIoApic(self.fOptIoApic) and fRc;

        # I/O APIC:
        if self.fOptPae is not None:
            fRc = oSession.enablePae(self.fOptPae) and fRc;

        # Set extra data
        for sExtraData in self.asOptExtraData:
            sKey, sValue = sExtraData.split(':');
            reporter.log('Set extradata: %s => %s' % (sKey, sValue))
            fRc = oSession.setExtraData(sKey, sValue) and fRc;

        # Save the settings.
        fRc = fRc and oSession.saveSettings()
        fRc = oSession.close() and fRc;

        return oVM if fRc else None;

    def _skipVmTest(self, oTestDrv, oVM):
        _ = oVM;
        #
        # Check for ubuntu installer vs. AMD host CPU.
        #
        if self.fInstVmFlags & self.kfUbuntuNewAmdBug:
            if self.isHostCpuAffectedByUbuntuNewAmdBug(oTestDrv):
                return True;

        return vboxtestvms.BaseTestVm._skipVmTest(self, oTestDrv, oVM);

    def getReconfiguredVm(self, oTestDrv, cCpus, sVirtMode, sParavirtMode = None):
        #
        # Do the standard reconfig in the base class first, it'll figure out
        # if we can run the VM as requested.
        #
        (fRc, oVM) = vboxtestvms.BaseTestVm.getReconfiguredVm(self, oTestDrv, cCpus, sVirtMode, sParavirtMode);
        if fRc is True:
            #
            # Make sure there is no HD from the previous run attached nor taking
            # up storage on the host.
            #
            fRc = self.recreateRecommendedHdd(oVM, oTestDrv);
            if fRc is True:
                #
                # Set up unattended installation.
                #
                try:
                    oIUnattended = oTestDrv.oVBox.createUnattendedInstaller();
                except:
                    fRc = reporter.errorXcpt();
                if fRc is True:
                    fRc = self.unattendedDetectOs(oIUnattended, oTestDrv);
                    if fRc is True:
                        fRc = self._unattendedConfigure(oIUnattended, oTestDrv);
                        if fRc is True:
                            fRc = self._unattendedDoIt(oIUnattended, oVM, oTestDrv);

        # Done.
        return (fRc, oVM)

    def isLoggedOntoDesktop(self):
        #
        # Normally all unattended installations should end up on the desktop.
        # An exception is a minimal install, but we currently don't support that.
        #
        return True;

    def getTestUser(self):
        # Default unattended installation user (parent knowns its password).
        return 'vboxuser';


    #
    # Our methods.
    #

    def unattendedDetectOs(self, oIUnattended, oTestDrv): # type: (Any, vbox.TestDriver) -> bool
        """
        Does the detectIsoOS operation and checks that the detect OSTypeId matches.

        Returns True on success, False w/ errors logged on failure.
        """

        #
        # Point the installer at the ISO and do the detection.
        #
        sInstallIso = self.sInstallIso
        if not os.path.isabs(sInstallIso):
            sInstallIso = os.path.join(oTestDrv.sResourcePath, sInstallIso);

        try:
            oIUnattended.isoPath = sInstallIso;
        except:
            return reporter.errorXcpt('sInstallIso=%s' % (sInstallIso,));

        try:
            oIUnattended.detectIsoOS();
        except:
            if oTestDrv.oVBoxMgr.xcptIsNotEqual(None, oTestDrv.oVBoxMgr.statuses.E_NOTIMPL):
                return reporter.errorXcpt('sInstallIso=%s' % (sInstallIso,));

        #
        # Get and log the result.
        #
        # Note! Current (6.0.97) fails with E_NOTIMPL even if it does some work.
        #
        try:
            sDetectedOSTypeId    = oIUnattended.detectedOSTypeId;
            sDetectedOSVersion   = oIUnattended.detectedOSVersion;
            sDetectedOSFlavor    = oIUnattended.detectedOSFlavor;
            sDetectedOSLanguages = oIUnattended.detectedOSLanguages;
            sDetectedOSHints     = oIUnattended.detectedOSHints;
        except:
            return reporter.errorXcpt('sInstallIso=%s' % (sInstallIso,));

        reporter.log('detectIsoOS result for "%s" (vm %s):' % (sInstallIso, self.sVmName));
        reporter.log('       DetectedOSTypeId: %s' % (sDetectedOSTypeId,));
        reporter.log('      DetectedOSVersion: %s' % (sDetectedOSVersion,));
        reporter.log('       DetectedOSFlavor: %s' % (sDetectedOSFlavor,));
        reporter.log('    DetectedOSLanguages: %s' % (sDetectedOSLanguages,));
        reporter.log('        DetectedOSHints: %s' % (sDetectedOSHints,));

        #
        # Check if the OS type matches.
        #
        if self.sKind != sDetectedOSTypeId:
            return reporter.error('sInstallIso=%s: DetectedOSTypeId is %s, expected %s'
                                  % (sInstallIso, sDetectedOSTypeId, self.sKind));

        return True;


class tdGuestOsInstTest1(vbox.TestDriver):
    """
    Unattended Guest OS installation tests using IUnattended.

    Scenario:
        - Create a new VM with default settings using IMachine::applyDefaults.
        - Setup unattended installation using IUnattended.
        - Start the VM and do the installation.
        - Wait for TXS to report for service.
        - If installing GAs:
            - Wait for GAs to report operational runlevel.
        - Save & restore state.
        - If installing GAs:
            - Test guest properties (todo).
            - Test guest controls.
            - Test shared folders.
    """


    def __init__(self):
        """
        Reinitialize child class instance.
        """
        vbox.TestDriver.__init__(self)
        self.fLegacyOptions = False;
        assert self.fEnableVrdp; # in parent driver.

        #
        # Our install test VM set.
        #
        oSet = vboxtestvms.TestVmSet(self.oTestVmManager, fIgnoreSkippedVm = True);
        oSet.aoTestVms.extend([
            # Windows7 RTM:
            UnattendedVm(oSet, 'tst-w7-32', 'Windows7',     '6.0/uaisos/en_windows_7_enterprise_x86_dvd_x15-70745.iso'), # ~6GiB
            UnattendedVm(oSet, 'tst-w7-64', 'Windows7_64',  '6.0/uaisos/en_windows_7_enterprise_x64_dvd_x15-70749.iso'), # ~10GiB
            ## @todo 15.10 fails with grub install error.
            #UnattendedVm(oSet, 'tst-ubuntu-15.10-64', 'Ubuntu_64', '6.0/uaisos/ubuntu-15.10-desktop-amd64.iso'),
            UnattendedVm(oSet, 'tst-ubuntu-16.04-64', 'Ubuntu_64', '6.0/uaisos/ubuntu-16.04-desktop-amd64.iso',      # ~5GiB
                         UnattendedVm.kfUbuntuAvx2Crash),
            UnattendedVm(oSet, 'tst-ubuntu-16.04-32', 'Ubuntu',    '6.0/uaisos/ubuntu-16.04-desktop-i386.iso'),      # ~4.5GiB
            UnattendedVm(oSet, 'tst-ubuntu-16.04.1-64', 'Ubuntu_64', '6.0/uaisos/ubuntu-16.04.1-desktop-amd64.iso'), # ~5GiB
            UnattendedVm(oSet, 'tst-ubuntu-16.04.1-32', 'Ubuntu',    '6.0/uaisos/ubuntu-16.04.1-desktop-i386.iso'),  # ~4.5GiB
            UnattendedVm(oSet, 'tst-ubuntu-16.04.2-64', 'Ubuntu_64', '6.0/uaisos/ubuntu-16.04.2-desktop-amd64.iso'), # ~5GiB
            UnattendedVm(oSet, 'tst-ubuntu-16.04.2-32', 'Ubuntu',    '6.0/uaisos/ubuntu-16.04.2-desktop-i386.iso'),  # ~4.5GiB
            UnattendedVm(oSet, 'tst-ubuntu-16.04.3-64', 'Ubuntu_64', '6.0/uaisos/ubuntu-16.04.3-desktop-amd64.iso'), # ~5GiB
            UnattendedVm(oSet, 'tst-ubuntu-16.04.3-32', 'Ubuntu',    '6.0/uaisos/ubuntu-16.04.3-desktop-i386.iso'),  # ~4.5GiB
            UnattendedVm(oSet, 'tst-ubuntu-16.04.4-64', 'Ubuntu_64', '6.0/uaisos/ubuntu-16.04.4-desktop-amd64.iso'), # ~5GiB
            UnattendedVm(oSet, 'tst-ubuntu-16.04.4-32', 'Ubuntu',    '6.0/uaisos/ubuntu-16.04.4-desktop-i386.iso'),  # ~4.5GiB
            UnattendedVm(oSet, 'tst-ubuntu-16.04.5-64', 'Ubuntu_64', '6.0/uaisos/ubuntu-16.04.5-desktop-amd64.iso'), # ~5GiB
            UnattendedVm(oSet, 'tst-ubuntu-16.04.5-32', 'Ubuntu',    '6.0/uaisos/ubuntu-16.04.5-desktop-i386.iso'),  # ~4.5GiB
            UnattendedVm(oSet, 'tst-ubuntu-16.04.6-64', 'Ubuntu_64', '6.0/uaisos/ubuntu-16.04.6-desktop-amd64.iso'), # ~5GiB
            UnattendedVm(oSet, 'tst-ubuntu-16.04.6-32', 'Ubuntu',    '6.0/uaisos/ubuntu-16.04.6-desktop-i386.iso'),  # ~4.5GiB
            UnattendedVm(oSet, 'tst-ubuntu-16.10-64', 'Ubuntu_64', '6.0/uaisos/ubuntu-16.10-desktop-amd64.iso'),     # ~5.5GiB
            ## @todo 16.10-32 doesn't ask for an IP, so it always fails.
            #UnattendedVm(oSet, 'tst-ubuntu-16.10-32', 'Ubuntu',    '6.0/uaisos/ubuntu-16.10-desktop-i386.iso'),      # ~5.5GiB?
            UnattendedVm(oSet, 'tst-ubuntu-17.04-64', 'Ubuntu_64', '6.0/uaisos/ubuntu-17.04-desktop-amd64.iso'),     # ~5GiB
            UnattendedVm(oSet, 'tst-ubuntu-17.04-32', 'Ubuntu',    '6.0/uaisos/ubuntu-17.04-desktop-i386.iso'),      # ~4.5GiB
            ## @todo ubuntu 17.10, 18.04 & 18.10 do not work.  They misses all the the build tools (make, gcc, perl, ++)
            ##       and has signed kmods:
            UnattendedVm(oSet, 'tst-ubuntu-17.10-64', 'Ubuntu_64', '6.0/uaisos/ubuntu-17.10-desktop-amd64.iso', # >4Gib
                         UnattendedVm.kfNoGAs),
            UnattendedVm(oSet, 'tst-ubuntu-18.04-64', 'Ubuntu_64', '6.0/uaisos/ubuntu-18.04-desktop-amd64.iso', # >6GiB
                         UnattendedVm.kfNoGAs),
            # 18.10 hangs reading install DVD during "starting partitioner..."
            #UnattendedVm(oSet, 'tst-ubuntu-18.10-64', 'Ubuntu_64', '6.0/uaisos/ubuntu-18.10-desktop-amd64.iso',
            #             UnattendedVm.kfNoGAs),
            UnattendedVm(oSet, 'tst-ubuntu-19.04-64', 'Ubuntu_64', '6.0/uaisos/ubuntu-19.04-desktop-amd64.iso', # >6GiB
                         UnattendedVm.kfNoGAs),
        ]);
        self.oTestVmSet = oSet;

        # For option parsing:
        self.aoSelectedVms = oSet.aoTestVms # type: list(UnattendedVm)

        # Number of VMs to test in parallel:
        self.cInParallel = 1;

        # Whether to do the save-and-restore test.
        self.fTestSaveAndRestore = True;

        #
        # Sub-test drivers.
        #
        self.addSubTestDriver(SubTstDrvAddSharedFolders1(self));
        self.addSubTestDriver(SubTstDrvAddGuestCtrl(self));


    #
    # Overridden methods.
    #

    def showUsage(self):
        """
        Extend usage info
        """
        rc = vbox.TestDriver.showUsage(self)
        reporter.log('');
        reporter.log('tdGuestOsUnattendedInst1 options:');
        reporter.log('  --parallel <num>');
        reporter.log('      Number of VMs to test in parallel.');
        reporter.log('      Default: 1');
        reporter.log('');
        reporter.log('  Options for working on selected test VMs:');
        reporter.log('  --select <vm1[:vm2[:..]]>');
        reporter.log('      Selects a test VM for the following configuration alterations.');
        reporter.log('      Default: All possible test VMs');
        reporter.log('  --copy <old-vm>=<new-vm>');
        reporter.log('      Creates and selects <new-vm> as a copy of <old-vm>.');
        reporter.log('  --guest-type <guest-os-type>');
        reporter.log('      Sets the guest-os type of the currently selected test VM.');
        reporter.log('  --install-iso <ISO file name>');
        reporter.log('      Sets ISO image to use for the selected test VM.');
        reporter.log('  --ram-adjust <MBs>');
        reporter.log('      Adjust the VM ram size by the given delta.  Both negative and positive');
        reporter.log('      values are accepted.');
        reporter.log('  --max-cpus <# CPUs>');
        reporter.log('      Sets the maximum number of guest CPUs for the selected VM.');
        reporter.log('  --set-extradata <key>:value');
        reporter.log('      Set VM extra data for the selected VM. Can be repeated.');
        reporter.log('  --ioapic, --no-ioapic');
        reporter.log('      Enable or disable the I/O apic for the selected VM.');
        reporter.log('  --pae, --no-pae');
        reporter.log('      Enable or disable PAE support (32-bit guests only) for the selected VM.');
        return rc

    def parseOption(self, asArgs, iArg):
        """
        Extend standard options set
        """

        if asArgs[iArg] == '--parallel':
            iArg = self.requireMoreArgs(1, asArgs, iArg);
            self.cInParallel = int(asArgs[iArg]);
            if self.cInParallel <= 0:
                self.cInParallel = 1;
        elif asArgs[iArg] == '--select':
            iArg = self.requireMoreArgs(1, asArgs, iArg);
            self.aoSelectedVms = [];
            for sTestVm in asArgs[iArg].split(':'):
                oTestVm = self.oTestVmSet.findTestVmByName(sTestVm);
                if not oTestVm:
                    raise base.InvalidOption('Unknown test VM: %s'  % (sTestVm,));
                self.aoSelectedVms.append(oTestVm);
        elif asArgs[iArg] == '--copy':
            iArg = self.requireMoreArgs(1, asArgs, iArg);
            asNames = asArgs[iArg].split('=');
            if len(asNames) != 2 or not asNames[0] or not asNames[1]:
                raise base.InvalidOption('The --copy option expects value on the form "old=new": %s'  % (asArgs[iArg],));
            oOldTestVm = self.oTestVmSet.findTestVmByName(asNames[0]);
            if not oOldTestVm:
                raise base.InvalidOption('Unknown test VM: %s'  % (asNames[0],));
            oNewTestVm = copy.deepcopy(oOldTestVm);
            oNewTestVm.sVmName = asNames[1];
            self.oTestVmSet.aoTestVms.append(oNewTestVm);
            self.aoSelectedVms = [oNewTestVm];
        elif asArgs[iArg] == '--guest-type':
            iArg = self.requireMoreArgs(1, asArgs, iArg);
            for oTestVm in self.aoSelectedVms:
                oTestVm.sKind = asArgs[iArg];
        elif asArgs[iArg] == '--install-iso':
            iArg = self.requireMoreArgs(1, asArgs, iArg);
            for oTestVm in self.aoSelectedVms:
                oTestVm.sInstallIso = asArgs[iArg];
        elif asArgs[iArg] == '--ram-adjust':
            iArg = self.requireMoreArgs(1, asArgs, iArg);
            for oTestVm in self.aoSelectedVms:
                oTestVm.iOptRamAdjust = int(asArgs[iArg]);
        elif asArgs[iArg] == '--max-cpus':
            iArg = self.requireMoreArgs(1, asArgs, iArg);
            for oTestVm in self.aoSelectedVms:
                oTestVm.iOptMaxCpus = int(asArgs[iArg]);
        elif asArgs[iArg] == '--set-extradata':
            iArg = self.requireMoreArgs(1, asArgs, iArg)
            sExtraData = asArgs[iArg];
            try:     _, _ = sExtraData.split(':');
            except: raise base.InvalidOption('Invalid extradata specified: %s' % (sExtraData, ));
            for oTestVm in self.aoSelectedVms:
                oTestVm.asOptExtraData.append(sExtraData);
        elif asArgs[iArg] == '--ioapic':
            for oTestVm in self.aoSelectedVms:
                oTestVm.fOptIoApic = True;
        elif asArgs[iArg] == '--no-ioapic':
            for oTestVm in self.aoSelectedVms:
                oTestVm.fOptIoApic = False;
        elif asArgs[iArg] == '--pae':
            for oTestVm in self.aoSelectedVms:
                oTestVm.fOptPae = True;
        elif asArgs[iArg] == '--no-pae':
            for oTestVm in self.aoSelectedVms:
                oTestVm.fOptPae = False;
        elif asArgs[iArg] == '--install-additions':
            for oTestVm in self.aoSelectedVms:
                oTestVm.fOptInstallAdditions = True;
        elif asArgs[iArg] == '--no-install-additions':
            for oTestVm in self.aoSelectedVms:
                oTestVm.fOptInstallAdditions = False;
        else:
            return vbox.TestDriver.parseOption(self, asArgs, iArg);
        return iArg + 1;

    def actionConfig(self):
        if not self.importVBoxApi(): # So we can use the constant below.
            return False;
        return self.oTestVmSet.actionConfig(self);

    def actionExecute(self):
        """
        Execute the testcase.
        """
        return self.oTestVmSet.actionExecute(self, self.testOneVmConfig)

    def testOneVmConfig(self, oVM, oTestVm): # type: (Any, UnattendedVm) -> bool
        """
        Install guest OS and wait for result
        """

        self.logVmInfo(oVM)
        reporter.testStart('Installing %s%s' % (oTestVm.sVmName, ' with GAs' if oTestVm.fOptInstallAdditions else ''))

        cMsTimeout = 40*60000;
        if not reporter.isLocal(): ## @todo need to figure a better way of handling timeouts on the testboxes ...
            cMsTimeout = 180 * 60000; # will be adjusted down.

        oSession, oTxsSession = self.startVmAndConnectToTxsViaTcp(oTestVm.sVmName, fCdWait = False, cMsTimeout = cMsTimeout);
        #oSession = self.startVmByName(oTestVm.sVmName); # (for quickly testing waitForGAs)
        if oSession is not None:
            # The guest has connected to TXS.
            reporter.log('Guest reported success via TXS.');
            reporter.testDone();

            fRc = True;
            # Kudge: GAs doesn't come up correctly, so we have to reboot the guest first:
            #        Looks like VBoxService isn't there.
            if oTestVm.fOptInstallAdditions:
                reporter.testStart('Rebooting');
                fRc, oTxsSession = self.txsRebootAndReconnectViaTcp(oSession, oTxsSession);
                reporter.testDone();

            # If we're installing GAs, wait for them to come online:
            if oTestVm.fOptInstallAdditions and fRc is True:
                reporter.testStart('Guest additions');
                aenmRunLevels = [vboxcon.AdditionsRunLevelType_Userland,];
                if oTestVm.isLoggedOntoDesktop():
                    aenmRunLevels.append(vboxcon.AdditionsRunLevelType_Desktop);
                fRc = self.waitForGAs(oSession, cMsTimeout = cMsTimeout / 2, aenmWaitForRunLevels = aenmRunLevels,
                                      aenmWaitForActive = (vboxcon.AdditionsFacilityType_VBoxGuestDriver,
                                                           vboxcon.AdditionsFacilityType_VBoxService,));
                reporter.testDone();

            # Now do a save & restore test:
            if fRc is True and self.fTestSaveAndRestore:
                fRc, oSession, oTxsSession = self.testSaveAndRestore(oSession, oTxsSession, oTestVm);

            # Test GAs if requested:
            if oTestVm.fOptInstallAdditions and fRc is True:
                for oSubTstDrv in self.aoSubTstDrvs:
                    if oSubTstDrv.fEnabled:
                        reporter.testStart(oSubTstDrv.sTestName);
                        fRc2, oTxsSession = oSubTstDrv.testIt(oTestVm, oSession, oTxsSession);
                        reporter.testDone(fRc2 is None);
                        if fRc2 is False:
                            fRc = False;

            if oSession is not None:
                fRc = self.terminateVmBySession(oSession) and fRc;
            return fRc is True

        reporter.error('Installation of %s has failed' % (oTestVm.sVmName,))
        #oTestVm.detatchAndDeleteHd(self); # Save space.
        reporter.testDone()
        return False

    def testSaveAndRestore(self, oSession, oTxsSession, oTestVm):
        """
        Tests saving and restoring the VM.
        """
        _ = oTestVm;
        reporter.testStart('Save');
        ## @todo
        reporter.testDone();
        reporter.testStart('Restore');
        ## @todo
        reporter.testDone();
        return (True, oSession, oTxsSession);

if __name__ == '__main__':
    sys.exit(tdGuestOsInstTest1().main(sys.argv))

