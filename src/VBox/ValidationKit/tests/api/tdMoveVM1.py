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
import time
import shutil

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

class SubTstDrvMoveVM1(base.SubTestDriverBase):
    """
    Sub-test driver for VM Move Test #1.
    """

    def __init__(self, oTstDrv):
        base.SubTestDriverBase.__init__(self, 'move-vm', oTstDrv)
        self.asRsrcs = self.__getResourceSet()

        for oRes in self.asRsrcs:
            reporter.log('Resource is "%s"' % (oRes,))

    def testIt(self):
        """
        Execute the sub-testcase.
        """
        reporter.log('ValidationKit folder is "%s"' % (g_ksValidationKitDir,))
        return  self.testVMMove()

    #
    # Test execution helpers.
    #

    #
    #createTestMachine
    #
    def createTestMachine(self):
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

        if fRc is False:
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
                fRc = False
            if not os.path.exists(sFilePath):
                reporter.log('medium file does not exist at "%s"' % (sFilePath,))
                fRc = False
        return fRc

    #
    #checkAPIVersion
    #
    def checkAPIVersion(self):
        if self.oTstDrv.fpApiVer >= 5.3:
            return True

        return False

    def __getResourceSet(self):
        # Construct the resource list the first time it's queried.
        if self.oTstDrv.asRsrcs is None:
            self.oTstDrv.asRsrcs = []
            self.oTstDrv.asRsrcs.append('5.3/isos/tdMoveVM.iso')
            self.oTstDrv.asRsrcs.append('5.3/floppy/tdMoveVM.img')

        return self.oTstDrv.asRsrcs

    def __testScenario_4(self, oMachine, sNewLoc):

        #Run VM and get new Session object
        oSession = self.oTstDrv.startVm(oMachine)

        #some time interval should be here for not closing VM just after start
        time.sleep(1)

        if oMachine.state != self.oTstDrv.oVBoxMgr.constants.MachineState_Running:
            reporter.log("Machine '%s' is not Running" % (oMachine.name))
            fRc = False

        #call Session::saveState(), already closes session unless it failed
        fRc = oSession.saveState()
        self.oTstDrv.terminateVmBySession(oSession)

        if fRc:
            #create a new Session object for moving VM
            oSession = self.oTstDrv.openSession(oMachine)
            fRc = self.moveVMToLocation(sNewLoc, oSession.o.machine) and fRc

            # cleaning up: get rid of saved state
            fRc = fRc and oSession.discardSavedState(True)
            if fRc is False:
                reporter.log('Failed to discard the saved state of machine')

            fRc = oSession.close() and fRc
            if fRc is False:
                reporter.log('Couldn\'t close machine session')

        return fRc

    def __testScenario_5(self, oMachine, sNewLoc, sOldLoc):

        fRc = True
        #create a new Session object
        oSession = self.oTstDrv.openSession(oMachine)

        sISOLoc = '5.3/isos/tdMoveVM1.iso'
        reporter.log("sHost is '%s', sResourcePath is '%s'" % (self.oTstDrv.sHost, self.oTstDrv.sResourcePath))
        sISOLoc = self.oTstDrv.getFullResourceName(sISOLoc)

        if not os.path.exists(sISOLoc):
            reporter.log('ISO file does not exist at "%s"' % (sISOLoc,))
            fRc = False

        #Copy ISO image from the common resource folder into machine folder
        shutil.copy(sISOLoc, sOldLoc)

        #attach ISO image to the IDE controller
        if fRc is True:
            #set actual ISO location
            sISOLoc = sOldLoc + os.sep + 'tdMoveVM1.iso'
            sController='IDE Controller'
            aoMediumAttachments = oMachine.getMediumAttachmentsOfController(sController)
            iPort = len(aoMediumAttachments)
            reporter.log('sISOLoc "%s", sController "%s", iPort "%s"' % (sISOLoc,sController,iPort))
            fRc = oSession.attachDvd(sISOLoc, sController, iPort, iDevice = 0)

        if fRc is True:
            fRc = self.moveVMToLocation(sNewLoc, oSession.o.machine) and fRc

        #detach ISO image
        fRc = oSession.detachHd(sController, iPort, 0)

        fRc = fRc and oSession.saveSettings()
        if fRc is False:
            reporter.log('Couldn\'t save machine settings after 5th scenario')

        fRc = oSession.close() and fRc
        if fRc is False:
            reporter.log('Couldn\'t close machine session')

        return fRc

    def __testScenario_6(self, oMachine, sNewLoc, sOldLoc):

        fRc = True
        #create a new Session object
        oSession = self.oTstDrv.openSession(oMachine)

        sFloppyLoc = '5.3/floppy/tdMoveVM1.img'
        sFloppyLoc = self.oTstDrv.getFullResourceName(sFloppyLoc)

        if not os.path.exists(sFloppyLoc):
            reporter.log('Floppy disk does not exist at "%s"' % (sFloppyLoc,))
            fRc = False

        #Copy floppy image from the common resource folder into machine folder
        shutil.copy(sFloppyLoc, sOldLoc)

        # attach floppy image
        if fRc is True:
            #set actual floppy location
            sFloppyLoc = sOldLoc + os.sep + 'tdMoveVM1.img'
            sController='Floppy Controller'
            reporter.log('sFloppyLoc "%s", sController "%s"' % (sFloppyLoc,sController))
            fRc = fRc and oSession.attachFloppy(sFloppyLoc, sController, 0, 0)

        if fRc is True:
            fRc = self.moveVMToLocation(sNewLoc, oSession.o.machine) and fRc

        #detach floppy image
        fRc = oSession.detachHd(sController, 0, 0)

        fRc = fRc and oSession.saveSettings()
        if fRc is False:
            reporter.log('Couldn\'t save machine settings after 6th scenario')

        fRc = oSession.close() and fRc
        if fRc is False:
            reporter.log('Couldn\'t close machine session')

        return fRc

    #
    #testVMMove
    #
    def testVMMove(self):
        """
        Test machine moving.
        """
        reporter.testStart('machine moving')

        if not self.oTstDrv.importVBoxApi():
            return False

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

        try:
            #create test machine
            oMachine = self.createTestMachine()

            if oMachine is None:
                reporter.error('Failed to create test machine')

            #create temporary subdirectory in the current working directory
            sOrigLoc = self.oTstDrv.sScratchPath
            sBaseLoc = os.path.join(sOrigLoc, 'moveFolder')
            os.mkdir(sBaseLoc, 0o775)

            sController='SATA Controller'
            aoMediumAttachments = oMachine.getMediumAttachmentsOfController(sController)

            #lock machine
            #get session machine
            oSession = self.oTstDrv.openSession(oMachine)
            fRc = True

            sNewLoc = sBaseLoc + os.sep
