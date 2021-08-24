/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVDPageBasic3 class implementation.
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
#include <QVBoxLayout>

/* GUI includes: */
#include "UIWizardCloneVDPageBasic3.h"
#include "UIWizardDiskEditors.h"
#include "UIWizardCloneVD.h"
#include "UICommon.h"
#include "UIMessageCenter.h"
#include "UIIconPool.h"
#include "QIFileDialog.h"
#include "QIRichTextLabel.h"
#include "QIToolButton.h"

/* COM includes: */
#include "CMediumFormat.h"

UIWizardCloneVDPageBasic3::UIWizardCloneVDPageBasic3(qulonglong uSourceDiskLogicaSize)
    : m_pMediumSizePathGroupBox(0)
{
    prepare(uSourceDiskLogicaSize);
}

void UIWizardCloneVDPageBasic3::prepare(qulonglong uSourceDiskLogicaSize)
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);

    m_pMediumSizePathGroupBox = new UIMediumSizeAndPathGroupBox(false /* expert mode */, 0 /* parent */, uSourceDiskLogicaSize);
    if (m_pMediumSizePathGroupBox)
    {
        pMainLayout->addWidget(m_pMediumSizePathGroupBox);
        connect(m_pMediumSizePathGroupBox, &UIMediumSizeAndPathGroupBox::sigMediumLocationButtonClicked,
                this, &UIWizardCloneVDPageBasic3::sltSelectLocationButtonClicked);
        connect(m_pMediumSizePathGroupBox, &UIMediumSizeAndPathGroupBox::sigMediumPathChanged,
                this, &UIWizardCloneVDPageBasic3::sltMediumPathChanged);
        connect(m_pMediumSizePathGroupBox, &UIMediumSizeAndPathGroupBox::sigMediumSizeChanged,
                this, &UIWizardCloneVDPageBasic3::sltMediumSizeChanged);
    }

    pMainLayout->addStretch();
    retranslateUi();
}

void UIWizardCloneVDPageBasic3::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardCloneVD::tr("Location and size of the disk image"));
}

void UIWizardCloneVDPageBasic3::initializePage()
{
    AssertReturnVoid(cloneWizard() && m_pMediumSizePathGroupBox);
    /* Translate page: */
    retranslateUi();
    UIWizardCloneVD *pWizard = cloneWizard();
    m_pMediumSizePathGroupBox->blockSignals(true);

    /* Initialize medium size widget and wizard's medium size parameter: */
    if (!m_userModifiedParameters.contains("MediumSize"))
    {
        m_pMediumSizePathGroupBox->setMediumSize(pWizard->sourceDiskLogicalSize());
        pWizard->setMediumSize(m_pMediumSizePathGroupBox->mediumSize());
    }

    if (!m_userModifiedParameters.contains("MediumPath"))
    {
        QString strExtension = UIDiskFormatsGroupBox::defaultExtension(pWizard->mediumFormat(), KDeviceType_HardDisk);
        QString strSourceDiskPath = QDir::toNativeSeparators(QFileInfo(pWizard->sourceDiskFilePath()).absolutePath());
        /* Disk name without the format extension: */
        QString strDiskName = QString("%1_%2").arg(QFileInfo(pWizard->sourceDiskName()).completeBaseName()).arg(tr("copy"));

        QString strMediumFilePath =
            UIDiskEditorGroupBox::constructMediumFilePath(UIDiskVariantGroupBox::appendExtension(strDiskName,
                                                                                                 strExtension), strSourceDiskPath);
        m_pMediumSizePathGroupBox->setMediumPath(strMediumFilePath);
        pWizard->setMediumPath(strMediumFilePath);
    }
    m_pMediumSizePathGroupBox->blockSignals(false);
}

bool UIWizardCloneVDPageBasic3::isComplete() const
{
    AssertReturn(m_pMediumSizePathGroupBox, false);

    return m_pMediumSizePathGroupBox->isComplete();
}

bool UIWizardCloneVDPageBasic3::validatePage()
{
    UIWizardCloneVD *pWizard = cloneWizard();
    AssertReturn(pWizard, false);
    /* Make sure such file doesn't exists already: */
    QString strMediumPath(pWizard->mediumPath());
    if (QFileInfo(strMediumPath).exists())
    {
        msgCenter().cannotOverwriteHardDiskStorage(strMediumPath, this);
        return false;
    }
    return pWizard->copyVirtualDisk();
}

UIWizardCloneVD *UIWizardCloneVDPageBasic3::cloneWizard() const
{
    return qobject_cast<UIWizardCloneVD*>(wizard());
}

void UIWizardCloneVDPageBasic3::sltSelectLocationButtonClicked()
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

void UIWizardCloneVDPageBasic3::sltMediumPathChanged(const QString &strPath)
{
    UIWizardCloneVD *pWizard = cloneWizard();
    AssertReturnVoid(pWizard);
    m_userModifiedParameters << "MediumPath";
    QString strMediumPath =
        UIDiskEditorGroupBox::appendExtension(strPath,
                                              UIDiskFormatsGroupBox::defaultExtension(pWizard->mediumFormat(), pWizard->deviceType()));
    pWizard->setMediumPath(strMediumPath);
    emit completeChanged();
}

void UIWizardCloneVDPageBasic3::sltMediumSizeChanged(qulonglong uSize)
{
    UIWizardCloneVD *pWizard = cloneWizard();
    AssertReturnVoid(pWizard);
    m_userModifiedParameters << "MediumSize";
    pWizard->setMediumSize(uSize);
    emit completeChanged();
}
