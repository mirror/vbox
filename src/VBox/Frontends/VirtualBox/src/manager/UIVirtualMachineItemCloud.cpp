/* $Id$ */
/** @file
 * VBox Qt GUI - UIVirtualMachineItemCloud class implementation.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* GUI includes: */
#include "UICommon.h"
#include "UIConverter.h"
#include "UIIconPool.h"
#include "UIVirtualMachineItemCloud.h"


UIVirtualMachineItemCloud::UIVirtualMachineItemCloud()
    : UIVirtualMachineItem(ItemType_CloudFake)
    , m_enmFakeCloudItemState(FakeCloudItemState_Loading)
{
    recache();
}

UIVirtualMachineItemCloud::UIVirtualMachineItemCloud(const QString &strName)
    : UIVirtualMachineItem(ItemType_CloudReal)
    , m_enmFakeCloudItemState(FakeCloudItemState_NotApplicable)
{
    m_strName = strName;
    recache();
}

UIVirtualMachineItemCloud::~UIVirtualMachineItemCloud()
{
}

void UIVirtualMachineItemCloud::recache()
{
    /* Determine attributes which are always available: */
    /// @todo is there something?

    /* Now determine whether VM is accessible: */
    m_fAccessible = true;
    if (m_fAccessible)
    {
        /* Reset last access error information: */
        m_comAccessError = CVirtualBoxErrorInfo();

        /* Determine own VM attributes: */
        m_strOSTypeId = "Other";

        /* Determine VM states: */
        m_enmMachineState = KMachineState_PoweredOff;
        m_strMachineStateName = gpConverter->toString(m_enmMachineState);
        if (itemType() == ItemType_CloudFake)
        {
            switch (m_enmFakeCloudItemState)
            {
                case UIVirtualMachineItemCloud::FakeCloudItemState_Loading:
                    m_machineStateIcon = UIIconPool::iconSet(":/state_loading_16px.png");
                    break;
                case UIVirtualMachineItemCloud::FakeCloudItemState_Done:
                    m_machineStateIcon = UIIconPool::iconSet(":/vm_new_16px.png");
                    break;
                default:
                    break;
            }
        }
        else
            m_machineStateIcon = gpConverter->toIcon(m_enmMachineState);

        /* Determine configuration access level: */
        m_enmConfigurationAccessLevel = ConfigurationAccessLevel_Null;

        /* Determine whether we should show this VM details: */
        m_fHasDetails = false;
    }
    else
    {
        /// @todo handle inaccessible cloud VM
    }

    /* Recache item pixmap: */
    recachePixmap();

    /* Retranslate finally: */
    retranslateUi();
}

void UIVirtualMachineItemCloud::recachePixmap()
{
    /* If machine is accessible: */
    if (m_fAccessible)
    {
        /* We are using icon corresponding to cached guest OS type: */
        if (   itemType() == ItemType_CloudFake
            && fakeCloudItemState() == FakeCloudItemState_Loading)
            m_pixmap = uiCommon().vmGuestOSTypePixmapDefault("Cloud", &m_logicalPixmapSize);
        else
            m_pixmap = uiCommon().vmGuestOSTypePixmapDefault(m_strOSTypeId, &m_logicalPixmapSize);
    }
    /* Otherwise: */
    else
    {
        /// @todo handle inaccessible cloud VM
    }
}

bool UIVirtualMachineItemCloud::isItemEditable() const
{
    return accessible();
}

bool UIVirtualMachineItemCloud::isItemSaved() const
{
    return    accessible()
           && machineState() == KMachineState_Saved;
}

bool UIVirtualMachineItemCloud::isItemPoweredOff() const
{
    return    accessible()
           && (   machineState() == KMachineState_PoweredOff
               || machineState() == KMachineState_Saved
               || machineState() == KMachineState_Teleported
               || machineState() == KMachineState_Aborted);
}

bool UIVirtualMachineItemCloud::isItemStarted() const
{
    return    isItemRunning()
           || isItemPaused();
}

bool UIVirtualMachineItemCloud::isItemRunning() const
{
    return    accessible()
           && (   machineState() == KMachineState_Running
               || machineState() == KMachineState_Teleporting
               || machineState() == KMachineState_LiveSnapshotting);
}

bool UIVirtualMachineItemCloud::isItemRunningHeadless() const
{
    return isItemRunning();
}

bool UIVirtualMachineItemCloud::isItemPaused() const
{
    return    accessible()
           && (   machineState() == KMachineState_Paused
               || machineState() == KMachineState_TeleportingPausedVM);
}

bool UIVirtualMachineItemCloud::isItemStuck() const
{
    return    accessible()
           && machineState() == KMachineState_Stuck;
}

void UIVirtualMachineItemCloud::retranslateUi()
{
    /* If machine is accessible: */
    if (m_fAccessible)
    {
        /* Update machine/state name: */
        if (itemType() == ItemType_CloudFake)
        {
            switch (m_enmFakeCloudItemState)
            {
                case UIVirtualMachineItemCloud::FakeCloudItemState_Loading:
                    m_strMachineStateName = tr("Loading ...");
                    break;
                case UIVirtualMachineItemCloud::FakeCloudItemState_Done:
                    m_strMachineStateName = tr("Up-To-Date");
                    break;
                default:
                    break;
            }
        }

        /* Update tool-tip: */
        m_strToolTipText = tr("<nobr><b>%1</b></nobr><br>"
                              "<nobr>%2</nobr>",
                              "VM tooltip (name, state)")
                              .arg(m_strName)
                              .arg(gpConverter->toString(m_enmMachineState));
    }
    /* Otherwise: */
    else
    {
        /* Update tool-tip: */
        m_strToolTipText = tr("<nobr><b>%1</b></nobr><br>"
                              "<nobr>Inaccessible</nobr>",
                              "Inaccessible VM tooltip (name)")
                              .arg(m_strName);

        /* We have our own translation for Null states: */
        m_strMachineStateName = tr("Inaccessible");
    }
}
