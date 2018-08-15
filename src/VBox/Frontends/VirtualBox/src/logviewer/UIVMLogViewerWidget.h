/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewerWidget class declaration.
 */

/*
 * Copyright (C) 2010-2018 Oracle Corporation
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
#include <QKeySequence>
#include <QPair>
#include <QWidget>

/* GUI includes: */
#include "QIManagerDialog.h"
#include "QIWithRetranslateUI.h"
#include "UILibraryDefs.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"

/* Forward declarations: */
class QITabWidget;
class QPlainTextEdit;
class QVBoxLayout;
class UIActionPool;
class UIToolBar;
class UIVMLogPage;
class UIVMLogViewerBookmarksPanel;
class UIVMLogViewerFilterPanel;
class UIVMLogViewerPanel;
class UIVMLogViewerSearchPanel;
class UIVMLogViewerSettingsPanel;

/** QWidget extension providing GUI for VirtualBox LogViewer. It
 *  encapsulates log pages, toolbar, a tab widget and manages
 *  interaction between these classes. */
class SHARED_LIBRARY_STUFF UIVMLogViewerWidget  : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigSetCloseButtonShortCut(QKeySequence);

public:

    /** Constructs the VM Log-Viewer by passing @a pParent to QWidget base-class constructor.
      * @param  enmEmbedding  Brings the type of widget embedding.
      * @param  pActionPool   Brings the action-pool reference.
      * @param  fShowToolbar  Brings whether we should create/show toolbar.
      * @param  comMachine    Brings the machine for which VM Log-Viewer is requested. */
    UIVMLogViewerWidget(EmbedTo enmEmbedding, UIActionPool *pActionPool,
                        bool fShowToolbar = true, const CMachine &comMachine = CMachine(), QWidget *pParent = 0);
    /** Destructs the VM Log-Viewer. */
    ~UIVMLogViewerWidget();
    /** Returns the width of the current log page. return 0 if there is no current log page: */
    int defaultLogPageWidth() const;

    /** Returns the menu. */
    QMenu *menu() const;

#ifdef VBOX_WS_MAC
    /** Returns the toolbar. */
    UIToolBar *toolbar() const { return m_pToolBar; }
#endif

    /** Defines the @a comMachine whose logs to show. */
    void setMachine(const CMachine &comMachine);
    QFont currentFont() const;

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
        /** Manages bookmark panel update when bookmark vector is updated. */
        void sltUpdateBookmarkPanel();
        /** Makes the current UIVMLogPage to goto (scroll) its bookmark with index @a index. */
        void gotoBookmark(int bookmarkIndex);
    /** @} */

    void sltPanelActionToggled(bool fChecked);
    /** Handles the search result highlight changes. */
    void sltSearchResultHighLigting();
    /** Handles the tab change of the logviewer. */
    void sltTabIndexChange(int tabIndex);
    /* if @a isOriginal true than the result of the filtering is equal to
       the original log file for some reason. */
    void sltFilterApplied(bool isOriginal);
    /* Handles the UIVMLogPage signal which is emitted when isFiltered property
       of UIVMLogPage is changed. */
    void sltLogPageFilteredChanged(bool isFiltered);

    /** @name Slots to handle signals from settings panel
     * @{ */
        void sltShowLineNumbers(bool bShowLineNumbers);
        void sltWrapLines(bool bWrapLine);
        void sltFontSizeChanged(int fontSize);
        void sltChangeFont(QFont font);
        void sltResetSettingsToDefault();
    /** @} */

private:

    /** @name Prepare/Cleanup
      * @{ */
        /** Prepares VM Log-Viewer. */
        void prepare();
        /** Prepares actions. */
        void prepareActions();
        /** Prepares toolbar. */
        void prepareToolBar();
        /** Prepares widgets. */
        void prepareWidgets();
        /** Loads settings.  */
        void loadSettings();

        /** Saves settings.  */
        void saveSettings();
        /** Cleanups VM Log-Viewer. */
        void cleanup();
    /** @} */

    /** @name Event handling stuff.
      * @{ */
        /** Handles translation event. */
        virtual void retranslateUi() /* override */;

        /** Handles Qt show @a pEvent. */
        virtual void showEvent(QShowEvent *pEvent) /* override */;
        /** Handles Qt key-press @a pEvent. */
        virtual void keyPressEvent(QKeyEvent *pEvent) /* override */;
    /** @} */


    /** Returns the log-page from the tab with index @a pIndex. */
    QPlainTextEdit* logPage(int pIndex) const;
    /** Returns the newly created log-page using @a strPage filename. */
    void createLogPage(const QString &strFileName, const QString &strLogContent, bool noLogsToShow = false);

    const UIVMLogPage *currentLogPage() const;
    UIVMLogPage *currentLogPage();

    /** Attempts to read the logs through the API, returns true if there exists any logs, false otherwise. */
    bool createLogViewerPages();

    /** Resets document (of the curent tab) and scrollbar highligthing */
    void resetHighlighthing();

    void hidePanel(UIVMLogViewerPanel* panel);
    void showPanel(UIVMLogViewerPanel* panel);

    /** Make sure escape key is assigned to only a single widget. This is done by checking
        several things in the following order:
        - when there are no more panels visible assign it to the parent dialog
        - grab it from the dialog as soon as a panel becomes visible again
        - assigned it to the most recently "unhidden" panel */
    void manageEscapeShortCut();

    /** Holds the widget's embedding type. */
    const EmbedTo m_enmEmbedding;
    /** Hold sthe action-pool reference. */
    UIActionPool *m_pActionPool;
    /** Holds whether we should create/show toolbar. */
    const bool    m_fShowToolbar;
    /** Holds the machine instance. */
    CMachine      m_comMachine;

    /** Holds whether the dialog is polished. */
    bool m_fIsPolished;

    /** Holds container for log-pages. */
    QITabWidget        *m_pTabWidget;
    /** Stores the UIVMLogPage instances. This is modified as we add and remove new tabs
     *  to the m_pTabWidget. Index is the index of the tab widget. */
    QVector<QWidget*>  m_logPageList;

    /** @name Panel instances and a QMap for mapping panel instances to related actions.
      * @{ */
        UIVMLogViewerSearchPanel    *m_pSearchPanel;
        UIVMLogViewerFilterPanel    *m_pFilterPanel;
        UIVMLogViewerBookmarksPanel *m_pBookmarksPanel;
        UIVMLogViewerSettingsPanel  *m_pSettingsPanel;
        QMap<UIVMLogViewerPanel*, QAction*> m_panelActionMap;
        QList<UIVMLogViewerPanel*>          m_visiblePanelsList;
    /** @} */
    QVBoxLayout         *m_pMainLayout;

    /** @name Toolbar and menu variables.
      * @{ */
        UIToolBar *m_pToolBar;
    /** @} */

    /** @name Toolbar and menu variables. Cache these to restore them after refresh.
     * @{ */
        /** Showing/hiding line numbers and line wraping settings are set per
            UIVMLogViewerWidget and applies to all log pages (all tabs) */
        bool  m_bShowLineNumbers;
        bool  m_bWrapLines;
        QFont m_font;
    /** @} */
    friend class UIVMLogViewerBookmarksPanel;
    friend class UIVMLogViewerFilterPanel;
    friend class UIVMLogViewerSearchPanel;
    friend class UIVMLogViewerSettingsPanel;
    friend class UIVMLogViewerPanel;
};

#endif /* !___UIVMLogViewerWidget_h___ */
