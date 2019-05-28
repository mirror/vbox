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
import os
import sys


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


class UnattendedVm(vboxtestvms.TestVm):
    """ Unattended Installation test VM. """

    ## @name VM option flags (OR together).
    ## @{
    kfIdeIrqDelay           = 0x1;
    kfUbuntuNewAmdBug       = 0x2;
    kfNoWin81Paravirt       = 0x4;
    ## @}

    ## IRQ delay extra data config for win2k VMs.
    kasIdeIrqDelay   = [ 'VBoxInternal/Devices/piix3ide/0/Config/IRQDelay:1', ];

    ## Install ISO paths relative to the testrsrc root.
    kasIosPathBases  = [ os.path.join('6.0', 'isos'), os.path.join('6.1', 'isos'), ];

    def __init__(self, oSet, sVmName, sKind, sInstallIso, fFlags = 0):
        vboxtestvms.TestVm.__init__(self, sVmName, oSet = oSet, sKind = sKind,
                                    fRandomPvPMode = False, sFirmwareType = None, sChipsetType = None,
                                    sHddControllerType = None, sDvdControllerType = None);
        self.sInstallIso    = sInstallIso;
        self.fInstVmFlags   = fFlags;

        # Adjustments over the defaults.
        self.iOptRamAdjust  = 0;
        self.fOptIoApic     = None;
        self.fOptPae        = None;
        self.asExtraData    = [];
        if fFlags & self.kfIdeIrqDelay:
            self.asOptExtraData = self.kasIdeIrqDelay;

    ## @todo split TestVm.
    def createVmInner(self, oTestDrv, eNic0AttachType, sDvdImage):
        """ Overloaded from TestVm to create using defaults rather than a set complicated config properties. """


    def detatchAndDeleteHd(self, oTestDrv):
        """
        Detaches and deletes the HD.
        Returns success indicator, error info logged.
        """
        fRc = False;
        oVM = oTestDrv.getVmByName(self.sVmName);
        if oVM is not None:
            oSession = oTestDrv.openSession(oVM);
            if oSession is not None:
                (fRc, oHd) = oSession.detachHd(self.sHddControllerType, iPort = 0, iDevice = 0);
                if fRc is True and oHd is not None:
                    fRc = oSession.saveSettings();
                    fRc = fRc and oTestDrv.oVBox.deleteHdByMedium(oHd);
                    fRc = fRc and oSession.saveSettings(); # Necessary for media reg?
                fRc = oSession.close() and fRc;
        return fRc;

    def getReconfiguredVm(self, oTestDrv, cCpus, sVirtMode, sParavirtMode = None):
        #
        # Do the standard reconfig in the base class first, it'll figure out
        # if we can run the VM as requested.
        #
        (fRc, oVM) = vboxtestvms.TestVm.getReconfiguredVm(self, oTestDrv, cCpus, sVirtMode, sParavirtMode);

        #
        # Make sure there is no HD from the previous run attached nor taking
        # up storage on the host.
        #
        if fRc is True:
            fRc = self.detatchAndDeleteHd(oTestDrv);

        #
        # Check for ubuntu installer vs. AMD host CPU.
        #
        if fRc is True and (self.fInstVmFlags & self.kfUbuntuNewAmdBug):
            if self.isHostCpuAffectedByUbuntuNewAmdBug(oTestDrv):
                return (None, None); # (skip)

        #
        # Make adjustments to the default config, and adding a fresh HD.
        #
        if fRc is True:
            oSession = oTestDrv.openSession(oVM);
            if oSession is not None:
                if self.sHddControllerType == self.ksSataController:
                    fRc = fRc and oSession.setStorageControllerType(vboxcon.StorageControllerType_IntelAhci,
                                                                    self.sHddControllerType);
                    fRc = fRc and oSession.setStorageControllerPortCount(self.sHddControllerType, 1);
                elif self.sHddControllerType == self.ksScsiController:
                    fRc = fRc and oSession.setStorageControllerType(vboxcon.StorageControllerType_LsiLogic,
                                                                    self.sHddControllerType);
                try:
                    sHddPath = os.path.join(os.path.dirname(oVM.settingsFilePath),
                                            '%s-%s-%s.vdi' % (self.sVmName, sVirtMode, cCpus,));
                except:
                    reporter.errorXcpt();
                    sHddPath = None;
                    fRc = False;

                fRc = fRc and oSession.createAndAttachHd(sHddPath,
                                                         cb = self.cGbHdd * 1024*1024*1024,
                                                         sController = self.sHddControllerType,
                                                         iPort = 0,
                                                         fImmutable = False);

                # Set proper boot order
                fRc = fRc and oSession.setBootOrder(1, vboxcon.DeviceType_HardDisk)
                fRc = fRc and oSession.setBootOrder(2, vboxcon.DeviceType_DVD)

                # Adjust memory if requested.
                if self.iOptRamAdjust != 0:
                    fRc = fRc and oSession.setRamSize(oSession.o.machine.memorySize + self.iOptRamAdjust);

                # Set extra data
                for sExtraData in self.asOptExtraData:
                    try:
                        sKey, sValue = sExtraData.split(':')
                    except ValueError:
                        raise base.InvalidOption('Invalid extradata specified: %s' % sExtraData)
                    reporter.log('Set extradata: %s => %s' % (sKey, sValue))
                    fRc = fRc and oSession.setExtraData(sKey, sValue)

                # Other variations?

                # Save the settings.
                fRc = fRc and oSession.saveSettings()
                fRc = oSession.close() and fRc;
            else:
                fRc = False;
            if fRc is not True:
                oVM = None;

        # Done.
        return (fRc, oVM)




