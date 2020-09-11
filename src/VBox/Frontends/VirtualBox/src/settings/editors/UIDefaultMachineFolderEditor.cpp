/* $Id$ */
/** @file
 * VBox Qt GUI - UIDefaultMachineFolderEditor class implementation.
 */

/*
 * Copyright (C) 2019-2020 Oracle Corporation
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

/* GUI includes: */
#include "UICommon.h"
#include "UIDefaultMachineFolderEditor.h"
#include "UIFilePathSelector.h"


UIDefaultMachineFolderEditor::UIDefaultMachineFolderEditor(QWidget *pParent /* = 0 */, bool fWithLabel /* = false */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_fWithLabel(fWithLabel)
    , m_strValue(QString())
    , m_pLabel(0)
    , m_pSelector(0)
{
    prepare();
}

void UIDefaultMachineFolderEditor::setValue(const QString &strValue)
{
    if (m_pSelector)
    {
        /* Update cached value and editor
         * if value has changed: */
        if (m_strValue != strValue)
        {
            m_strValue = strValue;
            m_pSelector->setPath(strValue);
        }
    }
}

QString UIDefaultMachineFolderEditor::value() const
{
    return m_pSelector ? m_pSelector->path() : m_strValue;
}

void UIDefaultMachineFolderEditor::retranslateUi()
{
    if (m_pLabel)
        m_pLabel->setText(tr("Default &Machine Folder:"));
    if (m_pSelector)
        m_pSelector->setWhatsThis(tr("Holds the path to the default virtual machine folder. This folder is used, "
                                     "if not explicitly specified otherwise, when creating new virtual machines."));
}

void UIDefaultMachineFolderEditor::sltHandleSelectorPathChanged()
{
    if (m_pSelector)
        emit sigValueChanged(m_pSelector->path());
}

void UIDefaultMachineFolderEditor::prepare()
{
    /* Create main layout: */
    QGridLayout *pMainLayout = new QGridLayout(this);
    if (pMainLayout)
    {
        pMainLayout->setContentsMargins(0, 0, 0, 0);
        int iRow = 0;

        /* Create label: */
        if (m_fWithLabel)
        {
            m_pLabel = new QLabel(this);
            if (m_pLabel)
                pMainLayout->addWidget(m_pLabel, 0, iRow++, 1, 1);
        }

        /* Create selector: */
        m_pSelector = new UIFilePathSelector(this);
        if (m_pSelector)
        {
            if (m_pLabel)
                m_pLabel->setBuddy(m_pSelector);
            m_pSelector->setHomeDir(uiCommon().homeFolder());
            connect(m_pSelector, &UIFilePathSelector::pathChanged,
                    this, &UIDefaultMachineFolderEditor::sltHandleSelectorPathChanged);

            pMainLayout->addWidget(m_pSelector, 0, iRow++, 1, 1);
        }
    }

    /* Apply language settings: */
    retranslateUi();
}
