/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class implementation.
 */

/*
 * Copyright (C) 2010-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/* Qt includes: */
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>

/* GUI includes: */
#include "UIIconPool.h"
#include "UIVMLogPage.h"
#include "UIVMLogViewerPanel.h"
#include "UIVMLogViewerWidget.h"
#include "UIVMLogViewerSearchPanel.h"
#include "UIVMLogViewerFilterPanel.h"
#include "UIVMLogViewerBookmarksPanel.h"

#ifdef VBOX_WS_MAC
# include "VBoxUtils-darwin.h"
#endif

UIVMLogViewerPanelNew::UIVMLogViewerPanelNew(QWidget *pParent, UIVMLogViewerWidget *pViewer)
    : QIWithRetranslateUI<UIDialogPanelBase>(pParent)
    , m_pViewer(pViewer)
    , m_pSearchWidget(0)
    , m_pFilterWidget(0)
    , m_pBookmarksWidget(0)
{
    prepare();
}

void UIVMLogViewerPanelNew::prepare()
{
    /* Search tab: */
    m_pSearchWidget = new UIVMLogViewerSearchPanel(0, m_pViewer);
    insertTab(Page_Search, m_pSearchWidget);

    connect(m_pSearchWidget, &UIVMLogViewerSearchPanel::sigHighlightingUpdated,
            this, &UIVMLogViewerPanelNew::sigHighlightingUpdated);
    connect(m_pSearchWidget, &UIVMLogViewerSearchPanel::sigSearchUpdated,
            this, &UIVMLogViewerPanelNew::sigSearchUpdated);

    /* Filter tab: */
    m_pFilterWidget = new UIVMLogViewerFilterPanel(0, m_pViewer);
    insertTab(Page_Filter, m_pFilterWidget);

    connect(m_pFilterWidget, &UIVMLogViewerFilterPanel::sigFilterApplied,
            this, &UIVMLogViewerPanelNew::sigFilterApplied);

    /* Bookmark tab: */
    m_pBookmarksWidget = new UIVMLogViewerBookmarksPanel(0, m_pViewer);
    insertTab(Page_Bookmark, m_pBookmarksWidget);

    connect(m_pBookmarksWidget, &UIVMLogViewerBookmarksPanel::sigDeleteBookmarkByIndex,
            this, &UIVMLogViewerPanelNew::sigDeleteBookmarkByIndex);
    connect(m_pBookmarksWidget, &UIVMLogViewerBookmarksPanel::sigDeleteAllBookmarks,
            this, &UIVMLogViewerPanelNew::sigDeleteAllBookmarks);
    connect(m_pBookmarksWidget, &UIVMLogViewerBookmarksPanel::sigBookmarkSelected,
            this, &UIVMLogViewerPanelNew::sigBookmarkSelected);

    retranslateUi();
}

void UIVMLogViewerPanelNew::refreshSearch()
{
    if (m_pSearchWidget)
        m_pSearchWidget->refreshSearch();
}

QVector<float> UIVMLogViewerPanelNew::matchLocationVector() const
{
    if (!m_pSearchWidget)
        return QVector<float>();
    return m_pSearchWidget->matchLocationVector();
}

int UIVMLogViewerPanelNew::matchCount() const
{
    if (!m_pSearchWidget)
        return 0;
    return m_pSearchWidget->matchCount();
}

void UIVMLogViewerPanelNew::applyFilter()
{
    if (m_pFilterWidget)
        m_pFilterWidget->applyFilter();
}

void UIVMLogViewerPanelNew::updateBookmarkList(const QVector<UIVMLogBookmark>& bookmarkList)
{
    if (m_pBookmarksWidget)
        m_pBookmarksWidget->updateBookmarkList(bookmarkList);
}

void UIVMLogViewerPanelNew::disableEnableBookmarking(bool flag)
{
    if (m_pBookmarksWidget)
        m_pBookmarksWidget->disableEnableBookmarking(flag);
}

void UIVMLogViewerPanelNew::retranslateUi()
{
    setTabText(Page_Search, "Find");
    setTabText(Page_Filter, "Filter");
    setTabText(Page_Bookmark, "Bookmarks");
}

bool UIVMLogViewerPanelNew::eventFilter(QObject *pObject, QEvent *pEvent)
{
    if (currentIndex() == static_cast<Page>(Page_Search))
    {
        if (m_pSearchWidget->handleSearchRelatedEvents(pObject, pEvent))
            return true;
    }
    return QIWithRetranslateUI<UIDialogPanelBase>::eventFilter(pObject, pEvent);
}


/*********************************************************************************************************************************
*   UIVMLogViewerPanel implementation.                                                                                           *
*********************************************************************************************************************************/

UIVMLogViewerPanel::UIVMLogViewerPanel(QWidget *pParent, UIVMLogViewerWidget *pViewer)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pViewer(pViewer)
{
}

void UIVMLogViewerPanel::retranslateUi()
{
}

UIVMLogViewerWidget* UIVMLogViewerPanel::viewer()
{
    return m_pViewer;
}

const UIVMLogViewerWidget* UIVMLogViewerPanel::viewer() const
{
    return m_pViewer;
}

QTextDocument  *UIVMLogViewerPanel::textDocument()
{
    QPlainTextEdit *pEdit = textEdit();
    if (!pEdit)
        return 0;
    return textEdit()->document();
}

QPlainTextEdit *UIVMLogViewerPanel::textEdit()
{
    if (!viewer())
        return 0;
    UIVMLogPage *logPage = viewer()->currentLogPage();
    if (!logPage)
        return 0;
    return logPage->textEdit();
}

const QString* UIVMLogViewerPanel::logString() const
{
    if (!viewer())
        return 0;
    const UIVMLogPage* const page = qobject_cast<const UIVMLogPage* const>(viewer()->currentLogPage());
    if (!page)
        return 0;
    return &(page->logString());
}