class tdGuestOsInstTest1(vbox.TestDriver):
    """
    Guest OS installation tests.

    Scenario:
        - Create new VM that corresponds specified installation ISO image.
        - Create HDD that corresponds to OS type that will be installed.
        - Boot VM from ISO image (i.e. install guest OS).
        - Wait for incomming TCP connection (guest should initiate such a
          connection in case installation has been completed successfully).
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
            # pylint: disable=C0301
            UnattendedVm(oSet, 'tst-w7-32', 'Windows7', 'en_windows_7_enterprise_x86_dvd_x15-70745.iso'),
            # pylint: enable=C0301
        ]);
        self.oTestVmSet = oSet;

        # For option parsing:
        self.aoSelectedVms = oSet.aoTestVms # type: list(UnattendedVm)

        # Number of VMs to test in parallel:
        self.cInParallel = 1;

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
            if len(asNames) != 2 or len(asNames[0]) == 0 or len(asNames[1]) == 0:
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
            for oTestVm in self.aoSelectedVms:
                oTestVm.asOptExtraData.append(asArgs[iArg]);
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
        else:
            return vbox.TestDriver.parseOption(self, asArgs, iArg);
        return iArg + 1;

    def actionConfig(self):
        if not self.importVBoxApi(): # So we can use the constant below.
            return False;
        return self.oTestVmSet.actionConfig(self, eNic0AttachType = vboxcon.NetworkAttachmentType_NAT);

    def actionExecute(self):
        """
        Execute the testcase.
        """
        return self.oTestVmSet.actionExecute(self, self.testOneVmConfig)

    def testOneVmConfig(self, oVM, oTestVm):
        """
        Install guest OS and wait for result
        """

        self.logVmInfo(oVM)
        reporter.testStart('Installing %s' % (oTestVm.sVmName,))

        cMsTimeout = 40*60000;
        if not reporter.isLocal(): ## @todo need to figure a better way of handling timeouts on the testboxes ...
            cMsTimeout = 180 * 60000; # will be adjusted down.

        oSession, _ = self.startVmAndConnectToTxsViaTcp(oTestVm.sVmName, fCdWait = False, cMsTimeout = cMsTimeout);
        if oSession is not None:
            # The guest has connected to TXS, so we're done (for now anyways).
            reporter.log('Guest reported success')
            ## @todo Do save + restore.

            reporter.testDone()
            fRc = self.terminateVmBySession(oSession)
            return fRc is True

        reporter.error('Installation of %s has failed' % (oTestVm.sVmName,))
        oTestVm.detatchAndDeleteHd(self); # Save space.
        reporter.testDone()
        return False

if __name__ == '__main__':
    sys.exit(tdGuestOsInstTest1().main(sys.argv))

