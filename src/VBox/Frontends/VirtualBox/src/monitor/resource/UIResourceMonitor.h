/* $Id$ */
/** @file
 * VBox Qt GUI - UIResourceMonitor class declaration.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_monitor_resource_UIResourceMonitor_h
#define FEQT_INCLUDED_SRC_monitor_resource_UIResourceMonitor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QMainWindow>

/* GUI includes: */
#include "QIManagerDialog.h"
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QAbstractButton;
class QFrame;
class QLabel;
class QTableView;
class QTreeWidgetItem;
class QIDialogButtonBox;
class UIActionPool;
class UIToolBar;
class UIResourceMonitorProxyModel;
class UIResourceMonitorModel;
class UIVMResourceMonitorHostStats;
class UIVMResourceMonitorHostStatsWidget;
class UIVMResourceMonitorTableView;

/** QWidget extension to display a Linux top like utility that sort running vm wrt. resource allocations. */
class UIResourceMonitorWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:


public:

    UIResourceMonitorWidget(EmbedTo enmEmbedding, UIActionPool *pActionPool,
                               bool fShowToolbar = true, QWidget *pParent = 0);
    ~UIResourceMonitorWidget();
    QMenu *columnVisiblityToggleMenu() const;
    QMenu *menu() const;

    bool isCurrentTool() const;
    void setIsCurrentTool(bool fIsCurrentTool);

#ifdef VBOX_WS_MAC
    UIToolBar *toolbar() const { return m_pToolBar; }
#endif

protected:

    /** @name Event-handling stuff.
      * @{ */
        virtual void retranslateUi() /* override */;
        virtual void resizeEvent(QResizeEvent *pEvent) /* override */;
        virtual void showEvent(QShowEvent *pEvent) /* override */;
        virtual void paintEvent(QPaintEvent *pEvent) /* override */;
    /** @} */

private slots:

    void sltHandleDataUpdate();
    void sltToggleColumnSelectionMenu(bool fChecked);
    void sltHandleColumnAction(bool fChecked);
    void sltHandleHostStatsUpdate(const UIVMResourceMonitorHostStats &stats);

private:

    void setColumnVisible(int iColumnId, bool fVisible);
    bool columnVisible(int iColumnId) const;
    void updateModelColumVisibilityCache();
    void computeMinimumColumnWidths();

    /** @name Prepare/cleanup cascade.
      * @{ */
        void prepare();
        void prepareWidgets();
        void prepareHostStatsWidgets();
        void prepareToolBar();
        void prepareActions();
        void loadSettings();
        void loadHiddenColumnList();
        void saveSettings();
    /** @} */

    /** @name General variables.
      * @{ */
        const EmbedTo m_enmEmbedding;
        UIActionPool *m_pActionPool;
        const bool    m_fShowToolbar;
    /** @} */

    /** @name Misc members.
      * @{ */
        UIToolBar *m_pToolBar;
        UIVMResourceMonitorTableView       *m_pTableView;
        UIResourceMonitorProxyModel        *m_pProxyModel;
        UIResourceMonitorModel             *m_pModel;
        QMenu                              *m_pColumnVisibilityToggleMenu;
        /* The key is the column id (VMResourceMonitorColumn) and value is column title. */
        QMap<int, QString>                  m_columnCaptions;
        /* The key is the column id (VMResourceMonitorColumn) and value is true if the column is visible. */
        QMap<int, bool>                     m_columnVisible;
        UIVMResourceMonitorHostStatsWidget *m_pHostStatsWidget;


    /** @} */
    /** Indicates if this widget's host tool is current tool. */
    bool    m_fIsCurrentTool;
};

class UIResourceMonitorFactory : public QIManagerDialogFactory
{
public:

    UIResourceMonitorFactory(UIActionPool *pActionPool = 0);

protected:

    virtual void create(QIManagerDialog *&pDialog, QWidget *pCenterWidget) /* override */;
    UIActionPool *m_pActionPool;
};

class UIResourceMonitor : public QIWithRetranslateUI<QIManagerDialog>
{
    Q_OBJECT;

private:

    UIResourceMonitor(QWidget *pCenterWidget, UIActionPool *pActionPool);

    virtual void retranslateUi() /* override */;

    /** @name Prepare/cleanup cascade.
      * @{ */
        virtual void configure() /* override */;
        virtual void configureCentralWidget() /* override */;
        virtual void configureButtonBox() /* override */;
        virtual void finalize() /* override */;
    /** @} */

    /** @name Widget stuff.
      * @{ */
        virtual UIResourceMonitorWidget *widget() /* override */;
    /** @} */

    /** @name Action related variables.
      * @{ */
        UIActionPool *m_pActionPool;
    /** @} */

    friend class UIResourceMonitorFactory;
};

#endif /* !FEQT_INCLUDED_SRC_monitor_resource_UIResourceMonitor_h */
