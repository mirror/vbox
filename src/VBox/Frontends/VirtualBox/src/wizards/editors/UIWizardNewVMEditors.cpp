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
#include <QVBoxLayout>

/* GUI includes: */
#include "UIUserNamePasswordEditor.h"
#include "UIWizardNewVMEditors.h"

/* Other VBox includes: */
#include "iprt/assert.h"

UIUserNamePasswordGroupBox::UIUserNamePasswordGroupBox(QWidget *pParent /* = 0 */)
    :QGroupBox(pParent)
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
