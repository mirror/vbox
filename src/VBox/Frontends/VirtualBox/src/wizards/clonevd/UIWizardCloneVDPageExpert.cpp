/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVDPageExpert class implementation.
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
#include <QDir>
#include <QGridLayout>

/* GUI includes: */
#include "QIToolButton.h"
#include "UICommon.h"
#include "UIConverter.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "UIWizardCloneVD.h"
#include "UIWizardCloneVDPageExpert.h"
#include "UIWizardDiskEditors.h"

/* COM includes: */
#include "CSystemProperties.h"


UIWizardCloneVDPageExpert::UIWizardCloneVDPageExpert(KDeviceType enmDeviceType, qulonglong uSourceDiskLogicaSize)
    :m_pFormatGroupBox(0)
    , m_pVariantGroupBox(0)
    , m_pMediumSizePathGroupBox(0)

{
    prepare(enmDeviceType, uSourceDiskLogicaSize);
}

void UIWizardCloneVDPageExpert::prepare(KDeviceType enmDeviceType, qulonglong uSourceDiskLogicaSize)
{
    QGridLayout *pMainLayout = new QGridLayout(this);

    m_pMediumSizePathGroupBox = new UIMediumSizeAndPathGroupBox(true /* expert mode */, 0 /* parent */, uSourceDiskLogicaSize);

    if (m_pMediumSizePathGroupBox)
        pMainLayout->addWidget(m_pMediumSizePathGroupBox, 0, 0, 2, 2);

    m_pFormatGroupBox = new UIDiskFormatsGroupBox(true /* expert mode */, enmDeviceType, 0);
    if (m_pFormatGroupBox)
        pMainLayout-> addWidget(m_pFormatGroupBox, 2, 0, 6, 1);

    m_pVariantGroupBox = new UIDiskVariantGroupBox(true /* expert mode */, 0);
    if (m_pVariantGroupBox)
        pMainLayout-> addWidget(m_pVariantGroupBox, 2, 1, 6, 1);
}


void UIWizardCloneVDPageExpert::sltMediumFormatChanged()
{
}

void UIWizardCloneVDPageExpert::sltSelectLocationButtonClicked()
{

}

void UIWizardCloneVDPageExpert::retranslateUi()
{

}

void UIWizardCloneVDPageExpert::initializePage()
{
    AssertReturnVoid(cloneWizard() && m_pMediumSizePathGroupBox && m_pFormatGroupBox && m_pVariantGroupBox);
    UIWizardCloneVD *pWizard = cloneWizard();

    pWizard->setMediumFormat(m_pFormatGroupBox->mediumFormat());

    pWizard->setMediumVariant(m_pVariantGroupBox->mediumVariant());
    m_pVariantGroupBox->setWidgetVisibility(pWizard->mediumFormat());

    /* Initialize medium size widget and wizard's medium size parameter: */
    m_pMediumSizePathGroupBox->blockSignals(true);
    m_pMediumSizePathGroupBox->setMediumSize(pWizard->sourceDiskLogicalSize());
    pWizard->setMediumSize(m_pMediumSizePathGroupBox->mediumSize());
    QString strExtension = UIDiskFormatsGroupBox::defaultExtension(pWizard->mediumFormat(), KDeviceType_HardDisk);
    QString strSourceDiskPath = QDir::toNativeSeparators(QFileInfo(pWizard->sourceDiskFilePath()).absolutePath());
    /* Disk name without the format extension: */
    QString strDiskName = QString("%1_%2").arg(QFileInfo(pWizard->sourceDiskName()).completeBaseName()).arg(tr("copy"));
    QString strMediumFilePath =
        UIDiskEditorGroupBox::constructMediumFilePath(UIDiskVariantGroupBox::appendExtension(strDiskName,
                                                                                             strExtension), strSourceDiskPath);
    m_pMediumSizePathGroupBox->setMediumPath(strMediumFilePath);
    pWizard->setMediumPath(strMediumFilePath);
    m_pMediumSizePathGroupBox->blockSignals(false);

    /* Translate page: */
    retranslateUi();
}

bool UIWizardCloneVDPageExpert::isComplete() const
{
    bool fResult = true;

    if (m_pFormatGroupBox)
        fResult = m_pFormatGroupBox->mediumFormat().isNull();
    if (m_pVariantGroupBox)
        fResult = m_pVariantGroupBox->isComplete();
    if (m_pMediumSizePathGroupBox)
        fResult =  m_pMediumSizePathGroupBox->isComplete();

    return fResult;
}

bool UIWizardCloneVDPageExpert::validatePage()
{
    UIWizardCloneVD *pWizard = cloneWizard();
    AssertReturn(pWizard, false);

    QString strMediumPath(pWizard->mediumPath());

    if (QFileInfo(strMediumPath).exists())
    {
        msgCenter().cannotOverwriteHardDiskStorage(strMediumPath, this);
        return false;
    }
    return pWizard->copyVirtualDisk();
}

UIWizardCloneVD *UIWizardCloneVDPageExpert::cloneWizard()
{
    return qobject_cast<UIWizardCloneVD*>(wizard());
}
