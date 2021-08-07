/* $Id$ */
/** @file
 * VBox Qt GUI - Various UINotificationObjects implementations.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
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
#include <QDir>
#include <QFileInfo>

/* GUI includes: */
#include "UICommon.h"
#include "UIDownloaderExtensionPack.h"
#include "UIDownloaderUserManual.h"
#include "UINotificationObjects.h"

/* COM includes: */
#include "CConsole.h"


/*********************************************************************************************************************************
*   Class UINotificationProgressMediumCreate implementation.                                                                     *
*********************************************************************************************************************************/

UINotificationProgressMediumCreate::UINotificationProgressMediumCreate(const CMedium &comTarget,
                                                                       qulonglong uSize,
                                                                       const QVector<KMediumVariant> &variants)
    : m_comTarget(comTarget)
    , m_uSize(uSize)
    , m_variants(variants)
{
    connect(this, &UINotificationProgress::sigProgressFinished,
            this, &UINotificationProgressMediumCreate::sltHandleProgressFinished);
}

QString UINotificationProgressMediumCreate::name() const
{
    return UINotificationProgress::tr("Creating medium ...");
}

QString UINotificationProgressMediumCreate::details() const
{
    return UINotificationProgress::tr("<b>Location:</b> %1<br><b>Size:</b> %2").arg(m_strLocation, uiCommon().formatSize(m_uSize));
}

CProgress UINotificationProgressMediumCreate::createProgress(COMResult &comResult)
{
    /* Acquire location: */
    m_strLocation = m_comTarget.GetLocation();
    if (!m_comTarget.isOk())
    {
        /* Store COM result: */
        comResult = m_comTarget;
        /* Return progress-wrapper: */
        return CProgress();
    }

    /* Initialize progress-wrapper: */
    CProgress comProgress = m_comTarget.CreateBaseStorage(m_uSize, m_variants);
    /* Store COM result: */
    comResult = m_comTarget;
    /* Return progress-wrapper: */
    return comProgress;
}

void UINotificationProgressMediumCreate::sltHandleProgressFinished()
{
    if (m_comTarget.isNotNull() && !m_comTarget.GetId().isNull())
        emit sigMediumCreated(m_comTarget);
}


/*********************************************************************************************************************************
*   Class UINotificationProgressMediumCopy implementation.                                                                       *
*********************************************************************************************************************************/

UINotificationProgressMediumCopy::UINotificationProgressMediumCopy(const CMedium &comSource,
                                                                   const CMedium &comTarget,
                                                                   const QVector<KMediumVariant> &variants)
    : m_comSource(comSource)
    , m_comTarget(comTarget)
    , m_variants(variants)
{
    connect(this, &UINotificationProgress::sigProgressFinished,
            this, &UINotificationProgressMediumCopy::sltHandleProgressFinished);
}

QString UINotificationProgressMediumCopy::name() const
{
    return UINotificationProgress::tr("Copying medium ...");
}

QString UINotificationProgressMediumCopy::details() const
{
    return UINotificationProgress::tr("<b>From:</b> %1<br><b>To:</b> %2").arg(m_strSourceLocation, m_strTargetLocation);
}

CProgress UINotificationProgressMediumCopy::createProgress(COMResult &comResult)
{
    /* Acquire locations: */
    m_strSourceLocation = m_comSource.GetLocation();
    if (!m_comSource.isOk())
    {
        /* Store COM result: */
        comResult = m_comSource;
        /* Return progress-wrapper: */
        return CProgress();
    }
    m_strTargetLocation = m_comTarget.GetLocation();
    if (!m_comTarget.isOk())
    {
        /* Store COM result: */
        comResult = m_comTarget;
        /* Return progress-wrapper: */
        return CProgress();
    }

    /* Initialize progress-wrapper: */
    CProgress comProgress = m_comSource.CloneTo(m_comTarget, m_variants, CMedium());
    /* Store COM result: */
    comResult = m_comSource;
    /* Return progress-wrapper: */
    return comProgress;
}

void UINotificationProgressMediumCopy::sltHandleProgressFinished()
{
    if (m_comTarget.isNotNull() && !m_comTarget.GetId().isNull())
        emit sigMediumCopied(m_comTarget);
}


/*********************************************************************************************************************************
*   Class UINotificationProgressMediumMove implementation.                                                                       *
*********************************************************************************************************************************/

UINotificationProgressMediumMove::UINotificationProgressMediumMove(const CMedium &comMedium,
                                                                   const QString &strLocation)
    : m_comMedium(comMedium)
    , m_strTo(strLocation)
{
}

QString UINotificationProgressMediumMove::name() const
{
    return UINotificationProgress::tr("Moving medium ...");
}

QString UINotificationProgressMediumMove::details() const
{
    return UINotificationProgress::tr("<b>From:</b> %1<br><b>To:</b> %2").arg(m_strFrom, m_strTo);
}

CProgress UINotificationProgressMediumMove::createProgress(COMResult &comResult)
{
    /* Acquire location: */
    m_strFrom = m_comMedium.GetLocation();
    if (!m_comMedium.isOk())
    {
        /* Store COM result: */
        comResult = m_comMedium;
        /* Return progress-wrapper: */
        return CProgress();
    }

    /* Initialize progress-wrapper: */
    CProgress comProgress = m_comMedium.MoveTo(m_strTo);
    /* Store COM result: */
    comResult = m_comMedium;
    /* Return progress-wrapper: */
    return comProgress;
}


/*********************************************************************************************************************************
*   Class UINotificationProgressMediumResize implementation.                                                                     *
*********************************************************************************************************************************/

UINotificationProgressMediumResize::UINotificationProgressMediumResize(const CMedium &comMedium,
                                                                       qulonglong uSize)
    : m_comMedium(comMedium)
    , m_uTo(uSize)
{
}

QString UINotificationProgressMediumResize::name() const
{
    return UINotificationProgress::tr("Resizing medium ...");
}

QString UINotificationProgressMediumResize::details() const
{
    return UINotificationProgress::tr("<b>From:</b> %1<br><b>To:</b> %2")
                                      .arg(uiCommon().formatSize(m_uFrom),
                                           uiCommon().formatSize(m_uTo));
}

CProgress UINotificationProgressMediumResize::createProgress(COMResult &comResult)
{
    /* Acquire size: */
    m_uFrom = m_comMedium.GetLogicalSize();
    if (!m_comMedium.isOk())
    {
        /* Store COM result: */
        comResult = m_comMedium;
        /* Return progress-wrapper: */
        return CProgress();
    }

    /* Initialize progress-wrapper: */
    CProgress comProgress = m_comMedium.Resize(m_uTo);
    /* Store COM result: */
    comResult = m_comMedium;
    /* Return progress-wrapper: */
    return comProgress;
}


