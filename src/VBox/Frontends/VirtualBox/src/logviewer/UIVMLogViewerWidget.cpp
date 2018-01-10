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
# include "UIVMLogViewerWidget.h"
# include "UIVMLogViewerBookmarksPanel.h"
# include "UIVMLogViewerFilterPanel.h"
# include "UIVMLogViewerSearchPanel.h"
# include "UIToolBar.h"

/* COM includes: */
# include "CSystemProperties.h"

# include "VBoxGlobal.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** We use a modified scrollbar style for our QPlainTextEdits to get the
    markings on the scrollbars correctly. The default scrollbarstyle does not
    reveal the height of the pushbuttons on the scrollbar to compute the marking
    locations correctlt. so we turn them off: */
const QString verticalScrollBarStyle("QScrollBar:vertical {"
                                     "border: 1px ridge grey; "
                                     "margin: 0px 0px 0 0px;}"
                                     "QScrollBar::handle:vertical {"
                                     "min-height: 10px;"
                                     "background: grey;}"
                                     "QScrollBar::add-line:vertical {"
                                     "width: 0px;}"
                                     "QScrollBar::sub-line:vertical {"
                                     "width: 0px;}");

const QString horizontalScrollBarStyle("QScrollBar:horizontal {"
                                       "border: 1px ridge grey; "
                                       "margin: 0px 0px 0 0px;}"
                                       "QScrollBar::handle:horizontal {"
                                       "min-height: 10px;"
                                       "background: grey;}"
                                       "QScrollBar::add-line:horizontal {"
                                       "height: 0px;}"
                                       "QScrollBar::sub-line:horizontal {"
                                       "height: 0px;}");

class UIIndicatorScrollBar : public QScrollBar
{
    Q_OBJECT;

public:

    UIIndicatorScrollBar(QWidget *parent = 0)
        :QScrollBar(parent)
    {
        setStyleSheet(verticalScrollBarStyle);
    }

    void setMarkingsVector(const QVector<float> &vector)
    {
        m_markingsVector = vector;
    }

protected:

    virtual void paintEvent(QPaintEvent *pEvent) /* override */
    {
        QScrollBar::paintEvent(pEvent);
        /* Put a red line to marking position: */
        for (int i = 0; i < m_markingsVector.size(); ++i)
        {
            QPointF p1 = QPointF(0, m_markingsVector[i] * height());
            QPointF p2 = QPointF(width(), m_markingsVector[i] * height());

            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(QPen(QColor(255, 0, 0, 75), 1.1f));
            painter.drawLine(p1, p2);
        }
    }

private:
    /* Stores the relative (to scrollbar's height) positions of markings,
       where we draw a horizontal line. */
    QVector<float> m_markingsVector;
};

/* Sub-class QPlainTextEdit for some addtional context menu items: */
class UIVMLogViewerTextEdit : public QPlainTextEdit
{
    Q_OBJECT;

signals:

    void sigContextMenuBookmarkAction(LogBookmark bookmark);

public:
    UIVMLogViewerTextEdit(QWidget* parent = 0, const QString& logFileName = QString())
        :QPlainTextEdit(parent),
         m_logFileName(logFileName)
    {
        //setStyleSheet("background-color: rgba(240, 240, 240, 75%) ");
    }

    const QString& logFileName() const
    {
        return m_logFileName;
    }

protected:

    void contextMenuEvent(QContextMenuEvent *pEvent)
    {
        QMenu *menu = createStandardContextMenu();
        QAction *pAction = menu->addAction(tr("Bookmark"));
        QTextBlock block = cursorForPosition(pEvent->pos()).block();
        m_iContextMenuBookmark.first = block.firstLineNumber();
        m_iContextMenuBookmark.second = block.text();

        if (pAction)
            connect(pAction, &QAction::triggered, this, &UIVMLogViewerTextEdit::sltBookmark);

        menu->exec(pEvent->globalPos());

        if (pAction)
            disconnect(pAction, &QAction::triggered, this, &UIVMLogViewerTextEdit::sltBookmark);

        delete menu;
    }

    virtual void mousePressEvent(QMouseEvent *pEvent)
    {
        QPlainTextEdit::mousePressEvent(pEvent);
    }

private slots:
    /// remove
    void sltBookmark()
    {
        emit sigContextMenuBookmarkAction(m_iContextMenuBookmark);
    }

private:

