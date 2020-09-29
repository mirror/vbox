/* $Id$ */
/** @file
 * VBox Qt GUI - UIHelpBrowserWidget class implementation.
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
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
#include <QDateTime>
#include <QDir>
#include <QFont>
#include <QMenu>
#include <QPainter>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QStyle>
#include <QTextBlock>
#include <QVBoxLayout>
#ifdef RT_OS_SOLARIS
# include <QFontDatabase>
#endif

/* GUI includes: */
#include "QIFileDialog.h"
#include "QITabWidget.h"
#include "UIActionPool.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "UIVMLogPage.h"
#include "UIHelpBrowserWidget.h"
#include "UIVMLogViewerBookmarksPanel.h"
#include "UIVMLogViewerFilterPanel.h"
#include "UIVMLogViewerSearchPanel.h"
#include "UIVMLogViewerOptionsPanel.h"
#include "QIToolBar.h"
#include "UICommon.h"

/* COM includes: */
#include "CSystemProperties.h"

UIHelpBrowserWidget::UIHelpBrowserWidget(EmbedTo enmEmbedding,
                                         bool fShowToolbar /* = true */,
                                         QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_enmEmbedding(enmEmbedding)
    , m_fShowToolbar(fShowToolbar)
    , m_fIsPolished(false)
    , m_pTabWidget(0)
    , m_pSearchPanel(0)
    , m_pFilterPanel(0)
    , m_pBookmarksPanel(0)
    , m_pOptionsPanel(0)
    , m_pMainLayout(0)
    , m_pToolBar(0)
    , m_bShowLineNumbers(true)
    , m_bWrapLines(false)
    , m_font(QFontDatabase::systemFont(QFontDatabase::FixedFont))
{
    /* Prepare VM Log-Viewer: */
    prepare();
    restorePanelVisibility();
}

UIHelpBrowserWidget::~UIHelpBrowserWidget()
{
    /* Cleanup VM Log-Viewer: */
    cleanup();
}

int UIHelpBrowserWidget::defaultLogPageWidth() const
{
    if (!m_pTabWidget)
        return 0;

    QWidget *pContainer = m_pTabWidget->currentWidget();
    if (!pContainer)
        return 0;

    QPlainTextEdit *pBrowser = pContainer->findChild<QPlainTextEdit*>();
    if (!pBrowser)
        return 0;
    /* Compute a width for 132 characters plus scrollbar and frame width: */
    int iDefaultWidth = pBrowser->fontMetrics().width(QChar('x')) * 132 +
                        pBrowser->verticalScrollBar()->width() +
                        pBrowser->frameWidth() * 2;

    return iDefaultWidth;
}

QMenu *UIHelpBrowserWidget::menu() const
{
    return 0;//m_pActionPool->action(UIActionIndex_M_LogWindow)->menu();
}

QFont UIHelpBrowserWidget::currentFont() const
{
    const UIVMLogPage* logPage = currentLogPage();
    if (!logPage)
        return QFont();
    return logPage->currentFont();
}

bool UIHelpBrowserWidget::shouldBeMaximized() const
{
    return gEDataManager->logWindowShouldBeMaximized();
}

void UIHelpBrowserWidget::sltDeleteBookmark(int index)
{
    UIVMLogPage* logPage = currentLogPage();
    if (!logPage)
        return;
    logPage->deleteBookmark(index);
    if (m_pBookmarksPanel)
        m_pBookmarksPanel->updateBookmarkList(logPage->bookmarkVector());
}

void UIHelpBrowserWidget::sltDeleteAllBookmarks()
{
    UIVMLogPage* logPage = currentLogPage();
    if (!logPage)
        return;
    logPage->deleteAllBookmarks();

    if (m_pBookmarksPanel)
        m_pBookmarksPanel->updateBookmarkList(logPage->bookmarkVector());
}

void UIHelpBrowserWidget::sltUpdateBookmarkPanel()
{
    if (!currentLogPage() || !m_pBookmarksPanel)
        return;
    m_pBookmarksPanel->updateBookmarkList(currentLogPage()->bookmarkVector());
}

void UIHelpBrowserWidget::gotoBookmark(int bookmarkIndex)
{
    if (!currentLogPage())
        return;
    currentLogPage()->scrollToBookmark(bookmarkIndex);
}

void UIHelpBrowserWidget::sltPanelActionToggled(bool fChecked)
{
    QAction *pSenderAction = qobject_cast<QAction*>(sender());
    if (!pSenderAction)
        return;
    UIDialogPanel* pPanel = 0;
    /* Look for the sender() within the m_panelActionMap's values: */
    for (QMap<UIDialogPanel*, QAction*>::const_iterator iterator = m_panelActionMap.begin();
        iterator != m_panelActionMap.end(); ++iterator)
    {
        if (iterator.value() == pSenderAction)
            pPanel = iterator.key();
    }
    if (!pPanel)
        return;
    if (fChecked)
        showPanel(pPanel);
    else
        hidePanel(pPanel);
}

void UIHelpBrowserWidget::sltSearchResultHighLigting()
{
    if (!m_pSearchPanel || !currentLogPage())
        return;
    currentLogPage()->setScrollBarMarkingsVector(m_pSearchPanel->matchLocationVector());
}

void UIHelpBrowserWidget::sltHandleSearchUpdated()
{
    if (!m_pSearchPanel || !currentLogPage())
        return;
}

void UIHelpBrowserWidget::sltTabIndexChange(int tabIndex)
{
    Q_UNUSED(tabIndex);

    /* Dont refresh the search here as it is refreshed by the filtering mechanism
       which is updated as tab current index changes: */

    /* We keep a separate QVector<LogBookmark> for each log page: */
    if (m_pBookmarksPanel && currentLogPage())
        m_pBookmarksPanel->updateBookmarkList(currentLogPage()->bookmarkVector());
}

void UIHelpBrowserWidget::sltFilterApplied(bool isOriginal)
{
    if (currentLogPage())
        currentLogPage()->setFiltered(!isOriginal);
    /* Reapply the search to get highlighting etc. correctly */
    if (m_pSearchPanel && m_pSearchPanel->isVisible())
        m_pSearchPanel->refresh();
}

void UIHelpBrowserWidget::sltLogPageFilteredChanged(bool isFiltered)
{
    /* Disable bookmark panel since bookmarks are stored as line numbers within
       the original log text and does not mean much in a reduced/filtered one. */
    if (m_pBookmarksPanel)
        m_pBookmarksPanel->disableEnableBookmarking(!isFiltered);
}

void UIHelpBrowserWidget::sltHandleHidePanel(UIDialogPanel *pPanel)
{
    hidePanel(pPanel);
}

void UIHelpBrowserWidget::sltShowLineNumbers(bool bShowLineNumbers)
{
    if (m_bShowLineNumbers == bShowLineNumbers)
        return;

    m_bShowLineNumbers = bShowLineNumbers;
    /* Set all log page instances. */
    for (int i = 0; i < m_logPageList.size(); ++i)
    {
        UIVMLogPage* pLogPage = qobject_cast<UIVMLogPage*>(m_logPageList[i]);
        if (pLogPage)
            pLogPage->setShowLineNumbers(m_bShowLineNumbers);
    }
}

void UIHelpBrowserWidget::sltWrapLines(bool bWrapLines)
{
    if (m_bWrapLines == bWrapLines)
        return;

    m_bWrapLines = bWrapLines;
    /* Set all log page instances. */
    for (int i = 0; i < m_logPageList.size(); ++i)
    {
        UIVMLogPage* pLogPage = qobject_cast<UIVMLogPage*>(m_logPageList[i]);
        if (pLogPage)
            pLogPage->setWrapLines(m_bWrapLines);
    }
}

void UIHelpBrowserWidget::sltFontSizeChanged(int fontSize)
{
    if (m_font.pointSize() == fontSize)
        return;
    m_font.setPointSize(fontSize);
    for (int i = 0; i < m_logPageList.size(); ++i)
    {
        UIVMLogPage* pLogPage = qobject_cast<UIVMLogPage*>(m_logPageList[i]);
        if (pLogPage)
            pLogPage->setCurrentFont(m_font);
    }
}

void UIHelpBrowserWidget::sltChangeFont(QFont font)
{
    if (m_font == font)
        return;
    m_font = font;
    for (int i = 0; i < m_logPageList.size(); ++i)
    {
        UIVMLogPage* pLogPage = qobject_cast<UIVMLogPage*>(m_logPageList[i]);
        if (pLogPage)
            pLogPage->setCurrentFont(m_font);
    }
}

void UIHelpBrowserWidget::sltResetOptionsToDefault()
{
    sltShowLineNumbers(true);
    sltWrapLines(false);
    sltChangeFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    if (m_pOptionsPanel)
    {
        m_pOptionsPanel->setShowLineNumbers(true);
        m_pOptionsPanel->setWrapLines(false);
        m_pOptionsPanel->setFontSizeInPoints(m_font.pointSize());
    }
}

void UIHelpBrowserWidget::prepare()
{
    /* Load options: */
    loadOptions();

    /* Prepare stuff: */
    prepareActions();
    /* Prepare widgets: */
    prepareWidgets();

    /* Apply language settings: */
    retranslateUi();

    /* Setup escape shortcut: */
    manageEscapeShortCut();
}

void UIHelpBrowserWidget::prepareActions()
{

}

void UIHelpBrowserWidget::prepareWidgets()
{
    /* Create main layout: */
    m_pMainLayout = new QVBoxLayout(this);
    AssertReturnVoid(m_pMainLayout);
}

void UIHelpBrowserWidget::prepareToolBar()
{
    /* Create toolbar: */
    m_pToolBar = new QIToolBar(parentWidget());
    if (m_pToolBar)
    {
        /* Configure toolbar: */
        const int iIconMetric = (int)(QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize));
        m_pToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
        m_pToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);


#ifdef VBOX_WS_MAC
        /* Check whether we are embedded into a stack: */
        if (m_enmEmbedding == EmbedTo_Stack)
        {
            /* Add into layout: */
            m_pMainLayout->addWidget(m_pToolBar);
        }
#else
        /* Add into layout: */
        m_pMainLayout->addWidget(m_pToolBar);
#endif
    }
}

