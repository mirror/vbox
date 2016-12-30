# -*- coding: utf-8 -*-
# $Id$

"""
Test Manager WUI - Admin - System changelog.
"""

__copyright__ = \
"""
Copyright (C) 2012-2016 Oracle Corporation

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
from testmanager.webui.wuicontentbase   import WuiListContentBase #, WuiTmLink;
#from testmanager.core.testbox           import TestBoxData;
#from testmanager.core.systemchangelog   import SystemChangelogLogic;
#from testmanager.core.useraccount       import UserAccountData;


class WuiAdminSystemChangelogList(WuiListContentBase):
    """
    WUI System Changelog Content Generator.
    """

    def __init__(self, aoEntries, iPage, cItemsPerPage, tsEffective, fnDPrint, oDisp, cDaysBack):
        WuiListContentBase.__init__(self, aoEntries, iPage, cItemsPerPage, tsEffective, 'System Changelog',
                                    fnDPrint = fnDPrint, oDisp = oDisp);
        self._asColumnHeaders = ['Date', 'Author', 'What', 'Description'];
        self._asColumnAttribs = ['', '', '', ''];
        _ = cDaysBack;

    def _formatListEntry(self, iEntry):
        from testmanager.webui.wuiadmin import WuiAdmin;
        oEntry  = self._aoEntries[iEntry];

        return [
            oEntry.tsEffective,
            oEntry.oAuthor.sUsername if oEntry.oAuthor is not None else '',
            '%s(%s)' % (oEntry.sWhat, oEntry.idWhat,),
            oEntry.sDesc,
        ];


