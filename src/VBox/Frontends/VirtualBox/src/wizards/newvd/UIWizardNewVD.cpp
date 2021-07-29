/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVD class implementation.
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
#include <QVariant>
#include <QPushButton>

/* GUI includes: */
#include "UICommon.h"
#include "UIMedium.h"
#include "UIMessageCenter.h"
#include "UINotificationCenter.h"
#include "UIWizardNewVD.h"
#include "UIWizardNewVDPageFileType.h"
#include "UIWizardNewVDPageVariant.h"
#include "UIWizardNewVDPageSizeLocation.h"
#include "UIWizardNewVDPageExpert.h"

/* COM includes: */
#include "CMediumFormat.h"


UIWizardNewVD::UIWizardNewVD(QWidget *pParent,
                             const QString &strDefaultName,
                             const QString &strDefaultPath,
                             qulonglong uDefaultSize,
                             WizardMode mode)
    : UINativeWizard(pParent, WizardType_NewVD, mode)
    , m_strDefaultName(strDefaultName)
    , m_strDefaultPath(strDefaultPath)
    , m_uDefaultSize(uDefaultSize)
    , m_iMediumVariantPageIndex(-1)
{
#ifndef VBOX_WS_MAC
    /* Assign watermark: */
    setPixmapName(":/wizard_new_harddisk.png");
#else /* VBOX_WS_MAC */
    /* Assign background image: */
    setPixmapName(":/wizard_new_harddisk_bg.png");
#endif /* VBOX_WS_MAC */
}

qulonglong UIWizardNewVD::mediumVariant() const
{
    return m_uMediumVariant;
}

void UIWizardNewVD::setMediumVariant(qulonglong uMediumVariant)
{
    m_uMediumVariant = uMediumVariant;
}

const CMediumFormat &UIWizardNewVD::mediumFormat()
{
    return m_comMediumFormat;
}

void UIWizardNewVD::setMediumFormat(const CMediumFormat &mediumFormat)
{
    m_comMediumFormat = mediumFormat;
    if (mode() == WizardMode_Basic)
        setMediumVariantPageVisibility();
}

const QString &UIWizardNewVD::mediumPath() const
{
    return m_strMediumPath;
}

void UIWizardNewVD::setMediumPath(const QString &strMediumPath)
{
    m_strMediumPath = strMediumPath;
}

qulonglong UIWizardNewVD::mediumSize() const
{
    return m_uMediumSize;
}

void UIWizardNewVD::setMediumSize(qulonglong uMediumSize)
{
    m_uMediumSize = uMediumSize;
}

void UIWizardNewVD::populatePages()
{
    switch (mode())
    {
        case WizardMode_Basic:
        case WizardMode_Expert:
        {
            addPage(new UIWizardNewVDPageFileType);
            m_iMediumVariantPageIndex = addPage(new UIWizardNewVDPageVariant);
            addPage(new UIWizardNewVDPageSizeLocation(m_strDefaultName, m_strDefaultPath, m_uDefaultSize));
            break;
        }

        // {
        //     //addPage(new UIWizardNewVDPageExpert);
        //     break;
        // }
        default:
        {
            AssertMsgFailed(("Invalid mode: %d", mode()));
            break;
        }
    }
}

bool UIWizardNewVD::createVirtualDisk()
{
    /* Gather attributes: */
    // const CMediumFormat comMediumFormat = field("mediumFormat").value<CMediumFormat>();
    // const qulonglong uVariant = field("mediumVariant").toULongLong();
    // const QString strMediumPath = field("mediumPath").toString();
    // const qulonglong uSize = field("mediumSize").toULongLong();
    /* Check attributes: */
    AssertReturn(!m_strMediumPath.isNull(), false);
    AssertReturn(m_uMediumSize > 0, false);

    /* Get VBox object: */
    CVirtualBox comVBox = uiCommon().virtualBox();

    /* Create new virtual disk image: */
    CMedium comVirtualDisk = comVBox.CreateMedium(m_comMediumFormat.GetName(),
                                                  m_strMediumPath, KAccessMode_ReadWrite, KDeviceType_HardDisk);
    if (!comVBox.isOk())
    {
        msgCenter().cannotCreateMediumStorage(comVBox, m_strMediumPath, this);
        return false;
    }

    /* Compose medium-variant: */
    QVector<KMediumVariant> variants(sizeof(qulonglong) * 8);
    for (int i = 0; i < variants.size(); ++i)
    {
        qulonglong temp = m_uMediumVariant;
        temp &= Q_UINT64_C(1) << i;
        variants[i] = (KMediumVariant)temp;
    }

    UINotificationProgressMediumCreate *pNotification = new UINotificationProgressMediumCreate(comVirtualDisk,
                                                                                               m_uMediumSize,
                                                                                               variants);
    connect(pNotification, &UINotificationProgressMediumCreate::sigMediumCreated,
            &uiCommon(), &UICommon::sltHandleMediumCreated);
    notificationCenter().append(pNotification);

    /* Positive: */
    return true;
}

void UIWizardNewVD::retranslateUi()
{
    UINativeWizard::retranslateUi();
    setWindowTitle(tr("Create Virtual Hard Disk"));
}

void UIWizardNewVD::setMediumVariantPageVisibility()
{
    AssertReturnVoid(!m_comMediumFormat.isNull());
    ULONG uCapabilities = 0;
    QVector<KMediumFormatCapabilities> capabilities;
    capabilities = m_comMediumFormat.GetCapabilities();
    for (int i = 0; i < capabilities.size(); i++)
        uCapabilities |= capabilities[i];

    int cTest = 0;
    if (uCapabilities & KMediumFormatCapabilities_CreateDynamic)
        ++cTest;
    if (uCapabilities & KMediumFormatCapabilities_CreateFixed)
        ++cTest;
    if (uCapabilities & KMediumFormatCapabilities_CreateSplit2G)
        ++cTest;
    setPageVisible(m_iMediumVariantPageIndex, cTest > 1);
}