############# 1 case. ##########################################################################################
            #   All disks attached to VM are located outside the VM's folder.
            #   There are no any snapshots and logs.
            #   In this case only VM setting file should be moved (.vbox file)
            fRc = self.moveVMToLocation(sNewLoc, oSession.o.machine) and fRc

            fRc = fRc and oSession.saveSettings()
            if fRc is False:
                reporter.log('Couldn\'t save machine settings after 1t scenario')

############# 2 case. ##########################################################################################
            #   All disks attached to VM are located inside the VM's folder.
            #   There are no any snapshots and logs.
            sOldLoc = sNewLoc + os.sep + oMachine.name + os.sep
            sNewLoc = os.path.join(sOrigLoc, 'moveFolder_2d_scenario')
            os.mkdir(sNewLoc, 0o775)
            aoMediumAttachments = oMachine.getMediumAttachmentsOfController(sController)
            oSubTstDrvMoveMedium1Instance = SubTstDrvMoveMedium1(self.oTstDrv)
            oSubTstDrvMoveMedium1Instance.setLocation(sOldLoc, aoMediumAttachments)

            del oSubTstDrvMoveMedium1Instance

            fRc = self.moveVMToLocation(sNewLoc, oSession.o.machine) and fRc

            fRc = fRc and oSession.saveSettings()
            if fRc is False:
                reporter.log('Couldn\'t save machine settings after 2nd scenario')

############# 3 case. ##########################################################################################
            #   There are snapshots.
            sOldLoc = sNewLoc + os.sep + oMachine.name + os.sep
            sNewLoc = os.path.join(sOrigLoc, 'moveFolder_3d_scenario')
            os.mkdir(sNewLoc, 0o775)

            cSnap = 2
            for counter in range(1,cSnap+1):
                strSnapshot = 'Snapshot' + str(counter)
                fRc = fRc and oSession.takeSnapshot(strSnapshot)
                if fRc is False:
                    reporter.error('Error: Can\'t take snapshot "%s".' % (strSnapshot,))
                    reporter.testFailure('Error: Can\'t take snapshot "%s".' % (strSnapshot,))

            aoMediumAttachments = oMachine.getMediumAttachmentsOfController(sController)
            if fRc is True:
                fRc = self.moveVMToLocation(sNewLoc, oSession.o.machine) and fRc

            fRc = fRc and oSession.saveSettings()
            if fRc is False:
                reporter.log('Couldn\'t save machine settings after 3d scenario')

############# 4 case. ##########################################################################################
            #   There are one or more save state files in the snapshots folder
            #   and some files in the logs folder.
            #   Here we run VM, next stop it in the "save" state.
            #   And next move VM

            sOldLoc = sNewLoc + os.sep + oMachine.name + os.sep
            sNewLoc = os.path.join(sOrigLoc, 'moveFolder_4th_scenario')
            os.mkdir(sNewLoc, 0o775)

            #Close Session object because after starting VM we get new instance of session
            fRc = oSession.close() and fRc
            if fRc is False:
                reporter.log('Couldn\'t close machine session')

            del oSession

            self.__testScenario_4(oMachine, sNewLoc)

############## 5 case. ##########################################################################################
            #There is an ISO image (.iso) attached to the VM.
            #Prerequisites - there is IDE Controller and there are no any images attached to it.

            sOldLoc = sNewLoc + os.sep + oMachine.name + os.sep
            sNewLoc = os.path.join(sOrigLoc, 'moveFolder_5th_scenario')
            os.mkdir(sNewLoc, 0o775)
            self.__testScenario_5(oMachine, sNewLoc, sOldLoc)

############# 6 case. ##########################################################################################
            #There is a floppy image (.img) attached to the VM.
            #Prerequisites - there is Floppy Controller and there are no any images attached to it.

            sOldLoc = sNewLoc + os.sep + oMachine.name + os.sep
            sNewLoc = os.path.join(sOrigLoc, 'moveFolder_6th_scenario')
            os.mkdir(sNewLoc, 0o775)
            self.__testScenario_6(oMachine, sNewLoc, sOldLoc)

############# 7 case. ##########################################################################################
#           #   There are shareable disk and immutable disk attached to the VM.
#           #
#
#           fRc = fRc and oSession.saveSettings()
#           if fRc is False:
#               reporter.log('Couldn\'t save machine settings')
#

            assert fRc is True
        except:
            reporter.errorXcpt()

        return reporter.testDone()[1] == 0


if __name__ == '__main__':
    sys.path.append(os.path.dirname(os.path.abspath(__file__)))
    from tdApi1 import tdApi1
    sys.exit(tdApi1([SubTstDrvMoveVM1]).main(sys.argv))