/*********************************************************************************************************************************
*   Class UINotificationProgressMediumDeletingStorage implementation.                                                            *
*********************************************************************************************************************************/

UINotificationProgressMediumDeletingStorage::UINotificationProgressMediumDeletingStorage(const CMedium &comMedium)
    : m_comMedium(comMedium)
{
    connect(this, &UINotificationProgress::sigProgressFinished,
            this, &UINotificationProgressMediumDeletingStorage::sltHandleProgressFinished);
}

QString UINotificationProgressMediumDeletingStorage::name() const
{
    return UINotificationProgress::tr("Deleting medium storage ...");
}

QString UINotificationProgressMediumDeletingStorage::details() const
{
    return UINotificationProgress::tr("<b>Location:</b> %1").arg(m_strLocation);
}

CProgress UINotificationProgressMediumDeletingStorage::createProgress(COMResult &comResult)
{
    /* Acquire location: */
    m_strLocation = m_comMedium.GetLocation();
    if (!m_comMedium.isOk())
    {
        /* Store COM result: */
        comResult = m_comMedium;
        /* Return progress-wrapper: */
        return CProgress();
    }

    /* Initialize progress-wrapper: */
    CProgress comProgress = m_comMedium.DeleteStorage();
    /* Store COM result: */
    comResult = m_comMedium;
    /* Return progress-wrapper: */
    return comProgress;
}

void UINotificationProgressMediumDeletingStorage::sltHandleProgressFinished()
{
    if (!error().isEmpty())
        emit sigMediumStorageDeleted(m_comMedium);
}


/*********************************************************************************************************************************
*   Class UINotificationProgressMachineCopy implementation.                                                                      *
*********************************************************************************************************************************/

UINotificationProgressMachineCopy::UINotificationProgressMachineCopy(const CMachine &comSource,
                                                                     const CMachine &comTarget,
                                                                     const KCloneMode &enmCloneMode,
                                                                     const QVector<KCloneOptions> &options)
    : m_comSource(comSource)
    , m_comTarget(comTarget)
    , m_enmCloneMode(enmCloneMode)
    , m_options(options)
{
    connect(this, &UINotificationProgress::sigProgressFinished,
            this, &UINotificationProgressMachineCopy::sltHandleProgressFinished);
}

QString UINotificationProgressMachineCopy::name() const
{
    return UINotificationProgress::tr("Copying machine ...");
}

QString UINotificationProgressMachineCopy::details() const
{
    return UINotificationProgress::tr("<b>From:</b> %1<br><b>To:</b> %2").arg(m_strSourceName, m_strTargetName);
}

CProgress UINotificationProgressMachineCopy::createProgress(COMResult &comResult)
{
    /* Acquire names: */
    m_strSourceName = m_comSource.GetName();
    if (!m_comSource.isOk())
    {
        /* Store COM result: */
        comResult = m_comSource;
        /* Return progress-wrapper: */
        return CProgress();
    }
    m_strTargetName = m_comTarget.GetName();
    if (!m_comTarget.isOk())
    {
        /* Store COM result: */
        comResult = m_comTarget;
        /* Return progress-wrapper: */
        return CProgress();
    }

    /* Initialize progress-wrapper: */
    CProgress comProgress = m_comSource.CloneTo(m_comTarget, m_enmCloneMode, m_options);
    /* Store COM result: */
    comResult = m_comSource;
    /* Return progress-wrapper: */
    return comProgress;
}

void UINotificationProgressMachineCopy::sltHandleProgressFinished()
{
    if (m_comTarget.isNotNull() && !m_comTarget.GetId().isNull())
        emit sigMachineCopied(m_comTarget);
}


/*********************************************************************************************************************************
*   Class UINotificationProgressMachineMove implementation.                                                                      *
*********************************************************************************************************************************/

UINotificationProgressMachineMove::UINotificationProgressMachineMove(const QUuid &uId,
                                                                     const QString &strDestination,
                                                                     const QString &strType)
    : m_uId(uId)
    , m_strDestination(QDir::toNativeSeparators(strDestination))
    , m_strType(strType)
{
    connect(this, &UINotificationProgress::sigProgressFinished,
            this, &UINotificationProgressMachineMove::sltHandleProgressFinished);
}

QString UINotificationProgressMachineMove::name() const
{
    return UINotificationProgress::tr("Moving machine ...");
}

QString UINotificationProgressMachineMove::details() const
{
    return UINotificationProgress::tr("<b>From:</b> %1<br><b>To:</b> %2").arg(m_strSource, m_strDestination);
}

CProgress UINotificationProgressMachineMove::createProgress(COMResult &comResult)
{
    /* Open a session thru which we will modify the machine: */
    m_comSession = uiCommon().openSession(m_uId, KLockType_Write);
    if (m_comSession.isNull())
        return CProgress();

    /* Get session machine: */
    CMachine comMachine = m_comSession.GetMachine();
    if (!m_comSession.isOk())
    {
        comResult = m_comSession;
        m_comSession.UnlockMachine();
        return CProgress();
    }

    /* Acquire VM source: */
    const QString strSettingFilePath = comMachine.GetSettingsFilePath();
    if (!comMachine.isOk())
    {
        comResult = comMachine;
        m_comSession.UnlockMachine();
        return CProgress();
    }
    QDir parentDir = QFileInfo(strSettingFilePath).absoluteDir();
    parentDir.cdUp();
    m_strSource = QDir::toNativeSeparators(parentDir.absolutePath());

    /* Initialize progress-wrapper: */
    CProgress comProgress = comMachine.MoveTo(m_strDestination, m_strType);
    /* Store COM result: */
    comResult = comMachine;
    /* Return progress-wrapper: */
    return comProgress;
}

void UINotificationProgressMachineMove::sltHandleProgressFinished()
{
    /* Unlock session finally: */
    m_comSession.UnlockMachine();
}


/*********************************************************************************************************************************
*   Class UINotificationProgressMachineSaveState implementation.                                                                 *
*********************************************************************************************************************************/

UINotificationProgressMachineSaveState::UINotificationProgressMachineSaveState(const QUuid &uId)
    : m_uId(uId)
{
    connect(this, &UINotificationProgress::sigProgressFinished,
            this, &UINotificationProgressMachineSaveState::sltHandleProgressFinished);
}

