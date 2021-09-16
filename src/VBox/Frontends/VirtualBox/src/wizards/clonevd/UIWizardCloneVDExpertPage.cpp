/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVDExpertPage class implementation.
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
#include "UIWizardCloneVDExpertPage.h"
#include "UIWizardDiskEditors.h"

/* COM includes: */
#include "CSystemProperties.h"

UIWizardCloneVDExpertPage::UIWizardCloneVDExpertPage(KDeviceType enmDeviceType, qulonglong uSourceDiskLogicaSize)
    :m_pFormatGroupBox(0)
    , m_pVariantGroupBox(0)
    , m_pMediumSizePathGroupBox(0)
    , m_enmDeviceType(enmDeviceType)
{
    prepare(enmDeviceType, uSourceDiskLogicaSize);
}

void UIWizardCloneVDExpertPage::prepare(KDeviceType enmDeviceType, qulonglong uSourceDiskLogicaSize)
{
    QGridLayout *pMainLayout = new QGridLayout(this);

    m_pMediumSizePathGroupBox = new UIMediumSizeAndPathGroupBox(true /* expert mode */, 0 /* parent */, uSourceDiskLogicaSize);

    if (m_pMediumSizePathGroupBox)
    {
        pMainLayout->addWidget(m_pMediumSizePathGroupBox, 0, 0, 4, 2);
        connect(m_pMediumSizePathGroupBox, &UIMediumSizeAndPathGroupBox::sigMediumLocationButtonClicked,
                this, &UIWizardCloneVDExpertPage::sltSelectLocationButtonClicked);
        connect(m_pMediumSizePathGroupBox, &UIMediumSizeAndPathGroupBox::sigMediumPathChanged,
                this, &UIWizardCloneVDExpertPage::sltMediumPathChanged);
        connect(m_pMediumSizePathGroupBox, &UIMediumSizeAndPathGroupBox::sigMediumSizeChanged,
                this, &UIWizardCloneVDExpertPage::sltMediumSizeChanged);
    }

    m_pFormatGroupBox = new UIDiskFormatsGroupBox(true /* expert mode */, enmDeviceType, 0);
    if (m_pFormatGroupBox)
    {
        pMainLayout-> addWidget(m_pFormatGroupBox, 4, 0, 7, 1);
        connect(m_pFormatGroupBox, &UIDiskFormatsGroupBox::sigMediumFormatChanged,
                this, &UIWizardCloneVDExpertPage::sltMediumFormatChanged);
    }

    m_pVariantGroupBox = new UIDiskVariantGroupBox(true /* expert mode */, 0);
    if (m_pVariantGroupBox)
    {
        pMainLayout-> addWidget(m_pVariantGroupBox, 4, 1, 3, 1);
        connect(m_pVariantGroupBox, &UIDiskVariantGroupBox::sigMediumVariantChanged,
                this, &UIWizardCloneVDExpertPage::sltMediumVariantChanged);
    }
}

void UIWizardCloneVDExpertPage::sltMediumFormatChanged()
{
    if (wizardWindow<UIWizardCloneVD>() && m_pFormatGroupBox)
        wizardWindow<UIWizardCloneVD>()->setMediumFormat(m_pFormatGroupBox->mediumFormat());
    updateDiskWidgetsAfterMediumFormatChange();
    emit completeChanged();
}

void UIWizardCloneVDExpertPage::sltSelectLocationButtonClicked()
{
    UIWizardCloneVD *pWizard = wizardWindow<UIWizardCloneVD>();
    AssertReturnVoid(pWizard);
    CMediumFormat comMediumFormat(pWizard->mediumFormat());
    QString strSelectedPath =
        UIDiskEditorGroupBox::openFileDialogForDiskFile(pWizard->mediumPath(), comMediumFormat, pWizard->deviceType(), pWizard);

    if (strSelectedPath.isEmpty())
        return;
    QString strMediumPath =
        UIDiskEditorGroupBox::appendExtension(strSelectedPath,
                                              UIDiskEditorGroupBox::defaultExtension(pWizard->mediumFormat(), pWizard->deviceType()));
    QFileInfo mediumPath(strMediumPath);
    m_pMediumSizePathGroupBox->setMediumFilePath(QDir::toNativeSeparators(mediumPath.absoluteFilePath()));
}

