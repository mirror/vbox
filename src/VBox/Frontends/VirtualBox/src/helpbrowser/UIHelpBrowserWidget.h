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
class UIDialogPanel;
class UIHelpBrowserAddressBar;
class UIHelpBrowserViewer;

class SHARED_LIBRARY_STUFF UIHelpBrowserWidget  : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigSetCloseButtonShortCut(QKeySequence);

public:

    UIHelpBrowserWidget(EmbedTo enmEmbedding, const QString &strHelpFilePath,
                        bool fShowToolbar = true, QWidget *pParent = 0);
    ~UIHelpBrowserWidget();

    QMenu *menu() const;

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
    void sltHandleHelpBrowserViewerSourceChange(const QUrl &source);
    void sltHandleContentsCreated();
    void sltHandleHomeAction();
    void sltHandleForwardAction();
    void sltHandleBackwardAction();
    void sltHandleForwardAvailable(bool fAvailable);
    void sltHandleBackwardAvailable(bool fAvailable);
    void sltHandleHistoryChanged();
    void sltHandleAddressBarIndexChanged(int index);
    void sltHandleAddBookmarkAction();
    void sltHandleIndexingStarted();
    void sltHandleIndexingFinished();
    void sltHandleSearchingStarted();
    void sltHandleSearchStart();

private:

    void prepare();
    void prepareActions();
    void prepareWidgets();
    void prepareSearchWidgets();
    void prepareToolBar();
    void prepareMenu();
    void loadOptions();

    void saveOptions();
    void cleanup();
    QUrl findIndexHtml() const;
    void show404Error(const QUrl &url);
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
#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DOCS_QHELP)
    QHelpEngine  *m_pHelpEngine;
#endif
    UIHelpBrowserAddressBar *m_pAddressBar;
    UIHelpBrowserViewer *m_pContentViewer;
    QSplitter           *m_pSplitter;
    QMenu               *m_pMenu;
    QHelpContentWidget  *m_pContentWidget;
    QHelpIndexWidget    *m_pIndexWidget;
    QHelpContentModel   *m_pContentModel;
    QHelpSearchEngine   *m_pHelpSearchEngine;
    QHelpSearchQueryWidget *m_pHelpSearchQueryWidget;
    QHelpSearchResultWidget *m_pHelpSearchResultWidget;
    QWidget             *m_pBookmarksWidget;
    QWidget             *m_pSearchContainerWidget;
    QAction             *m_pShowHideSideBarAction;
    QAction             *m_pShowHideToolBarAction;
    QAction             *m_pHomeAction;
    QAction             *m_pForwardAction;
    QAction             *m_pBackwardAction;
    QAction             *m_pAddBookmarkAction;
    QString              m_strPageNotFoundText;
    /* This is set t true when handling QHelpContentModel::contentsCreated signal. */
    bool                 m_fModelContentCreated;
};

#endif /* !FEQT_INCLUDED_SRC_helpbrowser_UIHelpBrowserWidget_h */
