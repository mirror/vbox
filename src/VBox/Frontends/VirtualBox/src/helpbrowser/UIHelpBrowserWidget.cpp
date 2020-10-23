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
#include <QHBoxLayout>
#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP)
 #include <QtHelp/QHelpEngine>
 #include <QtHelp/QHelpContentWidget>
 #include <QtHelp/QHelpIndexWidget>
#endif
#include <QMenu>
#include <QScrollBar>
#include <QStyle>
#include <QSplitter>
#include <QTextBrowser>
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

enum HelpBrowserTabs
{
    HelpBrowserTabs_TOC = 0,
    HelpBrowserTabs_Index,
    HelpBrowserTabs_Bookmarks,
    HelpBrowserTabs_Max
};

class UIHelpBrowserViewer : public QTextBrowser
{
    Q_OBJECT;

public:

    UIHelpBrowserViewer(const QHelpEngine *pHelpEngine, QWidget *pParent = 0);
    virtual QVariant loadResource(int type, const QUrl &name) /* override */;

private:

#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP)
    const QHelpEngine* m_pHelpEngine;
#endif
};

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


UIHelpBrowserWidget::UIHelpBrowserWidget(EmbedTo enmEmbedding,
                                         const QString &strHelpFilePath,
                                         bool fShowToolbar /* = true */,
                                         QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_enmEmbedding(enmEmbedding)
    , m_fShowToolbar(fShowToolbar)
    , m_fIsPolished(false)
    , m_pMainLayout(0)
    , m_pTabWidget(0)
    , m_pToolBar(0)
    , m_strHelpFilePath(strHelpFilePath)
#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP)
    , m_pHelpEngine(0)
#endif
    , m_pContentViewer(0)
    , m_pSplitter(0)
    , m_pMenu(0)
    , m_pContentWidget(0)
    , m_pIndexWidget(0)
    , m_pBookmarksWidget(0)
    , m_pShowHideContentsWidgetAction(0)
{
    prepare();
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
    loadOptions();

    prepareActions();
    prepareWidgets();
    prepareMenu();
    retranslateUi();
}

void UIHelpBrowserWidget::prepareActions()
{
    m_pShowHideContentsWidgetAction = new QAction(this);
    m_pShowHideContentsWidgetAction->setData(HelpBrowserTabs_TOC);
}

void UIHelpBrowserWidget::prepareWidgets()
{
    /* Create main layout: */
    m_pMainLayout = new QHBoxLayout(this);
    m_pSplitter = new QSplitter;

    AssertReturnVoid(m_pMainLayout && m_pSplitter);

    m_pMainLayout->addWidget(m_pSplitter);
#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP)
    m_pHelpEngine = new QHelpEngine(m_strHelpFilePath, this);
    m_pBookmarksWidget = new QWidget(this);
    m_pTabWidget = new QITabWidget;
    AssertReturnVoid(m_pTabWidget && m_pHelpEngine && m_pBookmarksWidget);

    m_pContentWidget = m_pHelpEngine->contentWidget();
    m_pIndexWidget = m_pHelpEngine->indexWidget();

    AssertReturnVoid(m_pContentWidget && m_pIndexWidget);
    m_pSplitter->addWidget(m_pTabWidget);
    m_pTabWidget->insertTab(HelpBrowserTabs_TOC, m_pContentWidget, QString());
    m_pTabWidget->insertTab(HelpBrowserTabs_Index, m_pIndexWidget, QString());
    m_pTabWidget->insertTab(HelpBrowserTabs_Bookmarks, m_pBookmarksWidget, QString());

    m_pContentViewer = new UIHelpBrowserViewer(m_pHelpEngine);
    AssertReturnVoid(m_pContentViewer);
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

    if (QFile(m_strHelpFilePath).exists() && m_pHelpEngine)
        m_pHelpEngine->setupData();
#endif
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

void UIHelpBrowserWidget::prepareMenu()
{
    m_pMenu = new QMenu(tr("View"), this);
    AssertReturnVoid(m_pMenu);
    m_pMenu->addAction(tr("View"));
}

void UIHelpBrowserWidget::loadOptions()
{
}

void UIHelpBrowserWidget::saveOptions()
{
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
        m_pTabWidget->setTabText(HelpBrowserTabs_Bookmarks, tr("Bookmarks"));
    }
    if (m_pShowHideContentsWidgetAction)
        m_pShowHideContentsWidgetAction->setText("Show/Hide Contents");
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


void UIHelpBrowserWidget::sltHandleHelpEngineSetupFinished()
{
#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP)
    AssertReturnVoid(m_pContentViewer && m_pHelpEngine);
    QList<QUrl> files = m_pHelpEngine->files(m_pHelpEngine->namespaceName(m_strHelpFilePath), QStringList());
    /* Search for the index of the index.htnl: */
    int iIndex = 0;
    for (int i = 0; i < files.size(); ++i)
    {
        if (files[i].toString().contains("index.html", Qt::CaseInsensitive))
        {
            iIndex = i;
            break;
        }
    }
    if (files.size() > iIndex)
        m_pContentViewer->setSource(files[iIndex]);
    /** @todo show some kind of error maybe. */
#endif
}

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
    m_pContentViewer->setSource(url);
#else
    Q_UNUSED(index);
#endif
}


#include "UIHelpBrowserWidget.moc"
