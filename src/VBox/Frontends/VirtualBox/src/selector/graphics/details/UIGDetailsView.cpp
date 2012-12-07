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
    , m_iMinimumWidthHint(0)
    , m_iMinimumHeightHint(0)
{
    /* Prepare palette: */
    preparePalette();

    /* Setup frame: */
    setFrameShape(QFrame::NoFrame);
    setFrameShadow(QFrame::Plain);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);

    /* Setup scroll-bars policy: */
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    /* Update scene-rect: */
    updateSceneRect();
}

void UIGDetailsView::sltMinimumWidthHintChanged(int iMinimumWidthHint)
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

void UIGDetailsView::sltMinimumHeightHintChanged(int iMinimumHeightHint)
{
    /* Is there something changed? */
    if (m_iMinimumHeightHint == iMinimumHeightHint)
        return;

    /* Remember new value: */
    m_iMinimumHeightHint = iMinimumHeightHint;

    /* Update scene-rect: */
    updateSceneRect();
}

void UIGDetailsView::preparePalette()
{
    /* Setup palette: */
    QPalette pal = qApp->palette();
    pal.setColor(QPalette::Base, pal.color(QPalette::Active, QPalette::Window));
    setPalette(pal);
}

void UIGDetailsView::resizeEvent(QResizeEvent*)
{
    /* Update scene-rect: */
    updateSceneRect();
    /* Notify listeners: */
    emit sigResized();
}

void UIGDetailsView::updateSceneRect()
{
    setSceneRect(0, 0, m_iMinimumWidthHint, m_iMinimumHeightHint);
}

