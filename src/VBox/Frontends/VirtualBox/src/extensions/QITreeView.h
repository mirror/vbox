/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QITreeView class declaration.
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

#ifndef FEQT_INCLUDED_SRC_extensions_QITreeView_h
#define FEQT_INCLUDED_SRC_extensions_QITreeView_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QTreeView>

/* GUI includes: */
#include "UILibraryDefs.h"

/* Forward declarations: */
class QITreeViewItem;
class QITreeView;


/** OObject subclass used as item for the QITreeView. */
class SHARED_LIBRARY_STUFF QITreeViewItem : public QObject
{
    Q_OBJECT;

public:

    /** Constructs tree-view item for passed @a pParent. */
    QITreeViewItem(QITreeView *pParent)
        : m_pParentTree(pParent)
        , m_pParentItem(0)
    {}

    /** Constructs tree-view item for passed @a pParent. */
    QITreeViewItem(QITreeViewItem *pParentItem)
        : m_pParentTree(pParentItem ? pParentItem->parentTree() : 0)
        , m_pParentItem(pParentItem)
    {}

    /** Returns the parent tree-view. */
    QITreeView *parentTree() const { return m_pParentTree; }
    /** Returns the parent tree-view item. */
    QITreeViewItem *parentItem() const { return m_pParentItem; }

    /** Returns the number of children. */
    virtual int childCount() const = 0;
    /** Returns the child item with @a iIndex. */
    virtual QITreeViewItem *childItem(int iIndex) const = 0;

    /** Returns the item text. */
    virtual QString text() const = 0;

    /** Returns the rectangle. */
    QRect rect() const;

    /** Returns the model-index: */
    QModelIndex modelIndex() const;

private:

    /** Holds the parent tree reference. */
    QITreeView     *m_pParentTree;
    /** Holds the parent item reference. */
    QITreeViewItem *m_pParentItem;
};


/** QTreeView subclass extending standard functionality. */
class SHARED_LIBRARY_STUFF QITreeView : public QTreeView
{
    Q_OBJECT;

signals:

    /** Notifies listeners about index changed from @a previous to @a current.*/
    void currentItemChanged(const QModelIndex &current, const QModelIndex &previous);

    /** Notifies listeners about painting of item branches.
      * @param  pPainter  Brings the painter to draw branches.
      * @param  rect      Brings the rectangle embedding branches.
      * @param  index     Brings the index of the item for which branches will be painted. */
    void drawItemBranches(QPainter *pPainter, const QRect &rect, const QModelIndex &index) const;

    /** Notifies listeners about mouse moved @a pEvent. */
    void mouseMoved(QMouseEvent *pEvent);
    /** Notifies listeners about mouse pressed @a pEvent. */
    void mousePressed(QMouseEvent *pEvent);
    /** Notifies listeners about mouse released @a pEvent. */
    void mouseReleased(QMouseEvent *pEvent);
    /** Notifies listeners about mouse double-clicked @a pEvent. */
    void mouseDoubleClicked(QMouseEvent *pEvent);

    /** Notifies listeners about mouse drag entered @a pEvent. */
    void dragEntered(QDragEnterEvent *pEvent);
    /** Notifies listeners about mouse drag moved @a pEvent. */
    void dragMoved(QDragMoveEvent *pEvent);
    /** Notifies listeners about mouse drag left @a pEvent. */
    void dragLeft(QDragLeaveEvent *pEvent);
    /** Notifies listeners about mouse drag dropped @a pEvent. */
    void dragDropped(QDropEvent *pEvent);

public:

    /** Constructs tree-view passing @a pParent to the base-class. */
    QITreeView(QWidget *pParent = 0);

    /** Returns the number of children. */
    virtual int childCount() const { return 0; }
    /** Returns the child item with @a iIndex. */
    virtual QITreeViewItem *childItem(int /* iIndex */) const { return 0; }

    /** Returns child rectangle. */
    QRect visualRect(const QModelIndex &index) const { return QTreeView::visualRect(index); }

protected slots:

    /** Handles index changed from @a previous to @a current.*/
    void currentChanged(const QModelIndex &current, const QModelIndex &previous);

protected:

    /** Handles painting of item branches.
      * @param  pPainter  Brings the painter to draw branches.
      * @param  rect      Brings the rectangle embedding branches.
      * @param  index     Brings the index of the item for which branches will be painted. */
    virtual void drawBranches(QPainter *pPainter, const QRect &rect, const QModelIndex &index) const /* override */;

    /** Handles mouse move @a pEvent. */
    virtual void mouseMoveEvent(QMouseEvent *pEvent) /* override */;
    /** Handles mouse press @a pEvent. */
    virtual void mousePressEvent(QMouseEvent *pEvent) /* override */;
    /** Handles mouse release @a pEvent. */
    virtual void mouseReleaseEvent(QMouseEvent *pEvent) /* override */;
    /** Handles mouse double-click @a pEvent. */
    virtual void mouseDoubleClickEvent(QMouseEvent *pEvent) /* override */;

    /** Handles mouse drag enter @a pEvent. */
    virtual void dragEnterEvent(QDragEnterEvent *pEvent) /* override */;
    /** Handles mouse drag move @a pEvent. */
    virtual void dragMoveEvent(QDragMoveEvent *pEvent) /* override */;
    /** Handles mouse drag leave @a pEvent. */
    virtual void dragLeaveEvent(QDragLeaveEvent *pEvent) /* override */;
    /** Handles mouse drop @a pEvent. */
    virtual void dropEvent(QDropEvent *pEvent) /* override */;

private:

    /** Prepares all. */
    void prepare();
};


#endif /* !FEQT_INCLUDED_SRC_extensions_QITreeView_h */
