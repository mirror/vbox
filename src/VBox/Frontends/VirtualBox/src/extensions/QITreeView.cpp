/* $Id$ */
/** @file
 * VBox Qt GUI - VirtualBox Qt extensions: QITreeView class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QMouseEvent>
# include <QPainter>

/* GUI includes: */
# include "QITreeView.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


QITreeView::QITreeView (QWidget *aParent)
    : QTreeView (aParent)
{
    /* Mark header hidden: */
    setHeaderHidden (true);
    /* Mark root hidden: */
    setRootIsDecorated (false);
}

void QITreeView::currentChanged (const QModelIndex &aCurrent, const QModelIndex &aPrevious)
{
    /* Notify listeners about it: */
    emit currentItemChanged (aCurrent, aPrevious);
    /* Call to base-class: */
    QTreeView::currentChanged (aCurrent, aPrevious);
}

void QITreeView::drawBranches (QPainter *aPainter, const QRect &aRect, const QModelIndex &aIndex) const
{
    /* Notify listeners about it: */
    emit drawItemBranches (aPainter, aRect, aIndex);
    /* Call to base-class: */
    QTreeView::drawBranches (aPainter, aRect, aIndex);
}

void QITreeView::mouseMoveEvent (QMouseEvent *aEvent)
{
    /* Reject event initially: */
    aEvent->setAccepted (false);
    /* Notify listeners about event allowing them to handle it: */
    emit mouseMoved (aEvent);
    /* Call to base-class only if event was not yet accepted: */
    if (!aEvent->isAccepted())
        QTreeView::mouseMoveEvent (aEvent);
}

void QITreeView::mousePressEvent (QMouseEvent *aEvent)
{
    /* Reject event initially: */
    aEvent->setAccepted (false);
    /* Notify listeners about event allowing them to handle it: */
    emit mousePressed (aEvent);
    /* Call to base-class only if event was not yet accepted: */
    if (!aEvent->isAccepted())
        QTreeView::mousePressEvent (aEvent);
}

void QITreeView::mouseDoubleClickEvent (QMouseEvent *aEvent)
{
    /* Reject event initially: */
    aEvent->setAccepted (false);
    /* Notify listeners about event allowing them to handle it: */
    emit mouseDoubleClicked (aEvent);
    /* Call to base-class only if event was not yet accepted: */
    if (!aEvent->isAccepted())
        QTreeView::mouseDoubleClickEvent (aEvent);
}