    /* Line number and text at the context menu position */
    LogBookmark m_iContextMenuBookmark;
    /* Name of the log file this text edit created to show. */
    QString m_logFileName;
};


UIVMLogViewerWidget::UIVMLogViewerWidget(EmbedTo enmEmbedding, QWidget *pParent /* = 0 */, const CMachine &machine /* = CMachine() */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_fIsPolished(false)
    , m_comMachine(machine)
    , m_pViewerContainer(0)
    , m_iCurrentTabIndex(-1)
    , m_pSearchPanel(0)
    , m_pFilterPanel(0)
    , m_pBookmarksPanel(0)
    , m_pMainLayout(0)
    , m_enmEmbedding(enmEmbedding)
    , m_pToolBar(0)
    , m_pActionFind(0)
    , m_pActionFilter(0)
    , m_pActionRefresh(0)
    , m_pActionSave(0)
    , m_pActionBookmark(0)
    , m_pMenu(0)
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
    if (!m_pViewerContainer)
        return 0;

    QWidget *pContainer = m_pViewerContainer->currentWidget();
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

void UIVMLogViewerWidget::sltPanelActionTriggered(bool checked)
{
    QAction *pSenderAction = qobject_cast<QAction*>(sender());
    if(!pSenderAction)
        return;
    UIVMLogViewerPanel* pPanel = 0;
    /* Look for the sender() within the m_panelActionMap's values: */
    for(QMap<UIVMLogViewerPanel*, QAction*>::const_iterator iterator = m_panelActionMap.begin();
        iterator != m_panelActionMap.end(); ++iterator)
    {
        if(iterator.value() == pSenderAction)
            pPanel = iterator.key();
    }
    if(!pPanel)
        return;
    if(checked)
        showPanel(pPanel);
    else
        hidePanel(pPanel);
}

void UIVMLogViewerWidget::sltShowHideSearchPanel()
{
    if (!m_pSearchPanel)
        return;
    /* Show/hide search-panel: */
    m_pSearchPanel->isHidden() ? m_pSearchPanel->show() : m_pSearchPanel->hide();
}

void UIVMLogViewerWidget::sltRefresh()
{
    /* Disconnect this connection to avoid initial signals during page creation/deletion: */
    disconnect(m_pViewerContainer, &QITabWidget::currentChanged, m_pFilterPanel, &UIVMLogViewerFilterPanel::applyFilter);
    disconnect(m_pViewerContainer, &QITabWidget::currentChanged, this, &UIVMLogViewerWidget::sltTabIndexChange);

    m_logMap.clear();
    m_pViewerContainer->setEnabled(true);
    /* Hide the container widget during updates to avoid flickering: */
    m_pViewerContainer->hide();
    while (m_pViewerContainer->count())
    {
        QWidget *pFirstPage = m_pViewerContainer->widget(0);
        m_pViewerContainer->removeTab(0);
        delete pFirstPage;
    }

    m_iCurrentTabIndex = -1;
    bool noLogsToShow = false;
    QString strDummyTabText;
    /* check if the machine is valid: */
    if (m_comMachine.isNull())
    {
        noLogsToShow = true;
        strDummyTabText = QString(tr("<p><b>No machine</b> is currently selected. Please select a "
                                     "Virtual Machine to see its logs"));
    }
    /* If machine is valid and check if there are any log files and create viewer tabs: */
    else if (!createLogViewerPages())
    {
        noLogsToShow = true;
        strDummyTabText = QString(tr("<p>No log files found. Press the "
                                     "<b>Refresh</b> button to rescan the log folder "
                                     "<nobr><b>%1</b></nobr>.</p>")
                                     .arg(m_comMachine.GetLogFolder()));
    }
    /* If the machine is not valid or has no log files show a single viewer tab: */
    if (noLogsToShow)
    {
        QPlainTextEdit *pDummyLog = createLogPage("VBox.log");
        pDummyLog->setWordWrapMode(QTextOption::WordWrap);
        pDummyLog->appendHtml(strDummyTabText);
        /* We don't want it to remain white: */
        QPalette pal = pDummyLog->palette();
        pal.setColor(QPalette::Base, pal.color(QPalette::Window));
        pDummyLog->setPalette(pal);
    }

    /* Show the first tab widget's page after the refresh: */
    m_pViewerContainer->setCurrentIndex(0);

    /* Apply the filter settings: */
    if (m_pFilterPanel)
        m_pFilterPanel->applyFilter();

    /* Setup this connection after refresh to avoid initial signals during page creation: */
    connect(m_pViewerContainer, &QITabWidget::currentChanged, m_pFilterPanel, &UIVMLogViewerFilterPanel::applyFilter);
    connect(m_pViewerContainer, &QITabWidget::currentChanged, this, &UIVMLogViewerWidget::sltTabIndexChange);

    m_iCurrentTabIndex = 0;

    /* Enable/Disable toolbar actions (except Refresh) & tab widget according log presence: */
    m_pActionFind->setEnabled(!noLogsToShow);
    m_pActionFilter->setEnabled(!noLogsToShow);
    m_pActionSave->setEnabled(!noLogsToShow);
    m_pActionBookmark->setEnabled(!noLogsToShow);
    m_pViewerContainer->setEnabled(!noLogsToShow);
    m_pViewerContainer->show();
    if (m_pSearchPanel && m_pSearchPanel->isVisible())
        m_pSearchPanel->refresh();
}

void UIVMLogViewerWidget::sltSave()
{
    if (m_comMachine.isNull())
        return;
    UIVMLogViewerTextEdit *logPage = qobject_cast<UIVMLogViewerTextEdit*>(currentLogPage());
    if (!logPage)
        return;
    /* Prepare "save as" dialog: */
    const QFileInfo fileInfo(logPage->logFileName());
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
        QFile::copy(m_comMachine.QueryLogFilename(m_pViewerContainer->currentIndex()), strNewFileName);
    }
}