QString UINotificationProgressMachineSaveState::name() const
{
    return UINotificationProgress::tr("Saving VM state ...");
}

QString UINotificationProgressMachineSaveState::details() const
{
    return UINotificationProgress::tr("<b>VM Name:</b> %1").arg(m_strName);
}

CProgress UINotificationProgressMachineSaveState::createProgress(COMResult &comResult)
{
    /* Open a session thru which we will modify the machine: */
    m_comSession = uiCommon().openExistingSession(m_uId);
    if (m_comSession.isNull())
        return CProgress();

    /* Get session machine: */
    CMachine comMachine = m_comSession.GetMachine();
    if (!m_comSession.isOk())
    {
        comResult = m_comSession;
        m_comSession.UnlockMachine();
        return CProgress();
    }

    /* Acquire VM name: */
    m_strName = comMachine.GetName();
    if (!comMachine.isOk())
    {
        comResult = comMachine;
        m_comSession.UnlockMachine();
        return CProgress();
    }

    /* Get machine state: */
    const KMachineState enmState = comMachine.GetState();
    if (!comMachine.isOk())
    {
        comResult = comMachine;
        m_comSession.UnlockMachine();
        return CProgress();
    }

    /* If VM isn't yet paused: */
    if (enmState != KMachineState_Paused)
    {
        /* Get session console: */
        CConsole comConsole = m_comSession.GetConsole();
        if (!m_comSession.isOk())
        {
            comResult = m_comSession;
            m_comSession.UnlockMachine();
            return CProgress();
        }

        /* Pause VM first: */
        comConsole.Pause();
        if (!comConsole.isOk())
        {
            comResult = comConsole;
            m_comSession.UnlockMachine();
            return CProgress();
        }
    }

    /* Initialize progress-wrapper: */
    CProgress comProgress = comMachine.SaveState();
    /* Store COM result: */
    comResult = comMachine;
    /* Return progress-wrapper: */
    return comProgress;
}

void UINotificationProgressMachineSaveState::sltHandleProgressFinished()
{
    /* Unlock session finally: */
    m_comSession.UnlockMachine();
}


/*********************************************************************************************************************************
*   Class UINotificationProgressMachinePowerDown implementation.                                                                 *
*********************************************************************************************************************************/

UINotificationProgressMachinePowerDown::UINotificationProgressMachinePowerDown(const QUuid &uId)
    : m_uId(uId)
{
    connect(this, &UINotificationProgress::sigProgressFinished,
            this, &UINotificationProgressMachinePowerDown::sltHandleProgressFinished);
}

QString UINotificationProgressMachinePowerDown::name() const
{
    return UINotificationProgress::tr("Powering VM down ...");
}

QString UINotificationProgressMachinePowerDown::details() const
{
    return UINotificationProgress::tr("<b>VM Name:</b> %1").arg(m_strName);
}

CProgress UINotificationProgressMachinePowerDown::createProgress(COMResult &comResult)
{
    /* Open a session thru which we will modify the machine: */
    m_comSession = uiCommon().openExistingSession(m_uId);
    if (m_comSession.isNull())
        return CProgress();

    /* Get session machine: */
    CMachine comMachine = m_comSession.GetMachine();
    if (!m_comSession.isOk())
    {
        comResult = m_comSession;
        m_comSession.UnlockMachine();
        return CProgress();
    }

    /* Acquire VM name: */
    m_strName = comMachine.GetName();
    if (!comMachine.isOk())
    {
        comResult = comMachine;
        m_comSession.UnlockMachine();
        return CProgress();
    }

    /* Get session console: */
    CConsole comConsole = m_comSession.GetConsole();
    if (!m_comSession.isOk())
    {
        comResult = m_comSession;
        m_comSession.UnlockMachine();
        return CProgress();
    }

    /* Initialize progress-wrapper: */
    CProgress comProgress = comConsole.PowerDown();
    /* Store COM result: */
    comResult = comConsole;
    /* Return progress-wrapper: */
    return comProgress;
}

void UINotificationProgressMachinePowerDown::sltHandleProgressFinished()
{
    /* Unlock session finally: */
    m_comSession.UnlockMachine();
}


/*********************************************************************************************************************************
*   Class UINotificationProgressMachineMediaRemove implementation.                                                               *
*********************************************************************************************************************************/

UINotificationProgressMachineMediaRemove::UINotificationProgressMachineMediaRemove(const CMachine &comMachine,
                                                                                   const CMediumVector &media)
    : m_comMachine(comMachine)
    , m_media(media)
{
}

QString UINotificationProgressMachineMediaRemove::name() const
{
    return UINotificationProgress::tr("Removing machine media ...");
}

QString UINotificationProgressMachineMediaRemove::details() const
{
    return UINotificationProgress::tr("<b>Machine Name:</b> %1").arg(m_strName);
}

CProgress UINotificationProgressMachineMediaRemove::createProgress(COMResult &comResult)
{
    /* Acquire names: */
    m_strName = m_comMachine.GetName();
    if (!m_comMachine.isOk())
    {
        /* Store COM result: */
        comResult = m_comMachine;
        /* Return progress-wrapper: */
        return CProgress();
    }

    /* Initialize progress-wrapper: */
    CProgress comProgress = m_comMachine.DeleteConfig(m_media);
    /* Store COM result: */
    comResult = m_comMachine;
    /* Return progress-wrapper: */
    return comProgress;
}


/*********************************************************************************************************************************
*   Class UINotificationProgressCloudMachineAdd implementation.                                                                  *
*********************************************************************************************************************************/

UINotificationProgressCloudMachineAdd::UINotificationProgressCloudMachineAdd(const CCloudClient &comClient,
                                                                             const CCloudMachine &comMachine,
                                                                             const QString &strInstanceName,
                                                                             const QString &strProviderShortName,
                                                                             const QString &strProfileName)
    : m_comClient(comClient)
    , m_comMachine(comMachine)
    , m_strInstanceName(strInstanceName)
    , m_strProviderShortName(strProviderShortName)
    , m_strProfileName(strProfileName)
{
    connect(this, &UINotificationProgress::sigProgressFinished,
            this, &UINotificationProgressCloudMachineAdd::sltHandleProgressFinished);
}

QString UINotificationProgressCloudMachineAdd::name() const
{
    return UINotificationProgress::tr("Adding cloud VM ...");
}