void UIHelpBrowserWidget::loadOptions()
{
    m_bWrapLines = gEDataManager->logViewerWrapLines();
    m_bShowLineNumbers = gEDataManager->logViewerShowLineNumbers();
    QFont loadedFont = gEDataManager->logViewerFont();
    if (loadedFont != QFont())
        m_font = loadedFont;
}

void UIHelpBrowserWidget::restorePanelVisibility()
{
    /** Reset the action states first: */
    foreach(QAction* pAction, m_panelActionMap.values())
    {
        pAction->blockSignals(true);
        pAction->setChecked(false);
        pAction->blockSignals(false);
    }

    /* Load the visible panel list and show them: */
    QStringList strNameList = gEDataManager->logViewerVisiblePanels();
    foreach(const QString strName, strNameList)
    {
        foreach(UIDialogPanel* pPanel, m_panelActionMap.keys())
        {
            if (strName == pPanel->panelName())
            {
                showPanel(pPanel);
                break;
            }
        }
    }
}

void UIHelpBrowserWidget::saveOptions()
{
    /* Save a list of currently visible panels: */
    QStringList strNameList;
    foreach(UIDialogPanel* pPanel, m_visiblePanelsList)
        strNameList.append(pPanel->panelName());
    gEDataManager->setLogViewerVisiblePanels(strNameList);

    gEDataManager->setLogViweverOptions(m_font, m_bWrapLines, m_bShowLineNumbers);
}

