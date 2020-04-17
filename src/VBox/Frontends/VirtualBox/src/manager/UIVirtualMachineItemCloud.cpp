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
    : UIVirtualMachineItem(ItemType_CloudFake)
    , m_enmFakeCloudItemState(FakeCloudItemState_Loading)
    , m_pTask(0)
{
    recache();
}

UIVirtualMachineItemCloud::UIVirtualMachineItemCloud(const CCloudMachine &comCloudMachine)
    : UIVirtualMachineItem(ItemType_CloudReal)
    , m_comCloudMachine(comCloudMachine)
    , m_enmFakeCloudItemState(FakeCloudItemState_NotApplicable)
    , m_pTask(0)
{
    recache();
}

UIVirtualMachineItemCloud::~UIVirtualMachineItemCloud()
{
}

void UIVirtualMachineItemCloud::updateInfoAsync(bool fDelayed)
{
    QTimer::singleShot(fDelayed ? 10000 : 0, this, SLOT(sltCreateGetCloudInstanceInfoTask()));
}

void UIVirtualMachineItemCloud::pause(QWidget *pParent)
{
    pauseOrResume(true /* pause? */, pParent);
}

void UIVirtualMachineItemCloud::resume(QWidget *pParent)
{
    pauseOrResume(false /* pause? */, pParent);
}

void UIVirtualMachineItemCloud::pauseOrResume(bool fPause, QWidget *pParent)
{
    /* Execute async method: */
    CProgress comProgress;
    if (fPause)
        comProgress = m_comCloudMachine.PowerDown();
    else
        comProgress = m_comCloudMachine.PowerUp();
    if (!m_comCloudMachine.isOk())
        msgCenter().cannotAcquireCloudMachineParameter(m_comCloudMachine);
    else
    {
        /* Show progress: */
        /// @todo use proper pause icon
        if (fPause)
            msgCenter().showModalProgressDialog(comProgress, UICommon::tr("Pause instance ..."),
                                                ":/progress_reading_appliance_90px.png", pParent, 0);
        else
            msgCenter().showModalProgressDialog(comProgress, UICommon::tr("Start instance ..."),
                                                ":/progress_start_90px.png", pParent, 0);
        if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
            msgCenter().cannotAcquireCloudClientParameter(comProgress);
        else
            updateInfoAsync(false /* delayed? */);
    }
}

void UIVirtualMachineItemCloud::recache()
{
    /* Determine attributes which are always available: */
    if (!m_comCloudMachine.isNull())
    {
        m_uId = m_comCloudMachine.GetId();
        m_strName = m_comCloudMachine.GetName();
    }

    /* Now determine whether VM is accessible: */
    m_fAccessible = !m_comCloudMachine.isNull()
                  ? m_comCloudMachine.GetAccessible()
                  : true;
    m_strAccessError =    !m_comCloudMachine.isNull()
                       && !m_comCloudMachine.GetAccessible()
                       && !m_comCloudMachine.GetAccessError().isNull()
                     ? UIErrorString::formatErrorInfo(m_comCloudMachine.GetAccessError())
                     : QString();

    /* Determine own VM attributes: */
    m_strOSTypeId =    !m_comCloudMachine.isNull()
                    && m_comCloudMachine.GetAccessible()
                  ? m_comCloudMachine.GetOSTypeId()
                  : "Other";

    /* Determine VM states: */
    m_enmMachineState =    !m_comCloudMachine.isNull()
                        && m_comCloudMachine.GetAccessible()
                      ? m_comCloudMachine.GetState()
                      : KMachineState_PoweredOff;
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
    m_fHasDetails = true;

    /* Recache item pixmap: */
    recachePixmap();

    /* Retranslate finally: */
    retranslateUi();
}

void UIVirtualMachineItemCloud::recachePixmap()
{
    /* We are using icon corresponding to cached guest OS type: */
    if (   itemType() == ItemType_CloudFake
        && fakeCloudItemState() == FakeCloudItemState_Loading)
        m_pixmap = uiCommon().vmGuestOSTypePixmapDefault("Cloud", &m_logicalPixmapSize);
    else
        m_pixmap = uiCommon().vmGuestOSTypePixmapDefault(m_strOSTypeId, &m_logicalPixmapSize);
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

bool UIVirtualMachineItemCloud::isItemCanBeSwitchedTo() const
{
    return false;
}

void UIVirtualMachineItemCloud::retranslateUi()
{
    /* If machine is accessible: */
    if (accessible())
    {
        if (itemType() == ItemType_CloudFake)
        {
            /* Update machine/state name: */
            switch (m_enmFakeCloudItemState)
            {
                case UIVirtualMachineItemCloud::FakeCloudItemState_Loading:
                    m_strMachineStateName = tr("Loading ...");
                    break;
                case UIVirtualMachineItemCloud::FakeCloudItemState_Done:
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
    AssertReturnVoid(itemType() == ItemType_CloudReal);

    /* Create and start task to acquire info async way only if there is no task yet: */
    if (!m_pTask)
    {
        m_pTask = new UITaskCloudRefreshMachineInfo(m_comCloudMachine);
        connect(uiCommon().threadPoolCloud(), &UIThreadPool::sigTaskComplete,
                this, &UIVirtualMachineItemCloud::sltHandleGetCloudInstanceInfoDone);
        uiCommon().threadPoolCloud()->enqueueTask(m_pTask);
    }
}

void UIVirtualMachineItemCloud::sltHandleGetCloudInstanceInfoDone(UITask *pTask)
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