QString UINotificationProgressCloudMachineAdd::details() const
{
    return UINotificationProgress::tr("<b>Provider:</b> %1<br><b>Profile:</b> %2<br><b>Instance Name:</b> %3")
                                      .arg(m_strProviderShortName, m_strProfileName, m_strInstanceName);
}

CProgress UINotificationProgressCloudMachineAdd::createProgress(COMResult &comResult)
{
    /* Initialize progress-wrapper: */
    CProgress comProgress = m_comClient.AddCloudMachine(m_strInstanceName, m_comMachine);
    /* Store COM result: */
    comResult = m_comClient;
    /* Return progress-wrapper: */
    return comProgress;
}

void UINotificationProgressCloudMachineAdd::sltHandleProgressFinished()
{
    if (m_comMachine.isNotNull() && !m_comMachine.GetId().isNull())
        emit sigCloudMachineAdded(m_strProviderShortName, m_strProfileName, m_comMachine);
}


/*********************************************************************************************************************************
*   Class UINotificationProgressCloudMachineCreate implementation.                                                               *
*********************************************************************************************************************************/

UINotificationProgressCloudMachineCreate::UINotificationProgressCloudMachineCreate(const CCloudClient &comClient,
                                                                                   const CCloudMachine &comMachine,
                                                                                   const CVirtualSystemDescription &comVSD,
                                                                                   const QString &strProviderShortName,
                                                                                   const QString &strProfileName)
    : m_comClient(comClient)
    , m_comMachine(comMachine)
    , m_comVSD(comVSD)
    , m_strProviderShortName(strProviderShortName)
    , m_strProfileName(strProfileName)
{
    connect(this, &UINotificationProgress::sigProgressFinished,
            this, &UINotificationProgressCloudMachineCreate::sltHandleProgressFinished);
}

QString UINotificationProgressCloudMachineCreate::name() const
{
    return UINotificationProgress::tr("Creating cloud VM ...");
}

QString UINotificationProgressCloudMachineCreate::details() const
{
    return UINotificationProgress::tr("<b>Provider:</b> %1<br><b>Profile:</b> %2<br><b>VM Name:</b> %3")
                                      .arg(m_strProviderShortName, m_strProfileName, m_strName);
}

CProgress UINotificationProgressCloudMachineCreate::createProgress(COMResult &comResult)
{
    /* Parse cloud VM name: */
    QVector<KVirtualSystemDescriptionType> types;
    QVector<QString> refs, origValues, configValues, extraConfigValues;
    m_comVSD.GetDescriptionByType(KVirtualSystemDescriptionType_Name, types,
                                  refs, origValues, configValues, extraConfigValues);
    if (!origValues.isEmpty())
        m_strName = origValues.first();

    /* Initialize progress-wrapper: */
    CProgress comProgress = m_comClient.CreateCloudMachine(m_comVSD, m_comMachine);
    /* Store COM result: */
    comResult = m_comClient;
    /* Return progress-wrapper: */
    return comProgress;
}

void UINotificationProgressCloudMachineCreate::sltHandleProgressFinished()
{
    if (m_comMachine.isNotNull() && !m_comMachine.GetId().isNull())
        emit sigCloudMachineCreated(m_strProviderShortName, m_strProfileName, m_comMachine);
}


/*********************************************************************************************************************************
*   Class UINotificationProgressCloudMachineRemove implementation.                                                               *
*********************************************************************************************************************************/

UINotificationProgressCloudMachineRemove::UINotificationProgressCloudMachineRemove(const CCloudMachine &comMachine,
                                                                                   bool fFullRemoval,
                                                                                   const QString &strProviderShortName,
                                                                                   const QString &strProfileName)
    : m_comMachine(comMachine)
    , m_fFullRemoval(fFullRemoval)
    , m_strProviderShortName(strProviderShortName)
    , m_strProfileName(strProfileName)
{
    connect(this, &UINotificationProgress::sigProgressFinished,
            this, &UINotificationProgressCloudMachineRemove::sltHandleProgressFinished);
}

QString UINotificationProgressCloudMachineRemove::name() const
{
    return   m_fFullRemoval
           ? UINotificationProgress::tr("Deleting cloud VM files ...")
           : UINotificationProgress::tr("Removing cloud VM ...");
}

QString UINotificationProgressCloudMachineRemove::details() const
{
    return UINotificationProgress::tr("<b>VM Name:</b> %1").arg(m_strName);
}

CProgress UINotificationProgressCloudMachineRemove::createProgress(COMResult &comResult)
{
    /* Acquire cloud VM name: */
    m_strName = m_comMachine.GetName();
    if (!m_comMachine.isOk())
    {
        /* Store COM result: */
        comResult = m_comMachine;
        /* Return progress-wrapper: */
        return CProgress();
    }

    /* Initialize progress-wrapper: */
    CProgress comProgress = m_fFullRemoval
                          ? m_comMachine.Remove()
                          : m_comMachine.Unregister();
    /* Store COM result: */
    comResult = m_comMachine;
    /* Return progress-wrapper: */
    return comProgress;
}

void UINotificationProgressCloudMachineRemove::sltHandleProgressFinished()
{
    if (error().isEmpty())
        emit sigCloudMachineRemoved(m_strProviderShortName, m_strProfileName, m_strName);
}


/*********************************************************************************************************************************
*   Class UINotificationProgressCloudMachinePowerDown implementation.                                                            *
*********************************************************************************************************************************/

UINotificationProgressCloudMachinePowerDown::UINotificationProgressCloudMachinePowerDown(const CCloudMachine &comMachine)
    : m_comMachine(comMachine)
{
}

QString UINotificationProgressCloudMachinePowerDown::name() const
{
    return   UINotificationProgress::tr("Powering cloud VM down ...");
}

QString UINotificationProgressCloudMachinePowerDown::details() const
{
    return UINotificationProgress::tr("<b>VM Name:</b> %1").arg(m_strName);
}

CProgress UINotificationProgressCloudMachinePowerDown::createProgress(COMResult &comResult)
{
    /* Acquire cloud VM name: */
    m_strName = m_comMachine.GetName();
    if (!m_comMachine.isOk())
    {
        /* Store COM result: */
        comResult = m_comMachine;
        /* Return progress-wrapper: */
        return CProgress();
    }

    /* Initialize progress-wrapper: */
    CProgress comProgress = m_comMachine.PowerDown();
    /* Store COM result: */
    comResult = m_comMachine;
    /* Return progress-wrapper: */
    return comProgress;
}


