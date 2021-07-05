/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMUnattendedPageBasic class implementation.
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
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpacerItem>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "UICommon.h"
#include "UIFilePathSelector.h"
#include "UIIconPool.h"
#include "UIUserNamePasswordEditor.h"
#include "UIWizardNewVMUnattendedPageBasic.h"
#include "UIWizardNewVM.h"

/* COM includes: */
#include "CHost.h"
#include "CSystemProperties.h"
#include "CUnattended.h"

// QString UIWizardNewVMUnattendedPage::productKey() const
// {
//     if (!m_pProductKeyLineEdit || !m_pProductKeyLineEdit->hasAcceptableInput())
//         return QString();
//     return m_pProductKeyLineEdit->text();
// }


bool UIWizardNewVMUnattendedPage::checkGAISOFile(UIFilePathSelector *pGAISOFilePathSelector)
{
    if (!pGAISOFilePathSelector)
        return false;
    /* GA ISO selector should not be empty since GA install check box is checked at this point: */
    const QString &strPath = pGAISOFilePathSelector->path();
    if (strPath.isNull() || strPath.isEmpty())
        return false;
    QFileInfo fileInfo(strPath);
    if (!fileInfo.exists() || !fileInfo.isReadable())
        return false;
    return true;
}



// bool UIWizardNewVMUnattendedPage::startHeadless() const
// {
//     if (!m_pStartHeadlessCheckBox)
//         return false;
//     return m_pStartHeadlessCheckBox->isChecked();
// }


UIWizardNewVMUnattendedPageBasic::UIWizardNewVMUnattendedPageBasic()
    : m_pLabel(0)
    , m_pUserNameContainer(0)
    , m_pAdditionalOptionsContainer(0)
    , m_pGAInstallationISOContainer(0)
    , m_pStartHeadlessCheckBox(0)
    , m_pUserNamePasswordEditor(0)
    , m_pHostnameLineEdit(0)
    , m_pHostnameLabel(0)
    , m_pGAISOPathLabel(0)
    , m_pGAISOFilePathSelector(0)
    , m_pProductKeyLineEdit(0)
    , m_pProductKeyLabel(0)
{
    m_pWizard = qobject_cast<UIWizardNewVM*>(wizard());
    prepare();
}

void UIWizardNewVMUnattendedPageBasic::prepare()
{
    QGridLayout *pMainLayout = new QGridLayout(this);

    m_pLabel = new QIRichTextLabel(this);
    if (m_pLabel)
        pMainLayout->addWidget(m_pLabel, 0, 0, 1, 2);
    pMainLayout->addWidget(createUserNameWidgets(), 1, 0, 1, 1);
    pMainLayout->addWidget(createAdditionalOptionsWidgets(), 1, 1, 1, 1);
    pMainLayout->addWidget(createGAInstallWidgets(), 2, 0, 1, 2);
    pMainLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding), 3, 0, 1, 2);

     createConnections();
}

void UIWizardNewVMUnattendedPageBasic::createConnections()
{
    if (m_pUserNamePasswordEditor)
    {
        connect(m_pUserNamePasswordEditor, &UIUserNamePasswordEditor::sigPasswordChanged,
                this, &UIWizardNewVMUnattendedPageBasic::sltPasswordChanged);
        connect(m_pUserNamePasswordEditor, &UIUserNamePasswordEditor::sigUserNameChanged,
                this, &UIWizardNewVMUnattendedPageBasic::sltUserNameChanged);
    }

    if (m_pGAISOFilePathSelector)
        connect(m_pGAISOFilePathSelector, &UIFilePathSelector::pathChanged,
                this, &UIWizardNewVMUnattendedPageBasic::sltGAISOPathChanged);
    if (m_pGAISOFilePathSelector)
        connect(m_pGAInstallationISOContainer, &QGroupBox::toggled,
                this, &UIWizardNewVMUnattendedPageBasic::sltInstallGACheckBoxToggle);
    if (m_pHostnameLineEdit)
        connect(m_pHostnameLineEdit, &QLineEdit::textChanged,
                this, &UIWizardNewVMUnattendedPageBasic::sltHostnameChanged);
    if (m_pProductKeyLineEdit)
        connect(m_pProductKeyLineEdit, &QLineEdit::textChanged,
                this, &UIWizardNewVMUnattendedPageBasic::sltProductKeyChanged);

}


