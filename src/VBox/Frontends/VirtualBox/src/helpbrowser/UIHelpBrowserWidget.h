/* $Id$ */
/** @file
 * VBox Qt GUI - UIHelpBrowserWidget class declaration.
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

#ifndef FEQT_INCLUDED_SRC_helpbrowser_UIHelpBrowserWidget_h
#define FEQT_INCLUDED_SRC_helpbrowser_UIHelpBrowserWidget_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */

#include <QKeySequence>
#include <QPair>
#include <QWidget>

/* GUI includes: */
#include "QIManagerDialog.h"
#include "QIWithRetranslateUI.h"
#include "UILibraryDefs.h"

/* COM includes: */
#include "COMEnums.h"

/* Forward declarations: */
class QHBoxLayout;
class QItemSelection;
class QVBoxLayout;
class QHelpEngine;
class QHelpContentModel;
class QHelpContentWidget;
class QHelpIndexWidget;
class QHelpSearchEngine;
class QHelpSearchQueryWidget;
class QHelpSearchResultWidget;
class QPlainTextEdit;
class QSplitter;
class QITabWidget;
class QIToolBar;
class UIActionPool;
class UIBookmarksListContainer;
class UIDialogPanel;
class UIHelpBrowserTabManager;

#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP) && (QT_VERSION >= QT_VERSION_CHECK(5, 9, 0))
class SHARED_LIBRARY_STUFF UIHelpBrowserWidget  : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigSetCloseButtonShortCut(QKeySequence);

public:

    UIHelpBrowserWidget(EmbedTo enmEmbedding, const QString &strHelpFilePath,
                        bool fShowToolbar = true, QWidget *pParent = 0);
    ~UIHelpBrowserWidget();

    QList<QMenu*> menus() const;

#ifdef VBOX_WS_MAC
    QIToolBar *toolbar() const { return m_pToolBar; }
#endif


protected:

    /** Returns whether the window should be maximized when geometry being restored. */
    virtual bool shouldBeMaximized() const /* override */;

private slots:

    void sltHandleHelpEngineSetupFinished();
    void sltHandleContentWidgetItemClicked(const QModelIndex &index);
    void sltHandleSideBarVisibility(bool togggled);
    void sltHandleToolBarVisibility(bool togggled);
    void sltShowPrintDialog();
    void sltHandleContentsCreated();
    void sltHandleIndexingStarted();
    void sltHandleIndexingFinished();
    void sltHandleSearchingStarted();
    void sltHandleSearchStart();
    void sltHandleHelpBrowserViewerSourceChange(const QUrl &source);
    void sltOpenLinkWithUrl(const QUrl &url);
    void sltShowLinksContextMenu(const QPoint &pos);
    void sltOpenLinkInNewTab();
    void sltOpenLink();
    void sltAddNewBookmark(const QUrl &url, const QString &strTitle);
    void sltHandleFontSizeactions();

private:

    void prepare();
    void prepareActions();
    void prepareWidgets();
    void prepareSearchWidgets();
    void prepareToolBar();
    void prepareMenu();
    void loadOptions();
    QStringList loadSavedUrlList();
    void saveOptions();
    void cleanup();
    QUrl findIndexHtml() const;
    /* Returns the url of the item with @p itemIndex. */
    QUrl contentWidgetUrl(const QModelIndex &itemIndex);
    void openLinkSlotHandler(QObject *pSenderObject, bool fOpenInNewTab);

    /** @name Event handling stuff.
     * @{ */
    /** Handles translation event. */
       virtual void retranslateUi() /* override */;

       /** Handles Qt show @a pEvent. */
       virtual void showEvent(QShowEvent *pEvent) /* override */;
       /** Handles Qt key-press @a pEvent. */
       virtual void keyPressEvent(QKeyEvent *pEvent) /* override */;
    /** @} */

    /** Holds the widget's embedding type. */
    const EmbedTo m_enmEmbedding;
    /** Hold sthe action-pool reference. */
    UIActionPool *m_pActionPool;
    /** Holds whether we should create/show toolbar. */
    const bool    m_fShowToolbar;

    /** Holds whether the dialog is polished. */
    bool m_fIsPolished;

    /** Holds container for log-pages. */
    QVBoxLayout         *m_pMainLayout;
    QHBoxLayout         *m_pTopLayout;

    QITabWidget *m_pTabWidget;
    /** @name Toolbar and menu variables.
     * @{ */
    QIToolBar *m_pToolBar;
    /** @} */

    QString       m_strHelpFilePath;
    QHelpEngine  *m_pHelpEngine;
    QSplitter           *m_pSplitter;
    QMenu               *m_pFileMenu;
    QMenu               *m_pViewMenu;
    QHelpContentWidget  *m_pContentWidget;
    QHelpIndexWidget    *m_pIndexWidget;
    QHelpContentModel   *m_pContentModel;
    QHelpSearchEngine   *m_pSearchEngine;
    QHelpSearchQueryWidget *m_pSearchQueryWidget;
    QHelpSearchResultWidget  *m_pSearchResultWidget;
    UIHelpBrowserTabManager  *m_pTabManager;
    UIBookmarksListContainer *m_pBookmarksWidget;
    QWidget             *m_pSearchContainerWidget;
    QAction             *m_pPrintDialogAction;
    QAction             *m_pShowHideSideBarAction;
    QAction             *m_pShowHideToolBarAction;
    QAction             *m_pFontSizeLargerAction;
    QAction             *m_pFontSizeSmallerAction;
    QAction             *m_pFontSizeResetAction;
    /* This is set t true when handling QHelpContentModel::contentsCreated signal. */
    bool                 m_fModelContentCreated;
};

#endif /* #if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP) && (QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)) */
#endif /* !FEQT_INCLUDED_SRC_helpbrowser_UIHelpBrowserWidget_h */