/*********************************************************************************************************************************
*   Class UINotificationProgressCloudMachineShutdown implementation.                                                             *
*********************************************************************************************************************************/

UINotificationProgressCloudMachineShutdown::UINotificationProgressCloudMachineShutdown(const CCloudMachine &comMachine)
    : m_comMachine(comMachine)
{
}

QString UINotificationProgressCloudMachineShutdown::name() const
{
    return   UINotificationProgress::tr("Shutting cloud VM down ...");
}

QString UINotificationProgressCloudMachineShutdown::details() const
{
    return UINotificationProgress::tr("<b>VM Name:</b> %1").arg(m_strName);
}

CProgress UINotificationProgressCloudMachineShutdown::createProgress(COMResult &comResult)
{
    /* Acquire cloud VM name: */
    m_strName = m_comMachine.GetName();
    if (!m_comMachine.isOk())
    {
        /* Store COM result: */
        comResult = m_comMachine;
        /* Return progress-wrapper: */
        return CProgress();
    }

    /* Initialize progress-wrapper: */
    CProgress comProgress = m_comMachine.Shutdown();
    /* Store COM result: */
    comResult = m_comMachine;
    /* Return progress-wrapper: */
    return comProgress;
}


/*********************************************************************************************************************************
*   Class UINotificationProgressCloudMachineTerminate implementation.                                                            *
*********************************************************************************************************************************/

UINotificationProgressCloudMachineTerminate::UINotificationProgressCloudMachineTerminate(const CCloudMachine &comMachine)
    : m_comMachine(comMachine)
{
}

QString UINotificationProgressCloudMachineTerminate::name() const
{
    return   UINotificationProgress::tr("Terminating cloud VM ...");
}

QString UINotificationProgressCloudMachineTerminate::details() const
{
    return UINotificationProgress::tr("<b>VM Name:</b> %1").arg(m_strName);
}

CProgress UINotificationProgressCloudMachineTerminate::createProgress(COMResult &comResult)
{
    /* Acquire cloud VM name: */
    m_strName = m_comMachine.GetName();
    if (!m_comMachine.isOk())
    {
        /* Store COM result: */
        comResult = m_comMachine;
        /* Return progress-wrapper: */
        return CProgress();
    }

    /* Initialize progress-wrapper: */
    CProgress comProgress = m_comMachine.Terminate();
    /* Store COM result: */
    comResult = m_comMachine;
    /* Return progress-wrapper: */
    return comProgress;
}


/*********************************************************************************************************************************
*   Class UINotificationProgressCloudConsoleConnectionCreate implementation.                                                     *
*********************************************************************************************************************************/

UINotificationProgressCloudConsoleConnectionCreate::UINotificationProgressCloudConsoleConnectionCreate(const CCloudMachine &comMachine,
                                                                                                       const QString &strPublicKey)
    : m_comMachine(comMachine)
    , m_strPublicKey(strPublicKey)
{
}

QString UINotificationProgressCloudConsoleConnectionCreate::name() const
{
    return UINotificationProgress::tr("Creating cloud console connection ...");
}

QString UINotificationProgressCloudConsoleConnectionCreate::details() const
{
    return UINotificationProgress::tr("<b>Cloud VM Name:</b> %1").arg(m_strName);
}

CProgress UINotificationProgressCloudConsoleConnectionCreate::createProgress(COMResult &comResult)
{
    /* Acquire cloud VM name: */
    m_strName = m_comMachine.GetName();
    if (!m_comMachine.isOk())
    {
        comResult = m_comMachine;
        return CProgress();
    }

    /* Initialize progress-wrapper: */
    CProgress comProgress = m_comMachine.CreateConsoleConnection(m_strPublicKey);
    /* Store COM result: */
    comResult = m_comMachine;
    /* Return progress-wrapper: */
    return comProgress;
}


/*********************************************************************************************************************************
*   Class UINotificationProgressCloudConsoleConnectionDelete implementation.                                                     *
*********************************************************************************************************************************/

UINotificationProgressCloudConsoleConnectionDelete::UINotificationProgressCloudConsoleConnectionDelete(const CCloudMachine &comMachine)
    : m_comMachine(comMachine)
{
}

QString UINotificationProgressCloudConsoleConnectionDelete::name() const
{
    return UINotificationProgress::tr("Deleting cloud console connection ...");
}

QString UINotificationProgressCloudConsoleConnectionDelete::details() const
{
    return UINotificationProgress::tr("<b>Cloud VM Name:</b> %1").arg(m_strName);
}

CProgress UINotificationProgressCloudConsoleConnectionDelete::createProgress(COMResult &comResult)
{
    /* Acquire cloud VM name: */
    m_strName = m_comMachine.GetName();
    if (!m_comMachine.isOk())
    {
        comResult = m_comMachine;
        return CProgress();
    }

    /* Initialize progress-wrapper: */
    CProgress comProgress = m_comMachine.DeleteConsoleConnection();
    /* Store COM result: */
    comResult = m_comMachine;
    /* Return progress-wrapper: */
    return comProgress;
}


/*********************************************************************************************************************************
*   Class UINotificationProgressSnapshotTake implementation.                                                                     *
*********************************************************************************************************************************/

UINotificationProgressSnapshotTake::UINotificationProgressSnapshotTake(const CMachine &comMachine,
                                                                       const QString &strSnapshotName,
                                                                       const QString &strSnapshotDescription)
    : m_comMachine(comMachine)
    , m_strSnapshotName(strSnapshotName)
    , m_strSnapshotDescription(strSnapshotDescription)
{
    connect(this, &UINotificationProgress::sigProgressFinished,
            this, &UINotificationProgressSnapshotTake::sltHandleProgressFinished);
}

QString UINotificationProgressSnapshotTake::name() const
{
    return UINotificationProgress::tr("Taking snapshot ...");
}

QString UINotificationProgressSnapshotTake::details() const
{
    return UINotificationProgress::tr("<b>VM Name:</b> %1<br><b>Snapshot Name:</b> %2").arg(m_strMachineName, m_strSnapshotName);
}

