/* $Id$ */
/** @file
 * VBox Qt GUI - UIAutoCaptureKeyboardEditor class implementation.
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
#include <QGridLayout>
#include <QLabel>

/* GUI includes: */
#include "UIAutoCaptureKeyboardEditor.h"


UIAutoCaptureKeyboardEditor::UIAutoCaptureKeyboardEditor(QWidget *pParent /* = 0 */, bool fWithLabel /* = false */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_fWithLabel(fWithLabel)
    , m_fValue(false)
    , m_pLabel(0)
    , m_pCheckBox(0)
{
    prepare();
}

void UIAutoCaptureKeyboardEditor::setValue(bool fValue)
{
    if (m_pCheckBox)
    {
        /* Update cached value and
         * check-box if value has changed: */
        if (m_fValue != fValue)
        {
            m_fValue = fValue;
            m_pCheckBox->setCheckState(fValue ? Qt::Checked : Qt::Unchecked);
        }
    }
}

bool UIAutoCaptureKeyboardEditor::value() const
{
    return m_pCheckBox ? m_pCheckBox->checkState() == Qt::Checked : m_fValue;
}

void UIAutoCaptureKeyboardEditor::retranslateUi()
{
    if (m_pLabel)
        m_pLabel->setText(tr("Extended Features:"));
    if (m_pCheckBox)
    {
        m_pCheckBox->setWhatsThis(tr("When checked, the keyboard is automatically captured every time the VM window is "
                                     "activated. When the keyboard is captured, all keystrokes (including system ones like "
                                     "Alt-Tab) are directed to the VM."));
        m_pCheckBox->setText(tr("&Auto Capture Keyboard"));
    }
}

void UIAutoCaptureKeyboardEditor::prepare()
{
    /* Prepare main layout: */
    QGridLayout *pLayoutMain = new QGridLayout(this);
    if (pLayoutMain)
    {
        pLayoutMain->setContentsMargins(0, 0, 0, 0);
        pLayoutMain->setColumnStretch(1, 1);
        int iColumn = 0;

        /* Prepare label: */
        if (m_fWithLabel)
        {
            m_pLabel = new QLabel(this);
            if (m_pLabel)
                pLayoutMain->addWidget(m_pLabel, 0, iColumn++);
        }
        /* Prepare check-box: */
        m_pCheckBox = new QCheckBox(this);
        if (m_pCheckBox)
            pLayoutMain->addWidget(m_pCheckBox, 0, iColumn++);
    }

    /* Apply language settings: */
    retranslateUi();
}
