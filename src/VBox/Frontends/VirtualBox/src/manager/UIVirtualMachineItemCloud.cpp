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

/* Qt includes: */
#include <QTimer>

/* GUI includes: */
#include "UICloudNetworkingStuff.h"
#include "UICommon.h"
#include "UIConverter.h"
#include "UIErrorString.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "UITaskCloudRefreshMachineInfo.h"
#include "UIThreadPool.h"
#include "UIVirtualMachineItemCloud.h"

/* COM includes: */
#include "CProgress.h"
#include "CVirtualBoxErrorInfo.h"


UIVirtualMachineItemCloud::UIVirtualMachineItemCloud()
    : UIVirtualMachineItem(UIVirtualMachineItemType_CloudFake)
    , m_enmFakeCloudItemState(UIFakeCloudVirtualMachineItemState_Loading)
    , m_pTask(0)
{
    recache();
}

UIVirtualMachineItemCloud::UIVirtualMachineItemCloud(const CCloudMachine &comCloudMachine)
    : UIVirtualMachineItem(UIVirtualMachineItemType_CloudReal)
    , m_comCloudMachine(comCloudMachine)
    , m_enmFakeCloudItemState(UIFakeCloudVirtualMachineItemState_NotApplicable)
    , m_pTask(0)
{
    recache();
}

UIVirtualMachineItemCloud::~UIVirtualMachineItemCloud()
{
}

void UIVirtualMachineItemCloud::setFakeCloudItemState(UIFakeCloudVirtualMachineItemState enmState)
{
    m_enmFakeCloudItemState = enmState;
    recache();
}

void UIVirtualMachineItemCloud::setFakeCloudItemErrorMessage(const QString &strErrorMessage)
{
    m_strFakeCloudItemErrorMessage = strErrorMessage;
    recache();
}

void UIVirtualMachineItemCloud::updateInfoAsync(bool fDelayed)
{
    QTimer::singleShot(fDelayed ? 10000 : 0, this, SLOT(sltCreateGetCloudInstanceInfoTask()));
}

void UIVirtualMachineItemCloud::recache()
{
    switch (itemType())
    {
        case UIVirtualMachineItemType_CloudFake:
        {
            /* Make sure cloud VM is NOT set: */
            AssertReturnVoid(m_comCloudMachine.isNull());

            /* Determine ID/name: */
            m_uId = QUuid();
            m_strName = QString();

            /* Determine whether VM is accessible: */
            m_fAccessible = m_strFakeCloudItemErrorMessage.isNull();
            m_strAccessError = m_strFakeCloudItemErrorMessage;

            /* Determine VM OS type: */
            m_strOSTypeId = "Other";

            /* Determine VM states: */
            m_enmMachineState = KMachineState_PoweredOff;
            m_strMachineStateName = gpConverter->toString(m_enmMachineState);
            switch (m_enmFakeCloudItemState)
            {
                case UIFakeCloudVirtualMachineItemState_Loading:
                    m_machineStateIcon = UIIconPool::iconSet(":/state_loading_16px.png");
                    break;
                case UIFakeCloudVirtualMachineItemState_Done:
                    m_machineStateIcon = UIIconPool::iconSet(":/vm_new_16px.png");
                    break;
                default:
                    break;
            }

            /* Determine configuration access level: */
            m_enmConfigurationAccessLevel = ConfigurationAccessLevel_Null;

            /* Determine whether we should show this VM details: */
            m_fHasDetails = true;

            break;
        }
        case UIVirtualMachineItemType_CloudReal:
        {
            /* Make sure cloud VM is set: */
            AssertReturnVoid(m_comCloudMachine.isNotNull());

            /* Determine ID/name: */
            m_uId = m_comCloudMachine.GetId();
            m_strName = m_comCloudMachine.GetName();

            /* Determine whether VM is accessible: */
            m_fAccessible = m_comCloudMachine.GetAccessible();
            m_strAccessError = !m_fAccessible ? UIErrorString::formatErrorInfo(m_comCloudMachine.GetAccessError()) : QString();

            /* Determine VM OS type: */
            m_strOSTypeId = m_fAccessible ? m_comCloudMachine.GetOSTypeId() : "Other";

            /* Determine VM states: */
            m_enmMachineState = m_fAccessible ? m_comCloudMachine.GetState() : KMachineState_PoweredOff;
            m_strMachineStateName = gpConverter->toString(m_enmMachineState);
            m_machineStateIcon = gpConverter->toIcon(m_enmMachineState);

            /* Determine configuration access level: */
            m_enmConfigurationAccessLevel = m_fAccessible ? ConfigurationAccessLevel_Full : ConfigurationAccessLevel_Null;

            /* Determine whether we should show this VM details: */
            m_fHasDetails = true;

            break;
        }
        default:
        {
            AssertFailed();
            break;
        }
    }

    /* Recache item pixmap: */
    recachePixmap();

    /* Retranslate finally: */
    retranslateUi();
}