CProgress UINotificationProgressSnapshotTake::createProgress(COMResult &comResult)
{
    /* Acquire VM id: */
    const QUuid uId = m_comMachine.GetId();
    if (!m_comMachine.isOk())
    {
        comResult = m_comMachine;
        return CProgress();
    }

    /* Acquire VM name: */
    m_strMachineName = m_comMachine.GetName();
    if (!m_comMachine.isOk())
    {
        comResult = m_comMachine;
        return CProgress();
    }

    /* Acquire session state: */
    const KSessionState enmSessionState = m_comMachine.GetSessionState();
    if (!m_comMachine.isOk())
    {
        comResult = m_comMachine;
        return CProgress();
    }

    /* Open a session thru which we will modify the machine: */
    if (enmSessionState != KSessionState_Unlocked)
        m_comSession = uiCommon().openExistingSession(uId);
    else
        m_comSession = uiCommon().openSession(uId);
    if (m_comSession.isNull())
        return CProgress();

    /* Get session machine: */
    CMachine comMachine = m_comSession.GetMachine();
    if (!m_comSession.isOk())
    {
        comResult = m_comSession;
        m_comSession.UnlockMachine();
        return CProgress();
    }

    /* Initialize progress-wrapper: */
    QUuid uSnapshotId;
    CProgress comProgress = comMachine.TakeSnapshot(m_strSnapshotName,
                                                    m_strSnapshotDescription,
                                                    true, uSnapshotId);
    /* Store COM result: */
    comResult = m_comMachine;
    /* Return progress-wrapper: */
    return comProgress;
}

void UINotificationProgressSnapshotTake::sltHandleProgressFinished()
{
    m_comSession.UnlockMachine();
}


/*********************************************************************************************************************************
*   Class UINotificationProgressSnapshotRestore implementation.                                                                  *
*********************************************************************************************************************************/

UINotificationProgressSnapshotRestore::UINotificationProgressSnapshotRestore(const CMachine &comMachine,
                                                                             const CSnapshot &comSnapshot)
    : m_comMachine(comMachine)
    , m_comSnapshot(comSnapshot)
{
    connect(this, &UINotificationProgress::sigProgressFinished,
            this, &UINotificationProgressSnapshotRestore::sltHandleProgressFinished);
}

QString UINotificationProgressSnapshotRestore::name() const
{
    return UINotificationProgress::tr("Restoring snapshot ...");
}

QString UINotificationProgressSnapshotRestore::details() const
{
    return UINotificationProgress::tr("<b>VM Name:</b> %1<br><b>Snapshot Name:</b> %2").arg(m_strMachineName, m_strSnapshotName);
}

CProgress UINotificationProgressSnapshotRestore::createProgress(COMResult &comResult)
{
    /* Acquire VM id: */
    const QUuid uId = m_comMachine.GetId();
    if (!m_comMachine.isOk())
    {
        comResult = m_comMachine;
        return CProgress();
    }

    /* Acquire VM name: */
    m_strMachineName = m_comMachine.GetName();
    if (!m_comMachine.isOk())
    {
        comResult = m_comMachine;
        return CProgress();
    }

    /* Acquire snapshot name: */
    m_strSnapshotName = m_comSnapshot.GetName();
    if (!m_comSnapshot.isOk())
    {
        comResult = m_comSnapshot;
        return CProgress();
    }

    /* Acquire session state: */
    const KSessionState enmSessionState = m_comMachine.GetSessionState();
    if (!m_comMachine.isOk())
    {
        comResult = m_comMachine;
        return CProgress();
    }

    /* Open a session thru which we will modify the machine: */
    if (enmSessionState != KSessionState_Unlocked)
        m_comSession = uiCommon().openExistingSession(uId);
    else
        m_comSession = uiCommon().openSession(uId);
    if (m_comSession.isNull())
        return CProgress();

    /* Get session machine: */
    CMachine comMachine = m_comSession.GetMachine();
    if (!m_comSession.isOk())
    {
        comResult = m_comSession;
        m_comSession.UnlockMachine();
        return CProgress();
    }

    /* Initialize progress-wrapper: */
    CProgress comProgress = comMachine.RestoreSnapshot(m_comSnapshot);
    /* Store COM result: */
    comResult = m_comMachine;
    /* Return progress-wrapper: */
    return comProgress;
}

void UINotificationProgressSnapshotRestore::sltHandleProgressFinished()
{
    m_comSession.UnlockMachine();
}


/*********************************************************************************************************************************
*   Class UINotificationProgressSnapshotDelete implementation.                                                                   *
*********************************************************************************************************************************/

UINotificationProgressSnapshotDelete::UINotificationProgressSnapshotDelete(const CMachine &comMachine,
                                                                           const QUuid &uSnapshotId)
    : m_comMachine(comMachine)
    , m_uSnapshotId(uSnapshotId)
{
    connect(this, &UINotificationProgress::sigProgressFinished,
            this, &UINotificationProgressSnapshotDelete::sltHandleProgressFinished);
}

QString UINotificationProgressSnapshotDelete::name() const
{
    return UINotificationProgress::tr("Deleting snapshot ...");
}

QString UINotificationProgressSnapshotDelete::details() const
{
    return UINotificationProgress::tr("<b>VM Name:</b> %1<br><b>Snapshot Name:</b> %2").arg(m_strMachineName, m_strSnapshotName);
}

CProgress UINotificationProgressSnapshotDelete::createProgress(COMResult &comResult)
{
    /* Acquire VM id: */
    const QUuid uId = m_comMachine.GetId();
    if (!m_comMachine.isOk())
    {
        comResult = m_comMachine;
        return CProgress();
    }

    /* Acquire VM name: */
    m_strMachineName = m_comMachine.GetName();
    if (!m_comMachine.isOk())
    {
        comResult = m_comMachine;
        return CProgress();
    }

    /* Acquire snapshot: */
    CSnapshot comSnapshot = m_comMachine.FindSnapshot(m_uSnapshotId.toString());
    if (!m_comMachine.isOk())
    {
        comResult = m_comMachine;
        return CProgress();
    }

    /* Acquire snapshot name: */
    m_strSnapshotName = comSnapshot.GetName();
    if (!comSnapshot.isOk())
    {
        comResult = comSnapshot;
        return CProgress();
    }

    /* Acquire session state: */
    const KSessionState enmSessionState = m_comMachine.GetSessionState();
    if (!m_comMachine.isOk())
    {
        comResult = m_comMachine;
        return CProgress();
    }

    /* Open a session thru which we will modify the machine: */
    if (enmSessionState != KSessionState_Unlocked)
        m_comSession = uiCommon().openExistingSession(uId);
    else
        m_comSession = uiCommon().openSession(uId);
    if (m_comSession.isNull())
        return CProgress();

    /* Get session machine: */
    CMachine comMachine = m_comSession.GetMachine();
    if (!m_comSession.isOk())
    {
        comResult = m_comSession;
        m_comSession.UnlockMachine();
        return CProgress();
    }

    /* Initialize progress-wrapper: */
    CProgress comProgress = comMachine.DeleteSnapshot(m_uSnapshotId);
    /* Store COM result: */
    comResult = m_comMachine;
    /* Return progress-wrapper: */
    return comProgress;
}

