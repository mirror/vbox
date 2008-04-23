/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIDialog class implementation
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


#include "QIDialog.h"
#ifdef Q_WS_MAC
#include "VBoxUtils.h"
#endif /* Q_WS_MAC */

QIDialog::QIDialog (QWidget *aParent, Qt::WindowFlags aFlags)
    : QDialog (aParent, aFlags) 
{
}

void QIDialog::showEvent (QShowEvent * /* aEvent */)
{
#ifdef Q_WS_MAC
    /* Two thinks to do for fixed size dialogs on MacOS X:
     * 1. make sure the layout is polished and have the right size
     * 2. disable that _unnecessary_ size grip (Bug in Qt?) */
    QSizePolicy policy = sizePolicy();
    if (policy.horizontalPolicy() == QSizePolicy::Fixed &&
        policy.verticalPolicy() == QSizePolicy::Fixed ||
        windowFlags() == Qt::Sheet)
    {
        adjustSize();
        ChangeWindowAttributes (::darwinToWindowRef (this), kWindowNoAttributes, kWindowResizableAttribute);
    }
#endif /* Q_WS_MAC */
}

