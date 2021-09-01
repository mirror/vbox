/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVDExpertPage class implementation.
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
#include "UIWizardDiskEditors.h"
#include "UIWizardNewVDExpertPage.h"
#include "UIWizardNewVD.h"
#include "UICommon.h"
#include "UIMessageCenter.h"

/* COM includes: */
#include "CSystemProperties.h"

UIWizardNewVDExpertPage::UIWizardNewVDExpertPage(const QString &strDefaultName, const QString &strDefaultPath, qulonglong uDefaultSize)
    : UINativeWizardPage()
    , m_pSizeAndPathGroup(0)
    , m_pFormatGroup(0)
    , m_pVariantGroup(0)
    , m_strDefaultName(strDefaultName)
    , m_strDefaultPath(strDefaultPath)
    , m_uDefaultSize(uDefaultSize)
    , m_uMediumSizeMin(_4M)
    , m_uMediumSizeMax(uiCommon().virtualBox().GetSystemProperties().GetInfoVDSize())
{
    prepare();
}

void UIWizardNewVDExpertPage::prepare()
{
    QGridLayout *pMainLayout = new QGridLayout(this);
    m_pSizeAndPathGroup = new UIMediumSizeAndPathGroupBox(true /* fExpertMode */, 0 /* parent */, _4M /* minimum size */);
    m_pFormatGroup = new UIDiskFormatsGroupBox(true /* fExpertMode */, KDeviceType_HardDisk, 0);
    m_pVariantGroup = new UIDiskVariantGroupBox(true /* fExpertMode */, 0);

    pMainLayout->addWidget(m_pSizeAndPathGroup, 0, 0, 4, 2);
    pMainLayout->addWidget(m_pFormatGroup, 4, 0, 7, 1);
    pMainLayout->addWidget(m_pVariantGroup, 4, 1, 3, 1);

    connect(m_pFormatGroup, &UIDiskFormatsGroupBox::sigMediumFormatChanged,
            this, &UIWizardNewVDExpertPage::sltMediumFormatChanged);
    connect(m_pVariantGroup, &UIDiskVariantGroupBox::sigMediumVariantChanged,
            this, &UIWizardNewVDExpertPage::sltMediumVariantChanged);
    connect(m_pSizeAndPathGroup, &UIMediumSizeAndPathGroupBox::sigMediumLocationButtonClicked,
            this, &UIWizardNewVDExpertPage::sltSelectLocationButtonClicked);
    connect(m_pSizeAndPathGroup, &UIMediumSizeAndPathGroupBox::sigMediumSizeChanged,
            this, &UIWizardNewVDExpertPage::sltMediumSizeChanged);
    connect(m_pSizeAndPathGroup, &UIMediumSizeAndPathGroupBox::sigMediumPathChanged,
            this, &UIWizardNewVDExpertPage::sltMediumPathChanged);

    retranslateUi();
}

void UIWizardNewVDExpertPage::sltMediumSizeChanged(qulonglong uSize)
{
    AssertReturnVoid(wizardWindow<UIWizardNewVD>());
    wizardWindow<UIWizardNewVD>()->setMediumSize(uSize);
    emit completeChanged();
}

void UIWizardNewVDExpertPage::sltMediumPathChanged(const QString &strPath)
{
    UIWizardNewVD *pWizard = wizardWindow<UIWizardNewVD>();
    AssertReturnVoid(pWizard);
    QString strMediumPath =
        UIDiskEditorGroupBox::appendExtension(strPath,
                                              UIDiskFormatsGroupBox::defaultExtension(pWizard->mediumFormat(), KDeviceType_HardDisk));
    pWizard->setMediumPath(strMediumPath);
    emit completeChanged();
}

void UIWizardNewVDExpertPage::sltMediumVariantChanged(qulonglong uVariant)
{
    AssertReturnVoid(wizardWindow<UIWizardNewVD>());
    wizardWindow<UIWizardNewVD>()->setMediumVariant(uVariant);
    emit completeChanged();
}

void UIWizardNewVDExpertPage::sltMediumFormatChanged()
{
    AssertReturnVoid(m_pFormatGroup);
    AssertReturnVoid(wizardWindow<UIWizardNewVD>());
    wizardWindow<UIWizardNewVD>()->setMediumFormat(m_pFormatGroup->mediumFormat());
    updateDiskWidgetsAfterMediumFormatChange();
    completeChanged();
}

