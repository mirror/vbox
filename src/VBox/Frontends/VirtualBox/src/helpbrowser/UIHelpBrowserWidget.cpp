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
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFont>
#include <QHBoxLayout>
#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP)
 #include <QtHelp/QHelpEngine>
 #include <QtHelp/QHelpContentWidget>
 #include <QtHelp/QHelpIndexWidget>
#include <QtHelp/QHelpSearchEngine>
#include <QtHelp/QHelpSearchQueryWidget>
#include <QtHelp/QHelpSearchResultWidget>
#endif
#include <QMenu>
#include <QScrollBar>
#include <QSpacerItem>
#include <QStyle>
#include <QSplitter>
#include <QTextBrowser>
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
#include "UIHelpBrowserWidget.h"
#include "QIToolBar.h"
#include "UICommon.h"
#include "UIIconPool.h"

/* COM includes: */
#include "CSystemProperties.h"

enum HelpBrowserTabs
{
    HelpBrowserTabs_TOC = 0,
    HelpBrowserTabs_Index,
    HelpBrowserTabs_Search,
    HelpBrowserTabs_Bookmarks,
    HelpBrowserTabs_Max
};
Q_DECLARE_METATYPE(HelpBrowserTabs);

class UIHelpBrowserAddressBar : public QComboBox
{
    Q_OBJECT;

public:
    UIHelpBrowserAddressBar(QWidget *pParent = 0);

};

class UIHelpBrowserViewer : public QTextBrowser
{
    Q_OBJECT;

public:

    UIHelpBrowserViewer(const QHelpEngine *pHelpEngine, QWidget *pParent = 0);
    virtual QVariant loadResource(int type, const QUrl &name) /* override */;
    void emitHistoryChangedSignal();

public slots:

    //virtual void setSource(const QUrl &name) /* override */;

private:

#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP)
    const QHelpEngine* m_pHelpEngine;
#endif
};

UIHelpBrowserAddressBar::UIHelpBrowserAddressBar(QWidget *pParent /* = 0 */)
    :QComboBox(pParent)
{
}

UIHelpBrowserViewer::UIHelpBrowserViewer(const QHelpEngine *pHelpEngine, QWidget *pParent /* = 0 */)
    :QTextBrowser(pParent)
#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP)
    , m_pHelpEngine(pHelpEngine)
#endif
{
#if !defined(RT_OS_LINUX) || !defined(VBOX_WITH_DOCS_QHELP)
    Q_UNUSED(pHelpEngine);
#endif
}

QVariant UIHelpBrowserViewer::loadResource(int type, const QUrl &name)
{
#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP)
    if (name.scheme() == "qthelp" && m_pHelpEngine)
        return QVariant(m_pHelpEngine->fileData(name));
    else
        return QTextBrowser::loadResource(type, name);
#else
    return QTextBrowser::loadResource(type, name);
#endif
}

void UIHelpBrowserViewer::emitHistoryChangedSignal()
{
    emit historyChanged();
    emit backwardAvailable(true);
}

