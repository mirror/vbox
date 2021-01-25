/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageBasic2 class implementation.
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
#include <QCheckBox>
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "UICommon.h"
#include "UIFilePathSelector.h"
#include "UIIconPool.h"
#include "UIUserNamePasswordEditor.h"
#include "UIWizardNewVMPageBasic2.h"
#include "UIWizardNewVM.h"

/* COM includes: */
#include "CHost.h"
#include "CSystemProperties.h"
#include "CUnattended.h"

UIWizardNewVMPage2::UIWizardNewVMPage2()
    : m_pISOFilePathSelector(0)
    , m_pUnattendedLabel(0)
{
}


void UIWizardNewVMPage2::markWidgets() const
{
    if (m_pISOFilePathSelector)
        m_pISOFilePathSelector->mark(!checkISOFile());
}

void UIWizardNewVMPage2::retranslateWidgets()
{
}

QString UIWizardNewVMPage2::ISOFilePath() const
{
    if (!m_pISOFilePathSelector)
        return QString();
    return m_pISOFilePathSelector->path();
}

bool UIWizardNewVMPage2::isUnattendedEnabled() const
{
    if (!m_pISOFilePathSelector)
        return false;
    const QString &strPath = m_pISOFilePathSelector->path();
    if (strPath.isNull() || strPath.isEmpty())
        return false;
    return true;
}

const QString &UIWizardNewVMPage2::detectedOSTypeId() const
{
    return m_strDetectedOSTypeId;
}

bool UIWizardNewVMPage2::determineOSType(const QString &strISOPath)
{
    QFileInfo isoFileInfo(strISOPath);
    if (!isoFileInfo.exists())
    {
        m_strDetectedOSTypeId.clear();
        return false;
    }

    CUnattended comUnatteded = uiCommon().virtualBox().CreateUnattendedInstaller();
    comUnatteded.SetIsoPath(strISOPath);
    comUnatteded.DetectIsoOS();

    m_strDetectedOSTypeId = comUnatteded.GetDetectedOSTypeId();
    return true;
}

bool UIWizardNewVMPage2::checkISOFile() const
{
    if (!m_pISOFilePathSelector)
        return true;
    const QString &strPath = m_pISOFilePathSelector->path();
    if (strPath.isNull() || strPath.isEmpty())
        return true;
    QFileInfo fileInfo(strPath);
    if (!fileInfo.exists() || !fileInfo.isReadable())
        return false;
    return true;
}

void UIWizardNewVMPage2::setTypeByISODetectedOSType(const QString &strDetectedOSType)
{
    Q_UNUSED(strDetectedOSType);
    // if (!strDetectedOSType.isEmpty())
    //     onNameChanged(strDetectedOSType);
}

UIWizardNewVMPageBasic2::UIWizardNewVMPageBasic2()
{
    prepare();
}

void UIWizardNewVMPageBasic2::prepare()
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);

    m_pUnattendedLabel = new QIRichTextLabel;
    if (m_pUnattendedLabel)
        pMainLayout->addWidget(m_pUnattendedLabel);

    m_pISOFilePathSelector = new UIFilePathSelector;
    if (m_pISOFilePathSelector)
    {
        m_pISOFilePathSelector->setResetEnabled(false);
        m_pISOFilePathSelector->setMode(UIFilePathSelector::Mode_File_Open);
        m_pISOFilePathSelector->setFileDialogFilters("*.iso *.ISO");
        pMainLayout->addWidget(m_pISOFilePathSelector);
    }

    pMainLayout->addStretch();

    registerField("ISOFilePath", this, "ISOFilePath");
    registerField("isUnattendedEnabled", this, "isUnattendedEnabled");
    registerField("detectedOSTypeId", this, "detectedOSTypeId");

    createConnections();
}

void UIWizardNewVMPageBasic2::createConnections()
{
    if (m_pISOFilePathSelector)
        connect(m_pISOFilePathSelector, &UIFilePathSelector::pathChanged,
                this, &UIWizardNewVMPageBasic2::sltISOPathChanged);
}

void UIWizardNewVMPageBasic2::retranslateUi()
{
    setTitle(UIWizardNewVM::tr("Unattended Guest OS Install Setup"));

    if (m_pUnattendedLabel)
        m_pUnattendedLabel->setText(UIWizardNewVM::tr("Please decide whether you want to start an unattended guest OS install "
                                             "in which case you will have to select a valid installation medium. If not, "
                                             "your virtual disk will have an empty virtual hard disk after vm creation."));
    retranslateWidgets();
}

void UIWizardNewVMPageBasic2::initializePage()
{
    retranslateUi();
}

bool UIWizardNewVMPageBasic2::isComplete() const
{
    markWidgets();
    return checkISOFile();
}

void UIWizardNewVMPageBasic2::cleanupPage()
{
}

int UIWizardNewVMPageBasic2::nextId() const
{
    if (isUnattendedEnabled())
        return UIWizardNewVM::Page3;
    return UIWizardNewVM::Page4;
}


void UIWizardNewVMPageBasic2::showEvent(QShowEvent *pEvent)
{
    UIWizardPage::showEvent(pEvent);
}

void UIWizardNewVMPageBasic2::sltISOPathChanged(const QString &strPath)
{
    determineOSType(strPath);
    setTypeByISODetectedOSType(m_strDetectedOSTypeId);

    emit completeChanged();
}
