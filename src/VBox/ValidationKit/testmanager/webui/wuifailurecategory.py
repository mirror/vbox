# -*- coding: utf-8 -*-
# $Id$

"""
Test Manager WUI - Failure Categories Web content generator.
"""

__copyright__ = \
"""
Copyright (C) 2012-2014 Oracle Corporation

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
from testmanager.webui.wuicontentbase  import WuiFormContentBase, WuiListContentBase, WuiTmLink
from testmanager.core.failurecategory  import FailureCategoryData
from testmanager.webui.wuibase         import WuiException


class WuiFailureCategory(WuiFormContentBase):
    """
    WUI Failure Category HTML content generator.
    """

    def __init__(self, oFailureCategoryData, sMode, oDisp):
        """
        Prepare & initialize parent
        """

        if sMode == WuiFormContentBase.ksMode_Add:
            sTitle = 'Add Failure Category'
            sSubmitAction = oDisp.ksActionFailureCategoryAdd
        elif sMode == WuiFormContentBase.ksMode_Edit:
            sTitle = 'Edit Failure Category'
            sSubmitAction = oDisp.ksActionFailureCategoryEdit
        else:
            raise WuiException('Unknown parameter')

        WuiFormContentBase.__init__(self, oFailureCategoryData, sMode, 'FailureCategory', oDisp, sTitle,
                                    sSubmitAction = sSubmitAction, fEditable = False); ## @todo non-standard action names.

    def _populateForm(self, oForm, oData):
        """
        Construct an HTML form
        """

        oForm.addIntRO      (FailureCategoryData.ksParam_idFailureCategory,   oData.idFailureCategory,  'Failure Category ID')
        oForm.addTimestampRO(FailureCategoryData.ksParam_tsEffective,         oData.tsEffective,        'Last changed')
        oForm.addTimestampRO(FailureCategoryData.ksParam_tsExpire,            oData.tsExpire,           'Expires (excl)')
        oForm.addIntRO      (FailureCategoryData.ksParam_uidAuthor,           oData.uidAuthor,          'Changed by UID')
        oForm.addText       (FailureCategoryData.ksParam_sShort,              oData.sShort,             'Short Description')
        oForm.addText       (FailureCategoryData.ksParam_sFull,               oData.sFull,              'Full Description')

        oForm.addSubmit()

        return True

class WuiFailureCategoryList(WuiListContentBase):
    """
    WUI Admin Failure Category Content Generator.
    """

    def __init__(self, aoEntries, iPage, cItemsPerPage, tsEffective, fnDPrint, oDisp):
        WuiListContentBase.__init__(self, aoEntries, iPage, cItemsPerPage, tsEffective,
                                    sTitle = 'Failure Categories', sId = 'failureCategories',
                                    fnDPrint = fnDPrint, oDisp = oDisp);

        self._asColumnHeaders = ['ID', 'Short Description', 'Full Description', 'Actions' ]
        self._asColumnAttribs = ['align="right"', 'align="center"', 'align="center"', 'align="center"']

    def _formatListEntry(self, iEntry):
        from testmanager.webui.wuiadmin import WuiAdmin
        oEntry = self._aoEntries[iEntry]

        return [ oEntry.idFailureCategory,
                 oEntry.sShort,
                 oEntry.sFull,
                 [ WuiTmLink('Modify', WuiAdmin.ksScriptName,
                             { WuiAdmin.ksParamAction: WuiAdmin.ksActionFailureCategoryShowEdit,
                               FailureCategoryData.ksParam_idFailureCategory: oEntry.idFailureCategory }),
                   WuiTmLink('Remove', WuiAdmin.ksScriptName,
                             { WuiAdmin.ksParamAction: WuiAdmin.ksActionFailureCategoryDel,
                               FailureCategoryData.ksParam_idFailureCategory: oEntry.idFailureCategory },
                             sConfirm = 'Do you really want to remove failure cateogry #%d?' % (oEntry.idFailureCategory,)),
        ] ];
