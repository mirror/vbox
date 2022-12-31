#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$

"""
VirtualBox Validation Kit - Clone Medium Test #1
"""

__copyright__ = \
"""
Copyright (C) 2010-2022 Oracle and/or its affiliates.

This file is part of VirtualBox base platform packages, as
available from https://www.virtualbox.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, in version 3 of the
License.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <https://www.gnu.org/licenses>.

The contents of this file may alternatively be used under the terms
of the Common Development and Distribution License Version 1.0
(CDDL), a copy of it is provided in the "COPYING.CDDL" file included
in the VirtualBox distribution, in which case the provisions of the
CDDL are applicable instead of those of the GPL.

You may elect to license modified versions of this file under the
terms and conditions of either the GPL or the CDDL or both.

SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
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


class SubTstDrvCloneMedium1(base.SubTestDriverBase):
    """
    Sub-test driver for Clone Medium Test #1.
    """

    def __init__(self, oTstDrv):
        base.SubTestDriverBase.__init__(self, oTstDrv, 'clone-medium', 'Move Medium');

    def testIt(self):
        """
        Execute the sub-testcase.
        """
        return self.testMediumClone()

    #
    # Test execution helpers.
    #

    def createTestMedium(self):
        oVirtualBox = self.oTstDrv.oVBoxMgr.getVirtualBox()

        #
        # Create Medium Object
        #
        try:
            oMedium = oVirtualBox.createMedium()
        except:
            return reporter.errorXcpt('IVirtualBox::createMedium failed')

        print(oMedium)


if __name__ == '__main__':
    sys.path.append(os.path.dirname(os.path.abspath(__file__)))
    from tdApi1 import tdApi1; # pylint: disable=relative-import
    sys.exit(tdApi1([SubTstDrvCloneMedium1]).main(sys.argv))

