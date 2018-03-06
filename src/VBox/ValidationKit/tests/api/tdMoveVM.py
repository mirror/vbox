#!/usr/bin/env python
# -*- coding: utf-8 -*-
# "$Id$"

"""
VirtualBox Validation Kit - VM Move Test #1
"""

__copyright__ = \
"""
Copyright (C) 2010-2018 Oracle Corporation

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
from testdriver import base
from testdriver import reporter
from testdriver import vboxcon
from testdriver import vboxwrappers
from tdMoveMedium1 import SubTstDrvMoveMedium1

class SubTstDrvMoveVM(base.SubTestDriverBase):
    """
    Sub-test driver for VM Move Test #1.
    """

    def __init__(self, oTstDrv):
        base.SubTestDriverBase.__init__(self, 'move-vm', oTstDrv)

    def testIt(self):
        """
        Execute the sub-testcase.
        """
        return  self.testVMMove()

    #
    # Test execution helpers.
    #

    #
    #createTestMachine
    #
    def createTestMachine(self, oListOfImageNames):
        oVM = self.oTstDrv.createTestVM('test-vm-move', 1, None, 4)
        if oVM is None:
            return None

        # create hard disk images, one for each file-based backend, using the first applicable extension
        fRc = True
        oSession = self.oTstDrv.openSession(oVM)
        aoDskFmts = self.oTstDrv.oVBoxMgr.getArray(self.oTstDrv.oVBox.systemProperties, 'mediumFormats')
        asFiles = []
        for oDskFmt in aoDskFmts:
            aoDskFmtCaps = self.oTstDrv.oVBoxMgr.getArray(oDskFmt, 'capabilities')
            if vboxcon.MediumFormatCapabilities_File not in aoDskFmtCaps \
                or vboxcon.MediumFormatCapabilities_CreateDynamic not in aoDskFmtCaps:
                continue
            (asExts, aTypes) = oDskFmt.describeFileExtensions()
            for i in range(0, len(asExts)):
                if aTypes[i] is vboxcon.DeviceType_HardDisk:
                    sExt = '.' + asExts[i]
                    break
            if sExt is None:
                fRc = False
                break
            sFile = 'test-vm-move' + str(len(asFiles)) + sExt
            sHddPath = os.path.join(self.oTstDrv.sScratchPath, sFile)
            oHd = oSession.createBaseHd(sHddPath, sFmt=oDskFmt.id, cb=1024*1024)
            if oHd is None:
                fRc = False
                break

            # attach HDD, IDE controller exists by default, but we use SATA just in case
            sController='SATA Controller'
            fRc = fRc and oSession.attachHd(sHddPath, sController, iPort = len(asFiles),
                                            fImmutable=False, fForceResource=False)
            if fRc:
                asFiles.append(sFile)

        fRc = fRc and oSession.saveSettings()
        fRc = oSession.close() and fRc

        if fRc is True:
            oListOfImageNames = asFiles
        else:
            oVM = None

        return oVM

    #
    #moveVMToLocation
    #
    def moveVMToLocation(self, sLocation, oVM):
        fRc = True
        try:
            #move machine
            reporter.log('Moving machine "%s" to the "%s"' % (oVM.name, sLocation))
            oType = 'basic'
            oProgress = vboxwrappers.ProgressWrapper(oVM.moveTo(sLocation, oType), self.oTstDrv.oVBoxMgr, self.oTstDrv,
                                                     'moving machine "%s"' % (oVM.name,))

        except:
            return reporter.errorXcpt('Machine::moveTo("%s") for machine "%s" failed' % (sLocation, oVM.name,))

        oProgress.wait()
        if oProgress.logResult() is False:
            fRc = False
            reporter.log('Progress object returned False')
        else:
            fRc = True

        return fRc

    #
    #checkLocation
    #
    def checkLocation(self, sLocation, aoMediumAttachments, asFiles):
        fRc = True
        for oAttachment in aoMediumAttachments:
            sFilePath = os.path.join(sLocation, asFiles[oAttachment.port])
            sActualFilePath = oAttachment.medium.location
            if os.path.abspath(sFilePath) != os.path.abspath(sActualFilePath):
                reporter.log('medium location expected to be "%s" but is "%s"' % (sFilePath, sActualFilePath))
                fRc = False;
            if not os.path.exists(sFilePath):
                reporter.log('medium file does not exist at "%s"' % (sFilePath,))
                fRc = False;
        return fRc

    #
    #checkAPIVersion
    #
    def checkAPIVersion(self):
        if self.oTstDrv.fpApiVer >= 5.3:
            return True

        return False

    #
    #testVMMove
    #
    def testVMMove(self):
        """
        Test machine moving.
        """
        reporter.testStart('machine moving')

        if not self.oTstDrv.importVBoxApi():
            return False;

        isSupported = self.checkAPIVersion()

        if isSupported is False:
            reporter.log('API version is below "%s". Just skip this test.' % (self.oTstDrv.fpApiVer))
            return reporter.testDone()[1] == 0
        else:
            reporter.log('API version is "%s". Continuing the test.' % (self.oTstDrv.fpApiVer))

        #Scenarios
        #1. All disks attached to VM are located outside the VM's folder.
        #   There are no any snapshots and logs.
        #   In this case only VM setting file should be moved (.vbox file)
        #   
        #2. All disks attached to VM are located inside the VM's folder.
        #   There are no any snapshots and logs.
        #   
        #3. There are snapshots.
        #
        #4. There are one or more save state files in the snapshots folder
        #   and some files in the logs folder.
        #
        #5. There is an ISO image (.iso) attached to the VM.
        #
        #6. There is a floppy image (.img) attached to the VM.
        #
        #7. There are shareable disk and immutable disk attached to the VM. 
        #
        #8. There is "read-only" disk attached to the VM.  

        try:
            #create test machine
            aListOfImageNames = []
            aMachine = self.createTestMachine(aListOfImageNames)

            if aMachine is None:
                reporter.error('Failed to create test machine')

            #create temporary subdirectory in the current working directory
            sOrigLoc = self.oTstDrv.sScratchPath
            sNewLoc = os.path.join(sOrigLoc, 'moveFolder')
            os.mkdir(sNewLoc, 0o775)

            sController='SATA Controller'
            aoMediumAttachments = aMachine.getMediumAttachmentsOfController(sController)

            #lock machine
            #get session machine
            oSession = self.oTstDrv.openSession(aMachine)
            fRc = True

            sMoveLoc = sNewLoc + os.sep
            #1 case.
            #   All disks attached to VM are located outside the VM's folder.
            #   There are no any snapshots and logs.
            #   In this case only VM setting file should be moved (.vbox file)
            fRc = self.moveVMToLocation(sMoveLoc, oSession.o.machine) and fRc

            fRc = fRc and oSession.saveSettings()
            if fRc is False:
                reporter.log('Couldn\'t save machine settings')

            #2 case.
            #   All disks attached to VM are located inside the VM's folder.
            #   There are no any snapshots and logs.
            sLoc = sMoveLoc + aMachine.name + os.sep
            sMoveLoc = os.path.join(sOrigLoc, 'moveFolder_2d_scenario')
            os.mkdir(sMoveLoc, 0o775)
            aoMediumAttachments = aMachine.getMediumAttachmentsOfController(sController)
            SubTstDrvMoveMedium1Instance = SubTstDrvMoveMedium1(self.oTstDrv)
            SubTstDrvMoveMedium1Instance.setLocation(sLoc, aoMediumAttachments)
            fRc = self.moveVMToLocation(sMoveLoc, oSession.o.machine) and fRc

            fRc = fRc and oSession.saveSettings()
            if fRc is False:
                reporter.log('Couldn\'t save machine settings')

            #3 case.
            #   There are snapshots.
            sLoc = sMoveLoc + aMachine.name + os.sep
            sMoveLoc = os.path.join(sOrigLoc, 'moveFolder_3d_scenario')
            os.mkdir(sMoveLoc, 0o775)

            n = 5
            for counter in range(1,n+1):
                strSnapshot = 'Snapshot' + str(counter)
                fRc = fRc and oSession.takeSnapshot(strSnapshot)
                if fRc is False:
                    reporter.error('Error: Can\'t take snapshot "%s".' % (strSnapshot,))
                    reporter.testFailure('Error: Can\'t take snapshot "%s".' % (strSnapshot,));

            aoMediumAttachments = aMachine.getMediumAttachmentsOfController(sController)
            if fRc is True:
                fRc = self.moveVMToLocation(sMoveLoc, oSession.o.machine) and fRc

            fRc = fRc and oSession.saveSettings()
            if fRc is False:
                reporter.log('Couldn\'t save machine settings')

            fRc = oSession.close() and fRc
            if fRc is False:
                reporter.log('Couldn\'t close machine session')

            assert fRc is True
        except:
            reporter.errorXcpt()

        return reporter.testDone()[1] == 0


if __name__ == '__main__':
    sys.path.append(os.path.dirname(os.path.abspath(__file__)))
    from tdApi1 import tdApi1
    sys.exit(tdApi1([SubTstDrvMoveVM]).main(sys.argv))

