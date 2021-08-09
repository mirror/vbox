/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVDPageExpert class implementation.
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
#include <QVBoxLayout>
#include <QRegExpValidator>
#include <QGroupBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QButtonGroup>
#include <QLineEdit>
#include <QSlider>
#include <QLabel>

/* GUI includes: */
#include "UIWizardDiskEditors.h"
#include "UIConverter.h"
#include "UIWizardNewVDPageExpert.h"
#include "UIWizardNewVD.h"
#include "UICommon.h"
#include "UIMessageCenter.h"
#include "UIIconPool.h"
#include "QIRichTextLabel.h"
#include "QIToolButton.h"
#include "QILineEdit.h"
#include "UIMediumSizeEditor.h"
#include "UIWizardNewVDPageSizeLocation.h"

/* COM includes: */
#include "CSystemProperties.h"


UIWizardNewVDPageExpert::UIWizardNewVDPageExpert(const QString &strDefaultName, const QString &strDefaultPath, qulonglong uDefaultSize)
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

void UIWizardNewVDPageExpert::prepare()
{
    QGridLayout *pMainLayout = new QGridLayout(this);
    m_pSizeAndPathGroup = new UIMediumSizeAndPathGroupBox(true /* fExpertMode */, 0);
    m_pFormatGroup = new UIDiskFormatsGroupBox(true /* fExpertMode */, 0);
    m_pVariantGroup = new UIDiskVariantGroupBox(true /* fExpertMode */, 0);

    pMainLayout->addWidget(m_pSizeAndPathGroup, 0, 0, 2, 2);
    pMainLayout->addWidget(m_pFormatGroup, 2, 0, 6, 1);
    pMainLayout->addWidget(m_pVariantGroup, 2, 1, 6, 1);

    connect(m_pFormatGroup, &UIDiskFormatsGroupBox::sigMediumFormatChanged,
            this, &UIWizardNewVDPageExpert::sltMediumFormatChanged);
    connect(m_pVariantGroup, &UIDiskVariantGroupBox::sigMediumVariantChanged,
            this, &UIWizardNewVDPageExpert::sltMediumVariantChanged);
    connect(m_pSizeAndPathGroup, &UIMediumSizeAndPathGroupBox::sigMediumLocationButtonClicked,
            this, &UIWizardNewVDPageExpert::sltSelectLocationButtonClicked);
    connect(m_pSizeAndPathGroup, &UIMediumSizeAndPathGroupBox::sigMediumSizeChanged,
            this, &UIWizardNewVDPageExpert::sltMediumSizeChanged);
    connect(m_pSizeAndPathGroup, &UIMediumSizeAndPathGroupBox::sigMediumPathChanged,
            this, &UIWizardNewVDPageExpert::sltMediumPathChanged);

    retranslateUi();
}

void UIWizardNewVDPageExpert::sltMediumSizeChanged(qulonglong uSize)
{
    newVDWizardPropertySet(MediumSize, uSize);
    emit completeChanged();
}

void UIWizardNewVDPageExpert::sltMediumPathChanged(const QString &strPath)
{
    UIWizardNewVD *pWizard = qobject_cast<UIWizardNewVD*>(wizard());
    AssertReturnVoid(pWizard);
    QString strMediumPath =
        UIDiskEditorGroupBox::appendExtension(strPath,
                                              UIDiskEditorGroupBox::defaultExtensionForMediumFormat(pWizard->mediumFormat()));
    newVDWizardPropertySet(MediumPath, strMediumPath);
    emit completeChanged();
}

void UIWizardNewVDPageExpert::sltMediumVariantChanged(qulonglong uVariant)
{
    newVDWizardPropertySet(MediumVariant, uVariant);
    emit completeChanged();
}

void UIWizardNewVDPageExpert::sltMediumFormatChanged()
{
    AssertReturnVoid(m_pFormatGroup);
    newVDWizardPropertySet(MediumFormat, m_pFormatGroup->mediumFormat());
    updateDiskWidgetsAfterMediumFormatChange();
    completeChanged();
}

