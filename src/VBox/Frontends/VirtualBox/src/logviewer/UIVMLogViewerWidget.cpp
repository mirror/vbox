/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class implementation.
 */

/*
 * Copyright (C) 2010-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QDateTime>
# include <QDir>
# include <QVBoxLayout>
# if defined(RT_OS_SOLARIS)
#  include <QFontDatabase>
# endif
# include <QPainter>
# include <QPlainTextEdit>
# include <QScrollBar>
# include <QTextBlock>

/* GUI includes: */
# include "QIFileDialog.h"
# include "QITabWidget.h"
# include "UIExtraDataManager.h"
# include "UIIconPool.h"
# include "UIMessageCenter.h"
# include "UIVMLogPage.h"
# include "UIVMLogViewerWidget.h"
# include "UIVMLogViewerBookmarksPanel.h"
# include "UIVMLogViewerFilterPanel.h"
# include "UIVMLogViewerSearchPanel.h"
# include "UIVMLogViewerSettingsPanel.h"
# include "UIToolBar.h"

/* COM includes: */
# include "CSystemProperties.h"

# include "VBoxGlobal.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIVMLogViewerWidget::UIVMLogViewerWidget(EmbedTo enmEmbedding, QWidget *pParent /* = 0 */, const CMachine &machine /* = CMachine() */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_fIsPolished(false)
    , m_comMachine(machine)
    , m_pTabWidget(0)
    , m_pSearchPanel(0)
    , m_pFilterPanel(0)
    , m_pBookmarksPanel(0)
    , m_pSettingsPanel(0)
    , m_pMainLayout(0)
    , m_enmEmbedding(enmEmbedding)
    , m_pToolBar(0)
    , m_pActionFind(0)
    , m_pActionFilter(0)
    , m_pActionRefresh(0)
    , m_pActionSave(0)
    , m_pActionBookmark(0)
    , m_pActionSettings(0)
    , m_pMenu(0)
    , m_bShowLineNumbers(true)
    , m_bWrapLines(false)
    , m_iFontSizeInPoints(9)
{
    /* Prepare VM Log-Viewer: */
    prepare();
}

UIVMLogViewerWidget::~UIVMLogViewerWidget()
{
    /* Cleanup VM Log-Viewer: */
    cleanup();
}

int UIVMLogViewerWidget::defaultLogPageWidth() const
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

bool UIVMLogViewerWidget::shouldBeMaximized() const
{
    return gEDataManager->logWindowShouldBeMaximized();
}

void UIVMLogViewerWidget::sltDeleteBookmark(int index)
{
    UIVMLogPage* logPage = currentLogPage();
    if (!logPage)
        return;
    logPage->deleteBookmark(index);
    if (m_pBookmarksPanel)
        m_pBookmarksPanel->updateBookmarkList(logPage->bookmarkVector());
}

void UIVMLogViewerWidget::sltDeleteAllBookmarks()
{
    UIVMLogPage* logPage = currentLogPage();
    if (!logPage)
        return;
    logPage->deleteAllBookmarks();

    if (m_pBookmarksPanel)
        m_pBookmarksPanel->updateBookmarkList(logPage->bookmarkVector());
}

void UIVMLogViewerWidget::sltUpdateBookmarkPanel()
{
    if (!currentLogPage() || !m_pBookmarksPanel)
        return;
    m_pBookmarksPanel->updateBookmarkList(currentLogPage()->bookmarkVector());
}

void UIVMLogViewerWidget::gotoBookmark(int bookmarkIndex)
{
    if (!currentLogPage())
        return;
    currentLogPage()->scrollToBookmark(bookmarkIndex);
}

void UIVMLogViewerWidget::sltPanelActionTriggered(bool checked)
{
    QAction *pSenderAction = qobject_cast<QAction*>(sender());
    if (!pSenderAction)
        return;
    UIVMLogViewerPanel* pPanel = 0;
    /* Look for the sender() within the m_panelActionMap's values: */
    for (QMap<UIVMLogViewerPanel*, QAction*>::const_iterator iterator = m_panelActionMap.begin();
        iterator != m_panelActionMap.end(); ++iterator)
    {
        if (iterator.value() == pSenderAction)
            pPanel = iterator.key();
    }
    if (!pPanel)
        return;
    if (checked)
        showPanel(pPanel);
    else
        hidePanel(pPanel);
}

