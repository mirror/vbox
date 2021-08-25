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
#include "UICommon.h"
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
    , m_enmDeviceType(enmDeviceType)
{
    prepare(enmDeviceType, uSourceDiskLogicaSize);
}

void UIWizardCloneVDPageExpert::prepare(KDeviceType enmDeviceType, qulonglong uSourceDiskLogicaSize)
{
    QGridLayout *pMainLayout = new QGridLayout(this);

    m_pMediumSizePathGroupBox = new UIMediumSizeAndPathGroupBox(true /* expert mode */, 0 /* parent */, uSourceDiskLogicaSize);

    if (m_pMediumSizePathGroupBox)
    {
        pMainLayout->addWidget(m_pMediumSizePathGroupBox, 0, 0, 4, 2);
        connect(m_pMediumSizePathGroupBox, &UIMediumSizeAndPathGroupBox::sigMediumLocationButtonClicked,
                this, &UIWizardCloneVDPageExpert::sltSelectLocationButtonClicked);
        connect(m_pMediumSizePathGroupBox, &UIMediumSizeAndPathGroupBox::sigMediumPathChanged,
                this, &UIWizardCloneVDPageExpert::sltMediumPathChanged);
        connect(m_pMediumSizePathGroupBox, &UIMediumSizeAndPathGroupBox::sigMediumSizeChanged,
                this, &UIWizardCloneVDPageExpert::sltMediumSizeChanged);
    }

    m_pFormatGroupBox = new UIDiskFormatsGroupBox(true /* expert mode */, enmDeviceType, 0);
    if (m_pFormatGroupBox)
    {
        pMainLayout-> addWidget(m_pFormatGroupBox, 4, 0, 7, 1);
        connect(m_pFormatGroupBox, &UIDiskFormatsGroupBox::sigMediumFormatChanged,
                this, &UIWizardCloneVDPageExpert::sltMediumFormatChanged);
    }

    m_pVariantGroupBox = new UIDiskVariantGroupBox(true /* expert mode */, 0);
    if (m_pVariantGroupBox)
    {
        pMainLayout-> addWidget(m_pVariantGroupBox, 4, 1, 3, 1);
        connect(m_pVariantGroupBox, &UIDiskVariantGroupBox::sigMediumVariantChanged,
                this, &UIWizardCloneVDPageExpert::sltMediumVariantChanged);
    }
}

void UIWizardCloneVDPageExpert::sltMediumFormatChanged()
{
    if (cloneWizard() && m_pFormatGroupBox)
        cloneWizard()->setMediumFormat(m_pFormatGroupBox->mediumFormat());
    updateDiskWidgetsAfterMediumFormatChange();
    emit completeChanged();
}

void UIWizardCloneVDPageExpert::sltSelectLocationButtonClicked()
{
    UIWizardCloneVD *pWizard = cloneWizard();
    AssertReturnVoid(pWizard);
    CMediumFormat comMediumFormat(pWizard->mediumFormat());
    QString strSelectedPath =
        UIDiskEditorGroupBox::openFileDialogForDiskFile(pWizard->mediumPath(), comMediumFormat, pWizard->deviceType(), pWizard);

    if (strSelectedPath.isEmpty())
        return;
    QString strMediumPath =
        UIDiskEditorGroupBox::appendExtension(strSelectedPath,
                                              UIDiskFormatsGroupBox::defaultExtension(pWizard->mediumFormat(), KDeviceType_HardDisk));
    QFileInfo mediumPath(strMediumPath);
    m_pMediumSizePathGroupBox->setMediumPath(QDir::toNativeSeparators(mediumPath.absoluteFilePath()));
}

void UIWizardCloneVDPageExpert::sltMediumVariantChanged(qulonglong uVariant)
{
    if (cloneWizard())
        cloneWizard()->setMediumVariant(uVariant);
}

void UIWizardCloneVDPageExpert::sltMediumSizeChanged(qulonglong uSize)
{
    UIWizardCloneVD *pWizard = cloneWizard();
    AssertReturnVoid(pWizard);
    pWizard->setMediumSize(uSize);
    emit completeChanged();
}

void UIWizardCloneVDPageExpert::sltMediumPathChanged(const QString &strPath)
{
    UIWizardCloneVD *pWizard = cloneWizard();
    AssertReturnVoid(pWizard);
    QString strMediumPath =
        UIDiskEditorGroupBox::appendExtension(strPath,
                                              UIDiskFormatsGroupBox::defaultExtension(pWizard->mediumFormat(), pWizard->deviceType()));
    pWizard->setMediumPath(strMediumPath);
    emit completeChanged();
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
    m_pVariantGroupBox->updateMediumVariantWidgetsAfterFormatChange(pWizard->mediumFormat());

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

void UIWizardCloneVDPageExpert::updateDiskWidgetsAfterMediumFormatChange()
{
    UIWizardCloneVD *pWizard = qobject_cast<UIWizardCloneVD*>(wizard());
    AssertReturnVoid(pWizard && m_pVariantGroupBox && m_pMediumSizePathGroupBox && m_pFormatGroupBox);
    const CMediumFormat &comMediumFormat = pWizard->mediumFormat();
    AssertReturnVoid(!comMediumFormat.isNull());

    m_pVariantGroupBox->blockSignals(true);
    m_pVariantGroupBox->updateMediumVariantWidgetsAfterFormatChange(comMediumFormat);
    m_pVariantGroupBox->blockSignals(false);

    m_pMediumSizePathGroupBox->blockSignals(true);
    m_pMediumSizePathGroupBox->updateMediumPath(comMediumFormat, m_pFormatGroupBox->formatExtensions(), m_enmDeviceType);
    m_pMediumSizePathGroupBox->blockSignals(false);
    /* Update the wizard parameters explicitly since we blocked th signals: */
    pWizard->setMediumPath(m_pMediumSizePathGroupBox->mediumPath());
    pWizard->setMediumVariant(m_pVariantGroupBox->mediumVariant());
}