void UIWizardNewVDExpertPage::sltSelectLocationButtonClicked()
{
    UIWizardNewVD *pWizard = wizardWindow<UIWizardNewVD>();
    AssertReturnVoid(pWizard);
    CMediumFormat comMediumFormat(pWizard->mediumFormat());
    QString strSelectedPath =
        UIDiskEditorGroupBox::openFileDialogForDiskFile(pWizard->mediumPath(), comMediumFormat,
                                                        KDeviceType_HardDisk, pWizard);
    if (strSelectedPath.isEmpty())
        return;
    QString strMediumPath =
        UIDiskEditorGroupBox::appendExtension(strSelectedPath,
                                              UIDiskFormatsGroupBox::defaultExtension(pWizard->mediumFormat(), KDeviceType_HardDisk));
    QFileInfo mediumPath(strMediumPath);
    m_pSizeAndPathGroup->setMediumPath(QDir::toNativeSeparators(mediumPath.absoluteFilePath()));
    emit completeChanged();
}

void UIWizardNewVDExpertPage::retranslateUi()
{
}

void UIWizardNewVDExpertPage::initializePage()
{
    UIWizardNewVD *pWizard = wizardWindow<UIWizardNewVD>();
    AssertReturnVoid(pWizard);
    /* First set the medium format of the wizard: */
    AssertReturnVoid(m_pFormatGroup);
    const CMediumFormat &comMediumFormat = m_pFormatGroup->mediumFormat();
    AssertReturnVoid(!comMediumFormat.isNull());
    pWizard->setMediumFormat(comMediumFormat);

    QString strExtension = UIDiskFormatsGroupBox::defaultExtension(comMediumFormat, KDeviceType_HardDisk);
    QString strMediumFilePath =
        UIDiskEditorGroupBox::constructMediumFilePath(UIDiskVariantGroupBox::appendExtension(m_strDefaultName,
                                                                                             strExtension), m_strDefaultPath);
    m_pSizeAndPathGroup->blockSignals(true);
    m_pSizeAndPathGroup->setMediumPath(strMediumFilePath);
    m_pSizeAndPathGroup->blockSignals(false);
    pWizard->setMediumPath(m_pSizeAndPathGroup->mediumPath());

    m_pSizeAndPathGroup->blockSignals(true);
    m_pSizeAndPathGroup->setMediumSize(m_uDefaultSize > m_uMediumSizeMin && m_uDefaultSize < m_uMediumSizeMax ? m_uDefaultSize : m_uMediumSizeMin);
    m_pSizeAndPathGroup->blockSignals(false);
    pWizard->setMediumSize(m_pSizeAndPathGroup->mediumSize());

    m_pVariantGroup->blockSignals(true);
    m_pVariantGroup->updateMediumVariantWidgetsAfterFormatChange(comMediumFormat);
    m_pVariantGroup->blockSignals(false);

    pWizard->setMediumVariant(m_pVariantGroup->mediumVariant());

    retranslateUi();
}

bool UIWizardNewVDExpertPage::isComplete() const
{
    UIWizardNewVD *pWizard = wizardWindow<UIWizardNewVD>();
    AssertReturn(pWizard, false);
    if (pWizard->mediumFormat().isNull())
        return false;
    if (pWizard->mediumVariant() == (qulonglong)KMediumVariant_Max)
        return false;
    if (pWizard->mediumPath().isEmpty())
        return false;
    if (pWizard->mediumSize() > m_uMediumSizeMax || pWizard->mediumSize() < m_uMediumSizeMin)
        return false;
    return true;
}

bool UIWizardNewVDExpertPage::validatePage()
{
    bool fResult = true;
    UIWizardNewVD *pWizard = wizardWindow<UIWizardNewVD>();
    AssertReturn(pWizard, false);
    /* Make sure such file doesn't exist already: */
    const QString strMediumPath(pWizard->mediumPath());
    fResult = !QFileInfo(strMediumPath).exists();
    if (!fResult)
    {
        msgCenter().cannotOverwriteHardDiskStorage(strMediumPath, this);
        return fResult;
    }

    /* Make sure we are passing FAT size limitation: */
    fResult = UIDiskEditorGroupBox::checkFATSizeLimitation(pWizard->mediumVariant(),
                                     pWizard->mediumPath(),
                                     pWizard->mediumSize());
    if (!fResult)
    {
        msgCenter().cannotCreateHardDiskStorageInFAT(strMediumPath, this);
        return fResult;
    }

    fResult = pWizard->createVirtualDisk();
    return fResult;
}

void UIWizardNewVDExpertPage::updateDiskWidgetsAfterMediumFormatChange()
{
    UIWizardNewVD *pWizard = wizardWindow<UIWizardNewVD>();
    AssertReturnVoid(pWizard && m_pVariantGroup && m_pSizeAndPathGroup && m_pFormatGroup);
    const CMediumFormat &comMediumFormat = pWizard->mediumFormat();
    AssertReturnVoid(!comMediumFormat.isNull());

    m_pVariantGroup->updateMediumVariantWidgetsAfterFormatChange(comMediumFormat);
    m_pSizeAndPathGroup->updateMediumPath(comMediumFormat, m_pFormatGroup->formatExtensions(), KDeviceType_HardDisk);
}