void UIWizardNewVDPageExpert::sltSelectLocationButtonClicked()
{
    UIWizardNewVD *pWizard = qobject_cast<UIWizardNewVD*>(wizard());
    AssertReturnVoid(pWizard);
    QString strSelectedPath = UIWizardNewVDSizeLocation::selectNewMediumLocation(pWizard);
    if (strSelectedPath.isEmpty())
        return;
    QString strMediumPath =
        UIDiskEditorGroupBox::appendExtension(strSelectedPath,
                                              UIDiskEditorGroupBox::defaultExtensionForMediumFormat(pWizard->mediumFormat()));
    QFileInfo mediumPath(strMediumPath);
    m_pSizeAndPathGroup->setMediumPath(QDir::toNativeSeparators(mediumPath.absoluteFilePath()));
    emit completeChanged();
}

void UIWizardNewVDPageExpert::retranslateUi()
{
}

void UIWizardNewVDPageExpert::initializePage()
{
    /* First set the medium format of the wizard: */
    AssertReturnVoid(m_pFormatGroup);
    const CMediumFormat &comMediumFormat = m_pFormatGroup->mediumFormat();
    AssertReturnVoid(!comMediumFormat.isNull());
    newVDWizardPropertySet(MediumFormat, comMediumFormat);

    QString strExtension = UIDiskEditorGroupBox::defaultExtensionForMediumFormat(comMediumFormat);
    QString strMediumFilePath =
        UIDiskEditorGroupBox::constructMediumFilePath(UIDiskVariantGroupBox::appendExtension(m_strDefaultName,
                                                                                             strExtension), m_strDefaultPath);
    m_pSizeAndPathGroup->blockSignals(true);
    m_pSizeAndPathGroup->setMediumPath(strMediumFilePath);
    m_pSizeAndPathGroup->blockSignals(false);
    newVDWizardPropertySet(MediumPath, m_pSizeAndPathGroup->mediumPath());

    m_pSizeAndPathGroup->blockSignals(true);
    m_pSizeAndPathGroup->setMediumSize(m_uDefaultSize > m_uMediumSizeMin && m_uDefaultSize < m_uMediumSizeMax ? m_uDefaultSize : m_uMediumSizeMin);
    m_pSizeAndPathGroup->blockSignals(false);
    newVDWizardPropertySet(MediumSize, m_pSizeAndPathGroup->mediumSize());

    m_pVariantGroup->blockSignals(true);
    m_pVariantGroup->updateMediumVariantWidgetsAfterFormatChange(comMediumFormat);
    m_pVariantGroup->blockSignals(false);

    newVDWizardPropertySet(MediumVariant, m_pVariantGroup->mediumVariant());

    retranslateUi();
}

bool UIWizardNewVDPageExpert::isComplete() const
{
    UIWizardNewVD *pWizard = qobject_cast<UIWizardNewVD*>(wizard());
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

bool UIWizardNewVDPageExpert::validatePage()
{
    bool fResult = true;
    UIWizardNewVD *pWizard = qobject_cast<UIWizardNewVD*>(wizard());
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

void UIWizardNewVDPageExpert::updateDiskWidgetsAfterMediumFormatChange()
{
    UIWizardNewVD *pWizard = qobject_cast<UIWizardNewVD*>(wizard());
    AssertReturnVoid(pWizard && m_pVariantGroup && m_pSizeAndPathGroup && m_pFormatGroup);
    const CMediumFormat &comMediumFormat = pWizard->mediumFormat();
    AssertReturnVoid(!comMediumFormat.isNull());

    /* Block signals of the updated widgets to avoid calling corresponding slots since they add the parameters to m_userModifiedParameters: */
    m_pVariantGroup->blockSignals(true);
    m_pVariantGroup->updateMediumVariantWidgetsAfterFormatChange(comMediumFormat);
    m_pVariantGroup->blockSignals(false);

    m_pSizeAndPathGroup->blockSignals(true);
    m_pSizeAndPathGroup->updateMediumPath(comMediumFormat, m_pFormatGroup->formatExtensions());
    m_pSizeAndPathGroup->blockSignals(false);
    /* Update the wizard parameters explicitly since we blocked th signals: */
    newVDWizardPropertySet(MediumPath, m_pSizeAndPathGroup->mediumPath());
    newVDWizardPropertySet(MediumVariant, m_pVariantGroup->mediumVariant());
}
