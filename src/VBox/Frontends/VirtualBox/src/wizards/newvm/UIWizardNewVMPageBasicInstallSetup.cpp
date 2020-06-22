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
#include <QIntValidator>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QSpacerItem>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "UIBaseMemoryEditor.h"
#include "UIBaseMemorySlider.h"
#include "UICommon.h"
#include "UIVirtualCPUEditor.h"
#include "UIWizardNewVMPageBasicInstallSetup.h"
#include "UIWizardNewVM.h"


UIUserNamePasswordEditor::UIUserNamePasswordEditor(QWidget *pParent /*  = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pUserNameField(0)
    , m_pPasswordField(0)
    , m_pPasswordRepeatField(0)
    , m_pUserNameFieldLabel(0)
    , m_pPasswordFieldLabel(0)
    , m_pPasswordRepeatFieldLabel(0)
{
    prepare();
}

QString UIUserNamePasswordEditor::userName() const
{
    if (m_pUserNameField)
        return m_pUserNameField->text();
    return QString();
}

void UIUserNamePasswordEditor::setUserName(const QString &strUserName)
{
    if (m_pUserNameField)
        return m_pUserNameField->setText(strUserName);
}

QString UIUserNamePasswordEditor::password() const
{
    if (m_pPasswordField)
        return m_pPasswordField->text();
    return QString();
}

void UIUserNamePasswordEditor::setPassword(const QString &strPassword)
{
    if (m_pPasswordField)
        m_pPasswordField->setText(strPassword);
    if (m_pPasswordRepeatField)
        m_pPasswordRepeatField->setText(strPassword);
}

void UIUserNamePasswordEditor::retranslateUi()
{
    if (m_pUserNameFieldLabel)
    {
        m_pUserNameFieldLabel->setText(UIWizardNewVM::tr("User Name:"));
        m_pUserNameFieldLabel->setToolTip(UIWizardNewVM::tr("Type the user name which will be used in attended install:"));

    }
    if (m_pPasswordFieldLabel)
    {
        m_pPasswordFieldLabel->setText(UIWizardNewVM::tr("Password:"));
        m_pPasswordFieldLabel->setToolTip(UIWizardNewVM::tr("Type the password for the user name"));

    }
    if (m_pPasswordRepeatFieldLabel)
    {
        m_pPasswordRepeatFieldLabel->setText(UIWizardNewVM::tr("Repeat Password:"));
        m_pPasswordRepeatFieldLabel->setToolTip(UIWizardNewVM::tr("Retype the password:"));
    }
}

void UIUserNamePasswordEditor::addLineEdit(QLabel *&pLabel, QLineEdit *&pLineEdit, QGridLayout *pLayout, bool fIsPasswordField /* = false */)
{
    static int iRow = 0;
    if (!pLayout || pLabel || pLineEdit)
        return;
    pLabel = new QLabel;
    if (!pLabel)
        return;
    pLayout->addWidget(pLabel, iRow, 0, 1, 1, Qt::AlignRight);

    pLineEdit = new QLineEdit;
    if (!pLineEdit)
        return;
    pLayout->addWidget(pLineEdit, iRow, 1, 1, 1);

    pLabel->setBuddy(pLineEdit);
    if (fIsPasswordField)
        pLineEdit->setEchoMode(QLineEdit::Password);
    ++iRow;
    return;
}

void UIUserNamePasswordEditor::prepare()
{
    QGridLayout *pMainLayout = new QGridLayout;
    if (!pMainLayout)
        return;
    setLayout(pMainLayout);

    addLineEdit(m_pUserNameFieldLabel, m_pUserNameField, pMainLayout);
    addLineEdit(m_pPasswordFieldLabel, m_pPasswordField, pMainLayout, true);
    addLineEdit(m_pPasswordRepeatFieldLabel, m_pPasswordRepeatField, pMainLayout, true);

    retranslateUi();
}

UIWizardNewVMPageInstallSetup::UIWizardNewVMPageInstallSetup()
    : m_pUserNamePasswordEditor(0)
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


UIWizardNewVMPageBasicInstallSetup::UIWizardNewVMPageBasicInstallSetup()
    : m_pLabel(0)
{
    /* Create widget: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        m_pLabel = new QIRichTextLabel(this);
        QGridLayout *pMemoryLayout = new QGridLayout;
        {
            m_pUserNamePasswordEditor = new UIUserNamePasswordEditor;
            pMemoryLayout->addWidget(m_pUserNamePasswordEditor, 0, 0);
        }
        if (m_pLabel)
            pMainLayout->addWidget(m_pLabel);
        pMainLayout->addLayout(pMemoryLayout);
        pMainLayout->addStretch();
    }


    /* Register fields: */
    registerField("userName", this, "userName");
    registerField("password", this, "password");
}

void UIWizardNewVMPageBasicInstallSetup::setDefaultUnattendedInstallData(const UIUnattendedInstallData &unattendedInstallData)
{
    /* Initialize the widget data: */

    if (m_pUserNamePasswordEditor)
    {
        m_pUserNamePasswordEditor->setUserName(unattendedInstallData.m_strUserName);
        m_pUserNamePasswordEditor->setPassword(unattendedInstallData.m_strPassword);
    }
}

void UIWizardNewVMPageBasicInstallSetup::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardNewVM::tr("User Name/Password and Time Zone Selection"));

    /* Translate widgets: */
    if (m_pLabel)
        m_pLabel->setText(UIWizardNewVM::tr("<p>Here you can specify the user name/password and time zone. "
                                            "The values you enter here will be used during the unattended install.</p>"));
}

void UIWizardNewVMPageBasicInstallSetup::initializePage()
{
    /* Translate page: */
    retranslateUi();

    /* Get recommended 'ram' field value: */
    // CGuestOSType type = field("type").value<CGuestOSType>();
    // m_pBaseMemoryEditor->setValue(type.GetRecommendedRAM());
    // m_pVirtualCPUEditor->setValue(1);

    // /* 'Ram' field should have focus initially: */
    // m_pBaseMemoryEditor->setFocus();
}

bool UIWizardNewVMPageBasicInstallSetup::isComplete() const
{
    return UIWizardPage::isComplete();
}
