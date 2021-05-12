/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMActivityListWidget class declaration.
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

#ifndef FEQT_INCLUDED_SRC_activity_overview_UIVMActivityListWidget_h
#define FEQT_INCLUDED_SRC_activity_overview_UIVMActivityListWidget_h
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
class UIVMActivityListWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:


public:

    UIVMActivityListWidget(EmbedTo enmEmbedding, UIActionPool *pActionPool,
                               bool fShowToolbar = true, QWidget *pParent = 0);
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


private:


    /** @name Prepare/cleanup cascade.
      * @{ */
        void prepare();
        void prepareWidgets();
        void prepareHostStatsWidgets();
        void prepareToolBar();
        void prepareActions();
        void updateColumnsMenu();
        void loadSettings();
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
        QMenu                              *m_pColumnVisibilityToggleMenu;
    /** @} */
    /** Indicates if this widget's host tool is current tool. */
    bool    m_fIsCurrentTool;
};


#endif /* !FEQT_INCLUDED_SRC_activity_overview_UIVMActivityListWidget_h */