UIHelpBrowserWidget::UIHelpBrowserWidget(EmbedTo enmEmbedding,
                                         const QString &strHelpFilePath,
                                         bool fShowToolbar /* = true */,
                                         QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_enmEmbedding(enmEmbedding)
    , m_fShowToolbar(fShowToolbar)
    , m_fIsPolished(false)
    , m_pMainLayout(0)
    , m_pTopLayout(0)
    , m_pTabWidget(0)
    , m_pToolBar(0)
    , m_strHelpFilePath(strHelpFilePath)
#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP)
    , m_pHelpEngine(0)
#endif
    , m_pAddressBar(0)
    , m_pContentViewer(0)
    , m_pSplitter(0)
    , m_pMenu(0)
    , m_pContentWidget(0)
    , m_pIndexWidget(0)
    , m_pContentModel(0)
    , m_pHelpSearchEngine(0)
    , m_pHelpSearchQueryWidget(0)
    , m_pHelpSearchResultWidget(0)
    , m_pBookmarksWidget(0)
    , m_pSearchContainerWidget(0)
    , m_pShowHideSideBarAction(0)
    , m_pShowHideToolBarAction(0)
    , m_pHomeAction(0)
    , m_pForwardAction(0)
    , m_pBackwardAction(0)
    , m_pAddBookmarkAction(0)
    , m_fModelContentCreated(false)
{
    qRegisterMetaType<HelpBrowserTabs>("HelpBrowserTabs");
    prepare();
    loadOptions();
}

UIHelpBrowserWidget::~UIHelpBrowserWidget()
{
    /* Cleanup VM Log-Viewer: */
    cleanup();
}

QMenu *UIHelpBrowserWidget::menu() const
{
    return m_pMenu;
}


bool UIHelpBrowserWidget::shouldBeMaximized() const
{
    return gEDataManager->logWindowShouldBeMaximized();
}

void UIHelpBrowserWidget::prepare()
{
    /* Create main layout: */
    m_pMainLayout = new QVBoxLayout(this);
    AssertReturnVoid(m_pMainLayout);

    prepareActions();
    prepareToolBar();
    prepareWidgets();
    prepareSearchWidgets();
    prepareMenu();
    retranslateUi();
}

void UIHelpBrowserWidget::prepareActions()
{
    m_pShowHideSideBarAction = new QAction(this);
    m_pShowHideSideBarAction->setCheckable(true);
    m_pShowHideSideBarAction->setChecked(true);
    connect(m_pShowHideSideBarAction, &QAction::toggled, this, &UIHelpBrowserWidget::sltHandleSideBarVisibility);

    m_pShowHideToolBarAction = new QAction(this);
    m_pShowHideToolBarAction->setCheckable(true);
    m_pShowHideToolBarAction->setChecked(true);
    connect(m_pShowHideToolBarAction, &QAction::toggled, this, &UIHelpBrowserWidget::sltHandleToolBarVisibility);


    m_pHomeAction =
        new QAction(UIIconPool::iconSet(":/help_browser_home_32px.png"), QString(), this);
    connect(m_pHomeAction, &QAction::triggered, this, &UIHelpBrowserWidget::sltHandleHomeAction);

    m_pForwardAction =
        new QAction(UIIconPool::iconSet(":/help_browser_forward_32px.png", ":/help_browser_forward_disabled_32px.png"), QString(), this);
    connect(m_pForwardAction, &QAction::triggered, this, &UIHelpBrowserWidget::sltHandleForwardAction);
    sltHandleForwardAvailable(false);

    m_pBackwardAction =
        new QAction(UIIconPool::iconSet(":/help_browser_backward_32px.png", ":/help_browser_backward_disabled_32px.png"), QString(), this);
    connect(m_pBackwardAction, &QAction::triggered, this, &UIHelpBrowserWidget::sltHandleBackwardAction);
    sltHandleBackwardAvailable(false);

    m_pAddBookmarkAction =
        new QAction(UIIconPool::iconSet(":/help_browser_add_bookmark.png"), QString(), this);
    connect(m_pBackwardAction, &QAction::triggered, this, &UIHelpBrowserWidget::sltHandleAddBookmarkAction);
}

void UIHelpBrowserWidget::prepareWidgets()
{
    m_pSplitter = new QSplitter;
    AssertReturnVoid(m_pSplitter);

    m_pMainLayout->addWidget(m_pSplitter);
#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP)
    m_pHelpEngine = new QHelpEngine(m_strHelpFilePath, this);
    m_pBookmarksWidget = new QWidget(this);
    m_pTabWidget = new QITabWidget;
    AssertReturnVoid(m_pTabWidget &&
                     m_pHelpEngine &&
                     m_pBookmarksWidget);

    m_pContentWidget = m_pHelpEngine->contentWidget();
    m_pIndexWidget = m_pHelpEngine->indexWidget();
    m_pContentModel = m_pHelpEngine->contentModel();

    AssertReturnVoid(m_pContentWidget && m_pIndexWidget && m_pContentModel);
    m_pSplitter->addWidget(m_pTabWidget);

    m_pTabWidget->insertTab(HelpBrowserTabs_TOC, m_pContentWidget, QString());
    m_pTabWidget->insertTab(HelpBrowserTabs_Index, m_pIndexWidget, QString());
    m_pTabWidget->insertTab(HelpBrowserTabs_Bookmarks, m_pBookmarksWidget, QString());

    m_pContentViewer = new UIHelpBrowserViewer(m_pHelpEngine);
    AssertReturnVoid(m_pContentViewer);

    connect(m_pContentViewer, &UIHelpBrowserViewer::sourceChanged,
        this, &UIHelpBrowserWidget::sltHandleHelpBrowserViewerSourceChange);
    connect(m_pContentViewer, &UIHelpBrowserViewer::forwardAvailable,
        this, &UIHelpBrowserWidget::sltHandleForwardAvailable);
    connect(m_pContentViewer, &UIHelpBrowserViewer::backwardAvailable,
        this, &UIHelpBrowserWidget::sltHandleBackwardAvailable);
    connect(m_pContentViewer, &UIHelpBrowserViewer::sourceChanged,
        this, &UIHelpBrowserWidget::sltHandleHelpBrowserViewerSourceChange);
    connect(m_pContentViewer, &UIHelpBrowserViewer::historyChanged,
        this, &UIHelpBrowserWidget::sltHandleHistoryChanged);

    m_pSplitter->addWidget(m_pContentViewer);

    m_pSplitter->setStretchFactor(0, 1);
    m_pSplitter->setStretchFactor(1, 4);
    m_pSplitter->setChildrenCollapsible(false);

    connect(m_pHelpEngine, &QHelpEngine::setupFinished,
            this, &UIHelpBrowserWidget::sltHandleHelpEngineSetupFinished);
    connect(m_pContentWidget, &QHelpContentWidget::linkActivated,
            m_pContentViewer, &UIHelpBrowserViewer::setSource);
    connect(m_pContentWidget, &QHelpContentWidget::clicked,
            this, &UIHelpBrowserWidget::sltHandleContentWidgetItemClicked);
    connect(m_pIndexWidget, &QHelpIndexWidget::linkActivated,
            m_pContentViewer, &UIHelpBrowserViewer::setSource);
    connect(m_pContentModel, &QHelpContentModel::contentsCreated,
            this, &UIHelpBrowserWidget::sltHandleContentsCreated);

    if (QFile(m_strHelpFilePath).exists() && m_pHelpEngine)
        m_pHelpEngine->setupData();
#endif
}

void UIHelpBrowserWidget::prepareSearchWidgets()
{
#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP)
# if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)

    AssertReturnVoid(m_pTabWidget && m_pHelpEngine);

    m_pSearchContainerWidget = new QWidget;
    m_pTabWidget->insertTab(HelpBrowserTabs_Search, m_pSearchContainerWidget, QString());

    m_pHelpSearchEngine = m_pHelpEngine->searchEngine();
    AssertReturnVoid(m_pHelpSearchEngine);

    m_pHelpSearchQueryWidget = m_pHelpSearchEngine->queryWidget();
    m_pHelpSearchResultWidget = m_pHelpSearchEngine->resultWidget();
    AssertReturnVoid(m_pHelpSearchQueryWidget && m_pHelpSearchResultWidget);

    QVBoxLayout *pSearchLayout = new QVBoxLayout(m_pSearchContainerWidget);
    pSearchLayout->addWidget(m_pHelpSearchQueryWidget);
    pSearchLayout->addWidget(m_pHelpSearchResultWidget);


    connect(m_pHelpSearchQueryWidget, &QHelpSearchQueryWidget::search,
            this, &UIHelpBrowserWidget::sltHandleSearchStart);
    // connect(resultWidget, &QHelpSearchResultWidget::requestShowLink,
    //         this, &SearchWidget::requestShowLink);

    // connect(searchEngine, &QHelpSearchEngine::searchingStarted,
    //         this, &SearchWidget::searchingStarted);
    // connect(searchEngine, &QHelpSearchEngine::searchingFinished,
    //         this, &SearchWidget::searchingFinished);




    // connect(m_pHelpSearchEngine, &QHelpSearchEngine::indexingStarted,
    //         this, &UIHelpBrowserWidget::sltHandleIndexingStarted);
    // connect(m_pHelpSearchEngine, &QHelpSearchEngine::indexingFinished,
    //         this, &UIHelpBrowserWidget::sltHandleIndexingFinished);

    //void      searchingFinished(int searchResultCount)


    connect(m_pHelpSearchEngine, &QHelpSearchEngine::searchingStarted,
            this, &UIHelpBrowserWidget::sltHandleSearchingStarted);

    m_pHelpSearchEngine->reindexDocumentation();
# endif//if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
#endif
}

void UIHelpBrowserWidget::prepareToolBar()
{
    m_pTopLayout = new QHBoxLayout;
    /* Create toolbar: */
    m_pToolBar = new QIToolBar(parentWidget());
    if (m_pToolBar)
    {
        /* Configure toolbar: */
        m_pToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
        const int iIconMetric = (int)(QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize));
        m_pToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
        m_pToolBar->addAction(m_pBackwardAction);
        m_pToolBar->addAction(m_pForwardAction);
        m_pToolBar->addAction(m_pHomeAction);
        m_pToolBar->addAction(m_pAddBookmarkAction);

#ifdef VBOX_WS_MAC
        /* Check whether we are embedded into a stack: */
        if (m_enmEmbedding == EmbedTo_Stack)
        {
            /* Add into layout: */
            m_pTopLayout->addWidget(m_pToolBar);
            m_pMainLayout->addLayout(m_pTopLayout);
        }
#else
        /* Add into layout: */
        m_pTopLayout->addWidget(m_pToolBar);
        m_pMainLayout->addLayout(m_pTopLayout);
#endif
    }
    m_pAddressBar = new UIHelpBrowserAddressBar();
    m_pAddressBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_pTopLayout->addWidget(m_pAddressBar);
    connect(m_pAddressBar, static_cast<void(UIHelpBrowserAddressBar::*)(int)>(&UIHelpBrowserAddressBar::currentIndexChanged),
            this, &UIHelpBrowserWidget::sltHandleAddressBarIndexChanged);

    //m_pTopLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Fixed));
}