void UIVMLogViewerWidget::sltShowHideFilterPanel()
{
    if (!m_pFilterPanel)
        return;
    /* Show/hide filter-panel: */
    m_pFilterPanel->isHidden() ? m_pFilterPanel->show() : m_pFilterPanel->hide();
}

void UIVMLogViewerWidget::sltSearchResultHighLigting()
{
    if (!m_pSearchPanel)
        return;

    if (!currentLogPage())
        return;
    UIIndicatorScrollBar* scrollBar = qobject_cast<UIIndicatorScrollBar*>(currentLogPage()->verticalScrollBar());
    if (scrollBar)
        scrollBar->setMarkingsVector(m_pSearchPanel->getMatchLocationVector());

    currentLogPage()->repaint();
}

void UIVMLogViewerWidget::sltTabIndexChange(int tabIndex)
{
    if (m_iCurrentTabIndex == tabIndex)
        return;

    resetHighlighthing();
    if (m_pSearchPanel)
        m_pSearchPanel->reset();
    m_iCurrentTabIndex = tabIndex;
}

void UIVMLogViewerWidget::sltFilterApplied()
{
    /* Reapply the search to get highlighting etc. correctly */
    if (m_pSearchPanel && m_pSearchPanel->isVisible())
        m_pSearchPanel->refresh();
}

void UIVMLogViewerWidget::sltShowHideBookmarkPanel()
{
    if (!m_pBookmarksPanel)
        return;
    m_pBookmarksPanel->isHidden() ? m_pBookmarksPanel->show() : m_pBookmarksPanel->hide();
}

void UIVMLogViewerWidget::sltCreateBookmarkAtCurrent()
{
    if (!currentLogPage())
        return;
    QWidget* viewport = currentLogPage()->viewport();
    if (!viewport)
        return;
    QPoint point(0.5 * viewport->width(), 0.5 * viewport->height());
    QTextBlock block = currentLogPage()->cursorForPosition(point).block();
    LogBookmark bookmark;
    bookmark.first = block.firstLineNumber();
    bookmark.second = block.text();
    sltCreateBookmarkAtLine(bookmark);
}

