/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGChooserView class implementation
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QScrollBar>

/* GUI includes: */
#include "UIGChooserView.h"
#include "UIGChooserItem.h"

UIGChooserView::UIGChooserView(QWidget *pParent)
    : QGraphicsView(pParent)
{
    /* Setup frame: */
    setFrameShape(QFrame::NoFrame);
    setFrameShadow(QFrame::Plain);

    /* Setup scroll-bars policy: */
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    /* Update scene-rect: */
    updateSceneRect();
}

void UIGChooserView::sltHandleRootItemResized(const QSizeF &size)
{
    /* Update scene-rect: */
    updateSceneRect(size);
}

void UIGChooserView::sltHandleRootItemMinimumWidthHintChanged(int iRootItemMinimumWidthHint)
{
    /* Set minimum view width according to root-item minimum-width-hint: */
    setMinimumWidth(2 * frameWidth() + iRootItemMinimumWidthHint + verticalScrollBar()->sizeHint().width());
}

void UIGChooserView::sltFocusChanged(UIGChooserItem *pFocusItem)
{
    /* Make sure focus-item set: */
    if (!pFocusItem)
        return;

    QSize viewSize = viewport()->size();
    QRectF geo = pFocusItem->geometry();
    geo &= QRectF(geo.topLeft(), viewSize);
    ensureVisible(geo, 0, 0);
}

void UIGChooserView::resizeEvent(QResizeEvent*)
{
    /* Update scene-rect: */
    updateSceneRect();
    /* Notify listeners: */
    emit sigResized();
}

void UIGChooserView::updateSceneRect(const QSizeF &sizeHint /* = QSizeF() */)
{
    QPointF topLeft = QPointF(0, 0);
    QSizeF rectSize = viewport()->size();
    if (!sizeHint.isNull())
        rectSize = rectSize.expandedTo(sizeHint);
    setSceneRect(QRectF(topLeft, rectSize));
}

