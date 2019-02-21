/* $Id$ */
/** @file
 * VBox Qt GUI - UIVisoBrowserBase class declaration.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_medium_viso_UIVisoBrowserBase_h
#define FEQT_INCLUDED_SRC_medium_viso_UIVisoBrowserBase_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QModelIndex>
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QItemSelection;
class QGridLayout;
class QLabel;
class QSplitter;
class QVBoxLayout;
class QTableView;
class QTreeView;
class UILocationSelector;

/** An abstract QWidget extension hosting a tree and table view. */
class UIVisoBrowserBase : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigTreeViewVisibilityChanged(bool fVisible);
    void sigCreateFileTableViewContextMenu(QWidget *pMenuRequester, const QPoint &point);

public:

    UIVisoBrowserBase(QWidget *pParent = 0);
    ~UIVisoBrowserBase();
    virtual void showHideHiddenObjects(bool bShow) = 0;
    /* Returns true if tree view is currently visible: */
    bool isTreeViewVisible() const;
    void hideTreeView();

public slots:

    void sltHandleTableViewItemDoubleClick(const QModelIndex &index);

protected:

    void prepareObjects();
    void prepareConnections();
    void updateLocationSelectorText(const QString &strText);

    virtual void tableViewItemDoubleClick(const QModelIndex &index) = 0;
    virtual void treeSelectionChanged(const QModelIndex &selectedTreeIndex) = 0;
    virtual void setTableRootIndex(QModelIndex index = QModelIndex()) = 0;
    virtual void setTreeCurrentIndex(QModelIndex index = QModelIndex()) = 0;

    virtual void resizeEvent(QResizeEvent *pEvent) /* override */;
    virtual bool eventFilter(QObject *pObj, QEvent *pEvent) /* override */;
    virtual void keyPressEvent(QKeyEvent *pEvent) /* override */;

    QTreeView          *m_pTreeView;
    QGridLayout        *m_pMainLayout;

protected slots:

    void sltFileTableViewContextMenu(const QPoint &point);

private slots:

    void sltHandleTreeSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void sltHandleTreeItemClicked(const QModelIndex &modelIndex);
    void sltExpandCollapseTreeView();

private:

    void updateTreeViewGeometry(bool fShow);
    UILocationSelector    *m_pLocationSelector;
};

#endif /* !FEQT_INCLUDED_SRC_medium_viso_UIVisoBrowserBase_h */