void UINotificationProgressSnapshotDelete::sltHandleProgressFinished()
{
    m_comSession.UnlockMachine();
}


/*********************************************************************************************************************************
*   Class UINotificationProgressApplianceExport implementation.                                                                  *
*********************************************************************************************************************************/

UINotificationProgressApplianceExport::UINotificationProgressApplianceExport(const CAppliance &comAppliance,
                                                                             const QString &strFormat,
                                                                             const QVector<KExportOptions> &options,
                                                                             const QString &strPath)
    : m_comAppliance(comAppliance)
    , m_strFormat(strFormat)
    , m_options(options)
    , m_strPath(strPath)
{
}

QString UINotificationProgressApplianceExport::name() const
{
    return UINotificationProgress::tr("Exporting appliance ...");
}

QString UINotificationProgressApplianceExport::details() const
{
    return UINotificationProgress::tr("<b>To:</b> %1").arg(m_strPath);
}

CProgress UINotificationProgressApplianceExport::createProgress(COMResult &comResult)
{
    /* Initialize progress-wrapper: */
    CProgress comProgress = m_comAppliance.Write(m_strFormat, m_options, m_strPath);
    /* Store COM result: */
    comResult = m_comAppliance;
    /* Return progress-wrapper: */
    return comProgress;
}


/*********************************************************************************************************************************
*   Class UINotificationProgressApplianceImport implementation.                                                                  *
*********************************************************************************************************************************/

UINotificationProgressApplianceImport::UINotificationProgressApplianceImport(const CAppliance &comAppliance,
                                                                             const QVector<KImportOptions> &options)
    : m_comAppliance(comAppliance)
    , m_options(options)
{
}

QString UINotificationProgressApplianceImport::name() const
{
    return UINotificationProgress::tr("Importing appliance ...");
}

QString UINotificationProgressApplianceImport::details() const
{
    return UINotificationProgress::tr("<b>From:</b> %1").arg(m_comAppliance.GetPath());
}

CProgress UINotificationProgressApplianceImport::createProgress(COMResult &comResult)
{
    /* Initialize progress-wrapper: */
    CProgress comProgress = m_comAppliance.ImportMachines(m_options);
    /* Store COM result: */
    comResult = m_comAppliance;
    /* Return progress-wrapper: */
    return comProgress;
}


/*********************************************************************************************************************************
*   Class UINotificationProgressExtensionPackInstall implementation.                                                             *
*********************************************************************************************************************************/

UINotificationProgressExtensionPackInstall::UINotificationProgressExtensionPackInstall(const CExtPackFile &comExtPackFile,
                                                                                       bool fReplace,
                                                                                       const QString &strExtensionPackName,
                                                                                       const QString &strDisplayInfo)
    : m_comExtPackFile(comExtPackFile)
    , m_fReplace(fReplace)
    , m_strExtensionPackName(strExtensionPackName)
    , m_strDisplayInfo(strDisplayInfo)
{
    connect(this, &UINotificationProgress::sigProgressFinished,
            this, &UINotificationProgressExtensionPackInstall::sltHandleProgressFinished);
}

QString UINotificationProgressExtensionPackInstall::name() const
{
    return UINotificationProgress::tr("Installing package ...");
}

QString UINotificationProgressExtensionPackInstall::details() const
{
    return UINotificationProgress::tr("<b>Name:</b> %1").arg(m_strExtensionPackName);
}

CProgress UINotificationProgressExtensionPackInstall::createProgress(COMResult &comResult)
{
    /* Initialize progress-wrapper: */
    CProgress comProgress = m_comExtPackFile.Install(m_fReplace, m_strDisplayInfo);
    /* Store COM result: */
    comResult = m_comExtPackFile;
    /* Return progress-wrapper: */
    return comProgress;
}

void UINotificationProgressExtensionPackInstall::sltHandleProgressFinished()
{
    if (error().isEmpty())
        emit sigExtensionPackInstalled(m_strExtensionPackName);
}


/*********************************************************************************************************************************
*   Class UINotificationProgressExtensionPackUninstall implementation.                                                           *
*********************************************************************************************************************************/

UINotificationProgressExtensionPackUninstall::UINotificationProgressExtensionPackUninstall(const CExtPackManager &comExtPackManager,
                                                                                           const QString &strExtensionPackName,
                                                                                           const QString &strDisplayInfo)
    : m_comExtPackManager(comExtPackManager)
    , m_strExtensionPackName(strExtensionPackName)
    , m_strDisplayInfo(strDisplayInfo)
{
    connect(this, &UINotificationProgress::sigProgressFinished,
            this, &UINotificationProgressExtensionPackUninstall::sltHandleProgressFinished);
}

QString UINotificationProgressExtensionPackUninstall::name() const
{
    return UINotificationProgress::tr("Uninstalling package ...");
}

QString UINotificationProgressExtensionPackUninstall::details() const
{
    return UINotificationProgress::tr("<b>Name:</b> %1").arg(m_strExtensionPackName);
}

CProgress UINotificationProgressExtensionPackUninstall::createProgress(COMResult &comResult)
{
    /* Initialize progress-wrapper: */
    CProgress comProgress = m_comExtPackManager.Uninstall(m_strExtensionPackName,
                                                          false /* forced removal? */,
                                                          m_strDisplayInfo);
    /* Store COM result: */
    comResult = m_comExtPackManager;
    /* Return progress-wrapper: */
    return comProgress;
}

void UINotificationProgressExtensionPackUninstall::sltHandleProgressFinished()
{
    if (error().isEmpty())
        emit sigExtensionPackUninstalled(m_strExtensionPackName);
}


/*********************************************************************************************************************************
*   Class UINotificationProgressHostOnlyNetworkInterfaceCreate implementation.                                                   *
*********************************************************************************************************************************/

UINotificationProgressHostOnlyNetworkInterfaceCreate::UINotificationProgressHostOnlyNetworkInterfaceCreate(const CHost &comHost,
                                                                                                           const CHostNetworkInterface &comInterface)
    : m_comHost(comHost)
    , m_comInterface(comInterface)
{
    connect(this, &UINotificationProgress::sigProgressFinished,
            this, &UINotificationProgressHostOnlyNetworkInterfaceCreate::sltHandleProgressFinished);
}