void UIVMLogViewerWidget::sltRefresh()
{
    if (!m_pTabWidget)
        return;
    /* Disconnect this connection to avoid initial signals during page creation/deletion: */
    disconnect(m_pTabWidget, &QITabWidget::currentChanged, m_pFilterPanel, &UIVMLogViewerFilterPanel::applyFilter);
    disconnect(m_pTabWidget, &QITabWidget::currentChanged, this, &UIVMLogViewerWidget::sltTabIndexChange);

    m_logPageList.clear();
    m_pTabWidget->setEnabled(true);
    /* Hide the container widget during updates to avoid flickering: */
    m_pTabWidget->hide();
    /* Clear the tab widget. This might be an overkill but most secure way to deal with the case where
       number of the log files changes. */
    while (m_pTabWidget->count())
    {
        QWidget *pFirstPage = m_pTabWidget->widget(0);
        m_pTabWidget->removeTab(0);
        delete pFirstPage;
    }

    bool noLogsToShow = createLogViewerPages();

    /* Show the first tab widget's page after the refresh: */
    m_pTabWidget->setCurrentIndex(0);

    /* Apply the filter settings: */
    if (m_pFilterPanel)
        m_pFilterPanel->applyFilter();

    /* Setup this connection after refresh to avoid initial signals during page creation: */
    if (m_pFilterPanel)
        connect(m_pTabWidget, &QITabWidget::currentChanged, m_pFilterPanel, &UIVMLogViewerFilterPanel::applyFilter);
    connect(m_pTabWidget, &QITabWidget::currentChanged, this, &UIVMLogViewerWidget::sltTabIndexChange);

    /* Enable/Disable toolbar actions (except Refresh) & tab widget according log presence: */
    if (m_pActionFind)
        m_pActionFind->setEnabled(!noLogsToShow);
    if (m_pActionFilter)
        m_pActionFilter->setEnabled(!noLogsToShow);
    if (m_pActionSave)
        m_pActionSave->setEnabled(!noLogsToShow);
    if (m_pActionBookmark)
        m_pActionBookmark->setEnabled(!noLogsToShow);
    //m_pTabWidget->setEnabled(!noLogsToShow);
    if (m_pActionSettings)
        m_pActionSettings->setEnabled(!noLogsToShow);
    m_pTabWidget->show();
    if (m_pSearchPanel && m_pSearchPanel->isVisible())
        m_pSearchPanel->refresh();
}

void UIVMLogViewerWidget::sltSave()
{
    if (m_comMachine.isNull())
        return;

    UIVMLogPage *logPage = currentLogPage();
    if (!logPage)
        return;

    const QString& fileName = logPage->fileName();
    if (fileName.isEmpty())
        return;
    /* Prepare "save as" dialog: */
    const QFileInfo fileInfo(fileName);
    /* Prepare default filename: */
    const QDateTime dtInfo = fileInfo.lastModified();
    const QString strDtString = dtInfo.toString("yyyy-MM-dd-hh-mm-ss");
    const QString strDefaultFileName = QString("%1-%2.log").arg(m_comMachine.GetName()).arg(strDtString);
    const QString strDefaultFullName = QDir::toNativeSeparators(QDir::home().absolutePath() + "/" + strDefaultFileName);

    const QString strNewFileName = QIFileDialog::getSaveFileName(strDefaultFullName,
                                                                 "",
                                                                 this,
                                                                 tr("Save VirtualBox Log As"),
                                                                 0 /* selected filter */,
                                                                 true /* resolve symlinks */,
                                                                 true /* confirm overwrite */);
    /* Make sure file-name is not empty: */
    if (!strNewFileName.isEmpty())
    {
        /* Delete the previous file if already exists as user already confirmed: */
        if (QFile::exists(strNewFileName))
            QFile::remove(strNewFileName);
        /* Copy log into the file: */
        QFile::copy(m_comMachine.QueryLogFilename(m_pTabWidget->currentIndex()), strNewFileName);
    }
}

void UIVMLogViewerWidget::sltSearchResultHighLigting()
{
    if (!m_pSearchPanel)
        return;

    if (!currentLogPage())
        return;
    currentLogPage()->setScrollBarMarkingsVector(m_pSearchPanel->getMatchLocationVector());
}

