/* $Id$ */
/** @file
 * VBox Qt GUI - UIDescriptionEditor class implementation.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
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
#include <QTextEdit>
#include <QVBoxLayout>

/* GUI includes: */
#include "UIDescriptionEditor.h"


UIDescriptionEditor::UIDescriptionEditor(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pTextEdit(0)
{
    prepare();
}

void UIDescriptionEditor::setValue(const QString &strValue)
{
    /* Update cached value and
     * text-edit if value has changed: */
    if (m_strValue != strValue)
    {
        m_strValue = strValue;
        if (m_pTextEdit)
            m_pTextEdit->setPlainText(strValue);
    }
}

QString UIDescriptionEditor::value() const
{
    return m_pTextEdit ? m_pTextEdit->toPlainText() : m_strValue;
}

void UIDescriptionEditor::retranslateUi()
{
    if (m_pTextEdit)
        m_pTextEdit->setToolTip(tr("Holds the description of the virtual machine. The description field is useful "
                                   "for commenting on configuration details of the installed guest OS."));
}

void UIDescriptionEditor::prepare()
{
    /* Prepare main layout: */
    QVBoxLayout *pLayout = new QVBoxLayout(this);
    if (pLayout)
    {
        pLayout->setContentsMargins(0, 0, 0, 0);

        /* Prepare text-edit: */
        m_pTextEdit = new QTextEdit(this);
        if (m_pTextEdit)
        {
            setFocusProxy(m_pTextEdit);
            m_pTextEdit->setAcceptRichText(false);
#ifdef VBOX_WS_MAC
            m_pTextEdit->setMinimumHeight(150);
#endif

            pLayout->addWidget(m_pTextEdit);
        }
    }

    /* Apply language settings: */
    retranslateUi();
}
