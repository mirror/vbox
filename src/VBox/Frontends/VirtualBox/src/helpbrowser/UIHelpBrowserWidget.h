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
#include <QPair>
#include <QWidget>

/* GUI includes: */
#include "QIManagerDialog.h"
#include "QIWithRetranslateUI.h"

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
class QSplitter;
class QITabWidget;
class QIToolBar;
class UIActionPool;
class UIBookmarksListContainer;
class UIHelpBrowserTabManager;

#ifdef VBOX_WITH_QHELP_VIEWER
class SHARED_LIBRARY_STUFF UIHelpBrowserWidget  : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigCloseDialog();
    void sigLinkHighlighted(const QString &strLink);
    void sigStatusBarVisible(bool fToggled);

public:

    UIHelpBrowserWidget(EmbedTo enmEmbedding, const QString &strHelpFilePath, QWidget *pParent = 0);
    ~UIHelpBrowserWidget();
    QList<QMenu*> menus() const;
    void showHelpForKeyword(const QString &strKeyword);
#ifdef VBOX_WS_MAC
    QIToolBar *toolbar() const { return m_pToolBar; }
#endif

    static const QPair<float, float> fontScaleMinMax;

protected:

    /** Returns whether the window should be maximized when geometry being restored. */
    virtual bool shouldBeMaximized() const /* override */;

private slots:

    void sltHandleHelpEngineSetupFinished();
    void sltHandleContentWidgetItemClicked(const QModelIndex &index);
    void sltHandleWidgetVisibilityToggle(bool togggled);
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
    void sltCopyLink();
    void sltAddNewBookmark(const QUrl &url, const QString &strTitle);
    void sltHandleZoomActions();
    void sltHandleTabListChanged(const QStringList &titleList);
    void sltHandleTabChoose();
    void sltHandleCurrentTabChanged(int iIndex);

private:

    void prepare();
    void prepareActions();
    void prepareWidgets();
    void prepareSearchWidgets();
    void prepareToolBar();
    void prepareMenu();
    void loadOptions();
    QStringList loadSavedUrlList();
    /** Bookmark list is save as url-title pairs. */
    void loadBookmarks();
    void saveBookmarks();
    void saveOptions();
    void cleanup();
    QUrl findIndexHtml() const;
    /* Returns the url of the item with @p itemIndex. */
    QUrl contentWidgetUrl(const QModelIndex &itemIndex);
    void openLinkSlotHandler(QObject *pSenderObject, bool fOpenInNewTab);
    void updateTabsMenu(const QStringList &titleList);

    /** @name Event handling stuff.
     * @{ */
    /** Handles translation event. */
       virtual void retranslateUi() /* override */;

       /** Handles Qt show @a pEvent. */
       virtual void showEvent(QShowEvent *pEvent) /* override */;
       /** Handles Qt key-press @a pEvent. */
       virtual void keyPressEvent(QKeyEvent *pEvent) /* override */;
    /** @} */
    /* Looks for Url for the keyword using QHelpEngine API and shows it in a new tab whne successful. */
    void findAndShowUrlForKeyword(const QString &strKeyword);

    /** Holds the widget's embedding type. */
    const EmbedTo m_enmEmbedding;
    UIActionPool *m_pActionPool;
    bool m_fIsPolished;

    QVBoxLayout         *m_pMainLayout;
    QHBoxLayout         *m_pTopLayout;
    /** Container tab widget for content, index, bookmark widgets. Sits on a side bar. */
    QITabWidget *m_pTabWidget;

    /** @name Toolbar and menu variables.
     * @{ */
       QIToolBar *m_pToolBar;
    /** @} */

    QString       m_strHelpFilePath;
    /** Start the browser with this keyword. When not empty widget is shown `only` with html viewer and single tab.*/
    QHelpEngine  *m_pHelpEngine;
    QSplitter           *m_pSplitter;
    QMenu               *m_pFileMenu;
    QMenu               *m_pViewMenu;
    QMenu               *m_pTabsMenu;
    QHelpContentWidget  *m_pContentWidget;
    QHelpIndexWidget    *m_pIndexWidget;
    QHelpContentModel   *m_pContentModel;
    QHelpSearchEngine   *m_pSearchEngine;
    QHelpSearchQueryWidget *m_pSearchQueryWidget;
    QHelpSearchResultWidget  *m_pSearchResultWidget;
    UIHelpBrowserTabManager  *m_pTabManager;
    UIBookmarksListContainer *m_pBookmarksWidget;
    QWidget             *m_pSearchContainerWidget;
    QAction             *m_pPrintAction;
    QAction             *m_pCloseDialogAction;
    QAction             *m_pShowHideSideBarAction;
    QAction             *m_pShowHideToolBarAction;
    QAction             *m_pShowHideFontScaleWidgetAction;
    QAction             *m_pShowHideStatusBarAction;
    QAction             *m_pFontSizeLargerAction;
    QAction             *m_pFontSizeSmallerAction;
    QAction             *m_pFontSizeResetAction;

    /* This is set t true when handling QHelpContentModel::contentsCreated signal. */
    bool                 m_fModelContentCreated;
    bool                 m_fIndexingFinished;
    /** This queue is used in unlikely case where possibly several keywords are requested to be shown
      *  but indexing is not yet finished. In that case we queue the keywords and process them after
      * after indexing is finished. */
    QStringList          m_keywordList;
};

#endif /* #ifdef VBOX_WITH_QHELP_VIEWER */
#endif /* !FEQT_INCLUDED_SRC_helpbrowser_UIHelpBrowserWidget_h */