void UIVMLogViewerWidget::sltTabIndexChange(int tabIndex)
{
    Q_UNUSED(tabIndex);

    resetHighlighthing();
    if (m_pSearchPanel)
        m_pSearchPanel->reset();

    /* We keep a separate QVector<LogBookmark> for each log page: */
    if (m_pBookmarksPanel && currentLogPage())
        m_pBookmarksPanel->updateBookmarkList(currentLogPage()->bookmarkVector());
}

void UIVMLogViewerWidget::sltFilterApplied(bool isOriginal)
{
    if (currentLogPage())
        currentLogPage()->setFiltered(!isOriginal);
    /* Reapply the search to get highlighting etc. correctly */
    if (m_pSearchPanel && m_pSearchPanel->isVisible())
        m_pSearchPanel->refresh();
}

void UIVMLogViewerWidget::sltLogPageFilteredChanged(bool isFiltered)
{
    /* Disable bookmark panel since bookmarks are stored as line numbers within
       the original log text and does not mean much in a reduced/filtered one. */
    if (m_pBookmarksPanel)
    {
        if (isFiltered)
        {
            m_pBookmarksPanel->setEnabled(false);
            m_pBookmarksPanel->setVisible(false);
        }
        else
            m_pBookmarksPanel->setEnabled(true);
    }
}

void UIVMLogViewerWidget::sltShowLineNumbers(bool bShowLineNumbers)
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

void UIVMLogViewerWidget::sltWrapLines(bool bWrapLines)
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

void UIVMLogViewerWidget::sltFontSizeChanged(int fontSize)
{
    if (m_iFontSizeInPoints == fontSize)
        return;
    m_iFontSizeInPoints = fontSize;
    for (int i = 0; i < m_logPageList.size(); ++i)
    {
        UIVMLogPage* pLogPage = qobject_cast<UIVMLogPage*>(m_logPageList[i]);
        if (pLogPage)
            pLogPage->setFontSizeInPoints(m_iFontSizeInPoints);
    }
}

void UIVMLogViewerWidget::setMachine(const CMachine &machine)
{
    if (machine == m_comMachine)
        return;
    m_comMachine = machine;
    sltRefresh();
}

void UIVMLogViewerWidget::prepare()
{
    m_pMainLayout = new QVBoxLayout(this);

    prepareActions();
    prepareToolBar();

    prepareMenu();
    prepareWidgets();


    /* Reading log files: */
    sltRefresh();

    /* Loading language constants: */
    retranslateUi();

    m_panelActionMap.insert(m_pBookmarksPanel, m_pActionBookmark);
    m_panelActionMap.insert(m_pSearchPanel, m_pActionFind);
    m_panelActionMap.insert(m_pFilterPanel, m_pActionFilter);
    m_panelActionMap.insert(m_pSettingsPanel, m_pActionSettings);
}

void UIVMLogViewerWidget::prepareWidgets()
{
    /* Configure layout: */
    layout()->setContentsMargins(0, 0, 0, 0);
#ifdef VBOX_WS_MAC
        layout()->setSpacing(10);
#else
        layout()->setSpacing(qApp->style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing) / 2);
