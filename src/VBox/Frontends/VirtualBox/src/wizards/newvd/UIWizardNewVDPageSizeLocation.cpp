/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVDPageSizeLocation class implementation.
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
#include <QVBoxLayout>

/* GUI includes: */
#include "UIWizardNewVDPageSizeLocation.h"
#include "UIWizardNewVD.h"
#include "UICommon.h"
#include "UIMessageCenter.h"
#include "UIWizardDiskEditors.h"

/* COM includes: */
#include "CSystemProperties.h"

UIWizardNewVDPageSizeLocation::UIWizardNewVDPageSizeLocation(const QString &strDefaultName,
                                                             const QString &strDefaultPath, qulonglong uDefaultSize)
    : m_pMediumSizePathGroup(0)
    , m_uMediumSizeMin(_4M)
    , m_uMediumSizeMax(uiCommon().virtualBox().GetSystemProperties().GetInfoVDSize())
    , m_strDefaultName(strDefaultName.isEmpty() ? QString("NewVirtualDisk1") : strDefaultName)
    , m_strDefaultPath(strDefaultPath)
    , m_uDefaultSize(uDefaultSize)
{
    prepare();
}

void UIWizardNewVDPageSizeLocation::prepare()
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    AssertReturnVoid(pMainLayout);
    m_pMediumSizePathGroup = new UIMediumSizeAndPathGroupBox(false /* fExpertMode */, 0);
    connect(m_pMediumSizePathGroup, &UIMediumSizeAndPathGroupBox::sigMediumSizeChanged,
            this, &UIWizardNewVDPageSizeLocation::sltMediumSizeChanged);
    connect(m_pMediumSizePathGroup, &UIMediumSizeAndPathGroupBox::sigMediumPathChanged,
            this, &UIWizardNewVDPageSizeLocation::sltMediumPathChanged);
    connect(m_pMediumSizePathGroup, &UIMediumSizeAndPathGroupBox::sigMediumLocationButtonClicked,
            this, &UIWizardNewVDPageSizeLocation::sltSelectLocationButtonClicked);
    pMainLayout->addWidget(m_pMediumSizePathGroup);
    pMainLayout->addStretch();
//     }
    retranslateUi();
}

void UIWizardNewVDPageSizeLocation::sltSelectLocationButtonClicked()
{
}

void UIWizardNewVDPageSizeLocation::sltMediumSizeChanged(qulonglong /*uSize*/)
{
    AssertReturnVoid(m_pMediumSizePathGroup);
    m_userModifiedParameters << "MediumSize";
    newVDWizardPropertySet(MediumSize, m_pMediumSizePathGroup->mediumSize());
}

void UIWizardNewVDPageSizeLocation::sltMediumPathChanged(const QString &/*strPath*/)
{
    AssertReturnVoid(m_pMediumSizePathGroup);
    m_userModifiedParameters << "MediumPath";
    newVDWizardPropertySet(MediumPath, m_pMediumSizePathGroup->mediumPath());
}

void UIWizardNewVDPageSizeLocation::retranslateUi()
{
    setTitle(UIWizardNewVD::tr("File location and size"));
}

void UIWizardNewVDPageSizeLocation::initializePage()
{
    UIWizardNewVD *pWizard = qobject_cast<UIWizardNewVD*>(wizard());
    AssertReturnVoid(pWizard && m_pMediumSizePathGroup);

    if (!m_userModifiedParameters.contains("MediumPath"))
    {
        const CMediumFormat comMediumFormat = pWizard->mediumFormat();
        AssertReturnVoid(!comMediumFormat.isNull());
        QString strExtension = UIDiskEditorGroupBox::defaultExtensionForMediumFormat(comMediumFormat);
        QString strMediumFilePath =
            UIDiskEditorGroupBox::constructMediumFilePath(UIDiskVariantGroupBox::appendExtension(m_strDefaultName,
                                                                                                    strExtension), m_strDefaultPath);
        m_pMediumSizePathGroup->blockSignals(true);
        m_pMediumSizePathGroup->setMediumPath(strMediumFilePath);
        m_pMediumSizePathGroup->blockSignals(false);
        newVDWizardPropertySet(MediumPath, m_pMediumSizePathGroup->mediumPath());
    }
    if (!m_userModifiedParameters.contains("MediumSize"))
    {
        m_pMediumSizePathGroup->blockSignals(true);
        m_pMediumSizePathGroup->setMediumSize(m_uDefaultSize > m_uMediumSizeMin && m_uDefaultSize < m_uMediumSizeMax ? m_uDefaultSize : m_uMediumSizeMin);
        m_pMediumSizePathGroup->blockSignals(false);
        newVDWizardPropertySet(MediumSize, m_pMediumSizePathGroup->mediumSize());
    }
    retranslateUi();
}

bool UIWizardNewVDPageSizeLocation::isComplete() const
{
    UIWizardNewVD *pWizard = qobject_cast<UIWizardNewVD*>(wizard());
    AssertReturn(pWizard, false);
    if (pWizard->mediumPath().isEmpty())
        return false;
    if (pWizard->mediumSize() > m_uMediumSizeMax || pWizard->mediumSize() < m_uMediumSizeMin)
        return false;
    return true;
}

bool UIWizardNewVDPageSizeLocation::validatePage()
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