void UIHelpBrowserWidget::cleanup()
{
    /* Save options: */
    saveOptions();
}

void UIHelpBrowserWidget::retranslateUi()
{
    /* Translate toolbar: */
#ifdef VBOX_WS_MAC
    // WORKAROUND:
    // There is a bug in Qt Cocoa which result in showing a "more arrow" when
    // the necessary size of the toolbar is increased. Also for some languages
    // the with doesn't match if the text increase. So manually adjust the size
    // after changing the text. */
    if (m_pToolBar)
        m_pToolBar->updateLayout();
#endif
}

void UIHelpBrowserWidget::showEvent(QShowEvent *pEvent)
{
    QWidget::showEvent(pEvent);

    /* One may think that QWidget::polish() is the right place to do things
     * below, but apparently, by the time when QWidget::polish() is called,
     * the widget style & layout are not fully done, at least the minimum
     * size hint is not properly calculated. Since this is sometimes necessary,
     * we provide our own "polish" implementation: */

    if (m_fIsPolished)
        return;

    m_fIsPolished = true;
}

void UIHelpBrowserWidget::keyPressEvent(QKeyEvent *pEvent)
{
    /* Depending on key pressed: */
    switch (pEvent->key())
    {
        /* Process Back key as switch to previous tab: */
        case Qt::Key_Back:
        {
            if (m_pTabWidget->currentIndex() > 0)
            {
                m_pTabWidget->setCurrentIndex(m_pTabWidget->currentIndex() - 1);
                return;
            }
            break;
        }
        /* Process Forward key as switch to next tab: */
        case Qt::Key_Forward:
        {
            if (m_pTabWidget->currentIndex() < m_pTabWidget->count())
            {
                m_pTabWidget->setCurrentIndex(m_pTabWidget->currentIndex() + 1);
                return;
            }
            break;
        }
        default:
            break;
    }
    QWidget::keyPressEvent(pEvent);
}

QPlainTextEdit* UIHelpBrowserWidget::logPage(int pIndex) const
{
    if (!m_pTabWidget->isEnabled())
        return 0;
    QWidget* pContainer = m_pTabWidget->widget(pIndex);
    if (!pContainer)
        return 0;
    QPlainTextEdit *pBrowser = pContainer->findChild<QPlainTextEdit*>();
    return pBrowser;
}

