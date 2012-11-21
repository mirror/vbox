/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGDetailsView class implementation
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
#include <QApplication>
#include <QScrollBar>

/* GUI includes: */
#include "UIGDetailsView.h"
#include "UIGChooserModel.h"

UIGDetailsView::UIGDetailsView(QWidget *pParent)
    : QGraphicsView(pParent)
{
    /* Setup palette: */
    QPalette pal = qApp->palette();
    pal.setColor(QPalette::Base, pal.color(QPalette::Active, QPalette::Window));
    setPalette(pal);

    /* Scrollbars policy: */
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    /* Update scene-rect: */
    updateSceneRect();
}

void UIGDetailsView::sltHandleRootItemResized(const QSizeF &size, int iMinimumWidth)
{
    /* Update scene-rect: */
    updateSceneRect(size);

    /* Set minimum width: */
    setMinimumWidth(2 * frameWidth() + iMinimumWidth +
                    verticalScrollBar()->sizeHint().width());
}

void UIGDetailsView::resizeEvent(QResizeEvent*)
{
    /* Update scene-rect: */
    updateSceneRect();
    /* Notify listeners: */
    emit sigResized();
}

void UIGDetailsView::updateSceneRect(const QSizeF &sizeHint /* = QSizeF() */)
{
    QPointF topLeft = QPointF(0, 0);
    QSizeF rectSize = viewport()->size();
    if (!sizeHint.isNull())
        rectSize = rectSize.expandedTo(sizeHint);
    setSceneRect(QRectF(topLeft, rectSize));
}

