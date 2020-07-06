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
#include <QToolBox>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "UIFilePathSelector.h"
#include "UIIconPool.h"
#include "UIUserNamePasswordEditor.h"
#include "UIWizardNewVMPageBasic2.h"
#include "UIWizardNewVM.h"


UIWizardNewVMPage2::UIWizardNewVMPage2()
    : m_pToolBox(0)
    , m_pUserNamePasswordEditor(0)
    , m_pHostnameLineEdit(0)
    , m_pHostnameLabel(0)
    , m_pInstallGACheckBox(0)
    , m_pISOPathLabel(0)
    , m_pISOFilePathSelector(0)
    , m_pProductKeyLineEdit(0)
    , m_pProductKeyLabel(0)

{
}

QString UIWizardNewVMPage2::userName() const
{
    if (m_pUserNamePasswordEditor)
        return m_pUserNamePasswordEditor->userName();
    return QString();
}

void UIWizardNewVMPage2::setUserName(const QString &strName)
{
    if (m_pUserNamePasswordEditor)
        return m_pUserNamePasswordEditor->setUserName(strName);
}

QString UIWizardNewVMPage2::password() const
{
    if (m_pUserNamePasswordEditor)
        return m_pUserNamePasswordEditor->password();
    return QString();
}

void UIWizardNewVMPage2::setPassword(const QString &strPassword)
{
    if (m_pUserNamePasswordEditor)
        return m_pUserNamePasswordEditor->setPassword(strPassword);
}

QString UIWizardNewVMPage2::hostname() const
{
    if (m_pHostnameLineEdit)
        return m_pHostnameLineEdit->text();
    return QString();
}

void UIWizardNewVMPage2::setHostname(const QString &strHostName)
{
    if (m_pHostnameLineEdit)
        return m_pHostnameLineEdit->setText(strHostName);
}

bool UIWizardNewVMPage2::installGuestAdditions() const
{
    if (!m_pInstallGACheckBox)
        return true;
    return m_pInstallGACheckBox->isChecked();
}

void UIWizardNewVMPage2::setInstallGuestAdditions(bool fInstallGA)
{
    if (m_pInstallGACheckBox)
        m_pInstallGACheckBox->setChecked(fInstallGA);
}

QString UIWizardNewVMPage2::guestAdditionsISOPath() const
{
    if (!m_pISOFilePathSelector)
        return QString();
    return m_pISOFilePathSelector->path();
}

void UIWizardNewVMPage2::setGuestAdditionsISOPath(const QString &strISOPath)
{
    if (m_pISOFilePathSelector)
        m_pISOFilePathSelector->setPath(strISOPath);
}

QString UIWizardNewVMPage2::productKey() const
{
    if (!m_pProductKeyLineEdit || !m_pProductKeyLineEdit->hasAcceptableInput())
        return QString();
    return m_pProductKeyLineEdit->text();
}

void UIWizardNewVMPage2::createUserNameHostNameWidgets()
{
    if (!m_pToolBox)
        return;
    QWidget *pContainer = new QWidget;
    QGridLayout *pGridLayout = new QGridLayout(pContainer);

    m_pUserNamePasswordEditor = new UIUserNamePasswordEditor;
    pGridLayout->addWidget(m_pUserNamePasswordEditor, 0, 0, 3, 4);
    m_pHostnameLabel = new QLabel;
    m_pHostnameLineEdit = new QLineEdit;
    pGridLayout->addWidget(m_pHostnameLabel, 3, 0, 1, 1, Qt::AlignRight);
    pGridLayout->addWidget(m_pHostnameLineEdit, 3, 1, 1, 3);
    m_pToolBox->insertItem(ToolBoxItems_UserNameHostname, pContainer, UIIconPool::iconSet(":/cloud_profile_manager_16px.png"), QString());
}

void UIWizardNewVMPage2::createGAInstallWidgets()
{
    if (!m_pToolBox)
        return;
    QWidget *pContainer = new QWidget;
    QGridLayout *pContainerLayout = new QGridLayout(pContainer);

    m_pInstallGACheckBox = new QCheckBox;
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
    }

    pContainerLayout->addWidget(m_pInstallGACheckBox, 0, 0, 1, 5);
    pContainerLayout->addWidget(m_pISOPathLabel, 1, 1, 1, 1);
    pContainerLayout->addWidget(m_pISOFilePathSelector, 1, 2, 1, 4);

    m_pToolBox->insertItem(ToolBoxItems_GAInstall, pContainer, UIIconPool::iconSet(":/cloud_profile_manager_16px.png"), QString());
}

void UIWizardNewVMPage2::createProductKeyWidgets()
{
    if (!m_pToolBox)
        return;
    QWidget *pContainer = new QWidget;
    QGridLayout *pGridLayout = new QGridLayout(pContainer);

    m_pProductKeyLabel = new QLabel;
    m_pProductKeyLineEdit = new QLineEdit;
    m_pProductKeyLineEdit->setInputMask(">NNNNN-NNNNN-NNNNN-NNNNN-NNNNN;#");
    pGridLayout->addWidget(m_pProductKeyLabel, 0, 0, 1, 1, Qt::AlignRight);
    pGridLayout->addWidget(m_pProductKeyLineEdit, 0, 1, 1, 3);
    m_pToolBox->insertItem(ToolBoxItems_ProductKey, pContainer, UIIconPool::iconSet(":/cloud_profile_manager_16px.png"), QString());
}

