/* $Id: $ */
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

#ifndef __QITreeView_h__
#define __QITreeView_h__

/* Qt includes: */
#include <QTreeView>


/** QTreeView subclass extending standard functionality. */
class QITreeView: public QTreeView
{
    Q_OBJECT;

public:

    /** Constructs table-view passing @a aParent to the base-class. */
    QITreeView (QWidget *aParent = 0);

signals:

    /** Notifies listeners about index changed from @a aPrevious to @a aCurrent.*/
    void currentItemChanged (const QModelIndex &aCurrent, const QModelIndex &aPrevious);

    /** Notifies listeners about painting of item branches.
      * @param  aPainter  Brings the painter to draw branches.
      * @param  aRect     Brings the rectangle embedding branches.
      * @param  aIndex    Brings the index of the item for which branches will be painted. */
    void drawItemBranches (QPainter *aPainter, const QRect &aRect, const QModelIndex &aIndex) const;

    /** Notifies listeners about mouse moved @a aEvent. */
    void mouseMoved (QMouseEvent *aEvent);
    /** Notifies listeners about mouse pressed @a aEvent. */
    void mousePressed (QMouseEvent *aEvent);
    /** Notifies listeners about mouse double-clicked @a aEvent. */
    void mouseDoubleClicked (QMouseEvent *aEvent);

protected slots:

    /** Handles index changed from @a aPrevious to @a aCurrent.*/
    void currentChanged (const QModelIndex &aCurrent, const QModelIndex &aPrevious);

protected:

    /** Handles painting of item branches.
      * @param  aPainter  Brings the painter to draw branches.
      * @param  aRect     Brings the rectangle embedding branches.
      * @param  aIndex    Brings the index of the item for which branches will be painted. */
    void drawBranches (QPainter *aPainter, const QRect &aRect, const QModelIndex &aIndex) const;

    /** Handles mouse move @a aEvent. */
    void mouseMoveEvent (QMouseEvent *aEvent);
    /** Handles mouse press @a aEvent. */
    void mousePressEvent (QMouseEvent *aEvent);
    /** Handles mouse double-click @a aEvent. */
    void mouseDoubleClickEvent (QMouseEvent *aEvent);
};

#endif /* !__QITreeView_h__ */

