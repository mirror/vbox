/* $Id$ */
/** @file
 * VBox Qt GUI - UIVirtualBoxManager definitions.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_manager_UIChooserDefs_h
#define FEQT_INCLUDED_SRC_manager_UIChooserDefs_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


/** Virtual machine item types. */
enum UIVirtualMachineItemType
{
    UIVirtualMachineItemType_Local,
    UIVirtualMachineItemType_CloudFake,
    UIVirtualMachineItemType_CloudReal
};


/** Fake cloud virtual machine item states. */
enum UIFakeCloudVirtualMachineItemState
{
    UIFakeCloudVirtualMachineItemState_NotApplicable,
    UIFakeCloudVirtualMachineItemState_Loading,
    UIFakeCloudVirtualMachineItemState_Done
};


#endif /* !FEQT_INCLUDED_SRC_manager_UIChooserDefs_h */