void UIHelpBrowserWidget::prepareMenu()
{
    m_pMenu = new QMenu(tr("View"), this);
    AssertReturnVoid(m_pMenu);

    m_pMenu->addAction(m_pShowHideSideBarAction);
    m_pMenu->addAction(m_pShowHideToolBarAction);

}

void UIHelpBrowserWidget::loadOptions()
{
#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP)
    if (m_pContentViewer && m_pHelpEngine)
    {
        QUrl url(gEDataManager->helpBrowserLastUrl());
        if (url.isEmpty())
            return;
        if (url.isValid())
        {
            if (m_pHelpEngine->findFile(url).isValid())
            {
                m_pContentViewer->setSource(url);
                m_pContentViewer->clearHistory();
            }
            else
                show404Error(url);
        }
    }
#endif
}

void UIHelpBrowserWidget::saveOptions()
{
    if (m_pContentViewer)
    {
        gEDataManager->setHelpBrowserLastUrl(m_pContentViewer->source().toString());
    }
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
    if (m_pTabWidget)
    {
        m_pTabWidget->setTabText(HelpBrowserTabs_TOC, tr("Contents"));
        m_pTabWidget->setTabText(HelpBrowserTabs_Index, tr("Index"));
        m_pTabWidget->setTabText(HelpBrowserTabs_Search, tr("Search"));
        m_pTabWidget->setTabText(HelpBrowserTabs_Bookmarks, tr("Bookmarks"));
    }
    if (m_pShowHideSideBarAction)
        m_pShowHideSideBarAction->setText(tr("Show/Hide Side Bar"));

    if (m_pShowHideToolBarAction)
        m_pShowHideToolBarAction->setText(tr("Show/Hide Tool Bar"));

    m_strPageNotFoundText = tr("<div><p><h3>404. Not found.</h3>The page <b>%1</b> could not be found.</p></div>");

    if (m_pHomeAction)
    {
        m_pHomeAction->setText(tr("Home"));
        m_pHomeAction->setToolTip(tr("Return to start page"));
    }

    if (m_pBackwardAction)
    {
        m_pBackwardAction->setText(tr("Backward"));
        m_pBackwardAction->setToolTip(tr("Navigate to previous page"));
    }

    if (m_pForwardAction)
    {
        m_pForwardAction->setText(tr("Forward"));
        m_pForwardAction->setToolTip(tr("Navigate to next page"));
    }
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
   QWidget::keyPressEvent(pEvent);
}

