/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageBasicUnattended class implementation.
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
#include <QLabel>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QGridLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "UICommon.h"
#include "UIFilePathSelector.h"
#include "UIMessageCenter.h"
#include "UINameAndSystemEditor.h"
#include "UIWizardNewVMPageBasicUnattended.h"
#include "UIWizardNewVM.h"

/* COM includes: */
#include "CHost.h"
#include "CSystemProperties.h"
#include "CUnattended.h"

UIWizardNewVMPageUnattended::UIWizardNewVMPageUnattended()
    : m_pUnattendedCheckBox(0)
    , m_pStartHeadlessCheckBox(0)
    , m_pISOSelectorLabel(0)
    , m_pStatusLabel(0)
    , m_pISOFilePathSelector(0)
    , m_ISOError(ISOError_NoFileName)
{
}

QString UIWizardNewVMPageUnattended::ISOFilePath() const
{
    if (!m_pISOFilePathSelector)
        return QString();
    return m_pISOFilePathSelector->path();
}

bool UIWizardNewVMPageUnattended::isUnattendedEnabled() const
{
    if (!m_pUnattendedCheckBox)
        return false;
    return m_pUnattendedCheckBox->isChecked();
}

bool UIWizardNewVMPageUnattended::startHeadless() const
{
    if (!m_pStartHeadlessCheckBox)
        return false;
    return m_pStartHeadlessCheckBox->isChecked();
}

const QString &UIWizardNewVMPageUnattended::detectedOSTypeId() const
{
    return m_strDetectedOSTypeId;
}

const QString &UIWizardNewVMPageUnattended::detectedOSVersion() const
{
    return m_strDetectedOSVersion;
}

const QString &UIWizardNewVMPageUnattended::detectedOSFlavor() const
{
    return m_strDetectedOSFlavor;
}

const QString &UIWizardNewVMPageUnattended::detectedOSLanguages() const
{
    return m_strDetectedOSLanguages;
}

const QString &UIWizardNewVMPageUnattended::detectedOSHints() const
{
    return m_strDetectedOSHints;
}

bool UIWizardNewVMPageUnattended::determineOSType(const QString &strISOPath)
{
    QFileInfo isoFileInfo(strISOPath);
    if (!isoFileInfo.exists())
    {
        m_ISOError = ISOError_DontExists;
        m_strDetectedOSTypeId.clear();
        m_strDetectedOSVersion.clear();
        m_strDetectedOSFlavor.clear();
        m_strDetectedOSLanguages.clear();
        m_strDetectedOSHints.clear();
        updateStatusLabel();
        return false;
    }

    CUnattended comUnatteded = uiCommon().virtualBox().CreateUnattendedInstaller();
    comUnatteded.SetIsoPath(strISOPath);
    comUnatteded.DetectIsoOS();

    m_strDetectedOSTypeId = comUnatteded.GetDetectedOSTypeId();
    m_strDetectedOSVersion = comUnatteded.GetDetectedOSVersion();
    m_strDetectedOSFlavor = comUnatteded.GetDetectedOSFlavor();
    m_strDetectedOSLanguages = comUnatteded.GetDetectedOSLanguages();
    m_strDetectedOSHints = comUnatteded.GetDetectedOSHints();

    updateStatusLabel();
    return true;
}

void UIWizardNewVMPageUnattended::updateStatusLabel()
{
    if (m_pStatusLabel)
    {
        m_pStatusLabel->setText(QString("<b>%1:</b> %2<br/><b>%3:</b> %4<br/><b>%5</b>: %6")
                                .arg(UIWizardNewVM::tr("Detected OS Type")).arg(m_strDetectedOSTypeId)
                                .arg(UIWizardNewVM::tr("Detected OS Version")).arg(m_strDetectedOSVersion)
                                .arg(UIWizardNewVM::tr("Detected OS Flavor")).arg(m_strDetectedOSFlavor));
    }
}