void UIHelpBrowserWidget::createLogPage(const QString &strFileName, const QString &strLogContent, bool noLogsToShow /* = false */)
{
    if (!m_pTabWidget)
        return;

    /* Create page-container: */
    UIVMLogPage* pLogPage = new UIVMLogPage();
    if (pLogPage)
    {
        connect(pLogPage, &UIVMLogPage::sigBookmarksUpdated, this, &UIHelpBrowserWidget::sltUpdateBookmarkPanel);
        connect(pLogPage, &UIVMLogPage::sigLogPageFilteredChanged, this, &UIHelpBrowserWidget::sltLogPageFilteredChanged);
        /* Initialize setting for this log page */
        pLogPage->setShowLineNumbers(m_bShowLineNumbers);
        pLogPage->setWrapLines(m_bWrapLines);
        pLogPage->setCurrentFont(m_font);

        /* Set the file name only if we really have log file to read. */
        if (!noLogsToShow)
            pLogPage->setLogFileName(strFileName);

        /* Add page-container to viewer-container: */
        int tabIndex = m_pTabWidget->insertTab(m_pTabWidget->count(), pLogPage, QFileInfo(strFileName).fileName());

        pLogPage->setTabIndex(tabIndex);
        m_logPageList.resize(m_pTabWidget->count());
        m_logPageList[tabIndex] = pLogPage;

        /* Set text edit since we want to display this text: */
        if (!noLogsToShow)
        {
            pLogPage->setTextEditText(strLogContent);
            /* Set the log string of the UIVMLogPage: */
            pLogPage->setLogString(strLogContent);
        }
        /* In case there are some errors append the error text as html: */
        else
        {
            pLogPage->setTextEditTextAsHtml(strLogContent);
            pLogPage->markForError();
        }
        pLogPage->setScrollBarMarkingsVector(m_pSearchPanel->matchLocationVector());
    }
}

const UIVMLogPage *UIHelpBrowserWidget::currentLogPage() const
{
    int currentTabIndex = m_pTabWidget->currentIndex();
    if (currentTabIndex >= m_logPageList.size())
        return 0;
    return qobject_cast<const UIVMLogPage*>(m_logPageList.at(currentTabIndex));
}

UIVMLogPage *UIHelpBrowserWidget::currentLogPage()
{
    int currentTabIndex = m_pTabWidget->currentIndex();
    if (currentTabIndex >= m_logPageList.size() || currentTabIndex == -1)
        return 0;

    return qobject_cast<UIVMLogPage*>(m_logPageList.at(currentTabIndex));
}

void UIHelpBrowserWidget::resetHighlighthing()
{
    /* Undo the document changes to remove highlighting: */
    UIVMLogPage* logPage = currentLogPage();
    if (!logPage)
        return;
    logPage->documentUndo();
    logPage->clearScrollBarMarkingsVector();
}

void UIHelpBrowserWidget::hidePanel(UIDialogPanel* panel)
{
    if (!panel)
        return;
    if (panel->isVisible())
        panel->setVisible(false);
    QMap<UIDialogPanel*, QAction*>::iterator iterator = m_panelActionMap.find(panel);
    if (iterator != m_panelActionMap.end())
    {
        if (iterator.value() && iterator.value()->isChecked())
            iterator.value()->setChecked(false);
    }
    m_visiblePanelsList.removeOne(panel);
    manageEscapeShortCut();
}

void UIHelpBrowserWidget::showPanel(UIDialogPanel* panel)
{
    if (panel && panel->isHidden())
        panel->setVisible(true);
    QMap<UIDialogPanel*, QAction*>::iterator iterator = m_panelActionMap.find(panel);
    if (iterator != m_panelActionMap.end())
    {
        if (!iterator.value()->isChecked())
            iterator.value()->setChecked(true);
    }
    m_visiblePanelsList.push_back(panel);
    manageEscapeShortCut();
}

void UIHelpBrowserWidget::manageEscapeShortCut()
{
    /* if there is no visible panels give the escape shortcut to parent dialog: */
    if (m_visiblePanelsList.isEmpty())
    {
        emit sigSetCloseButtonShortCut(QKeySequence(Qt::Key_Escape));
        return;
    }
    /* Take the escape shortcut from the dialog: */
    emit sigSetCloseButtonShortCut(QKeySequence());
    /* Just loop thru the visible panel list and set the esc key to the
       panel which made visible latest */
    for (int i = 0; i < m_visiblePanelsList.size() - 1; ++i)
    {
        m_visiblePanelsList[i]->setCloseButtonShortCut(QKeySequence());
    }
    m_visiblePanelsList.back()->setCloseButtonShortCut(QKeySequence(Qt::Key_Escape));
}