void UIVMLogViewerWidget::sltCreateBookmarkAtLine(LogBookmark bookmark)
{
    QVector<LogBookmark> *pBookmarkVector = currentBookmarkVector();
    if (!pBookmarkVector)
        return;
    pBookmarkVector->push_back(bookmark);
    if (m_pBookmarksPanel)
    {
        m_pBookmarksPanel->updateBookmarkList();
        m_pBookmarksPanel->setBookmarkIndex(pBookmarkVector->size() - 1);
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

    /* Prepare widgets: */
    prepareWidgets();


    prepareActions();

    prepareToolBar();
    prepareMenu();

    /* Reading log files: */
    sltRefresh();

    /* Loading language constants: */
    retranslateUi();

    m_panelActionMap.insert(m_pBookmarksPanel, m_pActionBookmark);
    m_panelActionMap.insert(m_pSearchPanel, m_pActionFind);
    m_panelActionMap.insert(m_pFilterPanel, m_pActionFilter);
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
    m_pViewerContainer = new QITabWidget(this);
    AssertPtrReturnVoid(m_pViewerContainer);
    {
        /* Add VM Log-Viewer container to main-layout: */
        m_pMainLayout->insertWidget(1, m_pViewerContainer);
    }

    /* Create VM Log-Viewer search-panel: */
    m_pSearchPanel = new UIVMLogViewerSearchPanel(this, this);
    AssertPtrReturnVoid(m_pSearchPanel);
    {
        /* Configure VM Log-Viewer search-panel: */
        installEventFilter(m_pSearchPanel);
        m_pSearchPanel->hide();
        /* Add VM Log-Viewer search-panel to main-layout: */
        m_pMainLayout->insertWidget(2, m_pSearchPanel);
        connect(m_pSearchPanel, &UIVMLogViewerSearchPanel::sigHighlightingUpdated,
                this, &UIVMLogViewerWidget::sltSearchResultHighLigting);
    }

    /* Create VM Log-Viewer filter-panel: */
    m_pFilterPanel = new UIVMLogViewerFilterPanel(this, this);
    AssertPtrReturnVoid(m_pFilterPanel);
    {
        /* Configure VM Log-Viewer filter-panel: */
        installEventFilter(m_pFilterPanel);
        m_pFilterPanel->hide();
        /* Add VM Log-Viewer filter-panel to main-layout: */
        m_pMainLayout->insertWidget(3, m_pFilterPanel);
        connect(m_pFilterPanel, &UIVMLogViewerFilterPanel::sigFilterApplied,
                this, &UIVMLogViewerWidget::sltFilterApplied);
    }

    m_pBookmarksPanel = new UIVMLogViewerBookmarksPanel(this, this);
    AssertPtrReturnVoid(m_pBookmarksPanel);
    {
        installEventFilter(m_pBookmarksPanel);
        m_pBookmarksPanel->hide();
        m_pMainLayout->insertWidget(4, m_pBookmarksPanel);
    }

}

void UIVMLogViewerWidget::prepareActions()
{
    /* Create and configure 'Find' action: */
    m_pActionFind = new QAction(this);
    AssertPtrReturnVoid(m_pActionFind);
    {
        m_pActionFind->setShortcut(QKeySequence("Ctrl+F"));
        m_pActionFind->setCheckable(true);
        connect(m_pActionFind, &QAction::triggered, this, &UIVMLogViewerWidget::sltPanelActionTriggered);
    }

    /* Create and configure 'Filter' action: */
    m_pActionFilter = new QAction(this);
    AssertPtrReturnVoid(m_pActionFilter);
    {
        m_pActionFilter->setShortcut(QKeySequence("Ctrl+T"));
        m_pActionFilter->setCheckable(true);
        connect(m_pActionFilter, &QAction::triggered, this, &UIVMLogViewerWidget::sltPanelActionTriggered);
    }
    /* Create and configure 'Bookmark' action: */
    m_pActionBookmark = new QAction(this);
    AssertPtrReturnVoid(m_pActionBookmark);
    {
        /* tie Ctrl+D to save only if we show this in a dialog since Ctrl+D is
           already assigned to another action in the selector UI: */
        if (m_enmEmbedding == EmbedTo_Dialog)
            m_pActionBookmark->setShortcut(QKeySequence("Ctrl+D"));
        m_pActionBookmark->setCheckable(true);
        connect(m_pActionBookmark, &QAction::triggered, this, &UIVMLogViewerWidget::sltPanelActionTriggered);
    }

    /* Create and configure 'Refresh' action: */
    m_pActionRefresh = new QAction(this);
    AssertPtrReturnVoid(m_pActionRefresh);
    {
        m_pActionRefresh->setShortcut(QKeySequence("F5"));
        connect(m_pActionRefresh, &QAction::triggered, this, &UIVMLogViewerWidget::sltRefresh);
    }

    /* Create and configure 'Save' action: */
    m_pActionSave = new QAction(this);
    AssertPtrReturnVoid(m_pActionSave);
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
}

void UIVMLogViewerWidget::prepareToolBar()
{
    /* Create toolbar: */
    m_pToolBar = new UIToolBar(parentWidget());
    AssertPtrReturnVoid(m_pToolBar);
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

#ifdef VBOX_WS_MAC
        /* Check whether we are embedded into a stack: */
        if (m_enmEmbedding == EmbedTo_Stack)
        {
            /* Add into layout: */
            layout()->addWidget(m_pToolBar);
        }
#else
        /* Add into layout: */
        m_pMainLayout->insertWidget(0, m_pToolBar);
#endif
    }
}

