#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$

"""
VirtualBox Validation Kit - Exotic and/or ancient OSes #1.
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
import os;
import sys;

# Only the main script needs to modify the path.
try:    __file__
except: __file__ = sys.argv[0];
g_ksValidationKitDir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))));
sys.path.append(g_ksValidationKitDir);

# Validation Kit imports.
from testdriver import reporter;
from testdriver import vbox;


class tdExoticOrAncient1(vbox.TestDriver):
    """
    VBox exotic and/or ancient OSes #1.
    """

    def __init__(self):
        vbox.TestDriver.__init__(self);
        self.asRsrcs            = None;
        self.oTestVmSet         = self.oTestVmManager.selectSet(  self.oTestVmManager.kfGrpAncient
                                                                | self.oTestVmManager.kfGrpExotic);

    #
    # Overridden methods.
    #
    def showUsage(self):
        rc = vbox.TestDriver.showUsage(self);
        return rc;

    def parseOption(self, asArgs, iArg):
        return vbox.TestDriver.parseOption(self, asArgs, iArg);

    def actionVerify(self):
        if self.sVBoxValidationKitIso is None or not os.path.isfile(self.sVBoxValidationKitIso):
            reporter.error('Cannot find the VBoxValidationKit.iso! (%s)'
                           'Please unzip a Validation Kit build in the current directory or in some parent one.'
                           % (self.sVBoxValidationKitIso,) );
            return False;
        return vbox.TestDriver.actionVerify(self);

    def actionConfig(self):
        # Make sure vboxapi has been imported so we can use the constants.
        if not self.importVBoxApi():
            return False;

        assert self.sVBoxValidationKitIso is not None;
        return self.oTestVmSet.actionConfig(self, sDvdImage = self.sVBoxValidationKitIso);

    def actionExecute(self):
        """
        Execute the testcase.
        """
        return self.oTestVmSet.actionExecute(self, self.testOneVmConfig)


    #
    # Test execution helpers.
    #

    def testOneVmConfig(self, oVM, oTestVm):
        """
        Runs the specified VM thru test #1.
        """

        # Simple test.
        self.logVmInfo(oVM);
        if oTestVm.fGrouping & self.oTestVmManager.kfGrpNoTxs:
            pass;   ## @todo serial port VMs.
        else:
            oSession, _ = self.startVmAndConnectToTxsViaTcp(oTestVm.sVmName, fCdWait = True);
            if oSession is not None:
                self.terminateVmBySession(oSession)
                return True;
        return None;

if __name__ == '__main__':
    sys.exit(tdExoticOrAncient1().main(sys.argv));

