/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageBasicGAInstall class implementation.
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
#include "UIWizardNewVMPageBasicGAInstall.h"
#include "UIWizardNewVM.h"

/* COM includes: */
#include "CHost.h"
#include "CUnattended.h"

UIWizardNewVMPageGAInstall::UIWizardNewVMPageGAInstall()
    : m_pInstallGACheckBox(0)
    , m_pISOPathLabel(0)
    , m_pISOFilePathSelector(0)
{
}

bool UIWizardNewVMPageGAInstall::installGuestAdditions() const
{
    if (!m_pInstallGACheckBox)
        return true;
    return m_pInstallGACheckBox->isChecked();
}

void UIWizardNewVMPageGAInstall::setInstallGuestAdditions(bool fInstallGA)
{
    if (m_pInstallGACheckBox)
        m_pInstallGACheckBox->setChecked(fInstallGA);
}

QString UIWizardNewVMPageGAInstall::guestAdditionsISOPath() const
{
    if (!m_pISOFilePathSelector)
        return QString();
    return m_pISOFilePathSelector->path();
}

void UIWizardNewVMPageGAInstall::setGuestAdditionsISOPath(const QString &strISOPath)
{
    if (m_pISOFilePathSelector)
        m_pISOFilePathSelector->setPath(strISOPath);
}

UIWizardNewVMPageBasicGAInstall::UIWizardNewVMPageBasicGAInstall()
    : UIWizardNewVMPageGAInstall()
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        m_pLabel = new QIRichTextLabel(this);
        m_pInstallGACheckBox = new QCheckBox;
        connect(m_pInstallGACheckBox, &QCheckBox::toggled, this, &UIWizardNewVMPageBasicGAInstall::sltInstallGACheckBoxToggle);
        m_pISOPathLabel = new QLabel;
        {
            m_pISOPathLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
            m_pISOPathLabel->setEnabled(false);
        }
        m_pISOFilePathSelector = new UIFilePathSelector;
        {
            m_pISOFilePathSelector->setResetEnabled(false);
            m_pISOFilePathSelector->setMode(UIFilePathSelector::Mode_File_Open);
            m_pISOFilePathSelector->setFileDialogFilters("*.iso *.ISO");
            m_pISOFilePathSelector->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
            m_pISOFilePathSelector->setEnabled(false);
            connect(m_pISOFilePathSelector, &UIFilePathSelector::pathChanged, this, &UIWizardNewVMPageBasicGAInstall::sltPathChanged);
        }
        pMainLayout->addWidget(m_pLabel);

        QGridLayout *pISOSelectorLayout = new QGridLayout;
        pISOSelectorLayout->addWidget(m_pInstallGACheckBox, 0, 0, 1, 5);
        pISOSelectorLayout->addWidget(m_pISOPathLabel, 1, 1, 1, 1);
        pISOSelectorLayout->addWidget(m_pISOFilePathSelector, 1, 2, 1, 4);

        pMainLayout->addLayout(pISOSelectorLayout);
        pMainLayout->addStretch();
    }

    /* Register fields: */
    registerField("installGuestAdditions", this, "installGuestAdditions");
    registerField("guestAdditionsISOPath", this, "guestAdditionsISOPath");
}

bool UIWizardNewVMPageBasicGAInstall::isComplete() const
{
    return checkISOFile();
}

int UIWizardNewVMPageBasicGAInstall::nextId() const
{
    UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(wizard());
    if (!pWizard || !pWizard->isUnattendedInstallEnabled() || !pWizard->isGuestOSTypeWindows())
        return UIWizardNewVM::PageHardware;
    return UIWizardNewVM::PageProductKey;
}

void UIWizardNewVMPageBasicGAInstall::sltInstallGACheckBoxToggle(bool fEnabled)
{
    if (m_pISOPathLabel)
        m_pISOPathLabel->setEnabled(fEnabled);
    if (m_pISOFilePathSelector)
        m_pISOFilePathSelector->setEnabled(fEnabled);
    emit completeChanged();
}

void UIWizardNewVMPageBasicGAInstall::sltPathChanged(const QString &strPath)
{
    Q_UNUSED(strPath);
    emit completeChanged();
}

void UIWizardNewVMPageBasicGAInstall::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardNewVM::tr("Unattended Guest OS Install"));

    /* Translate widgets: */
    m_pLabel->setText(UIWizardNewVM::tr("<p>Guest Additions install</p>"));
    m_pInstallGACheckBox->setText(UIWizardNewVM::tr("Install guest additions"));
    m_pISOPathLabel->setText(UIWizardNewVM::tr("Installation medium:"));
    m_pISOFilePathSelector->setToolTip(UIWizardNewVM::tr("Please select an installation medium (ISO file)"));
}

bool UIWizardNewVMPageGAInstall::checkISOFile() const
{
    if (m_pInstallGACheckBox && m_pInstallGACheckBox->isChecked())
    {
        QString strISOFilePath = m_pISOFilePathSelector ? m_pISOFilePathSelector->path() : QString();
        if (!QFileInfo(strISOFilePath).exists())
            return false;
    }
    return true;
}

void UIWizardNewVMPageBasicGAInstall::initializePage()
{
    /* Translate page: */
    retranslateUi();
    if (m_pInstallGACheckBox)
        m_pInstallGACheckBox->setFocus();
}

// void UIWizardNewVMPageBasicGAInstall::cleanupPage()
// {
//     UIWizardPage::cleanupPage();
// }

// bool UIWizardNewVMPageBasicGAInstall::validatePage()
// {
//     return checkISOFile();
// }

// void UIWizardNewVMPageBasicGAInstall::updateStatusLabel()
// {
//     UIWizardNewVMPageGAInstall::updateStatusLabel();
//     emit sigDetectedOSTypeChanged();
// }