void UIHelpBrowserWidget::sltHandleSideBarVisibility(bool fToggled)
{
    if (m_pTabWidget)
        m_pTabWidget->setVisible(fToggled);
}

void UIHelpBrowserWidget::sltHandleToolBarVisibility(bool fToggled)
{
    if (m_pToolBar)
        m_pToolBar->setVisible(fToggled);
}

QUrl UIHelpBrowserWidget::findIndexHtml() const
{
#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP)
    QList<QUrl> files = m_pHelpEngine->files(m_pHelpEngine->namespaceName(m_strHelpFilePath), QStringList());
    int iIndex = -1;
    for (int i = 0; i < files.size(); ++i)
    {
        if (files[i].toString().contains("index.html", Qt::CaseInsensitive))
        {
            iIndex = i;
            break;
        }
    }
    if (iIndex == -1)
    {
        /* If index html/htm could not be found try to find a html file at least: */
        for (int i = 0; i < files.size(); ++i)
        {
            if (files[i].toString().contains(".html", Qt::CaseInsensitive) ||
                files[i].toString().contains(".htm", Qt::CaseInsensitive))
            {
                iIndex = i;
                break;
            }
        }
    }
    if (iIndex != -1 && files.size() > iIndex)
        return files[iIndex];
    return QUrl();
#else
    return QUrl();
#endif
}