#endif

    /* Create VM Log-Viewer container: */
    m_pTabWidget = new QITabWidget;
    if (m_pTabWidget)
    {
        /* Add VM Log-Viewer container to main-layout: */
        m_pMainLayout->addWidget(m_pTabWidget);
    }

    /* Create VM Log-Viewer search-panel: */
    m_pSearchPanel = new UIVMLogViewerSearchPanel(0, this);
    if (m_pSearchPanel)
    {
        /* Configure VM Log-Viewer search-panel: */
        installEventFilter(m_pSearchPanel);
        m_pSearchPanel->hide();
        /* Add VM Log-Viewer search-panel to main-layout: */
        m_pMainLayout->addWidget(m_pSearchPanel);
        connect(m_pSearchPanel, &UIVMLogViewerSearchPanel::sigHighlightingUpdated,
                this, &UIVMLogViewerWidget::sltSearchResultHighLigting);
    }

    /* Create VM Log-Viewer filter-panel: */
    m_pFilterPanel = new UIVMLogViewerFilterPanel(0, this);
    if (m_pFilterPanel)
    {
        /* Configure VM Log-Viewer filter-panel: */
        installEventFilter(m_pFilterPanel);
        m_pFilterPanel->hide();
        /* Add VM Log-Viewer filter-panel to main-layout: */
        m_pMainLayout->addWidget(m_pFilterPanel);
        connect(m_pFilterPanel, &UIVMLogViewerFilterPanel::sigFilterApplied,
                this, &UIVMLogViewerWidget::sltFilterApplied);
    }

    m_pBookmarksPanel = new UIVMLogViewerBookmarksPanel(0, this);
    if (m_pBookmarksPanel)
    {
        installEventFilter(m_pBookmarksPanel);
        m_pBookmarksPanel->hide();
        m_pMainLayout->addWidget(m_pBookmarksPanel);
        connect(m_pBookmarksPanel, &UIVMLogViewerBookmarksPanel::sigDeleteBookmark,
                this, &UIVMLogViewerWidget::sltDeleteBookmark);
        connect(m_pBookmarksPanel, &UIVMLogViewerBookmarksPanel::sigDeleteAllBookmarks,
                this, &UIVMLogViewerWidget::sltDeleteAllBookmarks);
        connect(m_pBookmarksPanel, &UIVMLogViewerBookmarksPanel::sigBookmarkSelected,
                this, &UIVMLogViewerWidget::gotoBookmark);
    }

    m_pSettingsPanel = new UIVMLogViewerSettingsPanel(0, this);
    if (m_pSettingsPanel)
    {
        installEventFilter(m_pSettingsPanel);
        m_pSettingsPanel->hide();
        /* Initialize settings' panel checkboxes and input fields: */
        m_pSettingsPanel->setShowLineNumbers(m_bShowLineNumbers);
        m_pSettingsPanel->setWrapLines(m_bWrapLines);
        m_pSettingsPanel->setFontSizeInPoints(m_iFontSizeInPoints);

        m_pMainLayout->addWidget(m_pSettingsPanel);
        connect(m_pSettingsPanel, &UIVMLogViewerSettingsPanel::sigShowLineNumbers, this, &UIVMLogViewerWidget::sltShowLineNumbers);
        connect(m_pSettingsPanel, &UIVMLogViewerSettingsPanel::sigWrapLines, this, &UIVMLogViewerWidget::sltWrapLines);
        connect(m_pSettingsPanel, &UIVMLogViewerSettingsPanel::sigFontSizeInPoints, this, &UIVMLogViewerWidget::sltFontSizeChanged);
    }
}

void UIVMLogViewerWidget::prepareActions()
{
    /* Create and configure 'Find' action: */
    m_pActionFind = new QAction(this);
    if (m_pActionFind)
    {
        m_pActionFind->setShortcut(QKeySequence("Ctrl+F"));
        m_pActionFind->setCheckable(true);
        connect(m_pActionFind, &QAction::triggered, this, &UIVMLogViewerWidget::sltPanelActionTriggered);
    }

    /* Create and configure 'Filter' action: */
    m_pActionFilter = new QAction(this);
    if (m_pActionFilter)
    {
        m_pActionFilter->setShortcut(QKeySequence("Ctrl+T"));
        m_pActionFilter->setCheckable(true);
        connect(m_pActionFilter, &QAction::triggered, this, &UIVMLogViewerWidget::sltPanelActionTriggered);
    }
    /* Create and configure 'Bookmark' action: */
    m_pActionBookmark = new QAction(this);
    if (m_pActionBookmark)
    {
        /* tie Ctrl+D to save only if we show this in a dialog since Ctrl+D is
           already assigned to another action in the selector UI: */
        if (m_enmEmbedding == EmbedTo_Dialog)
            m_pActionBookmark->setShortcut(QKeySequence("Ctrl+D"));
        m_pActionBookmark->setCheckable(true);
        connect(m_pActionBookmark, &QAction::triggered, this, &UIVMLogViewerWidget::sltPanelActionTriggered);
    }

    /* Create and configure 'Settings' action: */
    m_pActionSettings = new QAction(this);
    if (m_pActionSettings)
    {
        m_pActionSettings->setCheckable(true);
        connect(m_pActionSettings, &QAction::triggered, this, &UIVMLogViewerWidget::sltPanelActionTriggered);
    }


    /* Create and configure 'Refresh' action: */
    m_pActionRefresh = new QAction(this);
    if (m_pActionRefresh)
    {
        m_pActionRefresh->setShortcut(QKeySequence("F5"));
        connect(m_pActionRefresh, &QAction::triggered, this, &UIVMLogViewerWidget::sltRefresh);
    }

    /* Create and configure 'Save' action: */
    m_pActionSave = new QAction(this);
    if (m_pActionSave)
    {
        /* tie Ctrl+S to save only if we show this in a dialog since Ctrl+S is
           already assigned to another action in the selector UI: */
        if (m_enmEmbedding == EmbedTo_Dialog)
            m_pActionSave->setShortcut(QKeySequence("Ctrl+S"));
        connect(m_pActionSave, &QAction::triggered, this, &UIVMLogViewerWidget::sltSave);
     }

    /* Update action icons: */
    prepareActionIcons();
}

