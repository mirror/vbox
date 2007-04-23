/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * InnoTek Qt extensions: QIStateIndicator class implementation
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#include "QIStateIndicator.h"

#include <qpainter.h>

/** @clas QIStateIndicator
 *
 *  The QIStateIndicator class is a simple class that can visually indicate
 *  the state of some thing, as described by the state property.
 */

/**
 *  Constructs a new QIStateIndicator instance. This instance is useless
 *  until icons are specified for necessary states.
 *
 *  @param state
 *      the initial indicator state
 */
QIStateIndicator::QIStateIndicator (
    int state,
    QWidget *parent, const char *name, WFlags f
) :
    QFrame (parent, name, f | WStaticContents | WMouseNoMask)
{
    st = state;
    sz = QSize (0, 0);
    state_icons.setAutoDelete (true);

    setSizePolicy (QSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed));
}

QSize QIStateIndicator::sizeHint() const
{
    return sz;
}

QPixmap QIStateIndicator::stateIcon (int s) const
{
    QPixmap *pm = state_icons [s];
    return pm ? QPixmap (*pm) : QPixmap();
}

/**
 *  Sets an icon for the specified state. The first icon set by this method
 *  defines the preferred size of this indicator. All other icons will be
 *  scaled to fit this size.
 *
 *  @note
 *      If this widget was constructed with the WNoAutoErase flag, then all
 *      transparent areas of the new state icon are filled with the widget
 *      background color (taken from its palette) to provide flicker free
 *      state redraws (which is useful for indicators that frequently
 *      change their state).
 */
void QIStateIndicator::setStateIcon (int s, const QPixmap &pm)
{
    QPixmap *icon;
    if (testWFlags (WNoAutoErase)) {
        icon = new QPixmap (pm.size());
#ifdef Q_WS_MAC /* the background color isn't the pattern used for the console window frame */
        const QPixmap *back = backgroundPixmap();
        if (back) /* Is ClearROP right? I've no clue about raster operations... */
            bitBlt (icon, 0, 0, back, 0, 0, pm.width(), pm.height(), ClearROP, false); 
        else
#endif 
            icon->fill (paletteBackgroundColor());
        bitBlt (icon, 0, 0, &pm, 0, 0, pm.width(), pm.height(), CopyROP, false);
    } else {
        icon = new QPixmap (pm);
    }

    state_icons.insert (s, icon);
    if (sz.isNull()) {
        sz = pm.size();
    }
}

void QIStateIndicator::setState (int s)
{
    st = s;
    repaint (false);
}

void QIStateIndicator::drawContents (QPainter *p)
{
    QPixmap *pm = state_icons [st];
    if (!pm)
        erase();
    else
        p->drawPixmap (contentsRect(), *pm);
}

void QIStateIndicator::mouseDoubleClickEvent (QMouseEvent * e)
{
    emit mouseDoubleClicked (this, e);
}

void QIStateIndicator::contextMenuEvent (QContextMenuEvent * e)
{
    emit contextMenuRequested (this, e);
}
