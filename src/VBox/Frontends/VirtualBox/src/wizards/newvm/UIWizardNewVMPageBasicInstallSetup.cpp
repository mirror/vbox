/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageBasicInstallSetup class implementation.
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
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "UIUserNamePasswordEditor.h"
#include "UIWizardNewVMPageBasicInstallSetup.h"
#include "UIWizardNewVM.h"


UIWizardNewVMPageInstallSetup::UIWizardNewVMPageInstallSetup()
    : m_pUserNamePasswordEditor(0)
    , m_pHostnameLineEdit(0)
    , m_pHostnameLabel(0)
{
}

QString UIWizardNewVMPageInstallSetup::userName() const
{
    if (m_pUserNamePasswordEditor)
        return m_pUserNamePasswordEditor->userName();
    return QString();
}

QString UIWizardNewVMPageInstallSetup::password() const
{
    if (m_pUserNamePasswordEditor)
        return m_pUserNamePasswordEditor->password();
    return QString();
}

QString UIWizardNewVMPageInstallSetup::hostname() const
{
    if (m_pHostnameLineEdit)
        return m_pHostnameLineEdit->text();
    return QString();
}

UIWizardNewVMPageBasicInstallSetup::UIWizardNewVMPageBasicInstallSetup()
    : m_pLabel(0)
{
    /* Create widget: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        m_pLabel = new QIRichTextLabel(this);
        QGridLayout *pGridLayout = new QGridLayout;
        {
            m_pUserNamePasswordEditor = new UIUserNamePasswordEditor;
            pGridLayout->addWidget(m_pUserNamePasswordEditor, 0, 0, 3, 4);
            connect(m_pUserNamePasswordEditor, &UIUserNamePasswordEditor::sigSomeTextChanged,
                    this, &UIWizardNewVMPageBasicInstallSetup::completeChanged);
            m_pHostnameLabel = new QLabel;
            m_pHostnameLineEdit = new QLineEdit;
            pGridLayout->addWidget(m_pHostnameLabel, 3, 0, 1, 1, Qt::AlignRight);
            pGridLayout->addWidget(m_pHostnameLineEdit, 3, 1, 1, 3);
        }
        if (m_pLabel)
            pMainLayout->addWidget(m_pLabel);
        pMainLayout->addLayout(pGridLayout);
        pMainLayout->addStretch();
    }

    /* Register fields: */
    registerField("userName", this, "userName");
    registerField("password", this, "password");
    registerField("hostname", this, "hostname");
}

void UIWizardNewVMPageBasicInstallSetup::setDefaultUnattendedInstallData(const UIUnattendedInstallData &unattendedInstallData)
{
    /* Initialize the widget data: */
    if (m_pUserNamePasswordEditor)
    {
        m_pUserNamePasswordEditor->setUserName(unattendedInstallData.m_strUserName);
        m_pUserNamePasswordEditor->setPassword(unattendedInstallData.m_strPassword);
    }
    if (m_pHostnameLineEdit)
        m_pHostnameLineEdit->setText(unattendedInstallData.m_strHostname);
}

void UIWizardNewVMPageBasicInstallSetup::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardNewVM::tr("User Name/Password and Hostname Settings"));

    /* Translate widgets: */
    if (m_pLabel)
        m_pLabel->setText(UIWizardNewVM::tr("<p>Here you can specify the user name/password and hostname. "
                                            "The values you enter here will be used during the unattended install.</p>"));
    if (m_pHostnameLabel)
        m_pHostnameLabel->setText(UIWizardNewVM::tr("Hostname:"));
}

void UIWizardNewVMPageBasicInstallSetup::initializePage()
{
    /* Translate page: */
    retranslateUi();
}

bool UIWizardNewVMPageBasicInstallSetup::isComplete() const
{
    if (m_pUserNamePasswordEditor)
        return m_pUserNamePasswordEditor->isComplete();
    return true;
}

int UIWizardNewVMPageBasicInstallSetup::nextId() const
{
    UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(wizard());
    if (!pWizard || !pWizard->isUnattendedInstallEnabled() || !pWizard->isGuestOSTypeWindows())
        return UIWizardNewVM::PageHardware;
    return UIWizardNewVM::PageProductKey;
}
