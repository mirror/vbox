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
class UIVMLogViewerFilterPanel;
class UIVMLogViewerSearchPanel;

/* Type definitions: */
typedef QPair<QString, QPlainTextEdit*> LogPage;
typedef QList<LogPage> LogBook;
typedef QMap<QPlainTextEdit*, QString> VMLogMap;

/** QIMainWindow extension
  * providing GUI with VirtualBox LogViewer. */
class UIVMLogViewerWidget  : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:
    /** Constructs the VM Log-Viewer by passing @a pParent to QWidget base-class constructor.
      * @param  machine  Specifies the machine for which VM Log-Viewer is requested. */
    UIVMLogViewerWidget(EmbedTo enmEmbedding, QWidget *pParent, const CMachine &machine);
    /** Destructs the VM Log-Viewer. */
    ~UIVMLogViewerWidget();
    /* Returns the width of the current log page. return 0 if there is no current log page: */
    int defaultLogPageWidth() const;

    /** Returns the menu. */
    QMenu *menu() const { return m_pMenu; }

#ifdef VBOX_WS_MAC
    /** Returns the toolbar. */
    UIToolBar *toolbar() const { return m_pToolBar; }
#endif

protected:


    /** Returns whether the window should be maximized when geometry being restored. */
    virtual bool shouldBeMaximized() const /* override */;

private slots:

    /** Handles search action triggering. */
    void sltFind();
    /** Handles refresh action triggering. */
    void sltRefresh();
    /** Handles save action triggering. */
    void sltSave();
    /** Handles filter action triggering. */
    void sltFilter();

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

    /** Returns the current log-page. */
    QPlainTextEdit* currentLogPage() const;
    /** Returns the newly created log-page using @a strPage filename. */
    QPlainTextEdit* createLogPage(const QString &strPage);
    /** Returns the content of current log-page. */
    const QString& currentLog();

    /** Holds whether the dialog is polished. */
    bool m_fIsPolished;

    /** Holds the machine instance. */
    CMachine m_comMachine;

    /** Holds container for log-pages. */
    QITabWidget *m_pViewerContainer;

    /** Holds the instance of search-panel. */
    UIVMLogViewerSearchPanel *m_pSearchPanel;

    /** Holds the list of log-pages. */
    LogBook m_book;

    /** Holds the instance of filter panel. */
    UIVMLogViewerFilterPanel *m_pFilterPanel;

    /** Holds the list of log-content. */
    VMLogMap m_logMap;

    QVBoxLayout      *m_pMainLayout;

    /** Holds the widget embedding type. */
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
        /** Holds the menu object instance. */
        QMenu     *m_pMenu;
    /** @} */

    friend class UIVMLogViewerSearchPanel;
    friend class UIVMLogViewerFilterPanel;
};

#endif /* !___UIVMLogViewerWidget_h___ */

