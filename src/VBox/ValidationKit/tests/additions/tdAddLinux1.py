#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$

"""
VirtualBox Validation Kit - Linux Additions install tests.
"""

__copyright__ = \
"""
Copyright (C) 2010-2017 Oracle Corporation

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
from testdriver import vbox
from testdriver import base
from testdriver import reporter
from testdriver import vboxcon


class tdGuestOsInstLinux(vbox.TestDriver):
    """
    Linux unattended installation with Additions.

    Scenario:
        - Create new VM that corresponds specified installation ISO image
        - Create HDD that corresponds to OS type that will be installed
        - Wait for the installed Additions to write their message to the
          machine log file.
    """

    def __init__(self):
        """
        Reinitialize child class instance.
        """
        vbox.TestDriver.__init__(self)

        self.sVmName             = 'TestVM'
        self.sHddName            = 'TestHdd.vdi'
        self.sIso                = None
        self.sFloppy             = None
        self.sIsoPathBase        = os.path.join(self.sResourcePath, '5.3', 'isos')
        self.fEnableIOAPIC       = True
        self.cCpus               = 1
        self.fEnableNestedPaging = True
        self.fEnablePAE          = False
        self.asExtraData         = []
        self.oUnattended         = None

    #
    # Overridden methods.
    #

    def showUsage(self):
        """
        Extend usage info
        """
        rc = vbox.TestDriver.showUsage(self)
        reporter.log('  --install-iso <ISO file name>')
        reporter.log('  --cpus <# CPUs>')
        reporter.log('  --no-ioapic')
        reporter.log('  --no-nested-paging')
        reporter.log('  --pae')
        reporter.log('  --set-extradata <key>:value')
        reporter.log('      Set VM extra data. This command line option might be used multiple times.')
        return rc

    def parseOption(self, asArgs, iArg):
        """
        Extend standard options set
        """
        if asArgs[iArg] == '--install-iso':
            iArg += 1
            if iArg >= len(asArgs): raise base.InvalidOption('The "--install-iso" option requires an argument')
            self.sIso = asArgs[iArg]
        elif asArgs[iArg] == '--cpus':
            iArg += 1
            if iArg >= len(asArgs): raise base.InvalidOption('The "--cpus" option requires an argument')
            self.cCpus = int(asArgs[iArg])
        elif asArgs[iArg] == '--no-ioapic':
            self.fEnableIOAPIC = False
        elif asArgs[iArg] == '--no-nested-paging':
            self.fEnableNestedPaging = False
        elif asArgs[iArg] == '--pae':
            self.fEnablePAE = True
        elif asArgs[iArg] == '--extra-mem':
            self.fEnablePAE = True
        elif asArgs[iArg] == '--set-extradata':
            iArg = self.requireMoreArgs(1, asArgs, iArg)
            self.asExtraData.append(asArgs[iArg])
        else:
            return vbox.TestDriver.parseOption(self, asArgs, iArg)

        return iArg + 1

    def actionConfig(self):
        """
        Configure pre-conditions.
        """

        if not self.importVBoxApi():
            return False

        assert self.sIso is not None

        # Create unattended object
        oUnattended = self.oVBox.createUnattendedInstaller()
        self.sIso = os.path.join(self.sIsoPathBase, self.sIso)
        assert os.path.isfile(self.sIso)
        oUnattended.isoPath = self.sIso
        oUnattended.user = "vbox"
        oUnattended.password = "password"
        oUnattended.installGuestAdditions = True
        try:
            oUnattended.detectIsoOS()
        except:
            pass
        sOSTypeId  = oUnattended.detectedOSTypeId
        sOSVersion = oUnattended.detectedOSVersion
        oGuestOSType = self.oVBox.getGuestOSType(sOSTypeId)
        assert oGuestOSType.familyId == "Linux"

        # Create VM itself
        eNic0AttachType = vboxcon.NetworkAttachmentType_NAT

        oVM = self.createTestVM(self.sVmName, 1, sKind = oGuestOSType.id, sDvdImage = None, cCpus = self.cCpus,
                                eNic0AttachType = eNic0AttachType, sNic0MacAddr = None)
        assert oVM is not None

        oSession = self.openSession(oVM)

        # Create HDD
        sHddPath = os.path.join(self.sScratchPath, self.sHddName)
        fRc = True
        if sOSTypeId == "RedHat" and sOSVersion.startswith("3."):
            sController = 'IDE Controller'
            fRc = fRc and oSession.setStorageControllerPortCount(sController, 1)
        else:
            sController = 'SATA Controller'
        fRc = oSession.setStorageControllerType(vboxcon.StorageControllerType_PIIX4, sController)

        fRc = fRc and oSession.createAndAttachHd(sHddPath, cb = 12 * 1024 * 1024 * 1024,
                                                 sController = sController, iPort = 0, fImmutable=False)

        # Enable HW virt
        fRc = fRc and oSession.enableVirtEx(True)

        # Enable I/O APIC
        fRc = fRc and oSession.enableIoApic(self.fEnableIOAPIC)

        # Enable Nested Paging
        fRc = fRc and oSession.enableNestedPaging(self.fEnableNestedPaging)

        # Enable PAE
        fRc = fRc and oSession.enablePae(self.fEnablePAE)

        # Remote desktop
        oSession.setupVrdp(True)

        # Set extra data
        if self.asExtraData != []:
            for sExtraData in self.asExtraData:
                try:
                    sKey, sValue = sExtraData.split(':')
                except ValueError:
                    raise base.InvalidOption('Invalid extradata specified: %s' % sExtraData)
                reporter.log('Set extradata: %s => %s' % (sKey, sValue))
                fRc = fRc and oSession.setExtraData(sKey, sValue)

        fRc = fRc and oSession.saveSettings()
        fRc = oSession.close()
        assert fRc is True

        # Set the machine on the unattended object
        oUnattended.machine = oVM

        # Additional set-up
        oUnattended.prepare()
        oUnattended.constructMedia()
        oUnattended.reconfigureVM()
        self.oUnattended = oUnattended

        return vbox.TestDriver.actionConfig(self)

    def actionExecute(self):
        """
        Execute the testcase itself.
        """
        if not self.importVBoxApi():
            return False
        return self.testDoInstallGuestOs()

    #
    # Test execution helpers.
    #

    def testDoInstallGuestOs(self):
        """
        Install guest OS and wait for result
        """
        reporter.testStart('Installing %s' % (os.path.basename(self.sIso),))

        cMsTimeout = 40*60000;
        if not reporter.isLocal(): ## @todo need to figure a better way of handling timeouts on the testboxes ...
            cMsTimeout = 180 * 60000; # will be adjusted down.

        oSession, _ = self.startVmAndConnectToTxsViaTcp(self.sVmName, fCdWait = False, cMsTimeout = cMsTimeout)
        if oSession is not None:
            # Wait until guest reported success
            reporter.log('Guest reported success')
            reporter.testDone()
            fRc = self.terminateVmBySession(oSession)
            return fRc is True
        reporter.error('Installation of %s has failed' % (self.sIso,))
        reporter.testDone()
        return False

if __name__ == '__main__':
    sys.exit(tdGuestOsInstLinux().main(sys.argv));