void UIVMLogViewerWidget::prepareActionIcons()
{
    QString strPrefix = "log_viewer";

    if (m_pActionFind)
        m_pActionFind->setIcon(UIIconPool::iconSet(QString(":/%1_find_24px.png").arg(strPrefix),
                                                       QString(":/%1_find_disabled_24px.png").arg(strPrefix)));

    if (m_pActionFilter)
        m_pActionFilter->setIcon(UIIconPool::iconSet(QString(":/%1_filter_24px.png").arg(strPrefix),
                                                         QString(":/%1_filter_disabled_24px.png").arg(strPrefix)));


    if (m_pActionRefresh)
        m_pActionRefresh->setIcon(UIIconPool::iconSet(QString(":/%1_refresh_24px.png").arg(strPrefix),
                                                          QString(":/%1_refresh_disabled_24px.png").arg(strPrefix)));


    if (m_pActionSave)
        m_pActionSave->setIcon(UIIconPool::iconSet(QString(":/%1_save_24px.png").arg(strPrefix),
                                                       QString(":/%1_save_disabled_24px.png").arg(strPrefix)));

    if (m_pActionBookmark)
        m_pActionBookmark->setIcon(UIIconPool::iconSet(QString(":/%1_bookmark_24px.png").arg(strPrefix),
                                                       QString(":/%1_bookmark_disabled_24px.png").arg(strPrefix)));

    if (m_pActionSettings)
        m_pActionSettings->setIcon(UIIconPool::iconSet(QString(":/%1_bookmark_24px.png").arg(strPrefix),
                                                       QString(":/%1_bookmark_disabled_24px.png").arg(strPrefix)));
}

void UIVMLogViewerWidget::prepareToolBar()
{
    /* Create toolbar: */
    m_pToolBar = new UIToolBar(parentWidget());
    if (m_pToolBar)
    {
        /* Configure toolbar: */
        const int iIconMetric = (int)(QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) * 1.375);
        m_pToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
        m_pToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        /* Add toolbar actions: */
        if (m_pActionFind)
            m_pToolBar->addAction(m_pActionFind);

        if (m_pActionFilter)
            m_pToolBar->addAction(m_pActionFilter);

        if (m_pActionRefresh)
            m_pToolBar->addAction(m_pActionRefresh);

        if (m_pActionSave)
            m_pToolBar->addAction(m_pActionSave);

        if (m_pActionBookmark)
            m_pToolBar->addAction(m_pActionBookmark);

        if (m_pActionSettings)
            m_pToolBar->addAction(m_pActionSettings);

#ifdef VBOX_WS_MAC
        /* Check whether we are embedded into a stack: */
        if (m_enmEmbedding == EmbedTo_Stack)
        {
            /* Add into layout: */
            layout()->addWidget(m_pToolBar);
        }
#else
        /* Add into layout: */
        m_pMainLayout->addWidget(m_pToolBar);
#endif
    }
}

void UIVMLogViewerWidget::prepareMenu()
{
    /* Create 'LogViewer' menu: */
    m_pMenu = new QMenu(this);
    if (m_pMenu)
    {
        if (m_pActionFind)
            m_pMenu->addAction(m_pActionFind);
        if (m_pActionFilter)
            m_pMenu->addAction(m_pActionFilter);
        if (m_pActionRefresh)
            m_pMenu->addAction(m_pActionRefresh);
        if (m_pActionSave)
            m_pMenu->addAction(m_pActionSave);
        if (m_pActionBookmark)
            m_pMenu->addAction(m_pActionBookmark);
        if (m_pActionSettings)
            m_pMenu->addAction(m_pActionSettings);
    }
}