void UIVMLogViewerWidget::prepareMenu()
{
    /* Create 'LogViewer' menu: */
    m_pMenu = new QMenu(this);
    AssertPtrReturnVoid(m_pMenu);
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
        m_pActionBookmark->setText(tr("&Bookmark..."));
        m_pActionBookmark->setToolTip(tr("Bookmark the line"));
        m_pActionBookmark->setStatusTip(tr("Bookmark the line"));
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

    /* Make sure the log view widget has the focus: */
    QWidget *pCurrentLogPage = currentLogPage();
    if (pCurrentLogPage)
        pCurrentLogPage->setFocus();
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
            if (m_pViewerContainer->currentIndex() > 0)
            {
                m_pViewerContainer->setCurrentIndex(m_pViewerContainer->currentIndex() - 1);
                return;
            }
            break;
        }
        /* Process Forward key as switch to next tab: */
        case Qt::Key_Forward:
        {
            if (m_pViewerContainer->currentIndex() < m_pViewerContainer->count())
            {
                m_pViewerContainer->setCurrentIndex(m_pViewerContainer->currentIndex() + 1);
                return;
            }
            break;
        }
        default:
            break;
    }
    QWidget::keyReleaseEvent(pEvent);
}

const QString* UIVMLogViewerWidget::currentLog()
{
    if (!currentLogPage())
        return 0;
    return &(m_logMap[currentLogPage()]);
}

QPlainTextEdit* UIVMLogViewerWidget::currentLogPage() const
{
    /* If viewer-container is enabled: */
    if (m_pViewerContainer->isEnabled())
    {
        /* Get and return current log-page: */
        QWidget *pContainer = m_pViewerContainer->currentWidget();
        QPlainTextEdit *pBrowser = pContainer->findChild<QPlainTextEdit*>();
        Assert(pBrowser);
        return pBrowser ? pBrowser : 0;
    }
    /* Return NULL by default: */
    return 0;
}

const QVector<LogBookmark>* UIVMLogViewerWidget::currentBookmarkVector() const
{
    UIVMLogViewerTextEdit *logPage = qobject_cast<UIVMLogViewerTextEdit*>(currentLogPage());
    if (!logPage)
        return 0;
    QString logFileName = logPage->logFileName();
    if (logFileName.isEmpty())
        return 0;

    return &(m_bookmarkMap[logFileName]);
}

QVector<LogBookmark>* UIVMLogViewerWidget::currentBookmarkVector()
{
    UIVMLogViewerTextEdit *logPage = qobject_cast<UIVMLogViewerTextEdit*>(currentLogPage());
    if (!logPage)
        return 0;
    QString logFileName = logPage->logFileName();
    if (logFileName.isEmpty())
        return 0;

    return &(m_bookmarkMap[logFileName]);
}

void UIVMLogViewerWidget::hidePanel(UIVMLogViewerPanel* panel)
{
    if (panel && panel->isVisible())
        panel->setVisible(false);
    QMap<UIVMLogViewerPanel*, QAction*>::iterator iterator = m_panelActionMap.find(panel);
    if(iterator != m_panelActionMap.end())
    {
        if(iterator.value()->isChecked())
            iterator.value()->setChecked(false);
    }
}

