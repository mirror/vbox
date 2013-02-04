/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGChooserView class implementation
 */

/*
 * Copyright (C) 2013 Oracle Corporation
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
    , m_iMinimumWidthHint(0)
    , m_iMinimumHeightHint(0)
{
    /* Setup frame: */
    setFrameShape(QFrame::NoFrame);
    setFrameShadow(QFrame::Plain);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);

    /* Setup scroll-bars policy: */
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    /* Update scene-rect: */
    updateSceneRect();
}

void UIGChooserView::sltMinimumWidthHintChanged(int iMinimumWidthHint)
{
    /* Is there something changed? */
    if (m_iMinimumWidthHint == iMinimumWidthHint)
        return;

    /* Remember new value: */
    m_iMinimumWidthHint = iMinimumWidthHint;

    /* Set minimum view width according passed width-hint: */
    setMinimumWidth(2 * frameWidth() + iMinimumWidthHint + verticalScrollBar()->sizeHint().width());

    /* Update scene-rect: */
    updateSceneRect();
}

void UIGChooserView::sltMinimumHeightHintChanged(int iMinimumHeightHint)
{
    /* Is there something changed? */
    if (m_iMinimumHeightHint == iMinimumHeightHint)
        return;

    /* Remember new value: */
    m_iMinimumHeightHint = iMinimumHeightHint;

    /* Update scene-rect: */
    updateSceneRect();
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
    /* Notify listeners: */
    emit sigResized();
}

void UIGChooserView::updateSceneRect()
{
    setSceneRect(0, 0, m_iMinimumWidthHint, m_iMinimumHeightHint);
}

