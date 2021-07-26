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

/* GUI includes: */
#include "UINotificationObjects.h"


/*********************************************************************************************************************************
*   Class UINotificationProgressMediumMove implementation.                                                                       *
*********************************************************************************************************************************/

UINotificationProgressMediumMove::UINotificationProgressMediumMove(const CMedium &comMedium,
                                                                   const QString &strFrom,
                                                                   const QString &strTo)
    : m_comMedium(comMedium)
    , m_strFrom(strFrom)
    , m_strTo(strTo)
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
    /* Initialize progress-wrapper: */
    CProgress comProgress = m_comMedium.MoveTo(m_strTo);
    /* Store COM result: */
    comResult = m_comMedium;
    /* Return progress-wrapper: */
    return comProgress;
}


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
    return UINotificationProgress::tr("<b>Location:</b> %1<br><b>Size:</b> %2")
                                     .arg(m_comTarget.GetLocation()).arg(m_uSize);
}

CProgress UINotificationProgressMediumCreate::createProgress(COMResult &comResult)
{
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
    return UINotificationProgress::tr("<b>From:</b> %1<br><b>To:</b> %2")
                                     .arg(m_comSource.GetLocation(), m_comTarget.GetLocation());
}

CProgress UINotificationProgressMediumCopy::createProgress(COMResult &comResult)
{
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
    return UINotificationProgress::tr("<b>From:</b> %1<br><b>To:</b> %2")
                                     .arg(m_comSource.GetName(), m_comTarget.GetName());
}

CProgress UINotificationProgressMachineCopy::createProgress(COMResult &comResult)
{
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
*   Class UINotificationProgressCloudMachineAdd implementation.                                                                  *
*********************************************************************************************************************************/

UINotificationProgressCloudMachineAdd::UINotificationProgressCloudMachineAdd(const CCloudClient &comClient,
                                                                             const CCloudMachine &comMachine,
                                                                             const QString &strInstanceName,
                                                                             const QString &strShortProviderName,
                                                                             const QString &strProfileName)
    : m_comClient(comClient)
    , m_comMachine(comMachine)
    , m_strInstanceName(strInstanceName)
    , m_strShortProviderName(strShortProviderName)
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
                                     .arg(m_strShortProviderName, m_strProfileName, m_strInstanceName);
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
        emit sigCloudMachineAdded(m_strShortProviderName, m_strProfileName, m_comMachine);
}


/*********************************************************************************************************************************
*   Class UINotificationProgressCloudMachineCreate implementation.                                                               *
*********************************************************************************************************************************/

UINotificationProgressCloudMachineCreate::UINotificationProgressCloudMachineCreate(const CCloudClient &comClient,
                                                                                   const CCloudMachine &comMachine,
                                                                                   const CVirtualSystemDescription &comVSD,
                                                                                   const QString &strShortProviderName,
                                                                                   const QString &strProfileName)
    : m_comClient(comClient)
    , m_comMachine(comMachine)
    , m_comVSD(comVSD)
    , m_strShortProviderName(strShortProviderName)
    , m_strProfileName(strProfileName)
{
    /* Parse cloud VM name: */
    QVector<KVirtualSystemDescriptionType> types;
    QVector<QString> refs, origValues, configValues, extraConfigValues;
    m_comVSD.GetDescriptionByType(KVirtualSystemDescriptionType_Name, types,
                                  refs, origValues, configValues, extraConfigValues);
    if (!origValues.isEmpty())
        m_strName = origValues.first();

    /* Listen for last progress signal: */
    connect(this, &UINotificationProgress::sigProgressFinished,
            this, &UINotificationProgressCloudMachineCreate::sltHandleProgressFinished);
}

QString UINotificationProgressCloudMachineCreate::name() const
{
    return UINotificationProgress::tr("Creating cloud VM ...");
}

QString UINotificationProgressCloudMachineCreate::details() const
{
    return UINotificationProgress::tr("<b>Provider:</b> %1<br><b>Profile:</b> %2<br><b>Name:</b> %3")
                                      .arg(m_strShortProviderName, m_strProfileName, m_strName);
}

CProgress UINotificationProgressCloudMachineCreate::createProgress(COMResult &comResult)
{
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
        emit sigCloudMachineCreated(m_strShortProviderName, m_strProfileName, m_comMachine);
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