void UIVMLogViewerWidget::showPanel(UIVMLogViewerPanel* panel)
{
    if (panel && panel->isHidden())
        panel->setVisible(true);
    QMap<UIVMLogViewerPanel*, QAction*>::iterator iterator = m_panelActionMap.find(panel);
    if(iterator != m_panelActionMap.end())
    {
        if(!iterator.value()->isChecked())
            iterator.value()->setChecked(true);
    }
}

QPlainTextEdit* UIVMLogViewerWidget::logPage(int pIndex) const
{
    if (!m_pViewerContainer->isEnabled())
        return 0;
    QWidget* pContainer = m_pViewerContainer->widget(pIndex);
    if (!pContainer)
        return 0;
    QPlainTextEdit *pBrowser = pContainer->findChild<QPlainTextEdit*>();
    return pBrowser;
}

bool UIVMLogViewerWidget::createLogViewerPages()
{
    if (m_comMachine.isNull())
        return false;

    bool logsExists = false;
    const CSystemProperties &sys = vboxGlobal().virtualBox().GetSystemProperties();
    unsigned cMaxLogs = sys.GetLogHistoryCount() + 1 /*VBox.log*/ + 1 /*VBoxHardening.log*/; /** @todo Add api for getting total possible log count! */
    for (unsigned i = 0; i < cMaxLogs; ++i)
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
                /* Create a log viewer page and append the read text to it: */
                QPlainTextEdit *pLogViewer = createLogPage(strFileName);
                pLogViewer->setPlainText(strText);
                /* Move the cursor position to end: */
                QTextCursor cursor = pLogViewer->textCursor();
                cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
                pLogViewer->setTextCursor(cursor);
                /* Add the log-text to the map: */
                m_logMap[pLogViewer] = strText;
                logsExists = true;
            }
        }
    }
    return logsExists;
}

QPlainTextEdit* UIVMLogViewerWidget::createLogPage(const QString &strFileName)
{
    /* Create page-container: */
    QWidget *pPageContainer = new QWidget;
    AssertPtrReturn(pPageContainer, 0);
    {
        /* Create page-layout: */
        QVBoxLayout *pPageLayout = new QVBoxLayout(pPageContainer);
        AssertPtrReturn(pPageLayout, 0);
        /* Create Log-Viewer: */
        UIVMLogViewerTextEdit *pLogViewer = new UIVMLogViewerTextEdit(pPageContainer, strFileName);
        connect(pLogViewer, &UIVMLogViewerTextEdit::sigContextMenuBookmarkAction,
                this, &UIVMLogViewerWidget::sltCreateBookmarkAtLine);

        AssertPtrReturn(pLogViewer, 0);
        {
            /* Configure Log-Viewer: */
            pLogViewer->setVerticalScrollBar(new UIIndicatorScrollBar());
            pLogViewer->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
            QScrollBar *pHorizontalScrollBar = pLogViewer->horizontalScrollBar();
            if (pHorizontalScrollBar)
                pHorizontalScrollBar->setStyleSheet(horizontalScrollBarStyle);
#if defined(RT_OS_SOLARIS)
            /* Use system fixed-width font on Solaris hosts as the Courier family fonts don't render well. */
            QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
#else
            QFont font;
            font.setFamily("Courier New,courier");
#endif
            pLogViewer->setFont(font);
            pLogViewer->setWordWrapMode(QTextOption::NoWrap);
            pLogViewer->setReadOnly(true);
            /* Add Log-Viewer to page-layout: */
            pPageLayout->addWidget(pLogViewer);
        }
        /* Add page-container to viewer-container: */
        m_pViewerContainer->addTab(pPageContainer, QFileInfo(strFileName).fileName());
        return pLogViewer;
    }
}

void UIVMLogViewerWidget::resetHighlighthing()
{
    /* Undo the document changes to remove highlighting: */
    QPlainTextEdit *pTextEdit = logPage(m_iCurrentTabIndex);
    if (pTextEdit)
    {
        QTextDocument *pDocument = pTextEdit->document();
        if (pDocument)
            pDocument->undo();
    }
    UIIndicatorScrollBar* scrollBar = qobject_cast<UIIndicatorScrollBar*>(pTextEdit->verticalScrollBar());
    if (scrollBar)
    {
        scrollBar->setMarkingsVector(QVector<float>());
        pTextEdit->repaint();
    }
}

#include "UIVMLogViewerWidget.moc"
