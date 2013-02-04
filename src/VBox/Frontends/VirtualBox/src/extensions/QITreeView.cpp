/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QITreeView class implementation
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes */
#include <QMouseEvent>
#include <QPainter>

/* Local includes */
#include "QITreeView.h"

QITreeView::QITreeView (QWidget *aParent)
    : QTreeView (aParent)
{
    setHeaderHidden (true);
    setRootIsDecorated (false);
}

void QITreeView::currentChanged (const QModelIndex &aCurrent, const QModelIndex &aPrevious)
{
    emit currentItemChanged (aCurrent, aPrevious);
    QTreeView::currentChanged (aCurrent, aPrevious);
}

void QITreeView::drawBranches (QPainter *aPainter, const QRect &aRect, const QModelIndex &aIndex) const
{
    emit drawItemBranches (aPainter, aRect, aIndex);
    QTreeView::drawBranches (aPainter, aRect, aIndex);
}

void QITreeView::mouseMoveEvent (QMouseEvent *aEvent)
{
    aEvent->setAccepted (false);
    emit mouseMoved (aEvent);
    if (!aEvent->isAccepted())
        QTreeView::mouseMoveEvent (aEvent);
}

void QITreeView::mousePressEvent (QMouseEvent *aEvent)
{
    aEvent->setAccepted (false);
    emit mousePressed (aEvent);
    if (!aEvent->isAccepted())
        QTreeView::mousePressEvent (aEvent);
}

void QITreeView::mouseDoubleClickEvent (QMouseEvent *aEvent)
{
    aEvent->setAccepted (false);
    emit mouseDoubleClicked (aEvent);
    if (!aEvent->isAccepted())
        QTreeView::mouseDoubleClickEvent (aEvent);
}

