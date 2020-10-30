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
#include <QPointer>
#include <QTimer>

/* GUI includes: */
#include "UICloudNetworkingStuff.h"
#include "UICommon.h"
#include "UIConverter.h"
#include "UIErrorString.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "UIProgressObject.h"
#include "UIThreadPool.h"
#include "UIVirtualMachineItemCloud.h"

/* COM includes: */
#include "CProgress.h"
#include "CVirtualBoxErrorInfo.h"


UIVirtualMachineItemCloud::UIVirtualMachineItemCloud(UIFakeCloudVirtualMachineItemState enmState)
    : UIVirtualMachineItem(UIVirtualMachineItemType_CloudFake)
    , m_enmMachineState(KCloudMachineState_Invalid)
    , m_enmFakeCloudItemState(enmState)
    , m_fRefreshScheduled(false)
    , m_pTimer(0)
{
    prepare();
}

UIVirtualMachineItemCloud::UIVirtualMachineItemCloud(const CCloudMachine &comCloudMachine)
    : UIVirtualMachineItem(UIVirtualMachineItemType_CloudReal)
    , m_comCloudMachine(comCloudMachine)
    , m_enmMachineState(KCloudMachineState_Invalid)
    , m_enmFakeCloudItemState(UIFakeCloudVirtualMachineItemState_NotApplicable)
    , m_fRefreshScheduled(false)
    , m_pTimer(0)
{
    prepare();
}

UIVirtualMachineItemCloud::~UIVirtualMachineItemCloud()
{
    cleanup();
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

void UIVirtualMachineItemCloud::updateInfoAsync(bool fDelayed, bool fSubscribe /* = false */)
{
    /* Mark update scheduled if requested: */
    if (fSubscribe)
        m_fRefreshScheduled = true;

    /* Ignore refresh request if timer or progress is already running: */
    if (m_pTimer->isActive() || m_pProgressHandler)
        return;

    /* Schedule refresh request in a 10 or 0 seconds: */
    m_pTimer->setInterval(fDelayed ? 10000 : 0);
    m_pTimer->start();
}

void UIVirtualMachineItemCloud::waitForAsyncInfoUpdateFinished()
{
    /* Mark update canceled in any case: */
    m_fRefreshScheduled = false;

    /* Cancel the progress-handler if any: */
    if (m_pProgressHandler)
        m_pProgressHandler->cancel();
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
            m_enmMachineState = KCloudMachineState_Stopped;
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
            m_enmMachineState = m_fAccessible ? m_comCloudMachine.GetState() : KCloudMachineState_Stopped;
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
           && machineState() == KCloudMachineState_Stopped;
}

bool UIVirtualMachineItemCloud::isItemPoweredOff() const
{
    return    accessible()
           && (   machineState() == KCloudMachineState_Stopped
               || machineState() == KCloudMachineState_Terminated);
}

bool UIVirtualMachineItemCloud::isItemStarted() const
{
    return    isItemRunning()
           || isItemPaused();
}

bool UIVirtualMachineItemCloud::isItemRunning() const
{
    return    accessible()
           && machineState() == KCloudMachineState_Running;
}

bool UIVirtualMachineItemCloud::isItemRunningHeadless() const
{
    return isItemRunning();
}

bool UIVirtualMachineItemCloud::isItemPaused() const
{
    return false;
}

bool UIVirtualMachineItemCloud::isItemStuck() const
{
    return false;
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

void UIVirtualMachineItemCloud::sltRefreshCloudMachineInfo()
{
    /* Make sure item is of real cloud type and is initialized: */
    AssertReturnVoid(itemType() == UIVirtualMachineItemType_CloudReal);

    /* Ignore refresh request if there is progress already: */
    if (m_pProgressHandler)
        return;

    /* Create 'Refresh' progress: */
    m_comProgress = m_comCloudMachine.Refresh();
    if (!m_comCloudMachine.isOk())
        msgCenter().cannotAcquireCloudMachineParameter(m_comCloudMachine);
    else
    {
        /* Prepare 'Refresh' progress handler: */
        m_pProgressHandler = new UIProgressObject(m_comProgress, this);
        if (m_pProgressHandler)
        {
            connect(m_pProgressHandler.data(), &UIProgressObject::sigProgressEventHandlingFinished,
                    this, &UIVirtualMachineItemCloud::sltHandleRefreshCloudMachineInfoDone);
            emit sigRefreshStarted();
        }
    }
}

void UIVirtualMachineItemCloud::sltHandleRefreshCloudMachineInfoDone()
{
    /* Was the progress canceled? */
    const bool fCanceled = m_comProgress.GetCanceled();

    /* If not canceled => check progress result: */
    if (!fCanceled && (!m_comProgress.isOk() || m_comProgress.GetResultCode() != 0))
        msgCenter().cannotAcquireCloudMachineParameter(m_comProgress);

    /* Recache: */
    recache();

    /* Cleanup the handler and the progress: */
    delete m_pProgressHandler;
    m_comProgress = CProgress();

    /* If not canceled => notify listeners: */
    if (!fCanceled)
        emit sigRefreshFinished();

    /* Refresh again if scheduled: */
    if (m_fRefreshScheduled)
        updateInfoAsync(true /* async? */);
}

void UIVirtualMachineItemCloud::prepare()
{
    /* Prepare timer: */
    m_pTimer = new QTimer(this);
    if (m_pTimer)
    {
        m_pTimer->setSingleShot(true);
        connect(m_pTimer, &QTimer::timeout,
                this, &UIVirtualMachineItemCloud::sltRefreshCloudMachineInfo);
    }

    /* Recache finally: */
    recache();
}

void UIVirtualMachineItemCloud::cleanup()
{
    /* Cleanup timer: */
    delete m_pTimer;
    m_pTimer = 0;
}
