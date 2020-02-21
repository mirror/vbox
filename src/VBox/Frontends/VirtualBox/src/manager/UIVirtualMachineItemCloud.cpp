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
#include "UICloudMachine.h"
#include "UICloudNetworkingStuff.h"
#include "UICommon.h"
#include "UIConverter.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "UITaskCloudGetInstanceInfo.h"
#include "UIThreadPool.h"
#include "UIVirtualMachineItemCloud.h"

/* COM includes: */
#include "CProgress.h"


UIVirtualMachineItemCloud::UIVirtualMachineItemCloud()
    : UIVirtualMachineItem(ItemType_CloudFake)
    , m_pCloudMachine(0)
    , m_enmFakeCloudItemState(FakeCloudItemState_Loading)
    , m_pTask(0)
{
    recache();
}

UIVirtualMachineItemCloud::UIVirtualMachineItemCloud(const UICloudMachine &guiCloudMachine)
    : UIVirtualMachineItem(ItemType_CloudReal)
    , m_pCloudMachine(new UICloudMachine(guiCloudMachine))
    , m_enmFakeCloudItemState(FakeCloudItemState_NotApplicable)
    , m_pTask(0)
{
    recache();
}

UIVirtualMachineItemCloud::~UIVirtualMachineItemCloud()
{
    delete m_pCloudMachine;
}

void UIVirtualMachineItemCloud::updateInfo(QWidget *pParent)
{
    /* Make sure item is of real cloud type and is initialized: */
    AssertReturnVoid(itemType() == ItemType_CloudReal);
    AssertPtrReturnVoid(m_pCloudMachine);

    /* Acquire info: */
    const QMap<KVirtualSystemDescriptionType, QString> infoMap = getInstanceInfo(m_pCloudMachine->client(),
                                                                                 m_strId,
                                                                                 pParent);

    /* Update info: */
    updateInfo(infoMap);
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
    /* Acquire cloud client: */
    CCloudClient comCloudClient = m_pCloudMachine->client();

    /* Now execute async method: */
    CProgress comProgress;
    if (fPause)
        comProgress = comCloudClient.PauseInstance(m_strId);
    else
        comProgress = comCloudClient.StartInstance(m_strId);
    if (!comCloudClient.isOk())
        msgCenter().cannotAcquireCloudClientParameter(comCloudClient);
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
    if (itemType() == ItemType_CloudReal)
    {
        AssertPtrReturnVoid(m_pCloudMachine);
        m_strId = m_pCloudMachine->id();
        m_strName = m_pCloudMachine->name();
    }

    /* Now determine whether VM is accessible: */
    m_fAccessible = true;
    if (m_fAccessible)
    {
        /* Reset last access error information: */
        m_comAccessError = CVirtualBoxErrorInfo();

        /* Determine own VM attributes: */
        m_strOSTypeId = "Other";

        /* Determine VM states: */
        if (   itemType() == ItemType_CloudFake
            || m_enmMachineState == KMachineState_Null)
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
        if (itemType() == ItemType_CloudFake)
        {
            /* Update machine/state name: */
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
    AssertPtrReturnVoid(m_pCloudMachine);

    /* Create and start task to acquire info async way only if there is no task yet: */
    if (!m_pTask)
    {
        m_pTask = new UITaskCloudGetInstanceInfo(m_pCloudMachine->client(), m_strId);
        connect(m_pTask, &UITask::sigComplete,
                this, &UIVirtualMachineItemCloud::sltHandleGetCloudInstanceInfoDone);
        uiCommon().threadPool()->enqueueTask(m_pTask);
    }
}

void UIVirtualMachineItemCloud::sltHandleGetCloudInstanceInfoDone(UITask *pTask)
{
    /* Skip unrelated tasks: */
    if (!m_pTask || pTask != m_pTask)
        return;

    /* Mark our task handled: */
    m_pTask = 0;

    /* Cast task to corresponding sub-class: */
    UITaskCloudGetInstanceInfo *pInfoTask = static_cast<UITaskCloudGetInstanceInfo*>(pTask);

    /* Update info: */
    updateInfo(pInfoTask->result());
}

void UIVirtualMachineItemCloud::updateInfo(const QMap<KVirtualSystemDescriptionType, QString> &infoMap)
{
    /* Prepare a map of known states: */
    QMap<QString, KMachineState> states;
    states["RUNNING"] = KMachineState_Running;
    states["STOPPED"] = KMachineState_Paused;
    states["STOPPING"] = KMachineState_Stopping;
    states["STARTING"] = KMachineState_Starting;

    /* Update our state value: */
    m_enmMachineState = states.value(infoMap.value(KVirtualSystemDescriptionType_CloudInstanceState), KMachineState_PoweredOff);

    /* Recache: */
    recache();

    /* Notify listeners finally: */
    emit sigStateChange();
}
