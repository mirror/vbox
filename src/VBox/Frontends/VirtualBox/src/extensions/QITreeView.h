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

/* Global includes */
#include <QTreeView>

/*
 * QITreeView class which extends standard QITreeView's functionality.
 */
class QITreeView: public QTreeView
{
    Q_OBJECT;

public:

    QITreeView (QWidget *aParent = 0);

signals:

    void currentItemChanged (const QModelIndex &aCurrent, const QModelIndex &aPrevious);
    void drawItemBranches (QPainter *aPainter, const QRect &aRect, const QModelIndex &aIndex) const;
    void mouseMoved (QMouseEvent *aEvent);
    void mousePressed (QMouseEvent *aEvent);
    void mouseDoubleClicked (QMouseEvent *aEvent);

protected slots:

    void currentChanged (const QModelIndex &aCurrent, const QModelIndex &aPrevious);

protected:

    void drawBranches (QPainter *aPainter, const QRect &aRect, const QModelIndex &aIndex) const;
    void mouseMoveEvent (QMouseEvent *aEvent);
    void mousePressEvent (QMouseEvent *aEvent);
    void mouseDoubleClickEvent (QMouseEvent *aEvent);
};

#endif /* __QITreeView_h__ */

