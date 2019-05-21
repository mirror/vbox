#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$

"""
VirtualBox Validation Kit - Create VM with IMachine::applyDefaults() Test
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
from testdriver import reporter;
from testdriver import vbox;
from testdriver import vboxcon;

class tdCreateVMWithDefaults1(vbox.TestDriver):
    """
    Create VM with defaults Test #1.
    """

    def __init__(self):
        vbox.TestDriver.__init__(self);
        self.asRsrcs = None;

    def getResourceSet(self):
        # Construct the resource list the first time it's queried.
        if self.asRsrcs is None:
            self.asRsrcs = [];
        return self.asRsrcs;

    def actionExecute(self):
        """
        Execute the testcase.
        """
        fRc = self.testCreateVMWithDefaults()
        return fRc;

    def createVMWithDefaults(self, sGuestType):
        sName = 'testvm_%s' % (sGuestType)
        # create VM manually, because the self.createTestVM() makes registration inside
        # the method, but IMachine::applyDefault() must be called before machine is registered
        try:
            if self.fpApiVer >= 4.2: # Introduces grouping (third parameter, empty for now).
                oVM = self.oVBox.createMachine("", sName, [], self.tryFindGuestOsId(sGuestType), "")
            elif self.fpApiVer >= 4.0:
                oVM = self.oVBox.createMachine("", sName, self.tryFindGuestOsId(sGuestType), "",
                                               False)
            elif self.fpApiVer >= 3.2:
                oVM = self.oVBox.createMachine(sName, self.tryFindGuestOsId(sGuestType), "", "",
                                               False)
            else:
                oVM = self.oVBox.createMachine(sName, self.tryFindGuestOsId(sGuestType), "", "")
            try:
                oVM.saveSettings()
            except:
                reporter.logXcpt()
                if self.fpApiVer >= 4.0:
                    try:
                        if self.fpApiVer >= 4.3:
                            oProgress = oVM.deleteConfig([])
                        else:
                            oProgress = oVM.delete(None);
                        self.waitOnProgress(oProgress)
                    except:
                        reporter.logXcpt()
                else:
                    try:    oVM.deleteSettings()
                    except: reporter.logXcpt()
                raise
        except:
            reporter.errorXcpt('failed to create vm "%s"' % (sName))
            return None

        if oVM is None:
            return False

        # apply settings
        fRc = True
        try:
            if self.fpApiVer >= 6.1:
                oVM.applyDefaults('')
                oVM.saveSettings();
            self.oVBox.registerMachine(oVM)
        except:
            reporter.logXcpt()
            fRc = False

        # some errors from applyDefaults can be observed only after further settings saving
        # change and save the size of the VM RAM as simple setting change.
        oSession = self.openSession(oVM)
        if oSession is None:
            fRc = False

        if fRc:
            try:
                oSession.memorySize = 4096
                oSession.saveSettings(True)
            except:
                reporter.logXcpt()
                fRc = False

        #delete VM
        try:
            oVM.unregister(vboxcon.CleanupMode_DetachAllReturnNone)
        except:
            reporter.logXcpt()

        if self.fpApiVer >= 4.0:
            try:
                if self.fpApiVer >= 4.3:
                    oProgress = oVM.deleteConfig([])
                else:
                    oProgress = oVM.delete(None)
                self.waitOnProgress(oProgress)

            except:
                reporter.logXcpt()

        else:
            try:    oVM.deleteSettings()
            except: reporter.logXcpt()

        return fRc

    def testCreateVMWithDefaults(self):
        """
        Test create VM with defaults.
        """
        if not self.importVBoxApi():
            return reporter.error('importVBoxApi');

        # Get the guest OS types.
        try:
            aoGuestTypes = self.oVBoxMgr.getArray(self.oVBox, 'guestOSTypes')
            if aoGuestTypes is None or len(aoGuestTypes) < 1:
                return reporter.error('No guest OS types');
        except:
            return reporter.errorXcpt();

        # Create VMs with defaults for each of the guest types.
        reporter.testStart('Create VMs with defaults');
        fRc = True
        for oGuestType in aoGuestTypes:
            try:
                sGuestType = oGuestType.id;
            except:
                fRc = reporter.errorXcpt();
            else:
                reporter.testStart(sGuestType);
                fRc = self.createVMWithDefaults(sGuestType) & fRc;
                reporter.testDone();
        reporter.testDone();

        return fRc

if __name__ == '__main__':
    sys.exit(tdCreateVMWithDefaults1().main(sys.argv))
