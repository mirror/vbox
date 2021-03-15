/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMActivityOverviewWidget class declaration.
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

#ifndef FEQT_INCLUDED_SRC_activity_overview_UIVMActivityOverviewWidget_h
#define FEQT_INCLUDED_SRC_activity_overview_UIVMActivityOverviewWidget_h
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
class QItemSelection;
class QLabel;
class QTableView;
class QTreeWidgetItem;
class QIDialogButtonBox;
class UIActionPool;
class QIToolBar;
class UIActivityOverviewProxyModel;
class UIActivityOverviewModel;
class UIVMActivityOverviewHostStats;
class UIVMActivityOverviewHostStatsWidget;
class UIVMActivityOverviewTableView;

/** QWidget extension to display a Linux top like utility that sort running vm wrt. resource allocations. */
class UIVMActivityOverviewWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigSwitchToMachinePerformancePane(const QUuid &uMachineId);

public:

    UIVMActivityOverviewWidget(EmbedTo enmEmbedding, UIActionPool *pActionPool,
                               bool fShowToolbar = true, QWidget *pParent = 0);
    ~UIVMActivityOverviewWidget();
    QMenu *columnVisiblityToggleMenu() const;
    QMenu *menu() const;

    bool isCurrentTool() const;
    void setIsCurrentTool(bool fIsCurrentTool);

#ifdef VBOX_WS_MAC
    QIToolBar *toolbar() const { return m_pToolBar; }
#endif

protected:

    /** @name Event-handling stuff.
      * @{ */
        virtual void retranslateUi() /* override */;
        virtual void showEvent(QShowEvent *pEvent) /* override */;
    /** @} */

private slots:

    void sltHandleDataUpdate();
    void sltToggleColumnSelectionMenu(bool fChecked);
    void sltHandleColumnAction(bool fChecked);
    void sltHandleHostStatsUpdate(const UIVMActivityOverviewHostStats &stats);
    void sltHandleTableContextMenuRequest(const QPoint &pos);
    void sltHandleShowVMActivityMonitor();
    void sltHandleTableSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

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
        void updateColumnsMenu();
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
        QIToolBar *m_pToolBar;
        UIVMActivityOverviewTableView       *m_pTableView;
        UIActivityOverviewProxyModel        *m_pProxyModel;
        UIActivityOverviewModel             *m_pModel;
        QMenu                              *m_pColumnVisibilityToggleMenu;
        /* The key is the column id (VMActivityOverviewColumn) and value is column title. */
        QMap<int, QString>                  m_columnTitles;
        /* The key is the column id (VMActivityOverviewColumn) and value is true if the column is visible. */
        QMap<int, bool>                     m_columnVisible;
        UIVMActivityOverviewHostStatsWidget *m_pHostStatsWidget;
        QAction                            *m_pVMActivityMonitorAction;
    /** @} */
    /** Indicates if this widget's host tool is current tool. */
    bool    m_fIsCurrentTool;
    int     m_iSortIndicatorWidth;
};

class UIVMActivityOverviewFactory : public QIManagerDialogFactory
{
public:

    UIVMActivityOverviewFactory(UIActionPool *pActionPool = 0);

protected:

    virtual void create(QIManagerDialog *&pDialog, QWidget *pCenterWidget) /* override */;
    UIActionPool *m_pActionPool;
};

class UIVMActivityOverviewDialog : public QIWithRetranslateUI<QIManagerDialog>
{
    Q_OBJECT;

private:

    UIVMActivityOverviewDialog(QWidget *pCenterWidget, UIActionPool *pActionPool);

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
        virtual UIVMActivityOverviewWidget *widget() /* override */;
    /** @} */

    /** @name Action related variables.
      * @{ */
        UIActionPool *m_pActionPool;
    /** @} */

    friend class UIVMActivityOverviewFactory;
};

#endif /* !FEQT_INCLUDED_SRC_activity_overview_UIVMActivityOverviewWidget_h */
