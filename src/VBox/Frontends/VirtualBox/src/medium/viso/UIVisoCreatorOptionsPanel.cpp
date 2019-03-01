/* $Id$ */
/** @file
 * VBox Qt GUI - UIVisoCreatorOptionsPanel class implementation.
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
#include <QHBoxLayout>

/* GUI includes: */
#include "QILabel.h"
#include "UIVisoCreatorOptionsPanel.h"

UIVisoCreatorOptionsPanel::UIVisoCreatorOptionsPanel(QWidget *pParent /* =0 */)
    : UIDialogPanel(pParent)
    , m_pShowHiddenObjectsCheckBox(0)
    , m_pShowHiddenObjectsLabel(0)
{
    prepareObjects();
    prepareConnections();
}

UIVisoCreatorOptionsPanel::~UIVisoCreatorOptionsPanel()
{
}

QString UIVisoCreatorOptionsPanel::panelName() const
{
    return "OptionsPanel";
}

void UIVisoCreatorOptionsPanel::setShowHiddenbjects(bool fShow)
{
    if (m_pShowHiddenObjectsCheckBox)
        m_pShowHiddenObjectsCheckBox->setChecked(fShow);
}

void UIVisoCreatorOptionsPanel::retranslateUi()
{
    if (m_pShowHiddenObjectsLabel)
        m_pShowHiddenObjectsLabel->setText(QApplication::translate("UIVisoCreator", "Show Hidden Objects"));
}

void UIVisoCreatorOptionsPanel::sltHandlShowHiddenObjectsChange(int iState)
{
    if (iState == static_cast<int>(Qt::Checked))
        sigShowHiddenObjects(true);
    else
        sigShowHiddenObjects(false);
}

void UIVisoCreatorOptionsPanel::prepareObjects()
{
    if (!mainLayout())
        return;

    m_pShowHiddenObjectsCheckBox = new QCheckBox;
    m_pShowHiddenObjectsLabel = new QILabel(QApplication::translate("UIVisoCreator", "Show Hidden Objects"));
    m_pShowHiddenObjectsLabel->setBuddy(m_pShowHiddenObjectsCheckBox);
    mainLayout()->addWidget(m_pShowHiddenObjectsCheckBox, 0, Qt::AlignLeft);
    mainLayout()->addWidget(m_pShowHiddenObjectsLabel, 0, Qt::AlignLeft);
    mainLayout()->addStretch(6);
    connect(m_pShowHiddenObjectsCheckBox, &QCheckBox::stateChanged,
            this, &UIVisoCreatorOptionsPanel::sltHandlShowHiddenObjectsChange);
}

void UIVisoCreatorOptionsPanel::prepareConnections()
{
}
