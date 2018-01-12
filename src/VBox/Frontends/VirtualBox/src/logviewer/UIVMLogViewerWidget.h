/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class declaration.
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

#ifndef ___UIVMLogViewerWidget_h___
#define ___UIVMLogViewerWidget_h___

/* Qt includes: */
#include <QWidget>
/* #include <QMap> */
#include <QPair>

/* GUI includes: */
#include "QIManagerDialog.h"
#include "QIWithRetranslateUI.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"

/* Forward declarations: */
class QITabWidget;
class QPlainTextEdit;
class QVBoxLayout;
class UIToolBar;
class UIVMLogPage;
class UIVMLogViewerBookmarksPanel;
class UIVMLogViewerFilterPanel;
class UIVMLogViewerPanel;
class UIVMLogViewerSearchPanel;

/* Type definitions: */
/** value is the content of the log file */
//typedef QMap<QPlainTextEdit*, QString> VMLogMap;
/** first is line number, second is block text */
//typedef QPair<int, QString> LogBookmark;
/** key is log file name, value is a vector of bookmarks. */
//typedef QMap<QString, QVector<LogBookmark> > BookmarkMap;


/** QWidget extension providing GUI for VirtualBox LogViewer. It
 *  encapsulates log pages, toolbar, a tab widget and manages
 *  interaction between these classes. */
class UIVMLogViewerWidget  : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:
    /** Constructs the VM Log-Viewer by passing @a pParent to QWidget base-class constructor.
      * @param  machine  Specifies the machine for which VM Log-Viewer is requested. */
    UIVMLogViewerWidget(EmbedTo enmEmbedding, QWidget *pParent = 0, const CMachine &machine = CMachine());
    /** Destructs the VM Log-Viewer. */
    ~UIVMLogViewerWidget();
    /** Returns the width of the current log page. return 0 if there is no current log page: */
    int defaultLogPageWidth() const;

    /** Returns the menu. */
    QMenu *menu() const { return m_pMenu; }

#ifdef VBOX_WS_MAC
    /** Returns the toolbar. */
    UIToolBar *toolbar() const { return m_pToolBar; }
#endif

    /** Sets the machine whose logs to show. */
    void setMachine(const CMachine &machine);

protected:

    /** Returns whether the window should be maximized when geometry being restored. */
    virtual bool shouldBeMaximized() const /* override */;

private slots:

    /** Handles refresh action triggering. */
    void sltRefresh();
    /** Handles save action triggering. */
    void sltSave();

    /** @name Bookmark related slots
     * @{ */
    /** Deletes the bookmark with @p index from the current logs bookmark list. */
        void sltDeleteBookmark(int index);
        /** Receives delete all signal from the bookmark panel and notifies UIVMLogPage. */
        void sltDeleteAllBookmarks();
        /** Manages bookmark panel update when bookmark vector is updated */
        void sltBoomarksUpdated();
        /* Makes the current UIVMLogPage to goto (scroll) its bookmark with index @a index. */
        void gotoBookmark(int bookmarkIndex);
    /** @} */

    void sltPanelActionTriggered(bool checked);
    /** Handles the search result highlight changes. */
    void sltSearchResultHighLigting();
    /** Handles the tab change of the logviewer. */
    void sltTabIndexChange(int tabIndex);
    void sltFilterApplied();

private:

    /** @name Prepare/Cleanup
      * @{ */
        /** Prepares VM Log-Viewer. */
        void prepare();
        /** Prepares widgets. */
        void prepareWidgets();
        void prepareActions();
        void prepareActionIcons();
        void prepareToolBar();
        void prepareMenu();

        /** Cleanups VM Log-Viewer. */
        void cleanup();
    /** @} */

    /** @name Event handling stuff.
      * @{ */
        /** Handles translation event. */
        void retranslateUi();

        /** Handles Qt show @a pEvent. */
        void showEvent(QShowEvent *pEvent);
        /** Handles Qt key-press @a pEvent. */
        void keyPressEvent(QKeyEvent *pEvent);
    /** @} */


    /** Returns the log-page from the tab with index @a pIndex. */
    QPlainTextEdit* logPage(int pIndex) const;
    /** Returns the newly created log-page using @a strPage filename. */
    void createLogPage(const QString &strFileName, const QString &strLogContent, bool noLogsToShow = false);


    UIVMLogPage *currentLogPage();
    const UIVMLogPage *currentLogPage() const;

    /** Attempts to read the logs through the API, returns true if there exists any logs, false otherwise. */
    bool createLogViewerPages();

    /** Resets document (of the curent tab) and scrollbar highligthing */
    void resetHighlighthing();

    void hidePanel(UIVMLogViewerPanel* panel);
    void showPanel(UIVMLogViewerPanel* panel);

    /** Holds whether the dialog is polished. */
    bool m_fIsPolished;

    /** Holds the machine instance. */
    CMachine m_comMachine;

    /** Holds container for log-pages. */
    QITabWidget        *m_pTabWidget;
    /** Stores the UIVMLogPage instances. This is modified as we add and remove new tabs
     *  to the m_pTabWidget. Index is the index of the tab widget. */
    QVector<QWidget*>  m_logPageList;

    /** Holds the index to the current tab: */
    //int          m_iCurrentTabIndex;

    /** Holds the instance of search-panel. */
    UIVMLogViewerSearchPanel    *m_pSearchPanel;
    /** Holds the instance of filter panel. */
    UIVMLogViewerFilterPanel    *m_pFilterPanel;
    UIVMLogViewerBookmarksPanel *m_pBookmarksPanel;
    QMap<UIVMLogViewerPanel*, QAction*> m_panelActionMap;

    /** Holds the list of log file content. */
    // VMLogMap             m_logMap;
    // mutable BookmarkMap  m_bookmarkMap;
    QVBoxLayout         *m_pMainLayout;

    /** Holds the widget's embedding type. */
    const EmbedTo m_enmEmbedding;

    /** @name Toolbar and menu variables.
      * @{ */
        /** Holds the toolbar widget instance. */
        UIToolBar *m_pToolBar;
        /** Holds the Find action instance. */
        QAction   *m_pActionFind;
        /** Holds the Filter action instance. */
        QAction   *m_pActionFilter;
        /** Holds the Refresh action instance. */
        QAction   *m_pActionRefresh;
        /** Holds the Save action instance. */
        QAction   *m_pActionSave;
        /** Holds the Bookmark action instance. */
        QAction   *m_pActionBookmark;
        /** Holds the menu object instance. */
        QMenu     *m_pMenu;
    /** @} */
    const bool     m_bMarkBookmarkLines;
    friend class UIVMLogViewerBookmarksPanel;
    friend class UIVMLogViewerFilterPanel;
    friend class UIVMLogViewerSearchPanel;
    friend class UIVMLogViewerPanel;
};

#endif /* !___UIVMLogViewerWidget_h___ */
