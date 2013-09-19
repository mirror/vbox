/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIStateIndicator class implementation
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "QIStateIndicator.h"

/* Qt includes */
#include <QPainter>
#ifdef Q_WS_MAC
# include <QContextMenuEvent>
#endif

QIStateIndicator::QIStateIndicator(QWidget *pParent /* = 0*/)
  : QIWithRetranslateUI<QFrame>(pParent)
  , mState(0)
  , mSize(0, 0)
{
    setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
}

QIStateIndicator::~QIStateIndicator()
{
    qDeleteAll (mStateIcons);
}

QSize QIStateIndicator::sizeHint() const
{
    return mSize;
}

QPixmap QIStateIndicator::stateIcon (int aState) const
{
    Icon *icon = mStateIcons [aState];
    return icon ? icon->pixmap : QPixmap();
}

/**
 *  Sets an icon for the specified state. The first icon set by this method
 *  defines the preferred size of this indicator. All other icons will be
 *  scaled to fit this size.
 *
 *  @note If this widget is constructed with the WNoAutoErase flag, then all
 *  transparent areas of the new state icon are filled with the widget
 *  background color or pixmap (as taken from the widget palette), to provide
 *  flicker free state redraws in one single operation (which is useful for
 *  indicators that frequently change their state).
 */
void QIStateIndicator::setStateIcon (int aState, const QPixmap &aPixmap)
{
    /* Here we just set the original pixmap. All actual work from the @note
     * above takes place in #drawContents(). */
    mStateIcons.insert (aState, new Icon (aPixmap));

    if (mSize.isNull())
        mSize = aPixmap.size();
}

void QIStateIndicator::setState (int aState)
{
    mState = aState;
    repaint();
}

void QIStateIndicator::paintEvent (QPaintEvent * /* aEv */)
{
    QPainter painter (this);
    drawContents (&painter);
}

void QIStateIndicator::drawContents (QPainter *aPainter)
{
    Icon *icon = mStateIcons [mState];
    if (icon)
        aPainter->drawPixmap (contentsRect(), icon->pixmap);
}

#ifdef Q_WS_MAC
/**
 * Make the left button also show the context menu to make things
 * simpler for users with single mouse button mice (laptops++).
 */
void QIStateIndicator::mousePressEvent (QMouseEvent *aEv)
{
    /* Do this for the left mouse button event only, cause in the case of the
     * right mouse button it could happen that the context menu event is
     * triggered twice. Also this isn't necessary for the middle mouse button
     * which would be some kind of overstated. */
    if (aEv->button() == Qt::LeftButton)
    {
        QContextMenuEvent qme (QContextMenuEvent::Mouse, aEv->pos(), aEv->globalPos());
        emit contextMenuRequested (this, &qme);
        if (qme.isAccepted())
            aEv->accept();
        else
            QFrame::mousePressEvent (aEv);
    }
    else
        QFrame::mousePressEvent (aEv);
}
#endif /* Q_WS_MAC */

void QIStateIndicator::mouseDoubleClickEvent (QMouseEvent * e)
{
    emit mouseDoubleClicked (this, e);
}

void QIStateIndicator::contextMenuEvent (QContextMenuEvent * e)
{
    emit contextMenuRequested (this, e);
}

