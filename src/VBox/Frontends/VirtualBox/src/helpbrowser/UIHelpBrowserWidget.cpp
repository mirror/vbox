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
#ifdef RT_OS_LINUX
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

class UIHelpBrowserViewer : public QTextBrowser
{
    Q_OBJECT;

public:

    UIHelpBrowserViewer(const QHelpEngine *pHelpEngine, QWidget *pParent = 0);
    virtual QVariant loadResource(int type, const QUrl &name) /* override */;

private:

#ifdef RT_OS_LINUX
    const QHelpEngine* m_pHelpEngine;
#endif
};

UIHelpBrowserViewer::UIHelpBrowserViewer(const QHelpEngine *pHelpEngine, QWidget *pParent /* = 0 */)
    :QTextBrowser(pParent)
#ifdef RT_OS_LINUX
    , m_pHelpEngine(pHelpEngine)
#endif
{
#ifndef RT_OS_LINUX
    Q_UNUSED(pHelpEngine);
#endif
}

QVariant UIHelpBrowserViewer::loadResource(int type, const QUrl &name)
{
#ifdef RT_OS_LINUX
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
#ifdef RT_OS_LINUX
    , m_pHelpEngine(0)
#endif
    , m_pTextBrowser(0)
    , m_pSplitter(0)
{
    /* Prepare VM Log-Viewer: */
    prepare();
}

UIHelpBrowserWidget::~UIHelpBrowserWidget()
{
    /* Cleanup VM Log-Viewer: */
    cleanup();
}

QMenu *UIHelpBrowserWidget::menu() const
{
    return 0;//m_pActionPool->action(UIActionIndex_M_LogWindow)->menu();
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


    retranslateUi();
}

void UIHelpBrowserWidget::prepareActions()
{

}

void UIHelpBrowserWidget::prepareWidgets()
{
    /* Create main layout: */
    m_pMainLayout = new QHBoxLayout(this);
    m_pSplitter = new QSplitter;

    AssertReturnVoid(m_pMainLayout && m_pSplitter);

    m_pMainLayout->addWidget(m_pSplitter);
#ifdef RT_OS_LINUX
    m_pHelpEngine = new QHelpEngine(m_strHelpFilePath, this);

    m_pTabWidget = new QITabWidget;
    AssertReturnVoid(m_pTabWidget);
    AssertReturnVoid(m_pHelpEngine->contentWidget() && m_pHelpEngine->indexWidget());
    m_pSplitter->addWidget(m_pTabWidget);
    m_pTabWidget->addTab(m_pHelpEngine->contentWidget(), tr("Contents"));
    m_pTabWidget->addTab(m_pHelpEngine->indexWidget(), tr("Index"));

    m_pTextBrowser = new UIHelpBrowserViewer(m_pHelpEngine);
    AssertReturnVoid(m_pTextBrowser);
    m_pSplitter->addWidget(m_pTextBrowser);

    m_pSplitter->setStretchFactor(0, 1);
    m_pSplitter->setStretchFactor(1, 4);
    m_pSplitter->setChildrenCollapsible(false);

    connect(m_pHelpEngine, &QHelpEngine::setupFinished,
            this, &UIHelpBrowserWidget::sltHandleHelpEngineSetupFinished);

    connect(m_pHelpEngine->contentWidget(), &QHelpContentWidget::linkActivated,
            m_pTextBrowser, &UIHelpBrowserViewer::setSource);
    connect(m_pHelpEngine->contentWidget(), &QHelpContentWidget::clicked,
            this, &UIHelpBrowserWidget::sltHandleContentWidgetItemClicked);


    connect(m_pHelpEngine->indexWidget(), &QHelpIndexWidget::linkActivated,
            m_pTextBrowser, &UIHelpBrowserViewer::setSource);

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
#ifdef RT_OS_LINUX
    AssertReturnVoid(m_pTextBrowser && m_pHelpEngine);
    QList<QUrl> files = m_pHelpEngine->files(m_pHelpEngine->namespaceName(m_strHelpFilePath), QStringList());
    if (!files.empty())
        m_pTextBrowser->setSource(files[0]);
    /** @todo show some kind of error maybe. */
#endif
}

void UIHelpBrowserWidget::sltHandleContentWidgetItemClicked(const QModelIndex &index)
{
#ifdef RT_OS_LINUX
    AssertReturnVoid(m_pTextBrowser && m_pHelpEngine && m_pHelpEngine->contentWidget());
    QHelpContentModel *pContentModel =
        qobject_cast<QHelpContentModel*>(m_pHelpEngine->contentWidget()->model());
    if (!pContentModel)
        return;
    QHelpContentItem *pItem = pContentModel->contentItemAt(index);
    if (!pItem)
        return;
    const QUrl &url = pItem->url();
    m_pTextBrowser->setSource(url);
#endif
}


#include "UIHelpBrowserWidget.moc"
