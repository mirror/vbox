/* $Id$ */
/** @file
 * VBox Qt GUI - UIUserNamePasswordEditor class implementation.
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
#include <QLabel>
#include <QVBoxLayout>

/* GUI includes: */
#include "UICommon.h"
#include "UIFilePathSelector.h"
#include "UIUserNamePasswordEditor.h"
#include "UIWizardNewVM.h"
#include "UIWizardNewVMEditors.h"

/* Other VBox includes: */
#include "iprt/assert.h"

/*********************************************************************************************************************************
*   UIUserNamePasswordGroupBox implementation.                                                                                           *
*********************************************************************************************************************************/

UIUserNamePasswordGroupBox::UIUserNamePasswordGroupBox(QWidget *pParent /* = 0 */)
    :QGroupBox(pParent)
    , m_pUserNamePasswordEditor(0)
{
    prepare();
}

void UIUserNamePasswordGroupBox::prepare()
{
    QVBoxLayout *pUserNameContainerLayout = new QVBoxLayout(this);
    m_pUserNamePasswordEditor = new UIUserNamePasswordEditor;
    AssertReturnVoid(m_pUserNamePasswordEditor);
    m_pUserNamePasswordEditor->setLabelsVisible(true);
    m_pUserNamePasswordEditor->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    pUserNameContainerLayout->addWidget(m_pUserNamePasswordEditor);

    connect(m_pUserNamePasswordEditor, &UIUserNamePasswordEditor::sigPasswordChanged,
            this, &UIUserNamePasswordGroupBox::sigPasswordChanged);
    connect(m_pUserNamePasswordEditor, &UIUserNamePasswordEditor::sigUserNameChanged,
            this, &UIUserNamePasswordGroupBox::sigUserNameChanged);
}

QString UIUserNamePasswordGroupBox::userName() const
{
    if (m_pUserNamePasswordEditor)
        return m_pUserNamePasswordEditor->userName();
    return QString();
}

void UIUserNamePasswordGroupBox::setUserName(const QString &strUserName)
{
    if (m_pUserNamePasswordEditor)
        m_pUserNamePasswordEditor->setUserName(strUserName);
}

QString UIUserNamePasswordGroupBox::password() const
{
    if (m_pUserNamePasswordEditor)
        return m_pUserNamePasswordEditor->password();
    return QString();
}

void UIUserNamePasswordGroupBox::setPassword(const QString &strPassword)
{
    if (m_pUserNamePasswordEditor)
        m_pUserNamePasswordEditor->setPassword(strPassword);
}

bool UIUserNamePasswordGroupBox::isComplete()
{
    if (m_pUserNamePasswordEditor)
        return m_pUserNamePasswordEditor->isComplete();
    return false;
}

void UIUserNamePasswordGroupBox::setLabelsVisible(bool fVisible)
{
    if (m_pUserNamePasswordEditor)
        m_pUserNamePasswordEditor->setLabelsVisible(fVisible);
}

/*********************************************************************************************************************************
*   UIUserNamePasswordGroupBox implementation.                                                                                           *
*********************************************************************************************************************************/

UIGAInstallationGroupBox::UIGAInstallationGroupBox(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QGroupBox>(pParent)
    , m_pGAISOPathLabel(0)
    , m_pGAISOFilePathSelector(0)

{
    prepare();
}

void UIGAInstallationGroupBox::prepare()
{
    setCheckable(true);

    QHBoxLayout *pGAInstallationISOLayout = new QHBoxLayout(this);
    AssertReturnVoid(pGAInstallationISOLayout);
    m_pGAISOPathLabel = new QLabel;
    AssertReturnVoid(m_pGAISOPathLabel);
    m_pGAISOPathLabel->setAlignment(Qt::AlignRight);
    m_pGAISOPathLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    pGAInstallationISOLayout->addWidget(m_pGAISOPathLabel);

    m_pGAISOFilePathSelector = new UIFilePathSelector;
    AssertReturnVoid(m_pGAISOFilePathSelector);

    m_pGAISOFilePathSelector->setResetEnabled(false);
    m_pGAISOFilePathSelector->setMode(UIFilePathSelector::Mode_File_Open);
    m_pGAISOFilePathSelector->setFileDialogFilters("ISO Images(*.iso *.ISO)");
    m_pGAISOFilePathSelector->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    m_pGAISOFilePathSelector->setInitialPath(uiCommon().defaultFolderPathForType(UIMediumDeviceType_DVD));
    if (m_pGAISOPathLabel)
        m_pGAISOPathLabel->setBuddy(m_pGAISOFilePathSelector);

    pGAInstallationISOLayout->addWidget(m_pGAISOFilePathSelector);

    connect(m_pGAISOFilePathSelector, &UIFilePathSelector::pathChanged,
            this, &UIGAInstallationGroupBox::sigPathChanged);
    connect(this, &UIGAInstallationGroupBox::toggled,
            this, &UIGAInstallationGroupBox::sltToggleWidgetsEnabled);
    retranslateUi();
}

void UIGAInstallationGroupBox::retranslateUi()
{
    if (m_pGAISOFilePathSelector)
        m_pGAISOFilePathSelector->setToolTip(UIWizardNewVM::tr("Please select an installation medium (ISO file)"));
    if (m_pGAISOPathLabel)
        m_pGAISOPathLabel->setText(UIWizardNewVM::tr("GA I&nstallation ISO:"));
    setTitle(UIWizardNewVM::tr("Gu&est Additions"));
    setToolTip(UIWizardNewVM::tr("<p>When checked the guest additions will be installed "
                                                                    "after the OS install.</p>"));
}

QString UIGAInstallationGroupBox::path() const
{
    if (m_pGAISOFilePathSelector)
        return m_pGAISOFilePathSelector->path();
    return QString();
}

void UIGAInstallationGroupBox::setPath(const QString &strPath, bool fRefreshText /* = true */)
{
    if (m_pGAISOFilePathSelector)
        m_pGAISOFilePathSelector->setPath(strPath, fRefreshText);
}

void UIGAInstallationGroupBox::mark(bool fError, const QString &strErrorMessage /* = QString() */)
{
    if (m_pGAISOFilePathSelector)
        m_pGAISOFilePathSelector->mark(fError, strErrorMessage);
}


void UIGAInstallationGroupBox::sltToggleWidgetsEnabled(bool fEnabled)
{
    if (m_pGAISOPathLabel)
        m_pGAISOPathLabel->setEnabled(fEnabled);

    if (m_pGAISOFilePathSelector)
        m_pGAISOFilePathSelector->setEnabled(m_pGAISOFilePathSelector);
}
