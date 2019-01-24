/* $Id$ */
/** @file
 * VBox Qt GUI - UIMediumSearchWidget class implementation.
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
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIComboBox.h"
#include "QIDialogButtonBox.h"
#include "QIToolButton.h"
#include "UIFDCreationDialog.h"
#include "UIIconPool.h"
#include "UIMediumSearchWidget.h"
#include "UIToolBar.h"

#ifdef VBOX_WS_MAC
# include "UIWindowMenuManager.h"
#endif /* VBOX_WS_MAC */

UIMediumSearchWidget::UIMediumSearchWidget(QWidget *pParent)
    :QIWithRetranslateUI<QWidget>(pParent)
    , m_pSearchComboxBox(0)
    , m_pSearchTermLineEdit(0)
    , m_pShowNextMatchButton(0)
    , m_pShowPreviousMatchButton(0)
{
    prepareWidgets();
}

void UIMediumSearchWidget::prepareWidgets()
{
    QHBoxLayout *pLayout = new QHBoxLayout;
    setLayout(pLayout);
    pLayout->setContentsMargins(0, 0, 0, 0);
    pLayout->setSpacing(0);

    m_pSearchComboxBox = new QIComboBox;
    if (m_pSearchComboxBox)
    {
        m_pSearchComboxBox->setEditable(false);
        m_pSearchComboxBox->insertItem(SearchByName, "Search By Name");
        m_pSearchComboxBox->insertItem(SearchByUUID, "Search By UUID");
        pLayout->addWidget(m_pSearchComboxBox);

        connect(m_pSearchComboxBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::currentIndexChanged),
                this, &UIMediumSearchWidget::sigPerformSearch);

    }

    m_pSearchTermLineEdit = new QLineEdit;
    if (m_pSearchTermLineEdit)
    {
        m_pSearchTermLineEdit->setClearButtonEnabled(true);
        pLayout->addWidget(m_pSearchTermLineEdit);
        connect(m_pSearchTermLineEdit, &QLineEdit::textChanged,
                this, &UIMediumSearchWidget::sigPerformSearch);
    }

    m_pShowPreviousMatchButton = new QIToolButton;
    if (m_pShowPreviousMatchButton)
    {
        m_pShowPreviousMatchButton->setIcon(UIIconPool::iconSet(":/log_viewer_search_backward_16px.png", ":/log_viewer_search_backward_disabled_16px.png"));
        connect(m_pShowPreviousMatchButton, &QIToolButton::clicked, this, &UIMediumSearchWidget::sigShowPreviousMatchingItem);
        pLayout->addWidget(m_pShowPreviousMatchButton);
    }
    m_pShowNextMatchButton = new QIToolButton;
    if (m_pShowNextMatchButton)
    {
        m_pShowNextMatchButton->setIcon(UIIconPool::iconSet(":/log_viewer_search_forward_16px.png", ":/log_viewer_search_forward_disabled_16px.png"));
        connect(m_pShowNextMatchButton, &QIToolButton::clicked, this, &UIMediumSearchWidget:: sigShowNextMatchingItem);
        pLayout->addWidget(m_pShowNextMatchButton);
    }

    retranslateUi();
}

UIMediumSearchWidget::SearchType UIMediumSearchWidget::searchType() const
{
    if (!m_pSearchComboxBox || m_pSearchComboxBox->currentIndex() >= static_cast<int>(SearchByMax))
        return SearchByMax;
    return static_cast<SearchType>(m_pSearchComboxBox->currentIndex());
}

QString UIMediumSearchWidget::searchTerm() const
{
    if (!m_pSearchTermLineEdit)
        return QString();
    return m_pSearchTermLineEdit->text();
}

void UIMediumSearchWidget::retranslateUi()
{
    if (m_pSearchComboxBox)
    {
        m_pSearchComboxBox->setItemText(SearchByName, UIMediumSearchWidget::tr("Search By Name"));
        m_pSearchComboxBox->setItemText(SearchByUUID, UIMediumSearchWidget::tr("Search By UUID"));
        m_pSearchComboxBox->setToolTip(UIMediumSearchWidget::tr("Select the search type"));
    }
    if (m_pSearchTermLineEdit)
        m_pSearchTermLineEdit->setToolTip("Enter the search term and press Return");
    if (m_pShowPreviousMatchButton)
        m_pShowPreviousMatchButton->setToolTip("Show the previous item matching the search term");
    if (m_pShowNextMatchButton)
        m_pShowNextMatchButton->setToolTip("Show the next item matching the search term");
}
