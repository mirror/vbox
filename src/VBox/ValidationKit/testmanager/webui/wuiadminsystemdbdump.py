# -*- coding: utf-8 -*-
# $Id$

"""
Test Manager WUI - System DB - Partial Dumping
"""

__copyright__ = \
"""
Copyright (C) 2012-2020 Oracle Corporation

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


# Validation Kit imports.
from testmanager.core.base              import ModelDataBase;
from testmanager.webui.wuicontentbase   import WuiFormContentBase;


class WuiAdminSystemDbDumpForm(WuiFormContentBase):
    """
    WUI Partial DB Dump HTML content generator.
    """

    def __init__(self, cDaysBack, oDisp):
        WuiFormContentBase.__init__(self, ModelDataBase(),
                                    WuiFormContentBase.ksMode_Edit, 'DbDump', oDisp, 'Partial DB Dump',
                                    sSubmitAction = oDisp.ksActionSystemDbDumpDownload);
        self._cDaysBack = cDaysBack;

    def _generateTopRowFormActions(self, oData):
        _ = oData;
        return [];

    def _populateForm(self, oForm, oData): # type: (WuiHlpForm, SchedGroupDataEx) -> bool
        """
        Construct an HTML form
        """
        _ = oData;

        oForm.addInt(self._oDisp.ksParamDaysBack, self._cDaysBack, 'How many days back to dump');
        oForm.addSubmit('Produce & Download');

        return True;

