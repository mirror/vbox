/* $Id$ */
/** @file
 * VBox Qt GUI - UIVisoCreatorOptionsDialog class implementation.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
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
#include <QGridLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSplitter>
#include <QStyle>

/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "QILabel.h"
#include "QILineEdit.h"
#include "QITabWidget.h"
#include "UIIconPool.h"
#include "UIToolBar.h"
#include "UIVisoHostBrowser.h"
#include "UIVisoCreator.h"
#include "UIVisoCreatorOptionsDialog.h"
#include "UIVisoContentBrowser.h"

UIVisoCreatorOptionsDialog::UIVisoCreatorOptionsDialog(const VisoOptions &visoOptions,
                                                       const BrowserOptions &browserOptions,
                                                       QWidget *pParent /* =0 */)
    : QIDialog(pParent)
    , m_pMainLayout(0)
    , m_pButtonBox(0)
    , m_pTab(0)
    , m_visoOptions(visoOptions)
    , m_browserOptions(browserOptions)
{
    prepareObjects();
    prepareConnections();
}

UIVisoCreatorOptionsDialog::~UIVisoCreatorOptionsDialog()
{
}

const BrowserOptions &UIVisoCreatorOptionsDialog::browserOptions() const
{
    return m_browserOptions;
}

const VisoOptions &UIVisoCreatorOptionsDialog::visoOptions() const
{
    return m_visoOptions;
}

void UIVisoCreatorOptionsDialog::sltHandlShowHiddenObjectsChange(int iState)
{
    if (iState == static_cast<int>(Qt::Checked))
        m_browserOptions.m_bShowHiddenObjects = true;
    else
        m_browserOptions.m_bShowHiddenObjects = false;
}

void UIVisoCreatorOptionsDialog::sltHandleVisoNameChange(const QString &strText)
{
    m_visoOptions.m_strVisoName = strText;
}

void UIVisoCreatorOptionsDialog::prepareObjects()
{
    m_pMainLayout = new QVBoxLayout;
    if (!m_pMainLayout)
        return;

    m_pTab = new QITabWidget;
    if (m_pTab)
    {
        m_pMainLayout->addWidget(m_pTab);
        m_pTab->insertTab(static_cast<int>(TabPage_VISO_Options), new QWidget, UIVisoCreator::tr("VISO Options"));
        m_pTab->insertTab(static_cast<int>(TabPage_Browser_Options), new QWidget, UIVisoCreator::tr("Browser Options"));

        m_pTab->setTabToolTip(static_cast<int>(TabPage_VISO_Options), UIVisoCreator::tr("Change VISO options"));
        m_pTab->setTabWhatsThis(static_cast<int>(TabPage_VISO_Options), UIVisoCreator::tr("Change VISO options"));

        m_pTab->setTabToolTip(static_cast<int>(TabPage_Browser_Options), UIVisoCreator::tr("Change Browser options"));
        m_pTab->setTabWhatsThis(static_cast<int>(TabPage_Browser_Options), UIVisoCreator::tr("Change Browser options"));


    }
    prepareTabWidget();

    m_pButtonBox = new QIDialogButtonBox;
    if (m_pButtonBox)
    {
        m_pButtonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
        m_pButtonBox->button(QDialogButtonBox::Cancel)->setShortcut(Qt::Key_Escape);
        m_pMainLayout->addWidget(m_pButtonBox);
    }
    setLayout(m_pMainLayout);
}

void UIVisoCreatorOptionsDialog::prepareConnections()
{
    if (m_pButtonBox)
    {
        connect(m_pButtonBox, &QIDialogButtonBox::rejected, this, &UIVisoCreatorOptionsDialog::close);
        connect(m_pButtonBox, &QIDialogButtonBox::accepted, this, &UIVisoCreatorOptionsDialog::accept);
    }
}

void UIVisoCreatorOptionsDialog::prepareTabWidget()
{
    if (!m_pTab)
        return;
    QWidget *pVisoPage = m_pTab->widget(static_cast<int>(TabPage_VISO_Options));
    if (pVisoPage)
    {
        QGridLayout *pLayout = new QGridLayout;

        QILineEdit *pVisoNameLineEdit = new QILineEdit;
        connect(pVisoNameLineEdit, &QILineEdit::textChanged,
                this, &UIVisoCreatorOptionsDialog::sltHandleVisoNameChange);
        pVisoNameLineEdit->setText(m_visoOptions.m_strVisoName);
        QILabel *pVisoNameLabel = new QILabel(UIVisoCreator::tr("VISO Name"));
        pVisoNameLabel->setBuddy(pVisoNameLineEdit);

        pLayout->addWidget(pVisoNameLabel, 0, 0, Qt::AlignTop);
        pLayout->addWidget(pVisoNameLineEdit, 0, 1, Qt::AlignTop);

        pVisoPage->setLayout(pLayout);
    }

    QWidget *pBrowserPage = m_pTab->widget(static_cast<int>(TabPage_Browser_Options));
    if (pBrowserPage)
    {
        QGridLayout *pLayout = new QGridLayout;

        QCheckBox *pShowHiddenObjectsCheckBox = new QCheckBox;
        pShowHiddenObjectsCheckBox->setChecked(m_browserOptions.m_bShowHiddenObjects);
        QILabel *pShowHiddenObjectsLabel = new QILabel(UIVisoCreator::tr("Show Hidden Objects"));
        pShowHiddenObjectsLabel->setBuddy(pShowHiddenObjectsCheckBox);
        pLayout->addWidget(pShowHiddenObjectsLabel, 0, 0, Qt::AlignTop);
        pLayout->addWidget(pShowHiddenObjectsCheckBox, 0, 1, Qt::AlignTop);
        connect(pShowHiddenObjectsCheckBox, &QCheckBox::stateChanged,
                this, &UIVisoCreatorOptionsDialog::sltHandlShowHiddenObjectsChange);
        pBrowserPage->setLayout(pLayout);
    }

}
