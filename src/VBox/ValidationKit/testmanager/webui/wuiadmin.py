# -*- coding: utf-8 -*-
# $Id$

"""
Test Manager Core - WUI - Admin Main page.
"""

__copyright__ = \
"""
Copyright (C) 2012-2015 Oracle Corporation

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


# Standard python imports.
import cgitb;
import sys;

# Validation Kit imports.
from common                                    import webutils
from testmanager                               import config;
from testmanager.core.buildsource              import BuildSourceLogic, BuildSourceData
from testmanager.core.build                    import BuildCategoryLogic, BuildCategoryData, BuildLogic, BuildData;
from testmanager.core.globalresource           import GlobalResourceLogic, GlobalResourceData
from testmanager.core.schedulerbase            import SchedulerBase
from testmanager.core.schedgroup               import SchedGroupLogic, SchedGroupData, SchedGroupDataEx
from testmanager.core.systemlog                import SystemLogLogic
from testmanager.core.testbox                  import TestBoxData, TestBoxLogic
from testmanager.core.testcase                 import TestCaseLogic, TestCaseData, TestCaseDataEx
from testmanager.core.useraccount              import UserAccountLogic, UserAccountData
from testmanager.core.testgroup                import TestGroupLogic, TestGroupDataEx;
from testmanager.core.failurecategory          import FailureCategoryLogic, FailureCategoryData
from testmanager.core.failurereason            import FailureReasonLogic, FailureReasonData
from testmanager.core.buildblacklist           import BuildBlacklistLogic, BuildBlacklistData
from testmanager.webui.wuibase                 import WuiDispatcherBase, WuiException
from testmanager.webui.wuiadminbuild           import WuiAdminBuild, WuiAdminBuildList
from testmanager.webui.wuiadminsystemlog       import WuiAdminSystemLogList
from testmanager.webui.wuiadminbuildsource     import WuiAdminBuildSrc, WuiAdminBuildSrcList;
from testmanager.webui.wuiadminbuildcategory   import WuiAdminBuildCat, WuiAdminBuildCatList;
from testmanager.webui.wuiadminglobalrsrc      import WuiGlobalResource, WuiGlobalResourceList
from testmanager.webui.wuiadmintestbox         import WuiTestBoxList, WuiTestBox
from testmanager.webui.wuiadmintestcase        import WuiTestCase, WuiTestCaseList
from testmanager.webui.wuiadminuseraccount     import WuiUserAccountList, WuiUserAccount
from testmanager.webui.wuiadmintestgroup       import WuiTestGroup, WuiTestGroupList;
from testmanager.webui.wuiadminschedgroup      import WuiSchedGroup, WuiAdminSchedGroupList;
from testmanager.webui.wuiadminbuildblacklist  import WuiAdminBuildBlacklist, WuiAdminListOfBlacklistItems
from testmanager.webui.wuifailurecategory      import WuiFailureCategory, WuiFailureCategoryList
from testmanager.webui.wuiadminfailurereason   import WuiAdminFailureReason, WuiAdminFailureReasonList


class WuiAdmin(WuiDispatcherBase):
    """
    WUI Admin main page.
    """

    ## The name of the script.
    ksScriptName = 'admin.py'


    ## @name Actions
    ## @{
    ksActionSystemLogList           = 'SystemLogList'

    ksActionUserList                = 'UserList'
    ksActionUserAdd                 = 'UserAdd'
    ksActionUserEdit                = 'UserEdit'
    ksActionUserAddPost             = 'UserAddPost'
    ksActionUserEditPost            = 'UserEditPost'
    ksActionUserDelPost             = 'UserDelPost'

    ksActionTestBoxList             = 'TestBoxList'
    ksActionTestBoxListPost         = 'TestBoxListPost'
    ksActionTestBoxAdd              = 'TestBoxAdd'
    ksActionTestBoxAddPost          = 'TestBoxAddPost'
    ksActionTestBoxEdit             = 'TestBoxEdit'
    ksActionTestBoxEditPost         = 'TestBoxEditPost'
    ksActionTestBoxDetails          = 'TestBoxDetails'
    ksActionTestBoxRemovePost       = 'TestBoxRemove'
    ksActionTestBoxesRegenQueues    = 'TestBoxesRegenQueues';

    ksActionTestCaseList            = 'TestCaseList'
    ksActionTestCaseAdd             = 'TestCaseAdd'
    ksActionTestCaseAddPost         = 'TestCaseAddPost'
    ksActionTestCaseClone           = 'TestCaseClone'
    ksActionTestCaseDetails         = 'TestCaseDetails'
    ksActionTestCaseEdit            = 'TestCaseEdit'
    ksActionTestCaseEditPost        = 'TestCaseEditPost'
    ksActionTestCaseDoRemove        = 'TestCaseDoRemove'

    ksActionGlobalRsrcShowAll       = 'GlobalRsrcShowAll'
    ksActionGlobalRsrcShowAdd       = 'GlobalRsrcShowAdd'
    ksActionGlobalRsrcShowEdit      = 'GlobalRsrcShowEdit'
    ksActionGlobalRsrcAdd           = 'GlobalRsrcAddPost'
    ksActionGlobalRsrcEdit          = 'GlobalRsrcEditPost'
    ksActionGlobalRsrcDel           = 'GlobalRsrcDelPost'

    ksActionBuildList               = 'BuildList'
    ksActionBuildAdd                = 'BuildAdd'
    ksActionBuildAddPost            = 'BuildAddPost'
    ksActionBuildClone              = 'BuildClone'
    ksActionBuildDetails            = 'BuildDetails'
    ksActionBuildDoRemove           = 'BuildDoRemove'
    ksActionBuildEdit               = 'BuildEdit'
    ksActionBuildEditPost           = 'BuildEditPost'

    ksActionBuildBlacklist          = 'BuildBlacklist';
    ksActionBuildBlacklistAdd       = 'BuildBlacklistAdd';
    ksActionBuildBlacklistAddPost   = 'BuildBlacklistAddPost';
    ksActionBuildBlacklistClone     = 'BuildBlacklistClone';
    ksActionBuildBlacklistDetails   = 'BuildBlacklistDetails';
    ksActionBuildBlacklistDoRemove  = 'BuildBlacklistDoRemove';
    ksActionBuildBlacklistEdit      = 'BuildBlacklistEdit';
    ksActionBuildBlacklistEditPost  = 'BuildBlacklistEditPost';

    ksActionFailureCategoryList     = 'FailureCategoryList';
    ksActionFailureCategoryAdd      = 'FailureCategoryAdd';
    ksActionFailureCategoryAddPost  = 'FailureCategoryAddPost';
    ksActionFailureCategoryDetails  = 'FailureCategoryDetails';
    ksActionFailureCategoryDoRemove = 'FailureCategoryDoRemove';
    ksActionFailureCategoryEdit     = 'FailureCategoryEdit';
    ksActionFailureCategoryEditPost = 'FailureCategoryEditPost';

    ksActionFailureReasonList       = 'FailureReasonList'
    ksActionFailureReasonAdd        = 'FailureReasonAdd'
    ksActionFailureReasonAddPost    = 'FailureReasonAddPost'
    ksActionFailureReasonDetails    = 'FailureReasonDetails'
    ksActionFailureReasonDoRemove   = 'FailureReasonDoRemove'
    ksActionFailureReasonEdit       = 'FailureReasonEdit'
    ksActionFailureReasonEditPost   = 'FailureReasonEditPost'

    ksActionBuildSrcList            = 'BuildSrcList'
    ksActionBuildSrcAdd             = 'BuildSrcAdd'
    ksActionBuildSrcAddPost         = 'BuildSrcAddPost'
    ksActionBuildSrcClone           = 'BuildSrcClone'
    ksActionBuildSrcDetails         = 'BuildSrcDetails'
    ksActionBuildSrcEdit            = 'BuildSrcEdit'
    ksActionBuildSrcEditPost        = 'BuildSrcEditPost'
    ksActionBuildSrcDoRemove        = 'BuildSrcDoRemove'

    ksActionBuildCategoryList       = 'BuildCategoryList'
    ksActionBuildCategoryAdd        = 'BuildCategoryAdd'
    ksActionBuildCategoryAddPost    = 'BuildCategoryAddPost'
    ksActionBuildCategoryClone      = 'BuildCategoryClone';
    ksActionBuildCategoryDetails    = 'BuildCategoryDetails';
    ksActionBuildCategoryDoRemove   = 'BuildCategoryDoRemove';

    ksActionTestGroupList           = 'TestGroupList'
    ksActionTestGroupAdd            = 'TestGroupAdd'
    ksActionTestGroupAddPost        = 'TestGroupAddPost'
    ksActionTestGroupClone          = 'TestGroupClone'
    ksActionTestGroupDetails        = 'TestGroupDetails'
    ksActionTestGroupDoRemove       = 'TestGroupDoRemove'
    ksActionTestGroupEdit           = 'TestGroupEdit'
    ksActionTestGroupEditPost       = 'TestGroupEditPost'
    ksActionTestCfgRegenQueues      = 'TestCfgRegenQueues'

    ksActionSchedGroupList          = 'SchedGroupList'
    ksActionSchedGroupAdd           = 'SchedGroupAdd';
    ksActionSchedGroupAddPost       = 'SchedGroupAddPost';
    ksActionSchedGroupClone         = 'SchedGroupClone';
    ksActionSchedGroupDetails       = 'SchedGroupDetails';
    ksActionSchedGroupDoRemove      = 'SchedGroupDel';
    ksActionSchedGroupEdit          = 'SchedGroupEdit';
    ksActionSchedGroupEditPost      = 'SchedGroupEditPost';
    ## @}

    def __init__(self, oSrvGlue):
        WuiDispatcherBase.__init__(self, oSrvGlue, self.ksScriptName);

        self._sTemplate     = 'template.html';

        # Use short form to avoid hitting the right margin (130) when using lambda.
        d = self._dDispatch;  # pylint: disable=C0103

        #
        # System Log actions.
        #
        d[self.ksActionSystemLogList]           = lambda: self._actionGenericListing(SystemLogLogic, WuiAdminSystemLogList)

        #
        # User Account actions.
        #
        d[self.ksActionUserList]                = lambda: self._actionGenericListing(UserAccountLogic, WuiUserAccountList)
        d[self.ksActionUserAdd]                 = lambda: self._actionGenericFormAdd(UserAccountData, WuiUserAccount)
        d[self.ksActionUserEdit]                = lambda: self._actionGenericFormEdit(UserAccountData, WuiUserAccount,
                                                                                      UserAccountData.ksParam_uid);
        d[self.ksActionUserAddPost]             = lambda: self._actionGenericFormAddPost(UserAccountData, UserAccountLogic,
                                                                                         WuiUserAccount, self.ksActionUserList)
        d[self.ksActionUserEditPost]            = lambda: self._actionGenericFormEditPost(UserAccountData, UserAccountLogic,
                                                                                          WuiUserAccount, self.ksActionUserList)
        d[self.ksActionUserDelPost]             = lambda: self._actionGenericDoRemove(UserAccountLogic,
                                                                                      UserAccountData.ksParam_uid,
                                                                                      self.ksActionUserList)

        #
        # TestBox actions.
        #
        d[self.ksActionTestBoxList]             = lambda: self._actionGenericListing(TestBoxLogic, WuiTestBoxList);
        d[self.ksActionTestBoxListPost]         = self._actionTestBoxListPost;
        d[self.ksActionTestBoxAdd]              = lambda: self._actionGenericFormAdd(TestBoxData, WuiTestBox);
        d[self.ksActionTestBoxAddPost]          = lambda: self._actionGenericFormAddPost(TestBoxData, TestBoxLogic,
                                                                                         WuiTestBox, self.ksActionTestBoxList);
        d[self.ksActionTestBoxDetails]          = lambda: self._actionGenericFormDetails(TestBoxData, TestBoxLogic, WuiTestBox,
                                                                                         'idTestBox', 'idGenTestBox');
        d[self.ksActionTestBoxEdit]             = lambda: self._actionGenericFormEdit(TestBoxData, WuiTestBox,
                                                                                      TestBoxData.ksParam_idTestBox);
        d[self.ksActionTestBoxEditPost]         = lambda: self._actionGenericFormEditPost(TestBoxData, TestBoxLogic,
                                                                                          WuiTestBox, self.ksActionTestBoxList);
        d[self.ksActionTestBoxRemovePost]       = lambda: self._actionGenericDoRemove(TestBoxLogic,
                                                                                      TestBoxData.ksParam_idTestBox,
                                                                                      self.ksActionTestBoxList)
        d[self.ksActionTestBoxesRegenQueues]    = self._actionRegenQueuesCommon;

        #
        # Test Case actions.
        #
        d[self.ksActionTestCaseList]            = lambda: self._actionGenericListing(TestCaseLogic, WuiTestCaseList);
        d[self.ksActionTestCaseAdd]             = lambda: self._actionGenericFormAdd(TestCaseDataEx, WuiTestCase);
        d[self.ksActionTestCaseAddPost]         = lambda: self._actionGenericFormAddPost(TestCaseDataEx, TestCaseLogic,
                                                                                         WuiTestCase, self.ksActionTestCaseList);
        d[self.ksActionTestCaseClone]           = lambda: self._actionGenericFormClone(  TestCaseDataEx, WuiTestCase,
                                                                                         'idTestCase', 'idGenTestCase');
        d[self.ksActionTestCaseDetails]         = lambda: self._actionGenericFormDetails(TestCaseDataEx, TestCaseLogic,
                                                                                         WuiTestCase, 'idTestCase',
                                                                                         'idGenTestCase');
        d[self.ksActionTestCaseEdit]            = lambda: self._actionGenericFormEdit(TestCaseDataEx, WuiTestCase,
                                                                                      TestCaseDataEx.ksParam_idTestCase);
        d[self.ksActionTestCaseEditPost]        = lambda: self._actionGenericFormEditPost(TestCaseDataEx, TestCaseLogic,
                                                                                          WuiTestCase, self.ksActionTestCaseList);
        d[self.ksActionTestCaseDoRemove]        = lambda: self._actionGenericDoRemove(TestCaseLogic,
                                                                                      TestCaseData.ksParam_idTestCase,
                                                                                      self.ksActionTestCaseList);

        #
        # Global Resource actions
        #
        d[self.ksActionGlobalRsrcShowAll]       = lambda: self._actionGenericListing(GlobalResourceLogic, WuiGlobalResourceList)
        d[self.ksActionGlobalRsrcShowAdd]       = lambda: self._actionGlobalRsrcShowAddEdit(WuiAdmin.ksActionGlobalRsrcAdd)
        d[self.ksActionGlobalRsrcShowEdit]      = lambda: self._actionGlobalRsrcShowAddEdit(WuiAdmin.ksActionGlobalRsrcEdit)
        d[self.ksActionGlobalRsrcAdd]           = lambda: self._actionGlobalRsrcAddEdit(WuiAdmin.ksActionGlobalRsrcAdd)
        d[self.ksActionGlobalRsrcEdit]          = lambda: self._actionGlobalRsrcAddEdit(WuiAdmin.ksActionGlobalRsrcEdit)
        d[self.ksActionGlobalRsrcDel]           = lambda: self._actionGenericDoDelOld(GlobalResourceLogic,
                                                                                      GlobalResourceData.ksParam_idGlobalRsrc,
                                                                                      self.ksActionGlobalRsrcShowAll)

        #
        # Build Source actions
        #
        d[self.ksActionBuildSrcList]        = lambda: self._actionGenericListing(BuildSourceLogic, WuiAdminBuildSrcList)
        d[self.ksActionBuildSrcAdd]         = lambda: self._actionGenericFormAdd(BuildSourceData, WuiAdminBuildSrc);
        d[self.ksActionBuildSrcAddPost]     = lambda: self._actionGenericFormAddPost(BuildSourceData, BuildSourceLogic,
                                                                                     WuiAdminBuildSrc, self.ksActionBuildSrcList);
        d[self.ksActionBuildSrcClone]       = lambda: self._actionGenericFormClone(  BuildSourceData, WuiAdminBuildSrc,
                                                                                     'idBuildSrc');
        d[self.ksActionBuildSrcDetails]     = lambda: self._actionGenericFormDetails(BuildSourceData, BuildSourceLogic,
                                                                                     WuiAdminBuildSrc, 'idBuildSrc');
        d[self.ksActionBuildSrcDoRemove]    = lambda: self._actionGenericDoRemove(BuildSourceLogic,
                                                                                  BuildSourceData.ksParam_idBuildSrc,
                                                                                  self.ksActionBuildSrcList);
        d[self.ksActionBuildSrcEdit]        = lambda: self._actionGenericFormEdit(BuildSourceData, WuiAdminBuildSrc,
                                                                                  BuildSourceData.ksParam_idBuildSrc);
        d[self.ksActionBuildSrcEditPost]    = lambda: self._actionGenericFormEditPost(BuildSourceData, BuildSourceLogic,
                                                                                      WuiAdminBuildSrc,
                                                                                      self.ksActionBuildSrcList);


        #
        # Build actions
        #
        d[self.ksActionBuildList]           = lambda: self._actionGenericListing(BuildLogic, WuiAdminBuildList)
        d[self.ksActionBuildAdd]            = lambda: self._actionGenericFormAdd(BuildData, WuiAdminBuild)
        d[self.ksActionBuildAddPost]        = lambda: self._actionGenericFormAddPost(BuildData, BuildLogic, WuiAdminBuild,
                                                                                     self.ksActionBuildList)
        d[self.ksActionBuildClone]          = lambda: self._actionGenericFormClone(  BuildData, WuiAdminBuild, 'idBuild');
        d[self.ksActionBuildDetails]        = lambda: self._actionGenericFormDetails(BuildData, BuildLogic,
                                                                                     WuiAdminBuild, 'idBuild');
        d[self.ksActionBuildDoRemove]       = lambda: self._actionGenericDoRemove(BuildLogic, BuildData.ksParam_idBuild,
                                                                                  self.ksActionBuildList);
        d[self.ksActionBuildEdit]           = lambda: self._actionGenericFormEdit(BuildData, WuiAdminBuild,
                                                                                  BuildData.ksParam_idBuild);
        d[self.ksActionBuildEditPost]       = lambda: self._actionGenericFormEditPost(BuildData, BuildLogic, WuiAdminBuild,
                                                                                      self.ksActionBuildList)

        #
        # Build Black List actions
        #
        d[self.ksActionBuildBlacklist]          = lambda: self._actionGenericListing(BuildBlacklistLogic,
                                                                                     WuiAdminListOfBlacklistItems);
        d[self.ksActionBuildBlacklistAdd]       = lambda: self._actionGenericFormAdd(BuildBlacklistData, WuiAdminBuildBlacklist);
        d[self.ksActionBuildBlacklistAddPost]   = lambda: self._actionGenericFormAddPost(BuildBlacklistData, BuildBlacklistLogic,
                                                                                         WuiAdminBuildBlacklist,
                                                                                         self.ksActionBuildBlacklist);
        d[self.ksActionBuildBlacklistClone]     = lambda: self._actionGenericFormClone(BuildBlacklistData,
                                                                                       WuiAdminBuildBlacklist,
                                                                                       'idBlacklisting');
        d[self.ksActionBuildBlacklistDetails]   = lambda: self._actionGenericFormDetails(BuildBlacklistData,
                                                                                         BuildBlacklistLogic,
                                                                                         WuiAdminBuildBlacklist,
                                                                                         'idBlacklisting');
        d[self.ksActionBuildBlacklistDoRemove]  = lambda: self._actionGenericDoRemove(BuildBlacklistLogic,
                                                                                      BuildBlacklistData.ksParam_idBlacklisting,
                                                                                      self.ksActionBuildBlacklist)
        d[self.ksActionBuildBlacklistEdit]      = lambda: self._actionGenericFormEdit(BuildBlacklistData,
                                                                                      WuiAdminBuildBlacklist,
                                                                                      BuildBlacklistData.ksParam_idBlacklisting);
        d[self.ksActionBuildBlacklistEditPost]  = lambda: self._actionGenericFormEditPost(BuildBlacklistData,
                                                                                          BuildBlacklistLogic,
                                                                                          WuiAdminBuildBlacklist,
                                                                                          self.ksActionBuildBlacklist)


        #
        # Failure Category actions
        #
        d[self.ksActionFailureCategoryList] = \
            lambda: self._actionGenericListing(FailureCategoryLogic, WuiFailureCategoryList);
        d[self.ksActionFailureCategoryAdd] = \
            lambda: self._actionGenericFormAdd(FailureCategoryData, WuiFailureCategory);
        d[self.ksActionFailureCategoryAddPost] = \
            lambda: self._actionGenericFormAddPost(FailureCategoryData, FailureCategoryLogic, WuiFailureCategory,
                                                   self.ksActionFailureCategoryList)
        d[self.ksActionFailureCategoryDetails] = \
            lambda: self._actionGenericFormDetails(FailureCategoryData, FailureCategoryLogic, WuiFailureCategory);

        d[self.ksActionFailureCategoryDoRemove] = \
            lambda: self._actionGenericDoRemove(FailureCategoryLogic, FailureCategoryData.ksParam_idFailureCategory,
                                                self.ksActionFailureCategoryList);
        d[self.ksActionFailureCategoryEdit] = \
            lambda: self._actionGenericFormEdit(FailureCategoryData, WuiFailureCategory,
                                                FailureCategoryData.ksParam_idFailureCategory);
        d[self.ksActionFailureCategoryEditPost] = \
            lambda: self._actionGenericFormEditPost(FailureCategoryData, FailureCategoryLogic, WuiFailureCategory,
                                                    self.ksActionFailureCategoryList);

        #
        # Failure Reason actions
        #
        d[self.ksActionFailureReasonList] = \
            lambda: self._actionGenericListing(FailureReasonLogic, WuiAdminFailureReasonList)

        d[self.ksActionFailureReasonAdd] = \
            lambda: self._actionGenericFormAdd(FailureReasonData, WuiAdminFailureReason);
        d[self.ksActionFailureReasonAddPost] = \
            lambda: self._actionGenericFormAddPost(FailureReasonData, FailureReasonLogic, WuiAdminFailureReason,
                                                   self.ksActionFailureReasonList);
        d[self.ksActionFailureReasonDetails] = \
            lambda: self._actionGenericFormDetails(FailureReasonData, FailureReasonLogic, WuiAdminFailureReason);
        d[self.ksActionFailureReasonDoRemove] = \
            lambda: self._actionGenericDoRemove(FailureReasonLogic, FailureReasonData.ksParam_idFailureReason,
                                                self.ksActionFailureReasonList);
        d[self.ksActionFailureReasonEdit] = \
            lambda: self._actionGenericFormEdit(FailureReasonData, WuiAdminFailureReason);

        d[self.ksActionFailureReasonEditPost] = \
            lambda: self._actionGenericFormEditPost(FailureReasonData, FailureReasonLogic, WuiAdminFailureReason,\
                                                    self.ksActionFailureReasonList)

        #
        # Build Category actions
        #
        d[self.ksActionBuildCategoryList]       = lambda: self._actionGenericListing(BuildCategoryLogic, WuiAdminBuildCatList);
        d[self.ksActionBuildCategoryAdd]        = lambda: self._actionGenericFormAdd(BuildCategoryData, WuiAdminBuildCat);
        d[self.ksActionBuildCategoryAddPost]    = lambda: self._actionGenericFormAddPost(BuildCategoryData, BuildCategoryLogic,
                                                                                     WuiAdminBuildCat,
                                                                                     self.ksActionBuildCategoryList);
        d[self.ksActionBuildCategoryClone]      = lambda: self._actionGenericFormClone(  BuildCategoryData, WuiAdminBuildCat,
                                                                                         'idBuildCategory');
        d[self.ksActionBuildCategoryDetails]    = lambda: self._actionGenericFormDetails(BuildCategoryData, BuildCategoryLogic,
                                                                                         WuiAdminBuildCat, 'idBuildCategory');
        d[self.ksActionBuildCategoryDoRemove]   = lambda: self._actionGenericDoRemove(BuildCategoryLogic,
                                                                                  BuildCategoryData.ksParam_idBuildCategory,
                                                                                  self.ksActionBuildCategoryList)

        #
        # Test Group actions
        #
        d[self.ksActionTestGroupList]       = lambda: self._actionGenericListing(TestGroupLogic, WuiTestGroupList);
        d[self.ksActionTestGroupAdd]        = lambda: self._actionGenericFormAdd(TestGroupDataEx, WuiTestGroup);
        d[self.ksActionTestGroupAddPost]    = lambda: self._actionGenericFormAddPost(TestGroupDataEx, TestGroupLogic,
                                                                                     WuiTestGroup, self.ksActionTestGroupList);
        d[self.ksActionTestGroupClone]      = lambda: self._actionGenericFormClone(  TestGroupDataEx, WuiTestGroup,
                                                                                     'idTestGroup');
        d[self.ksActionTestGroupDetails]    = lambda: self._actionGenericFormDetails(TestGroupDataEx, TestGroupLogic,
                                                                                     WuiTestGroup, 'idTestGroup');
        d[self.ksActionTestGroupEdit]       = lambda: self._actionGenericFormEdit(TestGroupDataEx, WuiTestGroup,
                                                                                  TestGroupDataEx.ksParam_idTestGroup);
        d[self.ksActionTestGroupEditPost]   = lambda: self._actionGenericFormEditPost(TestGroupDataEx, TestGroupLogic,
                                                                                      WuiTestGroup, self.ksActionTestGroupList);
        d[self.ksActionTestGroupDoRemove]   = lambda: self._actionGenericDoRemove(TestGroupLogic,
                                                                                  TestGroupDataEx.ksParam_idTestGroup,
                                                                                  self.ksActionTestGroupList)
        d[self.ksActionTestCfgRegenQueues]  = self._actionRegenQueuesCommon;

        #
        # Scheduling Group actions
        #
        d[self.ksActionSchedGroupList]      = lambda: self._actionGenericListing(SchedGroupLogic, WuiAdminSchedGroupList)
        d[self.ksActionSchedGroupAdd]       = lambda: self._actionGenericFormAdd(SchedGroupDataEx, WuiSchedGroup);
        d[self.ksActionSchedGroupClone]     = lambda: self._actionGenericFormClone(  SchedGroupDataEx, WuiSchedGroup,
                                                                                     'idSchedGroup');
        d[self.ksActionSchedGroupDetails]   = lambda: self._actionGenericFormDetails(SchedGroupDataEx, SchedGroupLogic,
                                                                                     WuiSchedGroup, 'idSchedGroup');
        d[self.ksActionSchedGroupEdit]      = lambda: self._actionGenericFormEdit(SchedGroupDataEx, WuiSchedGroup,
                                                                                  SchedGroupData.ksParam_idSchedGroup);
        d[self.ksActionSchedGroupAddPost]   = lambda: self._actionGenericFormAddPost(SchedGroupDataEx, SchedGroupLogic,
                                                                                     WuiSchedGroup, self.ksActionSchedGroupList);
        d[self.ksActionSchedGroupEditPost]  = lambda: self._actionGenericFormEditPost(SchedGroupDataEx, SchedGroupLogic,
                                                                                      WuiSchedGroup, self.ksActionSchedGroupList);
        d[self.ksActionSchedGroupDoRemove]  = lambda: self._actionGenericDoRemove(SchedGroupLogic,
                                                                                  SchedGroupData.ksParam_idSchedGroup,
                                                                                  self.ksActionSchedGroupList)

        self._aaoMenus = \
        [
            [
                'Builds',       self._sActionUrlBase + self.ksActionBuildList,
                [
                    [ 'Builds',                 self._sActionUrlBase + self.ksActionBuildList ],
                    [ 'Blacklist',              self._sActionUrlBase + self.ksActionBuildBlacklist ],
                    [ 'Build Sources',          self._sActionUrlBase + self.ksActionBuildSrcList ],
                    [ 'Build Categories',       self._sActionUrlBase + self.ksActionBuildCategoryList ],
                    [ 'New Build',              self._sActionUrlBase + self.ksActionBuildAdd ],
                    [ 'New Blacklisting',       self._sActionUrlBase + self.ksActionBuildBlacklistAdd ],
                    [ 'New Build Source',       self._sActionUrlBase + self.ksActionBuildSrcAdd],
                    [ 'New Build Category',     self._sActionUrlBase + self.ksActionBuildCategoryAdd ],
                ]
            ],
            [
                'Failure Reasons',       self._sActionUrlBase + self.ksActionFailureReasonList,
                [
                    [ 'Failure Categories',     self._sActionUrlBase + self.ksActionFailureCategoryList ],
                    [ 'Failure Reasons',        self._sActionUrlBase + self.ksActionFailureReasonList ],
                    [ 'New Failure Category',   self._sActionUrlBase + self.ksActionFailureCategoryAdd ],
                    [ 'New Failure Reason',     self._sActionUrlBase + self.ksActionFailureReasonAdd ],
                ]
            ],
            [
                'System',      self._sActionUrlBase + self.ksActionSystemLogList,
                [
                    [ 'System Log',             self._sActionUrlBase + self.ksActionSystemLogList ],
                    [ 'User Accounts',          self._sActionUrlBase + self.ksActionUserList ],
                    [ 'New User',               self._sActionUrlBase + self.ksActionUserAdd ],
                ]
            ],
            [
                'TestBoxes',   self._sActionUrlBase + self.ksActionTestBoxList,
                [
                    [ 'TestBoxes',              self._sActionUrlBase + self.ksActionTestBoxList ],
                    [ 'Scheduling Groups',      self._sActionUrlBase + self.ksActionSchedGroupList ],
                    [ 'New TestBox',            self._sActionUrlBase + self.ksActionTestBoxAdd ],
                    [ 'New Scheduling Group',   self._sActionUrlBase + self.ksActionSchedGroupAdd ],
                    [ 'Regenerate All Scheduling Queues', self._sActionUrlBase + self.ksActionTestBoxesRegenQueues ],
                ]
            ],
            [
                'Test Config', self._sActionUrlBase + self.ksActionTestGroupList,
                [
                    [ 'Test Cases',             self._sActionUrlBase + self.ksActionTestCaseList ],
                    [ 'Test Groups',            self._sActionUrlBase + self.ksActionTestGroupList ],
                    [ 'Global Resources',       self._sActionUrlBase + self.ksActionGlobalRsrcShowAll ],
                    [ 'New Test Case',          self._sActionUrlBase + self.ksActionTestCaseAdd ],
                    [ 'New Test Group',         self._sActionUrlBase + self.ksActionTestGroupAdd ],
                    [ 'New Global Resource',    self._sActionUrlBase + self.ksActionGlobalRsrcShowAdd ],
                    [ 'Regenerate All Scheduling Queues', self._sActionUrlBase + self.ksActionTestCfgRegenQueues ],
                ]
            ],
            [
                '> Test Results', 'index.py?' + webutils.encodeUrlParams(self._dDbgParams), []
            ],
        ];


    def _actionDefault(self):
        """Show the default admin page."""
        self._sAction = self.ksActionTestBoxList;
        return self._actionGenericListing(TestBoxLogic, WuiTestBoxList);

    def _actionGenericDoDelOld(self, oCoreObjectLogic, sCoreObjectIdFieldName, sRedirectAction):
        """
        Delete entry (using oLogicType.remove).

        @param oCoreObjectLogic         A *Logic class

        @param sCoreObjectIdFieldName   Name of HTTP POST variable that
                                        contains object ID information

        @param sRedirectAction          An action for redirect user to
                                        in case of operation success
        """
        iCoreDataObjectId = self.getStringParam(sCoreObjectIdFieldName) # STRING?!?!
        self._checkForUnknownParameters()

        try:
            self._sPageTitle  = None
            self._sPageBody   = None
            self._sRedirectTo = self._sActionUrlBase + sRedirectAction
            return oCoreObjectLogic(self._oDb).remove(self._oCurUser.uid, iCoreDataObjectId)
        except Exception as oXcpt:
            self._oDb.rollback();
            self._sPageTitle  = 'Unable to delete record'
            self._sPageBody   = str(oXcpt);
            if config.g_kfDebugDbXcpt:
                self._sPageBody += cgitb.html(sys.exc_info());
            self._sRedirectTo = None

        return False

    def _actionGenericDoRemove(self, oLogicType, sParamId, sRedirAction):
        """
        Delete entry (using oLogicType.removeEntry).

        oLogicType is a class that implements addEntry.

        sParamId is the name (ksParam_...) of the HTTP variable hold the ID of
        the database entry to delete.

        sRedirAction is what action to redirect to on success.
        """
        idEntry = self.getIntParam(sParamId, iMin = 1, iMax = 0x7fffffe)
        fCascade = self.getBoolParam('fCascadeDelete', False);
        self._checkForUnknownParameters()

        try:
            self._sPageTitle  = None
            self._sPageBody   = None
            self._sRedirectTo = self._sActionUrlBase + sRedirAction;
            return oLogicType(self._oDb).removeEntry(self._oCurUser.uid, idEntry, fCascade = fCascade, fCommit = True);
        except Exception as oXcpt:
            self._oDb.rollback();
            self._sPageTitle  = 'Unable to delete entry';
            self._sPageBody   = str(oXcpt);
            if config.g_kfDebugDbXcpt:
                self._sPageBody += cgitb.html(sys.exc_info());
            self._sRedirectTo = None;
        return False;


    #
    # System Category.
    #

    # (all generic)

    #
    # TestBox & Scheduling Category.
    #

    def _actionTestBoxListPost(self):
        """Actions on a list of testboxes."""

        # Parameters.
        aidTestBoxes = self.getListOfIntParams(TestBoxData.ksParam_idTestBox, iMin = 1, aiDefaults = []);
        sListAction  = self.getStringParam(self.ksParamListAction);
        if sListAction in [asDesc[0] for asDesc in WuiTestBoxList.kasTestBoxActionDescs]:
            idAction = None;
        else:
            asActionPrefixes = [ 'setgroup-', ];
            i = 0;
            while i < len(asActionPrefixes) and not sListAction.startswith(asActionPrefixes[i]):
                i += 1;
            if i >= len(asActionPrefixes):
                raise WuiException('Parameter "%s" has an invalid value: "%s"' % (self.ksParamListAction, sListAction,));
            idAction = sListAction[len(asActionPrefixes[i]):];
            if not idAction.isdigit():
                raise WuiException('Parameter "%s" has an invalid value: "%s"' % (self.ksParamListAction, sListAction,));
            idAction = int(idAction);
            sListAction = sListAction[:len(asActionPrefixes[i]) - 1];
        self._checkForUnknownParameters();


        # Take action.
        if sListAction is 'none':
            pass;
        else:
            oLogic = TestBoxLogic(self._oDb);
            aoTestBoxes = []
            for idTestBox in aidTestBoxes:
                aoTestBoxes.append(TestBoxData().initFromDbWithId(self._oDb, idTestBox));

            if sListAction in [ 'enable', 'disable' ]:
                fEnable = sListAction == 'enable';
                for oTestBox in aoTestBoxes:
                    if oTestBox.fEnabled != fEnable:
                        oTestBox.fEnabled = fEnable;
                        oLogic.editEntry(oTestBox, self._oCurUser.uid, fCommit = False);
            elif sListAction == 'setgroup':
                for oTestBox in aoTestBoxes:
                    if oTestBox.idSchedGroup != idAction:
                        oTestBox.idSchedGroup = idAction;
                        oLogic.editEntry(oTestBox, self._oCurUser.uid, fCommit = False);
            else:
                for oTestBox in aoTestBoxes:
                    if oTestBox.enmPendingCmd != sListAction:
                        oTestBox.enmPendingCmd = sListAction;
                        oLogic.editEntry(oTestBox, self._oCurUser.uid, fCommit = False);
            self._oDb.commit();

        # Re-display the list.
        self._sPageTitle  = None;
        self._sPageBody   = None;
        self._sRedirectTo = self._sActionUrlBase + self.ksActionTestBoxList;
        return True;

    ## @todo scheduling groups code goes here...

    def _actionRegenQueuesCommon(self):
        """
        Common code for ksActionTestBoxesRegenQueues and ksActionTestCfgRegenQueues.

        Too lazy to put this in some separate place right now.
        """
        self._checkForUnknownParameters();
        ## @todo should also be changed to a POST with a confirmation dialog preceeding it.

        self._sPageTitle = 'Regenerate All Scheduling Queues';
        self._sPageBody  = '';
        aoGroups = SchedGroupLogic(self._oDb).getAll();
        for oGroup in aoGroups:
            self._sPageBody += '<h3>%s (ID %#d)</h3>' % (webutils.escapeElem(oGroup.sName), oGroup.idSchedGroup);
            try:
                (aoErrors, asMessages) = SchedulerBase.recreateQueue(self._oDb, self._oCurUser.uid, oGroup.idSchedGroup, 2);
            except Exception as oXcpt:
                self._oDb.rollback();
                self._sPageBody += '<p>SchedulerBase.recreateQueue threw an exception: %s</p>' \
                                % (webutils.escapeElem(str(oXcpt)),);
                self._sPageBody += cgitb.html(sys.exc_info());
            else:
                if len(aoErrors) == 0:
                    self._sPageBody += '<p>Successfully regenerated.</p>';
                else:
                    for oError in aoErrors:
                        if oError[1] is None:
                            self._sPageBody += '<p>%s.</p>' % (webutils.escapeElem(oError[0]),);
                        ## @todo links.
                        #elif isinstance(oError[1], TestGroupData):
                        #    self._sPageBody += '<p>%s.</p>' % (webutils.escapeElem(oError[0]),);
                        #elif isinstance(oError[1], TestGroupCase):
                        #    self._sPageBody += '<p>%s.</p>' % (webutils.escapeElem(oError[0]),);
                        else:
                            self._sPageBody += '<p>%s. [Cannot link to %s]</p>' \
                                             % (webutils.escapeElem(oError[0]), webutils.escapeElem(str(oError[1])));
                for sMsg in asMessages:
                    self._sPageBody += '<p>%s<p>\n' % (webutils.escapeElem(sMsg),);
        return True;



    #
    # Test Config Category.
    #

    def _actionGlobalRsrcShowAddEdit(self, sAction): # pylint: disable=C0103
        """Show Global Resource creation or edit dialog"""

        oGlobalResourceLogic = GlobalResourceLogic(self._oDb)
        if sAction == WuiAdmin.ksActionGlobalRsrcEdit:
            idGlobalRsrc = self.getIntParam(GlobalResourceData.ksParam_idGlobalRsrc, iDefault = -1)
            oData = oGlobalResourceLogic.getById(idGlobalRsrc)
        else:
            oData = GlobalResourceData()
            oData.convertToParamNull()

        self._checkForUnknownParameters()

        oContent = WuiGlobalResource(oData)
        (self._sPageTitle, self._sPageBody) = oContent.showAddModifyPage(sAction)

        return True

    def _actionGlobalRsrcAddEdit(self, sAction):
        """Add or modify Global Resource record"""

        oData = GlobalResourceData()
        oData.initFromParams(self, fStrict=True)

        self._checkForUnknownParameters()

        if self._oSrvGlue.getMethod() != 'POST':
            raise WuiException('Expected "POST" request, got "%s"' % (self._oSrvGlue.getMethod(),))

        oGlobalResourceLogic = GlobalResourceLogic(self._oDb)
        dErrors = oData.validateAndConvert(self._oDb);
        if len(dErrors) == 0:
            if sAction == WuiAdmin.ksActionGlobalRsrcAdd:
                oGlobalResourceLogic.addGlobalResource(self._oCurUser.uid, oData)
            elif sAction == WuiAdmin.ksActionGlobalRsrcEdit:
                idGlobalRsrc = self.getStringParam(GlobalResourceData.ksParam_idGlobalRsrc)
                oGlobalResourceLogic.editGlobalResource(self._oCurUser.uid, idGlobalRsrc, oData)
            else:
                raise WuiException('Invalid parameter.')
            self._sPageTitle  = None;
            self._sPageBody   = None;
            self._sRedirectTo = self._sActionUrlBase + self.ksActionGlobalRsrcShowAll;
        else:
            oContent = WuiGlobalResource(oData)
            (self._sPageTitle, self._sPageBody) = oContent.showAddModifyPage(sAction, dErrors=dErrors)

        return True

    def _generatePage(self):
        """Override parent handler in order to change page titte"""
        if self._sPageTitle is not None:
            self._sPageTitle = 'Test Manager Admin - ' + self._sPageTitle

        return WuiDispatcherBase._generatePage(self)
