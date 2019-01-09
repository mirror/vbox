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
#include "UIVisoCreator.h"
#include "UIVisoCreatorOptionsDialog.h"

UIVisoCreatorOptionsDialog::UIVisoCreatorOptionsDialog(const BrowserOptions &browserOptions, QWidget *pParent /* =0 */)
    : QIDialog(pParent)
    , m_pMainLayout(0)
    , m_pButtonBox(0)
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

void UIVisoCreatorOptionsDialog::sltHandlShowHiddenObjectsChange(int iState)
{
    if (iState == static_cast<int>(Qt::Checked))
        m_browserOptions.m_bShowHiddenObjects = true;
    else
        m_browserOptions.m_bShowHiddenObjects = false;
}

void UIVisoCreatorOptionsDialog::prepareObjects()
{
    m_pMainLayout = new QGridLayout;
    if (!m_pMainLayout)
        return;

    QCheckBox *pShowHiddenObjectsCheckBox = new QCheckBox;
    pShowHiddenObjectsCheckBox->setChecked(m_browserOptions.m_bShowHiddenObjects);
    QILabel *pShowHiddenObjectsLabel = new QILabel(UIVisoCreator::tr("Show Hidden Objects"));
    pShowHiddenObjectsLabel->setBuddy(pShowHiddenObjectsCheckBox);
    m_pMainLayout->addWidget(pShowHiddenObjectsLabel, 0, 0);
    m_pMainLayout->addWidget(pShowHiddenObjectsCheckBox, 0, 1);
    connect(pShowHiddenObjectsCheckBox, &QCheckBox::stateChanged,
            this, &UIVisoCreatorOptionsDialog::sltHandlShowHiddenObjectsChange);

    m_pButtonBox = new QIDialogButtonBox;
    if (m_pButtonBox)
    {
        m_pButtonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
        m_pButtonBox->button(QDialogButtonBox::Cancel)->setShortcut(Qt::Key_Escape);
        m_pMainLayout->addWidget(m_pButtonBox, 1, 0, 1, 2);
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
