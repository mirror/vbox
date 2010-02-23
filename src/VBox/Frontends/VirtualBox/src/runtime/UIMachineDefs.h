/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Defines for Virtual Machine classes
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef __UIMachineDefs_h__
#define __UIMachineDefs_h__

/* Machine states enum: */
enum UIVisualStateType
{
    UIVisualStateType_Normal,
    UIVisualStateType_Fullscreen,
    UIVisualStateType_Seamless
};

/* Machine elements enum: */
enum UIVisualElement
{
    UIVisualElement_WindowCaption         = 0x01,
    UIVisualElement_MouseIntegrationStuff = 0x02,
    UIVisualElement_PauseStuff            = 0x04,
    UIVisualElement_HDStuff               = 0x08,
    UIVisualElement_CDStuff               = 0x10,
    UIVisualElement_FDStuff               = 0x20,
    UIVisualElement_NetworkStuff          = 0x40,
    UIVisualElement_USBStuff              = 0x80,
    UIVisualElement_VRDPStuff             = 0x100,
    UIVisualElement_SharedFolderStuff     = 0x200,
    UIVisualElement_VirtualizationStuff   = 0x400,
    UIVisualElement_AllStuff              = 0xFFFF
};

/* Mouse states enum: */
enum UIMouseStateType
{
    UIMouseStateType_MouseCaptured         = 0x01,
    UIMouseStateType_MouseAbsolute         = 0x02,
    UIMouseStateType_MouseAbsoluteDisabled = 0x04,
    UIMouseStateType_MouseNeedsHostCursor  = 0x08
};

/* Machine View states enum: */
enum UIViewStateType
{
    UIViewStateType_KeyboardCaptured = 0x01,
    UIViewStateType_HostKeyPressed   = 0x02
};

#endif // __UIMachineDefs_h__

