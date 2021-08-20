/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVD class implementation.
 */

/*
 * Copyright (C) 2006-2021 Oracle Corporation
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
#include "UIMedium.h"
#include "UIMessageCenter.h"
#include "UINotificationCenter.h"
#include "UIWizardCloneVD.h"
#include "UIWizardCloneVDPageBasic1.h"
#include "UIWizardCloneVDPageBasic2.h"
#include "UIWizardCloneVDPageBasic3.h"
#include "UIWizardCloneVDPageExpert.h"

/* COM includes: */
#include "CMediumFormat.h"


UIWizardCloneVD::UIWizardCloneVD(QWidget *pParent, const CMedium &comSourceVirtualDisk)
    : UINativeWizard(pParent, WizardType_CloneVD)
    , m_comSourceVirtualDisk(comSourceVirtualDisk)
    , m_enmDeviceType(m_comSourceVirtualDisk.GetDeviceType())
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

KDeviceType UIWizardCloneVD::deviceType() const
{
    return m_enmDeviceType;
}

bool UIWizardCloneVD::copyVirtualDisk()
{
    /* Gather attributes: */
    // const CMediumFormat comMediumFormat = field("mediumFormat").value<CMediumFormat>();
    // const qulonglong uVariant = field("mediumVariant").toULongLong();
    // const QString strMediumPath = field("mediumPath").toString();
    // const qulonglong uSize = field("mediumSize").toULongLong();
    // /* Check attributes: */
    // AssertReturn(!strMediumPath.isNull(), false);
    // AssertReturn(uSize > 0, false);

    // /* Get VBox object: */
    // CVirtualBox comVBox = uiCommon().virtualBox();

    // /* Create new virtual disk image: */
    // CMedium comVirtualDisk = comVBox.CreateMedium(comMediumFormat.GetName(), strMediumPath, KAccessMode_ReadWrite, m_enmDeviceType);
    // if (!comVBox.isOk())
    // {
    //     msgCenter().cannotCreateMediumStorage(comVBox, strMediumPath, this);
    //     return false;
    // }

    // /* Compose medium-variant: */
    // QVector<KMediumVariant> variants(sizeof(qulonglong) * 8);
    // for (int i = 0; i < variants.size(); ++i)
    // {
    //     qulonglong temp = uVariant;
    //     temp &= Q_UINT64_C(1) << i;
    //     variants[i] = (KMediumVariant)temp;
    // }

    // /* Copy medium: */
    // UINotificationProgressMediumCopy *pNotification = new UINotificationProgressMediumCopy(m_comSourceVirtualDisk,
    //                                                                                        comVirtualDisk,
    //                                                                                        variants);
    // connect(pNotification, &UINotificationProgressMediumCopy::sigMediumCopied,
    //         &uiCommon(), &UICommon::sltHandleMediumCreated);
    // gpNotificationCenter->append(pNotification);

    /* Positive: */
    return true;
}

void UIWizardCloneVD::retranslateUi()
{
    /* Translate wizard: */
    setWindowTitle(tr("Copy Virtual Disk Image"));
    UINativeWizard::retranslateUi();
}

void UIWizardCloneVD::populatePages()
{
    /* Create corresponding pages: */
    switch (mode())
    {
        case WizardMode_Basic:
        case WizardMode_Expert:
        {
            addPage(new UIWizardCloneVDPageBasic1(m_enmDeviceType));
            m_iMediumVariantPageIndex = addPage(new UIWizardCloneVDPageBasic2(m_enmDeviceType));
            addPage(new UIWizardCloneVDPageBasic3(sourceDiskLogicalSize()));
            break;
        }
        // case WizardMode_Expert:
        // {
        //     setPage(PageExpert, new UIWizardCloneVDPageExpert(m_enmDeviceType));
        //     break;
        // }
        default:
        {
            AssertMsgFailed(("Invalid mode: %d", mode()));
            break;
        }
    }
}

const CMediumFormat &UIWizardCloneVD::mediumFormat() const
{
    return m_comMediumFormat;
}

void UIWizardCloneVD::setMediumFormat(const CMediumFormat &comMediumFormat)
{
    m_comMediumFormat = comMediumFormat;
    if (mode() == WizardMode_Basic)
        setMediumVariantPageVisibility();
}

qulonglong UIWizardCloneVD::mediumVariant() const
{
    return m_uMediumVariant;
}

void UIWizardCloneVD::setMediumVariant(qulonglong uMediumVariant)
{
    m_uMediumVariant = uMediumVariant;
}

qulonglong UIWizardCloneVD::mediumSize() const
{
    return m_uMediumSize;
}

void UIWizardCloneVD::setMediumSize(qulonglong uMediumSize)
{
    m_uMediumSize = uMediumSize;
}

const QString &UIWizardCloneVD::mediumPath() const
{
    return m_strMediumPath;
}

void UIWizardCloneVD::setMediumPath(const QString &strPath)
{
    m_strMediumPath = strPath;
}

qulonglong UIWizardCloneVD::sourceDiskLogicalSize() const
{
    if (m_comSourceVirtualDisk.isNull())
        return 0;
    return m_comSourceVirtualDisk.GetLogicalSize();
}

QString UIWizardCloneVD::sourceDiskFilePath() const
{
    if (m_comSourceVirtualDisk.isNull())
        return QString();
    return m_comSourceVirtualDisk.GetLocation();
}

QString UIWizardCloneVD::sourceDiskName() const
{
    if (m_comSourceVirtualDisk.isNull())
        return QString();
    return m_comSourceVirtualDisk.GetName();
}

void UIWizardCloneVD::setMediumVariantPageVisibility()
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
