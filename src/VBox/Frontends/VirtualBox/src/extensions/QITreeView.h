/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QITreeView class declaration.
 */

/*
 * Copyright (C) 2009-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___QITreeView_h___
#define ___QITreeView_h___

/* Qt includes: */
#include <QTreeView>


/** QTreeView subclass extending standard functionality. */
class QITreeView: public QTreeView
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
    /** Notifies listeners about mouse double-clicked @a pEvent. */
    void mouseDoubleClicked(QMouseEvent *pEvent);

public:

    /** Constructs table-view passing @a pParent to the base-class. */
    QITreeView(QWidget *pParent = 0);

protected slots:

    /** Handles index changed from @a previous to @a current.*/
    void currentChanged(const QModelIndex &current, const QModelIndex &previous);

protected:

    /** Handles painting of item branches.
      * @param  pPainter  Brings the painter to draw branches.
      * @param  rect      Brings the rectangle embedding branches.
      * @param  index     Brings the index of the item for which branches will be painted. */
    void drawBranches(QPainter *pPainter, const QRect &rect, const QModelIndex &index) const;

    /** Handles mouse move @a pEvent. */
    void mouseMoveEvent(QMouseEvent *pEvent);
    /** Handles mouse press @a pEvent. */
    void mousePressEvent(QMouseEvent *pEvent);
    /** Handles mouse double-click @a pEvent. */
    void mouseDoubleClickEvent(QMouseEvent *pEvent);
};

#endif /* !___QITreeView_h___ */