void UIWizardNewVMUnattendedPageBasic::retranslateUi()
{
    setTitle(UIWizardNewVM::tr("Unattended Guest OS Install Setup"));
    if (m_pLabel)
        m_pLabel->setText(UIWizardNewVM::tr("<p>You can configure the unattended guest OS install by modifying username, password, and "
                                            "hostname. Additionally you can enable guest additions install. "
                                            "For Microsoft Windows guests it is possible to provide a product key.</p>"));
    if (m_pHostnameLabel)
        m_pHostnameLabel->setText(UIWizardNewVM::tr("Hostna&me:"));

    if (m_pGAISOPathLabel)
        m_pGAISOPathLabel->setText(UIWizardNewVM::tr("GA I&nstallation ISO:"));
    if (m_pGAISOFilePathSelector)
        m_pGAISOFilePathSelector->setToolTip(UIWizardNewVM::tr("Please select an installation medium (ISO file)"));
    if (m_pGAInstallationISOContainer)
    {
        m_pGAInstallationISOContainer->setTitle(UIWizardNewVM::tr("Gu&est Additions"));
        m_pGAInstallationISOContainer->setToolTip(UIWizardNewVM::tr("<p>When checked the guest additions will be installed "
                                                           "after the OS install.</p>"));
    }
    if (m_pProductKeyLabel)
        m_pProductKeyLabel->setText(UIWizardNewVM::tr("&Product Key:"));
    if (m_pUserNameContainer)
        m_pUserNameContainer->setTitle(UIWizardNewVM::tr("Username and Password"));
    if (m_pAdditionalOptionsContainer)
        m_pAdditionalOptionsContainer->setTitle(UIWizardNewVM::tr("Additional Options"));
    if (m_pStartHeadlessCheckBox)
    {
        m_pStartHeadlessCheckBox->setText(UIWizardNewVM::tr("&Install in Background"));
        m_pStartHeadlessCheckBox->setToolTip(UIWizardNewVM::tr("<p>When checked, the newly created virtual machine will be started "
                                                               "in headless mode (without a GUI) for the unattended guest OS install.</p>"));
    }
}


void UIWizardNewVMUnattendedPageBasic::initializePage()
{
    disableEnableProductKeyWidgets(isProductKeyWidgetEnabled());
    disableEnableGAWidgets(m_pGAInstallationISOContainer ? m_pGAInstallationISOContainer->isChecked() : false);
    retranslateUi();
}

bool UIWizardNewVMUnattendedPageBasic::isComplete() const
{
    markWidgets();
    if (m_pWizard && m_pWizard->installGuestAdditions() &&
        !UIWizardNewVMUnattendedPage::checkGAISOFile(m_pGAISOFilePathSelector))
        return false;
    if (m_pUserNamePasswordEditor && !m_pUserNamePasswordEditor->isComplete())
        return false;
    return true;
}

void UIWizardNewVMUnattendedPageBasic::cleanupPage()
{
}

void UIWizardNewVMUnattendedPageBasic::showEvent(QShowEvent *pEvent)
{
    UINativeWizardPage::showEvent(pEvent);
}

void UIWizardNewVMUnattendedPageBasic::sltInstallGACheckBoxToggle(bool fEnabled)
{
    disableEnableGAWidgets(fEnabled);
    if (m_pWizard)
        m_pWizard->setInstallGuestAdditions(fEnabled);
    emit completeChanged();
}

void UIWizardNewVMUnattendedPageBasic::sltGAISOPathChanged(const QString &strPath)
{
    if (m_pWizard)
        m_pWizard->setGuestAdditionsISOPath(strPath);
    emit completeChanged();
}

void UIWizardNewVMUnattendedPageBasic::sltPasswordChanged(const QString &strPassword)
{
    if (m_pWizard)
        m_pWizard->setPassword(strPassword);
    emit completeChanged();
}

void UIWizardNewVMUnattendedPageBasic::sltUserNameChanged(const QString &strUserName)
{
    if (m_pWizard)
        m_pWizard->setUserName(strUserName);
    emit completeChanged();
}

bool UIWizardNewVMUnattendedPageBasic::isProductKeyWidgetEnabled() const
{
    if (!m_pWizard || !m_pWizard->isUnattendedEnabled() || !m_pWizard->isGuestOSTypeWindows())
        return false;
    return true;
}

void UIWizardNewVMUnattendedPageBasic::sltHostnameChanged(const QString &strHostname)
{
    if (m_pWizard)
        m_pWizard->setHostname(strHostname);
}

void UIWizardNewVMUnattendedPageBasic::sltProductKeyChanged(const QString &strProductKey)
{
    if (m_pWizard)
        m_pWizard->setProductKey(strProductKey);
}

QWidget *UIWizardNewVMUnattendedPageBasic::createUserNameWidgets()
{
    if (m_pUserNameContainer)
        return m_pUserNameContainer;

    m_pUserNameContainer = new QGroupBox;
    QVBoxLayout *pUserNameContainerLayout = new QVBoxLayout(m_pUserNameContainer);
    m_pUserNamePasswordEditor = new UIUserNamePasswordEditor;
    if (m_pUserNamePasswordEditor)
    {
        m_pUserNamePasswordEditor->setLabelsVisible(true);
        pUserNameContainerLayout->addWidget(m_pUserNamePasswordEditor);
    }
    return m_pUserNameContainer;
}