bool UIWizardNewVMPage2::checkGAISOFile() const
{
    if (m_pInstallGACheckBox && m_pInstallGACheckBox->isChecked())
    {
        QString strISOFilePath = m_pISOFilePathSelector ? m_pISOFilePathSelector->path() : QString();
        if (!QFileInfo(strISOFilePath).exists())
            return false;
    }
    return true;
}

UIWizardNewVMPageBasic2::UIWizardNewVMPageBasic2()
    : m_pLabel(0)
{
    prepare();
}

void UIWizardNewVMPageBasic2::prepare()
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    m_pToolBox = new QToolBox;
    pMainLayout->addWidget(m_pToolBox);

    {
        m_pLabel = new QIRichTextLabel(this);
        if (m_pLabel)
            pMainLayout->addWidget(m_pLabel);
        pMainLayout->addWidget(m_pToolBox);
        pMainLayout->addStretch();
    }

    createUserNameHostNameWidgets();
    createGAInstallWidgets();
    createProductKeyWidgets();

    registerField("userName", this, "userName");
    registerField("password", this, "password");
    registerField("hostname", this, "hostname");
    registerField("installGuestAdditions", this, "installGuestAdditions");
    registerField("guestAdditionsISOPath", this, "guestAdditionsISOPath");
    registerField("productKey", this, "productKey");

    createConnections();
}

void UIWizardNewVMPageBasic2::createConnections()
{
    if (m_pUserNamePasswordEditor)
        connect(m_pUserNamePasswordEditor, &UIUserNamePasswordEditor::sigSomeTextChanged,
                this, &UIWizardNewVMPageBasic2::completeChanged);
    if (m_pInstallGACheckBox)
        connect(m_pInstallGACheckBox, &QCheckBox::toggled, this,
                &UIWizardNewVMPageBasic2::sltInstallGACheckBoxToggle);
    if (m_pISOFilePathSelector)
        connect(m_pISOFilePathSelector, &UIFilePathSelector::pathChanged,
                this, &UIWizardNewVMPageBasic2::sltGAISOPathChanged);
}

void UIWizardNewVMPageBasic2::retranslateUi()
{
    setTitle(UIWizardNewVM::tr("Unattended Guest OS Install Setup"));
    if (m_pLabel)
        m_pLabel->setText(UIWizardNewVM::tr("<p>Here you can configure the unattended install by modifying username, password, and "
                                            "hostname. You can additionally enable guest additions install and a product key for "
                                            "Microsoft Windows guests.</p>"));
    if (m_pHostnameLabel)
        m_pHostnameLabel->setText(UIWizardNewVM::tr("Hostname:"));
    if (m_pToolBox)
    {
        m_pToolBox->setItemText(ToolBoxItems_UserNameHostname, UIWizardNewVM::tr("Username and hostname"));
        m_pToolBox->setItemText(ToolBoxItems_GAInstall, UIWizardNewVM::tr("Guest additions install"));
        m_pToolBox->setItemText(ToolBoxItems_ProductKey, UIWizardNewVM::tr("Product key"));
    }
    if (m_pInstallGACheckBox)
        m_pInstallGACheckBox->setText(UIWizardNewVM::tr("Install guest additions"));
    if (m_pISOPathLabel)
        m_pISOPathLabel->setText(UIWizardNewVM::tr("Installation medium:"));
    if (m_pISOFilePathSelector)
        m_pISOFilePathSelector->setToolTip(UIWizardNewVM::tr("Please select an installation medium (ISO file)"));
    if (m_pProductKeyLabel)
        m_pProductKeyLabel->setText(UIWizardNewVM::tr("Product Key:"));
}

void UIWizardNewVMPageBasic2::initializePage()
{
    retranslateUi();
}

bool UIWizardNewVMPageBasic2::isComplete() const
{
    if (!checkGAISOFile())
        return false;
    if (m_pUserNamePasswordEditor)
        return m_pUserNamePasswordEditor->isComplete();
    return true;
}

void UIWizardNewVMPageBasic2::cleanupPage()
{
}

void UIWizardNewVMPageBasic2::showEvent(QShowEvent *pEvent)
{
    if (m_pToolBox)
    {
        QWidget *pProductKeyWidget = m_pToolBox->widget(ToolBoxItems_ProductKey);
        if (pProductKeyWidget)
            pProductKeyWidget->setEnabled(isProductKeyWidgetVisible());
    }
    UIWizardPage::showEvent(pEvent);
}


void UIWizardNewVMPageBasic2::sltInstallGACheckBoxToggle(bool fEnabled)
{
    if (m_pISOPathLabel)
        m_pISOPathLabel->setEnabled(fEnabled);
    if (m_pISOFilePathSelector)
        m_pISOFilePathSelector->setEnabled(fEnabled);
    emit completeChanged();
}

void UIWizardNewVMPageBasic2::sltGAISOPathChanged(const QString &strPath)
{
    Q_UNUSED(strPath);
    emit completeChanged();
}

bool UIWizardNewVMPageBasic2::isProductKeyWidgetVisible() const
{
    UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(wizard());
    if (!pWizard || !pWizard->isUnattendedInstallEnabled() || !pWizard->isGuestOSTypeWindows())
        return false;
    return true;
}