void UIVMLogViewerWidget::cleanup()
{
}

void UIVMLogViewerWidget::retranslateUi()
{
    if (m_pMenu)
    {
        m_pMenu->setTitle(tr("&Log Viewer"));
    }

    if (m_pActionFind)
    {
        m_pActionFind->setText(tr("&Find"));
        m_pActionFind->setToolTip(tr("Find a string within the log"));
        m_pActionFind->setStatusTip(tr("Find a string within the log"));
    }

    if (m_pActionFilter)
    {
        m_pActionFilter->setText(tr("&Filter"));
        m_pActionFilter->setToolTip(tr("Filter the log wrt. the given string"));
        m_pActionFilter->setStatusTip(tr("Filter the log wrt. the given string"));
    }

    if (m_pActionRefresh)
    {
        m_pActionRefresh->setText(tr("&Refresh"));
        m_pActionRefresh->setToolTip(tr("Reload the log"));
        m_pActionRefresh->setStatusTip(tr("Reload the log"));
    }

    if (m_pActionSave)
    {
        m_pActionSave->setText(tr("&Save..."));
        m_pActionSave->setToolTip(tr("Save the log"));
        m_pActionSave->setStatusTip(tr("Save the log"));
    }

    if (m_pActionBookmark)
    {
        m_pActionBookmark->setText(tr("&Bookmarks"));
        m_pActionBookmark->setToolTip(tr("Bookmark the line"));
        m_pActionBookmark->setStatusTip(tr("Bookmark the line"));
    }

    if (m_pActionSettings)
    {
        m_pActionSettings->setText(tr("&Settings"));
        m_pActionSettings->setToolTip(tr("LogViewer Settings"));
        m_pActionSettings->setStatusTip(tr("LogViewer Settings"));
    }

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

void UIVMLogViewerWidget::showEvent(QShowEvent *pEvent)
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

void UIVMLogViewerWidget::keyPressEvent(QKeyEvent *pEvent)
{
    /* Depending on key pressed: */
    switch (pEvent->key())
    {
        /* Process key escape as VM Log Viewer close: */
        case Qt::Key_Escape:
        {
            return;
        }
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
    QWidget::keyReleaseEvent(pEvent);
}

const UIVMLogPage *UIVMLogViewerWidget::currentLogPage() const
{
    int currentTabIndex = m_pTabWidget->currentIndex();
    if (currentTabIndex >= m_logPageList.size())
        return 0;
    return qobject_cast<const UIVMLogPage*>(m_logPageList.at(currentTabIndex));
}
UIVMLogPage *UIVMLogViewerWidget::currentLogPage()
{
    int currentTabIndex = m_pTabWidget->currentIndex();
    if (currentTabIndex >= m_logPageList.size() || currentTabIndex == -1)
        return 0;

    return qobject_cast<UIVMLogPage*>(m_logPageList.at(currentTabIndex));
}

void UIVMLogViewerWidget::hidePanel(UIVMLogViewerPanel* panel)
{
    if (panel && panel->isVisible())
        panel->setVisible(false);
    QMap<UIVMLogViewerPanel*, QAction*>::iterator iterator = m_panelActionMap.find(panel);
    if (iterator != m_panelActionMap.end())
    {
        if (iterator.value()->isChecked())
            iterator.value()->setChecked(false);
    }
}

void UIVMLogViewerWidget::showPanel(UIVMLogViewerPanel* panel)
{
    if (panel && panel->isHidden())
        panel->setVisible(true);
    QMap<UIVMLogViewerPanel*, QAction*>::iterator iterator = m_panelActionMap.find(panel);
    if (iterator != m_panelActionMap.end())
    {
        if (!iterator.value()->isChecked())
            iterator.value()->setChecked(true);
    }
}

QPlainTextEdit* UIVMLogViewerWidget::logPage(int pIndex) const
{
    if (!m_pTabWidget->isEnabled())
        return 0;
    QWidget* pContainer = m_pTabWidget->widget(pIndex);
    if (!pContainer)
        return 0;
    QPlainTextEdit *pBrowser = pContainer->findChild<QPlainTextEdit*>();
    return pBrowser;
}

bool UIVMLogViewerWidget::createLogViewerPages()
{
    bool noLogsToShow = false;

    QString strDummyTabText;
    /* check if the machine is valid: */
    if (m_comMachine.isNull())
    {
        noLogsToShow = true;
        strDummyTabText = QString(tr("<p><b>No machine</b> is currently selected or the selected machine is not valid. "
                                     "Please select a Virtual Machine to see its logs"));
    }

    const CSystemProperties &sys = vboxGlobal().virtualBox().GetSystemProperties();
    unsigned cMaxLogs = sys.GetLogHistoryCount() + 1 /*VBox.log*/ + 1 /*VBoxHardening.log*/; /** @todo Add api for getting total possible log count! */
    bool logFileRead = false;
    for (unsigned i = 0; i < cMaxLogs && !noLogsToShow; ++i)
    {
        /* Query the log file name for index i: */
        QString strFileName = m_comMachine.QueryLogFilename(i);
        if (!strFileName.isEmpty())
        {
            /* Try to read the log file with the index i: */
            ULONG uOffset = 0;
            QString strText;
            while (true)
            {
                QVector<BYTE> data = m_comMachine.ReadLog(i, uOffset, _1M);
                if (data.size() == 0)
                    break;
                strText.append(QString::fromUtf8((char*)data.data(), data.size()));
                uOffset += data.size();
            }
            /* Anything read at all? */
            if (uOffset > 0)
            {
                logFileRead = true;
                createLogPage(strFileName, strText);
            }
        }
    }
    if (!noLogsToShow && !logFileRead)
    {
        noLogsToShow = true;
        strDummyTabText = QString(tr("<p>No log files found. Press the "
                                     "<b>Refresh</b> button to rescan the log folder "
                                     "<nobr><b>%1</b></nobr>.</p>")
                                     .arg(m_comMachine.GetLogFolder()));
    }

    /* if noLogsToShow then ceate a single log page with an error message: */
    if (noLogsToShow)
    {
        createLogPage("No Logs", strDummyTabText, noLogsToShow);
    }
    return noLogsToShow;
}

void UIVMLogViewerWidget::createLogPage(const QString &strFileName, const QString &strLogContent, bool noLogsToShow /* = false */)
{
    if (!m_pTabWidget)
        return;

    /* Create page-container: */
    UIVMLogPage* pLogPage = new UIVMLogPage();
    if (pLogPage)
    {
        connect(pLogPage, &UIVMLogPage::sigBookmarksUpdated, this, &UIVMLogViewerWidget::sltUpdateBookmarkPanel);
        connect(pLogPage, &UIVMLogPage::sigLogPageFilteredChanged, this, &UIVMLogViewerWidget::sltLogPageFilteredChanged);
        /* Initialize setting for this log page */
        pLogPage->setShowLineNumbers(m_bShowLineNumbers);
        pLogPage->setWrapLines(m_bWrapLines);
        pLogPage->setFontSizeInPoints(m_iFontSizeInPoints);

        /* Set the file name only if we really have log file to read. */
        if (!noLogsToShow)
            pLogPage->setFileName(strFileName);

        /* Add page-container to viewer-container: */
        int tabIndex = m_pTabWidget->insertTab(m_pTabWidget->count(), pLogPage, QFileInfo(strFileName).fileName());

        pLogPage->setTabIndex(tabIndex);
        m_logPageList.resize(m_pTabWidget->count());
        m_logPageList[tabIndex] = pLogPage;

        /* Set the log string of the UIVMLogPage: */
        pLogPage->setLogString(strLogContent);
        /* Set text edit since we want to display this text: */
        if (!noLogsToShow)
            pLogPage->setTextEditText(strLogContent);
        /* In case there are some errors append the error text as html: */
        else
        {
            pLogPage->setTextEditTextAsHtml(strLogContent);
            pLogPage->markForError();
        }
    }
}

void UIVMLogViewerWidget::resetHighlighthing()
{
    /* Undo the document changes to remove highlighting: */
    UIVMLogPage* logPage = currentLogPage();
    if (!logPage)
        return;
    logPage->documentUndo();
    logPage->clearScrollBarMarkingsVector();
}