void UIWizardCloneVDExpertPage::sltMediumVariantChanged(qulonglong uVariant)
{
    if (wizardWindow<UIWizardCloneVD>())
        wizardWindow<UIWizardCloneVD>()->setMediumVariant(uVariant);
}

void UIWizardCloneVDExpertPage::sltMediumSizeChanged(qulonglong uSize)
{
    UIWizardCloneVD *pWizard = wizardWindow<UIWizardCloneVD>();
    AssertReturnVoid(pWizard);
    pWizard->setMediumSize(uSize);
    emit completeChanged();
}

void UIWizardCloneVDExpertPage::sltMediumPathChanged(const QString &strPath)
{
    UIWizardCloneVD *pWizard = wizardWindow<UIWizardCloneVD>();
    AssertReturnVoid(pWizard);
    QString strMediumPath =
        UIDiskEditorGroupBox::appendExtension(strPath,
                                              UIDiskEditorGroupBox::defaultExtension(pWizard->mediumFormat(), pWizard->deviceType()));
    pWizard->setMediumPath(strMediumPath);
    emit completeChanged();
}

void UIWizardCloneVDExpertPage::retranslateUi()
{
}

void UIWizardCloneVDExpertPage::initializePage()
{
    AssertReturnVoid(wizardWindow<UIWizardCloneVD>() && m_pMediumSizePathGroupBox && m_pFormatGroupBox && m_pVariantGroupBox);
    UIWizardCloneVD *pWizard = wizardWindow<UIWizardCloneVD>();

    pWizard->setMediumFormat(m_pFormatGroupBox->mediumFormat());

    pWizard->setMediumVariant(m_pVariantGroupBox->mediumVariant());
    m_pVariantGroupBox->updateMediumVariantWidgetsAfterFormatChange(pWizard->mediumFormat());

    /* Initialize medium size widget and wizard's medium size parameter: */
    m_pMediumSizePathGroupBox->blockSignals(true);
    m_pMediumSizePathGroupBox->setMediumSize(pWizard->sourceDiskLogicalSize());
    pWizard->setMediumSize(m_pMediumSizePathGroupBox->mediumSize());
    QString strExtension = UIDiskEditorGroupBox::defaultExtension(pWizard->mediumFormat(), pWizard->deviceType());
    QString strSourceDiskPath = QDir::toNativeSeparators(QFileInfo(pWizard->sourceDiskFilePath()).absolutePath());
    /* Disk name without the format extension: */
    QString strDiskName = QString("%1_%2").arg(QFileInfo(pWizard->sourceDiskName()).completeBaseName()).arg(tr("copy"));
    QString strMediumFilePath =
        UIDiskEditorGroupBox::constructMediumFilePath(UIDiskVariantGroupBox::appendExtension(strDiskName,
                                                                                             strExtension), strSourceDiskPath);
    m_pMediumSizePathGroupBox->setMediumFilePath(strMediumFilePath);
    pWizard->setMediumPath(strMediumFilePath);
    m_pMediumSizePathGroupBox->blockSignals(false);

    /* Translate page: */
    retranslateUi();
}

bool UIWizardCloneVDExpertPage::isComplete() const
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

bool UIWizardCloneVDExpertPage::validatePage()
{
    UIWizardCloneVD *pWizard = wizardWindow<UIWizardCloneVD>();
    AssertReturn(pWizard, false);

    QString strMediumPath(pWizard->mediumPath());

    if (QFileInfo(strMediumPath).exists())
    {
        msgCenter().cannotOverwriteHardDiskStorage(strMediumPath, this);
        return false;
    }
    return pWizard->copyVirtualDisk();
}

void UIWizardCloneVDExpertPage::updateDiskWidgetsAfterMediumFormatChange()
{
    UIWizardCloneVD *pWizard = wizardWindow<UIWizardCloneVD>();
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
    pWizard->setMediumPath(m_pMediumSizePathGroupBox->mediumFilePath());
    pWizard->setMediumVariant(m_pVariantGroupBox->mediumVariant());
}