void UIVirtualMachineItemCloud::recachePixmap()
{
    /* We are using icon corresponding to cached guest OS type: */
    if (   itemType() == UIVirtualMachineItemType_CloudFake
        && fakeCloudItemState() == UIFakeCloudVirtualMachineItemState_Loading)
        m_pixmap = uiCommon().vmGuestOSTypePixmapDefault("Cloud", &m_logicalPixmapSize);
    else
        m_pixmap = uiCommon().vmGuestOSTypePixmapDefault(m_strOSTypeId, &m_logicalPixmapSize);
}

bool UIVirtualMachineItemCloud::isItemEditable() const
{
    return    accessible()
           && itemType() == UIVirtualMachineItemType_CloudReal;
}

bool UIVirtualMachineItemCloud::isItemRemovable() const
{
    return    accessible()
           && itemType() == UIVirtualMachineItemType_CloudReal;
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

bool UIVirtualMachineItemCloud::isItemCanBeSwitchedTo() const
{
    return false;
}

void UIVirtualMachineItemCloud::retranslateUi()
{
    /* If machine is accessible: */
    if (accessible())
    {
        if (itemType() == UIVirtualMachineItemType_CloudFake)
        {
            /* Update machine/state name: */
            switch (m_enmFakeCloudItemState)
            {
                case UIFakeCloudVirtualMachineItemState_Loading:
                    m_strMachineStateName = tr("Loading ...");
                    break;
                case UIFakeCloudVirtualMachineItemState_Done:
                    m_strMachineStateName = tr("Empty");
                    break;
                default:
                    break;
            }

            /* Update tool-tip: */
            m_strToolTipText = m_strMachineStateName;
        }
        else
        {
            /* Update tool-tip: */
            m_strToolTipText = QString("<nobr><b>%1</b></nobr><br>"
                                       "<nobr>%2</nobr>")
                                       .arg(m_strName)
                                       .arg(gpConverter->toString(m_enmMachineState));
        }
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

void UIVirtualMachineItemCloud::sltCreateGetCloudInstanceInfoTask()
{
    /* Make sure item is of real cloud type and is initialized: */
    AssertReturnVoid(itemType() == UIVirtualMachineItemType_CloudReal);

    /* Create and start task to refresh info async way only if there is no task yet: */
    if (!m_pTask)
    {
        m_pTask = new UITaskCloudRefreshMachineInfo(m_comCloudMachine);
        connect(uiCommon().threadPoolCloud(), &UIThreadPool::sigTaskComplete,
                this, &UIVirtualMachineItemCloud::sltHandleRefreshCloudMachineInfoDone);
        uiCommon().threadPoolCloud()->enqueueTask(m_pTask);
    }
}

void UIVirtualMachineItemCloud::sltHandleRefreshCloudMachineInfoDone(UITask *pTask)
{
    /* Skip unrelated tasks: */
    if (!m_pTask || pTask != m_pTask)
        return;

    /* Mark our task handled: */
    m_pTask = 0;

    /* Recache: */
    recache();

    /* Notify listeners finally: */
    emit sigStateChange();
}
