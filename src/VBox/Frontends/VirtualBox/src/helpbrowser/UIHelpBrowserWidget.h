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
class QHelpEngine;
class QPlainTextEdit;
class UIHelpBrowserViewer;
class QHBoxLayout;
class QITabWidget;
class QIToolBar;
class UIActionPool;
class UIDialogPanel;
class QSplitter;

/** QWidget extension providing GUI for VirtualBox LogViewer. It
 *  encapsulates log pages, toolbar, a tab widget and manages
 *  interaction between these classes. */
class SHARED_LIBRARY_STUFF UIHelpBrowserWidget  : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigSetCloseButtonShortCut(QKeySequence);

public:

    /** Constructs the VM Log-Viewer by passing @a pParent to QWidget base-class constructor.
      * @param  enmEmbedding  Brings the type of widget embedding.
      * @param  fShowToolbar  Brings whether we should create/show toolbar.*/
    UIHelpBrowserWidget(EmbedTo enmEmbedding, const QString &strHelpFilePath,
                        bool fShowToolbar = true, QWidget *pParent = 0);
    /** Destructs the VM Log-Viewer. */
    ~UIHelpBrowserWidget();

    /** Returns the menu. */
    QMenu *menu() const;

#ifdef VBOX_WS_MAC
    /** Returns the toolbar. */
    QIToolBar *toolbar() const { return m_pToolBar; }
#endif


protected:

    /** Returns whether the window should be maximized when geometry being restored. */
    virtual bool shouldBeMaximized() const /* override */;

private slots:

    void sltHandleHelpEngineSetupFinished();
    void sltHandleContentWidgetItemClicked(const QModelIndex &index);

private:

    void prepare();
    void prepareActions();
    void prepareWidgets();
    void prepareToolBar();
    void loadOptions();

    void saveOptions();
    void cleanup();

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
    QHBoxLayout         *m_pMainLayout;
    QITabWidget *m_pTabWidget;
    /** @name Toolbar and menu variables.
      * @{ */
        QIToolBar *m_pToolBar;
    /** @} */

    QString       m_strHelpFilePath;
#ifdef RT_OS_LINUX
    QHelpEngine  *m_pHelpEngine;
#endif
    UIHelpBrowserViewer *m_pTextBrowser;
    QSplitter           *m_pSplitter;
};

#endif /* !FEQT_INCLUDED_SRC_helpbrowser_UIHelpBrowserWidget_h */
