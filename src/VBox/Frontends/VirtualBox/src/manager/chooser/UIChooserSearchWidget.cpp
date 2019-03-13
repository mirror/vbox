/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserSearchWidget class implementation.
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
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
#include "QILineEdit.h"
#include "UIChooserDefs.h"
#include "UIChooserSearchWidget.h"

UIChooserSearchWidget::UIChooserSearchWidget(QWidget *pParent)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pLineEdit(0)
    , m_pMainLayout(0)

{
    prepareWidgets();
    prepareConnections();
}

void UIChooserSearchWidget::prepareWidgets()
{
    m_pMainLayout = new QHBoxLayout;
    if (!m_pMainLayout)
        return;
    m_pMainLayout->setSpacing(2);
    m_pMainLayout->setContentsMargins(0, 0, 0, 0);
    m_pLineEdit = new QILineEdit;
    if (m_pLineEdit)
    {
        m_pMainLayout->addWidget(m_pLineEdit);
    }

    setLayout(m_pMainLayout);
}

void UIChooserSearchWidget::prepareConnections()
{
    if (m_pLineEdit)
    {
        connect(m_pLineEdit, &QILineEdit::textEdited,
                this, &UIChooserSearchWidget::sltHandleSearchTermChange);
    }
}

void UIChooserSearchWidget::showEvent(QShowEvent *pEvent)
{
    Q_UNUSED(pEvent);
    if (m_pLineEdit)
        m_pLineEdit->setFocus();
}

void UIChooserSearchWidget::retranslateUi()
{
}

void UIChooserSearchWidget::sltHandleSearchTermChange(const QString &strSearchTerm)
{
    emit sigRedoSearch(strSearchTerm, UIChooserItemSearchFlag_Machine);
}
