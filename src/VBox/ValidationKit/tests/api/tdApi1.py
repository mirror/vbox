#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$

"""
VirtualBox Validation Kit - API Test wrapper #1 combining all API sub-tests
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


class tdApi1(vbox.TestDriver):
    """
    API Test wrapper #1.
    """

    def __init__(self, aSubTestDrivers = None):
        vbox.TestDriver.__init__(self)
        self.asRsrcs = None;
        for oSubTestDriverClass in aSubTestDrivers:
            self.addSubTestDriver(oSubTestDriverClass(self));

    #
    # Overridden methods.
    #

    def getResourceSet(self):
        if self.asRsrcs is None:
            self.asRsrcs = [];
            for oSubTstDrv in self.aoSubTstDrvs:
                self.asRsrcs.extend(oSubTstDrv.asRsrcs);
        return self.asRsrcs;

    def actionConfig(self):
        """
        Import the API.
        """
        if not self.importVBoxApi():
            return False
        return True

    def actionExecute(self):
        """
        Execute the testcase, i.e. all sub-tests.
        """
        fRc = True
        for oSubTstDrv in self.aoSubTstDrvs:
            fRc &= oSubTstDrv.testIt()
        return fRc


if __name__ == '__main__':
    sys.path.append(os.path.dirname(os.path.abspath(__file__)))
    from tdPython1 import SubTstDrvPython1
    from tdAppliance1 import SubTstDrvAppliance1
    from tdMoveMedium1 import SubTstDrvMoveMedium1
    from tdTreeDepth1 import SubTstDrvTreeDepth1
    from tdMoveVM1 import SubTstDrvMoveVM1
    sys.exit(tdApi1([SubTstDrvPython1, SubTstDrvAppliance1, SubTstDrvMoveMedium1,
                     SubTstDrvTreeDepth1, SubTstDrvMoveVM1]).main(sys.argv))