QString UINotificationProgressHostOnlyNetworkInterfaceCreate::name() const
{
    return UINotificationProgress::tr("Creating Host-only Network Interface ...");
}

QString UINotificationProgressHostOnlyNetworkInterfaceCreate::details() const
{
    return UINotificationProgress::tr("<b>Name:</b> %1").arg("TBD");
}

CProgress UINotificationProgressHostOnlyNetworkInterfaceCreate::createProgress(COMResult &comResult)
{
    /* Initialize progress-wrapper: */
    CProgress comProgress = m_comHost.CreateHostOnlyNetworkInterface(m_comInterface);
    /* Store COM result: */
    comResult = m_comHost;
    /* Return progress-wrapper: */
    return comProgress;
}

void UINotificationProgressHostOnlyNetworkInterfaceCreate::sltHandleProgressFinished()
{
    if (error().isEmpty())
        emit sigHostOnlyNetworkInterfaceCreated(m_comInterface);
}


/*********************************************************************************************************************************
*   Class UINotificationProgressHostOnlyNetworkInterfaceRemove implementation.                                                   *
*********************************************************************************************************************************/

UINotificationProgressHostOnlyNetworkInterfaceRemove::UINotificationProgressHostOnlyNetworkInterfaceRemove(const CHost &comHost,
                                                                                                           const QUuid &uInterfaceId)
    : m_comHost(comHost)
    , m_uInterfaceId(uInterfaceId)
{
    connect(this, &UINotificationProgress::sigProgressFinished,
            this, &UINotificationProgressHostOnlyNetworkInterfaceRemove::sltHandleProgressFinished);
}

QString UINotificationProgressHostOnlyNetworkInterfaceRemove::name() const
{
    return UINotificationProgress::tr("Removing Host-only Network Interface ...");
}

QString UINotificationProgressHostOnlyNetworkInterfaceRemove::details() const
{
    return UINotificationProgress::tr("<b>Name:</b> %1").arg(m_strInterfaceName);
}

CProgress UINotificationProgressHostOnlyNetworkInterfaceRemove::createProgress(COMResult &comResult)
{
    /* Acquire interface: */
    CHostNetworkInterface comInterface = m_comHost.FindHostNetworkInterfaceById(m_uInterfaceId);
    if (!m_comHost.isOk())
    {
        comResult = m_comHost;
        return CProgress();
    }

    /* Acquire interface name: */
    m_strInterfaceName = comInterface.GetName();
    if (!comInterface.isOk())
    {
        comResult = comInterface;
        return CProgress();
    }

    /* Initialize progress-wrapper: */
    CProgress comProgress = m_comHost.RemoveHostOnlyNetworkInterface(m_uInterfaceId);
    /* Store COM result: */
    comResult = m_comHost;
    /* Return progress-wrapper: */
    return comProgress;
}

void UINotificationProgressHostOnlyNetworkInterfaceRemove::sltHandleProgressFinished()
{
    if (error().isEmpty())
        emit sigHostOnlyNetworkInterfaceRemoved(m_strInterfaceName);
}


/*********************************************************************************************************************************
*   Class UINotificationDownloaderExtensionPack implementation.                                                                  *
*********************************************************************************************************************************/

/* static */
UINotificationDownloaderExtensionPack *UINotificationDownloaderExtensionPack::s_pInstance = 0;

/* static */
UINotificationDownloaderExtensionPack *UINotificationDownloaderExtensionPack::instance(const QString &strPackName)
{
    if (!s_pInstance)
        new UINotificationDownloaderExtensionPack(strPackName);
    return s_pInstance;
}

/* static */
bool UINotificationDownloaderExtensionPack::exists()
{
    return !!s_pInstance;
}

UINotificationDownloaderExtensionPack::UINotificationDownloaderExtensionPack(const QString &strPackName)
    : m_strPackName(strPackName)
{
    s_pInstance = this;
}

UINotificationDownloaderExtensionPack::~UINotificationDownloaderExtensionPack()
{
    s_pInstance = 0;
}

QString UINotificationDownloaderExtensionPack::name() const
{
    return UINotificationDownloader::tr("Downloading Extension Pack ...");
}

QString UINotificationDownloaderExtensionPack::details() const
{
    return UINotificationProgress::tr("<b>Name:</b> %1").arg(m_strPackName);
}

UIDownloader *UINotificationDownloaderExtensionPack::createDownloader()
{
    /* Create and configure the Extension Pack downloader: */
    UIDownloaderExtensionPack *pDownloader = new UIDownloaderExtensionPack;
    if (pDownloader)
    {
        connect(pDownloader, &UIDownloaderExtensionPack::sigDownloadFinished,
                this, &UINotificationDownloaderExtensionPack::sigExtensionPackDownloaded);
        return pDownloader;
    }
    return 0;
}


/*********************************************************************************************************************************
*   Class UINotificationDownloaderUserManual implementation.                                                                     *
*********************************************************************************************************************************/

/* static */
UINotificationDownloaderUserManual *UINotificationDownloaderUserManual::s_pInstance = 0;

/* static */
UINotificationDownloaderUserManual *UINotificationDownloaderUserManual::instance(const QString &strFileName)
{
    if (!s_pInstance)
        new UINotificationDownloaderUserManual(strFileName);
    return s_pInstance;
}

/* static */
bool UINotificationDownloaderUserManual::exists()
{
    return !!s_pInstance;
}

UINotificationDownloaderUserManual::UINotificationDownloaderUserManual(const QString &strFileName)
    : m_strFileName(strFileName)
{
    s_pInstance = this;
}

UINotificationDownloaderUserManual::~UINotificationDownloaderUserManual()
{
    s_pInstance = 0;
}

QString UINotificationDownloaderUserManual::name() const
{
    return UINotificationDownloader::tr("Downloading User Manual ...");
}

QString UINotificationDownloaderUserManual::details() const
{
    return UINotificationProgress::tr("<b>Name:</b> %1").arg(m_strFileName);
}

UIDownloader *UINotificationDownloaderUserManual::createDownloader()
{
    /* Create and configure the User Manual downloader: */
    UIDownloaderUserManual *pDownloader = new UIDownloaderUserManual;
    if (pDownloader)
    {
        connect(pDownloader, &UIDownloaderUserManual::sigDownloadFinished,
                this, &UINotificationDownloaderUserManual::sigUserManualDownloaded);
        return pDownloader;
    }
    return 0;
}
