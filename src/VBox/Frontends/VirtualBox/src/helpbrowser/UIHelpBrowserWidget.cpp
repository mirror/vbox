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
#ifdef VBOX_WS_X11
 #include <QtHelp/QHelpEngine>
 #include <QtHelp/QHelpContentWidget>
#endif
#include <QMenu>
#include <QScrollBar>
#include <QStyle>
#include <QTextBrowser>
#include <QHBoxLayout>
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

#ifdef VBOX_WS_X11
    const QHelpEngine* m_pHelpEngine;
#endif
};

UIHelpBrowserViewer::UIHelpBrowserViewer(const QHelpEngine *pHelpEngine, QWidget *pParent /* = 0 */)
    :QTextBrowser(pParent)
#ifdef VBOX_WS_X11
    , m_pHelpEngine(pHelpEngine)
#endif
{
#ifndef VBOX_WS_X11
    Q_UNUSED(pHelpEngine);
#endif
}

QVariant UIHelpBrowserViewer::loadResource(int type, const QUrl &name)
{
#ifdef VBOX_WS_X11
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
#ifdef VBOX_WS_X11
    , m_pHelpEngine(0)
#endif
    , m_pTextBrowser(0)
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
    AssertReturnVoid(m_pMainLayout);
#ifdef VBOX_WS_X11
    m_pHelpEngine = new QHelpEngine(m_strHelpFilePath, this);
    connect(m_pHelpEngine, &QHelpEngine::setupFinished,
            this, &UIHelpBrowserWidget::sltHandleHelpEngineSetupFinished);

    // m_pTabWidget = new QITabWidget;
    // m_pMainLayout->addWidget(m_pTabWidget);
    m_pTextBrowser = new UIHelpBrowserViewer(m_pHelpEngine);
    AssertReturnVoid(m_pTextBrowser);
    m_pMainLayout->addWidget(m_pTextBrowser);

    if (QFile(m_strHelpFilePath).exists() && m_pHelpEngine)
    {
        bool fSetupResult = m_pHelpEngine->setupData();
        //m_pHelpEngine->registerDocumentation(m_strHelpFilePath));
        printf("setup data %d %s\n", fSetupResult, qPrintable(m_strHelpFilePath));
    }
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
#ifdef VBOX_WS_X11
    AssertReturnVoid(m_pTextBrowser && m_pHelpEngine);

    QList<QUrl> files = m_pHelpEngine->files(m_pHelpEngine->namespaceName(m_strHelpFilePath), QStringList());
    if (!files.empty())
        m_pTextBrowser->setSource(files[0]);
    /*  @todo: show some kind of error maybe. */
#endif
}

#include "UIHelpBrowserWidget.moc"
