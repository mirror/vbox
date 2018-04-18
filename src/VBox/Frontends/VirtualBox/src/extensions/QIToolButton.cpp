/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QIToolButton class implementation.
 */

/*
 * Copyright (C) 2009-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* GUI includes: */
#include "QIToolButton.h"


QIToolButton::QIToolButton(QWidget *pParent /* = 0 */)
    : QToolButton(pParent)
{
#ifdef VBOX_WS_MAC
    /* Keep size-hint alive: */
    const QSize sh = sizeHint();
    setStyleSheet("QToolButton { border: 0px none black; margin: 0px 0px 0px 0px; } QToolButton::menu-indicator {image: none;}");
    setFixedSize(sh);
#else /* !VBOX_WS_MAC */
    setAutoRaise(true);
#endif /* !VBOX_WS_MAC */
}

void QIToolButton::setAutoRaise(bool fEnabled)
{
#ifdef VBOX_WS_MAC
    /* Ignore for macOS: */
    Q_UNUSED(fEnabled);
#else /* !VBOX_WS_MAC */
    /* Call to base-class: */
    QToolButton::setAutoRaise(fEnabled);
#endif /* !VBOX_WS_MAC */
}

void QIToolButton::removeBorder()
{
    setStyleSheet("QToolButton { border: 0px }");
}