void UIHelpBrowserWidget::show404Error(const QUrl &url)
{
    if (m_pContentWidget)
        m_pContentViewer->setText(m_strPageNotFoundText.arg(url.toString()));
}

void UIHelpBrowserWidget::sltHandleHelpEngineSetupFinished()
{
#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP)
    AssertReturnVoid(m_pContentViewer && m_pHelpEngine);
    /* Search for the index of the index.htnl: */
    QUrl url = findIndexHtml();
    if (url.isValid())
        m_pContentViewer->setSource(url);
    else
        show404Error(url);
#endif
}

void sltHandleHomeAction();

void UIHelpBrowserWidget::sltHandleContentWidgetItemClicked(const QModelIndex &index)
{
#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP)
    AssertReturnVoid(m_pContentViewer && m_pHelpEngine && m_pContentWidget);
    QHelpContentModel *pContentModel =
        qobject_cast<QHelpContentModel*>(m_pContentWidget->model());
    if (!pContentModel)
        return;
    QHelpContentItem *pItem = pContentModel->contentItemAt(index);
    if (!pItem)
        return;
    const QUrl &url = pItem->url();
    m_pContentViewer->blockSignals(true);
    m_pContentViewer->setSource(url);
    m_pContentViewer->blockSignals(false);
    /* emit historyChanged signal explicitly since we have blocked the signals: */
    m_pContentViewer->emitHistoryChangedSignal();
#else
    Q_UNUSED(index);
#endif
}

