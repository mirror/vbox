#!/usr/bin/env python
# -*- coding: utf-8 -*-
# "$Id$"

"""
VirtualBox Validation Kit - connection to OCI #1
"""

__copyright__ = \
"""
Copyright (C) 2010-2019 Oracle Corporation

Oracle Corporation confidential
All rights reserved
"""
__version__ = "$Revision$"


# Standard Python imports.
import os
import sys
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

from collections import defaultdict

class SubTstOciConnection1(base.SubTestDriverBase):
    """
    Sub-test driver for connection to OCI Test #1.
    """

    def __init__(self, oTstDrv):
        base.SubTestDriverBase.__init__(self, oTstDrv, 'oci-connection', 'OCI connection');

    def testIt(self):
        """
        Execute the sub-testcase.
        """
        reporter.testStart('OCI connection')
        fRc = self.testOciConnection()
        return reporter.testDone(fRc is None)[1] == 0;

    #
    # Test execution helpers.
    #

    def checkAPIVersion(self):
        return self.oTstDrv.fpApiVer >= 6.0;

    def __testScenario_1(self):
        fRc = True
        oVirtualBox = self.oTstDrv.oVBoxMgr.getVirtualBox()
        if oVirtualBox is None:
            return reporter.error('Failed to get VirtualBox object')
            
        cpm = oVirtualBox.cloudProviderManager;
        if cpm is None:
            return reporter.error('Failed to get Cloud provider manager')

        pCloudProvider = cpm.getProviderByShortName("OCI");
        if oVirtualBox is None:
            return reporter.error('Failed to get Cloud provider')

        reporter.log('Provider Id "%s" ' % (pCloudProvider.id,))

        pCloudProfile = pCloudProvider.getProfileByName("vbox-test-user");
        if oVirtualBox is None:
            return reporter.error('Failed to get Cloud profile')

        reporter.log('Profile properties: name "%s" ' % (pCloudProfile.name,))

        pCloudclient = pCloudProfile.createCloudClient()
        if pCloudclient is None:
            return reporter.error('Failed to create Cloud client')

        try:
            machineState = ()
            oProgress, dsNamesObject, dsIdsObject = pCloudclient.listInstances(machineState)

            while not oProgress.completed:
                oProgress.waitForCompletion(60*60)

            dsNames = dsNamesObject.getValues()
            for sValue in dsNames:
                reporter.log('Instance name "%s" ' % (sValue,))
        except:
            return reporter.errorXcpt('failed on getting the list of the instances from the OCI')

        return fRc

    def testOciConnection(self):
        """
        Test OCI connection.
        """
        if not self.oTstDrv.importVBoxApi():
            return False

        fSupported = self.checkAPIVersion()
        reporter.log('ValidationKit folder is "%s"' % (g_ksValidationKitDir,))
        reporter.log("sHost is '%s', sResourcePath is '%s'" % (self.oTstDrv.sHost, self.oTstDrv.sResourcePath,))

        if fSupported is False:
            reporter.log('API version %s is too old. Just skip this test.' % (self.oTstDrv.fpApiVer,))
            return None;
        reporter.log('API version is "%s".' % (self.oTstDrv.fpApiVer,))

        try:
            fRc = self.__testScenario_1()
        except:
            fRc = reporter.errorXcpt()

        return fRc;


if __name__ == '__main__':
    sys.path.append(os.path.dirname(os.path.abspath(__file__)))
    from tdApi1 import tdApi1; # pylint: disable=relative-import
    sys.exit(tdApi1([SubTstOciConnection1]).main(sys.argv))

