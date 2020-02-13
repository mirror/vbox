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
#include "UICloudMachine.h"
#include "UICommon.h"
#include "UIConverter.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "UIVirtualMachineItemCloud.h"

/* COM includes: */
#include "CAppliance.h"
#include "CVirtualBox.h"
#include "CVirtualSystemDescription.h"


UIVirtualMachineItemCloud::UIVirtualMachineItemCloud()
    : UIVirtualMachineItem(ItemType_CloudFake)
    , m_pCloudMachine(0)
    , m_enmFakeCloudItemState(FakeCloudItemState_Loading)
{
    recache();
}

UIVirtualMachineItemCloud::UIVirtualMachineItemCloud(const UICloudMachine &guiCloudMachine)
    : UIVirtualMachineItem(ItemType_CloudReal)
    , m_pCloudMachine(new UICloudMachine(guiCloudMachine))
    , m_enmFakeCloudItemState(FakeCloudItemState_NotApplicable)
{
    recache();
}

UIVirtualMachineItemCloud::~UIVirtualMachineItemCloud()
{
    delete m_pCloudMachine;
}

void UIVirtualMachineItemCloud::updateState(QWidget *pParent)
{
    /* Make sure item is of real cloud type: */
    AssertReturnVoid(itemType() == ItemType_CloudReal);

    /* Update state: */
    const QString strState = acquireInstanceInfo(KVirtualSystemDescriptionType_CloudInstanceState, pParent);
    QMap<QString, KMachineState> states;
    states["RUNNING"] = KMachineState_Running;
    states["STOPPED"] = KMachineState_Paused;
    states["STOPPING"] = KMachineState_Stopping;
    states["STARTING"] = KMachineState_Starting;
    m_enmMachineState = states.value(strState, KMachineState_PoweredOff);

    /* Recache: */
    recache();

    /* Notify listeners finally: */
    emit sigStateChange();
}

QString UIVirtualMachineItemCloud::acquireInstanceInfo(KVirtualSystemDescriptionType enmType, QWidget *pParent)
{
    /* Make sure item is of real cloud type and is initialized: */
    AssertReturn(itemType() == ItemType_CloudReal, QString());
    AssertPtrReturn(m_pCloudMachine, QString());

    /* Get VirtualBox object: */
    CVirtualBox comVBox = uiCommon().virtualBox();

    /* Create appliance: */
    CAppliance comAppliance = comVBox.CreateAppliance();
    if (!comVBox.isOk())
        msgCenter().cannotCreateAppliance(comVBox);
    else
    {
        /* Append it with one (1) description we need: */
        comAppliance.CreateVirtualSystemDescriptions(1);
        if (!comAppliance.isOk())
            msgCenter().cannotCreateVirtualSystemDescription(comAppliance);
        else
        {
            /* Get received description: */
            QVector<CVirtualSystemDescription> descriptions = comAppliance.GetVirtualSystemDescriptions();
            AssertReturn(!descriptions.isEmpty(), QString());
            CVirtualSystemDescription comDescription = descriptions.at(0);

            /* Acquire cloud client: */
            CCloudClient comCloudClient = m_pCloudMachine->client();

            /* Now execute GetInstanceInfo async method: */
            CProgress comProgress = comCloudClient.GetInstanceInfo(m_strId, comDescription);
            if (!comCloudClient.isOk())
                msgCenter().cannotAcquireCloudClientParameter(comCloudClient);
            else
            {
                /* Show "Acquire instance info" progress: */
                msgCenter().showModalProgressDialog(comProgress, UICommon::tr("Acquire instance info ..."),
                                                    ":/progress_reading_appliance_90px.png", pParent, 0);
                if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
                    msgCenter().cannotAcquireCloudClientParameter(comProgress);
                else
                {
                    /* Now acquire description of certain type: */
                    QVector<KVirtualSystemDescriptionType> types;
                    QVector<QString> refs, origValues, configValues, extraConfigValues;
                    comDescription.GetDescriptionByType(enmType, types, refs, origValues, configValues, extraConfigValues);

                    /* Return first config value if we have one: */
                    AssertReturn(!configValues.isEmpty(), QString());
                    return configValues.at(0);
                }
            }
        }
    }

    /* Return null string by default: */
    return QString();
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
            updateState(pParent);
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
