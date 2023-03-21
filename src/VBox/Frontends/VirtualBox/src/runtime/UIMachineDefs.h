/* $Id$ */
/** @file
 * VBox Qt GUI - Defines for Virtual Machine classes.
 */

/*
 * Copyright (C) 2010-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef FEQT_INCLUDED_SRC_runtime_UIMachineDefs_h
#define FEQT_INCLUDED_SRC_runtime_UIMachineDefs_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QIcon>
#include <QString>
#include <QUuid>

/* GUI includes: */
#include "UIDefs.h"

/* Other VBox includes: */
#include <iprt/cdefs.h>

/** Machine window visual element types. */
enum UIVisualElement
{
    UIVisualElement_WindowTitle           = RT_BIT(0),
    UIVisualElement_MouseIntegrationStuff = RT_BIT(1),
    UIVisualElement_IndicatorPoolStuff    = RT_BIT(2),
    UIVisualElement_FeaturesStuff         = RT_BIT(12),
#ifndef VBOX_WS_MAC
    UIVisualElement_MiniToolBar           = RT_BIT(13),
#endif
    UIVisualElement_AllStuff              = 0xFFFF
};

/** Mouse state types. */
enum UIMouseStateType
{
    UIMouseStateType_MouseCaptured         = RT_BIT(0),
    UIMouseStateType_MouseAbsolute         = RT_BIT(1),
    UIMouseStateType_MouseAbsoluteDisabled = RT_BIT(2),
    UIMouseStateType_MouseNeedsHostCursor  = RT_BIT(3)
};

/** Keyboard state types. */
enum UIKeyboardStateType
{
    UIKeyboardStateType_KeyboardUnavailable     = 0,
    UIKeyboardStateType_KeyboardAvailable       = RT_BIT(0),
    UIKeyboardStateType_KeyboardCaptured        = RT_BIT(1),
    UIKeyboardStateType_HostKeyPressed          = RT_BIT(2),
    UIKeyboardStateType_HostKeyPressedInsertion = RT_BIT(3)
};

/** Robust struct to bring storage device info to machine-logic. */
struct StorageDeviceInfo
{
    QString      m_strControllerName;
    StorageSlot  m_guiStorageSlot;
    QIcon        m_icon;
};

/** Robust struct to bring USB device info to machine-logic. */
struct USBDeviceInfo
{
    QUuid    m_uId;
    QString  m_strName;
    QString  m_strToolTip;
    bool     m_fIsChecked;
    bool     m_fIsEnabled;
};

/** Robust struct to bring web cam device info to machine-logic. */
struct WebcamDeviceInfo
{
    QString  m_strName;
    QString  m_strPath;
    QString  m_strToolTip;
    bool     m_fIsChecked;
};

#endif /* !FEQT_INCLUDED_SRC_runtime_UIMachineDefs_h */