void UIHelpBrowserWidget::sltHandleHelpBrowserViewerSourceChange(const QUrl &source)
{
#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP)
    if (m_fModelContentCreated && m_pContentWidget && source.isValid() && m_pContentModel)
    {
        QModelIndex index = m_pContentWidget->indexOf(source);
        QItemSelectionModel *pSelectionModel = m_pContentWidget->selectionModel();
        if (pSelectionModel && index.isValid())
        {
            m_pContentWidget->blockSignals(true);
            pSelectionModel->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            m_pContentWidget->scrollTo(index, QAbstractItemView::EnsureVisible);
            m_pContentWidget->expand(index);
            m_pContentWidget->blockSignals(false);
        }
    }
#else
    Q_UNUSED(source);
#endif
}

void UIHelpBrowserWidget::sltHandleContentsCreated()
{
    m_fModelContentCreated = true;
    if (m_pContentViewer)
        sltHandleHelpBrowserViewerSourceChange(m_pContentViewer->source());
}

void UIHelpBrowserWidget::sltHandleHomeAction()
{
    if (!m_pContentViewer)
        return;

    QUrl homeUrl = findIndexHtml();
    if (!homeUrl.isValid())
        return;
    m_pContentViewer->setSource(homeUrl);
}

void UIHelpBrowserWidget::sltHandleForwardAction()
{
    if (m_pContentViewer)
        m_pContentViewer->forward();
}

void UIHelpBrowserWidget::sltHandleBackwardAction()
{
    if (m_pContentViewer)
        m_pContentViewer->backward();
}

void UIHelpBrowserWidget::sltHandleForwardAvailable(bool fAvailable)
{
    if (m_pForwardAction)
        m_pForwardAction->setEnabled(fAvailable);
}

void UIHelpBrowserWidget::sltHandleBackwardAvailable(bool fAvailable)
{
    if (m_pBackwardAction)
        m_pBackwardAction->setEnabled(fAvailable);
}

void UIHelpBrowserWidget::sltHandleHistoryChanged()
{
    if (!m_pContentViewer)
        return;
    int iCurrentIndex = 0;
    /* QTextBrower history has negative and positive indices for bacward and forward items, respectively.
     * 0 is the current item: */
    m_pAddressBar->blockSignals(true);
    m_pAddressBar->clear();
    for (int i = -1 * m_pContentViewer->backwardHistoryCount(); i <= m_pContentViewer->forwardHistoryCount(); ++i)
    {
        m_pAddressBar->addItem(m_pContentViewer->historyTitle(i), i);
        if (i == 0)
            iCurrentIndex = m_pAddressBar->count();
    }
    /* Make sure address bar show the current item: */
    m_pAddressBar->setCurrentIndex(iCurrentIndex - 1);
    m_pAddressBar->blockSignals(false);
}

void UIHelpBrowserWidget::sltHandleAddressBarIndexChanged(int iIndex)
{
    if (!m_pAddressBar && iIndex >= m_pAddressBar->count())
        return;
    int iHistoryIndex = m_pAddressBar->itemData(iIndex).toInt();
    /* There seems to be no way to one-step-jump to a history item: */
    if (iHistoryIndex == 0)
        return;
    else if (iHistoryIndex > 0)
        for (int i = 0; i < iHistoryIndex; ++i)
            m_pContentViewer->forward();
    else
        for (int i = 0; i > iHistoryIndex ; --i)
            m_pContentViewer->backward();

}

void UIHelpBrowserWidget::sltHandleAddBookmarkAction()
{
}

void UIHelpBrowserWidget::sltHandleIndexingStarted()
{
    printf("indexing started\n");
}

void UIHelpBrowserWidget::sltHandleIndexingFinished()
{
    printf("indexing finished\n");
}

void UIHelpBrowserWidget::sltHandleSearchingStarted()
{
    printf("search started\n");
}

void UIHelpBrowserWidget::sltHandleSearchStart()
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    AssertReturnVoid(m_pHelpSearchEngine && m_pHelpSearchQueryWidget);
    m_pHelpSearchEngine->search(m_pHelpSearchQueryWidget->searchInput());
#endif
}
#include "UIHelpBrowserWidget.moc"