QWidget *UIWizardNewVMUnattendedPageBasic::createAdditionalOptionsWidgets()
{
    if (m_pAdditionalOptionsContainer)
        return m_pAdditionalOptionsContainer;

    m_pAdditionalOptionsContainer = new QGroupBox;
    QGridLayout *pAdditionalOptionsContainerLayout = new QGridLayout(m_pAdditionalOptionsContainer);
    pAdditionalOptionsContainerLayout->setColumnStretch(0, 0);
    pAdditionalOptionsContainerLayout->setColumnStretch(1, 1);

    m_pProductKeyLabel = new QLabel;
    if (m_pProductKeyLabel)
    {
        m_pProductKeyLabel->setAlignment(Qt::AlignRight);
        m_pProductKeyLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        pAdditionalOptionsContainerLayout->addWidget(m_pProductKeyLabel, 0, 0);
    }
    m_pProductKeyLineEdit = new QLineEdit;
    if (m_pProductKeyLineEdit)
    {
        m_pProductKeyLineEdit->setInputMask(">NNNNN-NNNNN-NNNNN-NNNNN-NNNNN;#");
        if (m_pProductKeyLabel)
            m_pProductKeyLabel->setBuddy(m_pProductKeyLineEdit);
        pAdditionalOptionsContainerLayout->addWidget(m_pProductKeyLineEdit, 0, 1, 1, 2);
    }

    m_pHostnameLabel = new QLabel;
    if (m_pHostnameLabel)
    {
        m_pHostnameLabel->setAlignment(Qt::AlignRight);
        m_pHostnameLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        pAdditionalOptionsContainerLayout->addWidget(m_pHostnameLabel, 1, 0);
    }
    m_pHostnameLineEdit = new QLineEdit;
    if (m_pHostnameLineEdit)
    {
        if (m_pHostnameLabel)
            m_pHostnameLabel->setBuddy(m_pHostnameLineEdit);
        pAdditionalOptionsContainerLayout->addWidget(m_pHostnameLineEdit, 1, 1, 1, 2);
    }

    m_pStartHeadlessCheckBox = new QCheckBox;
    if (m_pStartHeadlessCheckBox)
        pAdditionalOptionsContainerLayout->addWidget(m_pStartHeadlessCheckBox, 2, 1);

    return m_pAdditionalOptionsContainer;
}

QWidget *UIWizardNewVMUnattendedPageBasic::createGAInstallWidgets()
{
    if (m_pGAInstallationISOContainer)
        return m_pGAInstallationISOContainer;

    /* Prepare GA Installation ISO container: */
    m_pGAInstallationISOContainer = new QGroupBox;
    if (m_pGAInstallationISOContainer)
    {
        m_pGAInstallationISOContainer->setCheckable(true);

        /* Prepare GA Installation ISO layout: */
        QHBoxLayout *pGAInstallationISOLayout = new QHBoxLayout(m_pGAInstallationISOContainer);
        if (pGAInstallationISOLayout)
        {
            /* Prepare GA ISO path label: */
            m_pGAISOPathLabel = new QLabel;
            if (m_pGAISOPathLabel)
            {
                m_pGAISOPathLabel->setAlignment(Qt::AlignRight);
                m_pGAISOPathLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

                pGAInstallationISOLayout->addWidget(m_pGAISOPathLabel);
            }
            /* Prepare GA ISO path editor: */
            m_pGAISOFilePathSelector = new UIFilePathSelector;
            {
                m_pGAISOFilePathSelector->setResetEnabled(false);
                m_pGAISOFilePathSelector->setMode(UIFilePathSelector::Mode_File_Open);
                m_pGAISOFilePathSelector->setFileDialogFilters("ISO Images(*.iso *.ISO)");
                m_pGAISOFilePathSelector->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
                m_pGAISOFilePathSelector->setInitialPath(uiCommon().defaultFolderPathForType(UIMediumDeviceType_DVD));
                if (m_pGAISOPathLabel)
                    m_pGAISOPathLabel->setBuddy(m_pGAISOFilePathSelector);

                pGAInstallationISOLayout->addWidget(m_pGAISOFilePathSelector);
            }
        }
    }

    return m_pGAInstallationISOContainer;
}

void UIWizardNewVMUnattendedPageBasic::disableEnableGAWidgets(bool fEnabled)
{
    if (m_pGAISOPathLabel)
        m_pGAISOPathLabel->setEnabled(fEnabled);
    if (m_pGAISOFilePathSelector)
        m_pGAISOFilePathSelector->setEnabled(fEnabled);
}

void UIWizardNewVMUnattendedPageBasic::disableEnableProductKeyWidgets(bool fEnabled)
{
    if (m_pProductKeyLabel)
        m_pProductKeyLabel->setEnabled(fEnabled);
    if (m_pProductKeyLineEdit)
        m_pProductKeyLineEdit->setEnabled(fEnabled);
}

void UIWizardNewVMUnattendedPageBasic::markWidgets() const
{
    if (m_pWizard && m_pWizard->installGuestAdditions())
        m_pGAISOFilePathSelector->mark(!UIWizardNewVMUnattendedPage::checkGAISOFile(m_pGAISOFilePathSelector));
}
