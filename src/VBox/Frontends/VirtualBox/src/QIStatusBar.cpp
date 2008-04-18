/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * innotek Qt extensions: QIStatusBar class implementation
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include "QIStatusBar.h"

#include <qpainter.h>
#include <qsizegrip.h>

/** @clas QIStatusLine
 *
 *  The QIStatusBar class is a replacement of QStatusBar that reimplements
 *  QStatusBar::paintEvent() to disable drawing of those sunken borders
 *  around every widget on the status bar.
 */

QIStatusBar::QIStatusBar (QWidget *parent, const char *name) :
    QStatusBar (parent, name)
{
    connect (
        this, SIGNAL( messageChanged (const QString &) ),
        this, SLOT( rememberLastMessage (const QString &) )
    );
}

/**
 *  Reimplemented to disable drawing of sunken borders around statusbar's
 *  widgets.
 */
void QIStatusBar::paintEvent (QPaintEvent *)
{
    QPainter p (this);

#ifndef QT_NO_SIZEGRIP
    // this will work provided that QStatusBar::setSizeGripEnabled() names
    // its resizer child as specified (at least Qt 3.3.x does this).
    QSizeGrip *resizer = (QSizeGrip *) child ("QStatusBar::resizer", "QSizeGrip");
    int psx = (resizer && resizer->isVisible()) ? resizer->x() : width() - 12;
#else
    int psx = width() - 12;
#endif

    if (!message.isEmpty()) {
        p.setPen (colorGroup().foreground());
        p.drawText (6, 0, psx, height(), AlignVCenter | SingleLine, message);
    }
}
