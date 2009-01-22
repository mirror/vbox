/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * QILineEdit class implementation
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#include "QILineEdit.h"

/* Qt includes */
#include <QStyleOptionFrame>

void QILineEdit::setFixedWidthByText (const QString &aText)
{
    QStyleOptionFrame sof;
    sof.initFrom (this);
    sof.rect = contentsRect();
    sof.lineWidth = hasFrame() ? style()->pixelMetric (QStyle::PM_DefaultFrameWidth) : 0;
    sof.midLineWidth = 0;
    sof.state |= QStyle::State_Sunken;

    /* The margins are based on qlineedit.cpp of Qt. Maybe they where changed
     * at some time in the future. */
    QSize sc (fontMetrics().width (aText) + 2*2,
              fontMetrics().xHeight()     + 2*1);
    QSize sa = style()->sizeFromContents (QStyle::CT_LineEdit, &sof, sc, this);

    setMaximumWidth (sa.width());
    setMinimumWidth (sa.width());
}