UIWizardNewVMPageBasicUnattended::UIWizardNewVMPageBasicUnattended()
    : UIWizardNewVMPageUnattended()
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        m_pLabel = new QIRichTextLabel(this);
        m_pUnattendedCheckBox = new QCheckBox;
        connect(m_pUnattendedCheckBox, &QCheckBox::toggled, this, &UIWizardNewVMPageBasicUnattended::sltUnattendedCheckBoxToggle);
        m_pISOSelectorLabel = new QLabel;
        {
            m_pISOSelectorLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
            m_pISOSelectorLabel->setEnabled(false);
        }
        m_pISOFilePathSelector = new UIFilePathSelector;
        {
            m_pISOFilePathSelector->setResetEnabled(false);
            m_pISOFilePathSelector->setMode(UIFilePathSelector::Mode_File_Open);
            m_pISOFilePathSelector->setFileDialogFilters("*.iso *.ISO");
            m_pISOFilePathSelector->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
            m_pISOFilePathSelector->setEnabled(false);
            connect(m_pISOFilePathSelector, &UIFilePathSelector::pathChanged, this, &UIWizardNewVMPageBasicUnattended::sltPathChanged);
        }
        m_pStartHeadlessCheckBox = new QCheckBox;
        m_pStartHeadlessCheckBox->setEnabled(false);
        m_pStatusLabel = new QLabel;
        m_pStatusLabel->setEnabled(false);
        pMainLayout->addWidget(m_pLabel);

        QGridLayout *pISOSelectorLayout = new QGridLayout;
        pISOSelectorLayout->addWidget(m_pUnattendedCheckBox, 0, 0, 1, 5);
        pISOSelectorLayout->addWidget(m_pISOSelectorLabel, 1, 1, 1, 1);
        pISOSelectorLayout->addWidget(m_pISOFilePathSelector, 1, 2, 1, 4);
        pISOSelectorLayout->addWidget(m_pStartHeadlessCheckBox, 2, 2, 1, 5);
        pISOSelectorLayout->addWidget(m_pStatusLabel, 3, 2, 1, 5);

        pMainLayout->addLayout(pISOSelectorLayout);
        pMainLayout->addStretch();
    }


    /* Register fields: */
    registerField("ISOFilePath", this, "ISOFilePath");
    registerField("isUnattendedEnabled", this, "isUnattendedEnabled");
    registerField("startHeadless", this, "startHeadless");
    registerField("startHeadless", this, "startHeadless");
    registerField("detectedOSTypeId", this, "detectedOSTypeId");
    registerField("detectedOSVersion", this, "detectedOSVersion");
    registerField("detectedOSFlavor", this, "detectedOSFlavor");
    registerField("detectedOSLanguages", this, "detectedOSLanguages");
    registerField("detectedOSHints", this, "detectedOSHints");


}

bool UIWizardNewVMPageBasicUnattended::isComplete() const
{
    bool fISOFileOK = checkISOFile();
    return fISOFileOK;
}

void UIWizardNewVMPageBasicUnattended::sltUnattendedCheckBoxToggle(bool fEnabled)
{
    if (m_pISOSelectorLabel)
        m_pISOSelectorLabel->setEnabled(fEnabled);
    if (m_pISOFilePathSelector)
        m_pISOFilePathSelector->setEnabled(fEnabled);
    if (m_pStartHeadlessCheckBox)
        m_pStartHeadlessCheckBox->setEnabled(fEnabled);
    if (m_pStatusLabel)
        m_pStatusLabel->setEnabled(fEnabled);
    emit completeChanged();
}

void UIWizardNewVMPageBasicUnattended::sltPathChanged(const QString &strPath)
{
    determineOSType(strPath);
    emit completeChanged();
}

void UIWizardNewVMPageBasicUnattended::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardNewVM::tr("Unattended Guest OS Install"));

    /* Translate widgets: */
    m_pLabel->setText(UIWizardNewVM::tr("You can enable the unattended (automated) guest OS install "
                                        "and select an installation medium. The guest OS will be "
                                        "installed after this window is closed. "));
    m_pUnattendedCheckBox->setText(UIWizardNewVM::tr("Enable unattended guest OS Install"));
    m_pISOSelectorLabel->setText(UIWizardNewVM::tr("Installation medium:"));
    if (m_pStartHeadlessCheckBox)
    {
        m_pStartHeadlessCheckBox->setText(UIWizardNewVM::tr("Start VM Headless"));
        m_pStartHeadlessCheckBox->setToolTip(UIWizardNewVM::tr("When checked the unattended will start the virtual machine headless"));
    }
    m_pISOFilePathSelector->setToolTip(UIWizardNewVM::tr("Please select an installation medium (ISO file) "
                                                         "for the unattended guest OS install"));
    updateStatusLabel();
}

bool UIWizardNewVMPageBasicUnattended::checkISOFile() const
{
    if (m_pUnattendedCheckBox && m_pUnattendedCheckBox->isChecked())
    {
        QString strISOFilePath = m_pISOFilePathSelector ? m_pISOFilePathSelector->path() : QString();
        if (!QFileInfo(strISOFilePath).exists())
            return false;
    }
    return true;
}

void UIWizardNewVMPageBasicUnattended::initializePage()
{
    /* Translate page: */
    retranslateUi();
    if (m_pUnattendedCheckBox)
        m_pUnattendedCheckBox->setFocus();
}

void UIWizardNewVMPageBasicUnattended::cleanupPage()
{
    UIWizardPage::cleanupPage();
}

bool UIWizardNewVMPageBasicUnattended::validatePage()
{
    return checkISOFile();
}

void UIWizardNewVMPageBasicUnattended::updateStatusLabel()
{
    UIWizardNewVMPageUnattended::updateStatusLabel();
    emit sigDetectedOSTypeChanged();
}
