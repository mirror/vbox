/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIStatusBar class implementation
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "QIStatusBar.h"

QIStatusBar::QIStatusBar (QWidget *aParent)
    : QStatusBar (aParent)
{
    connect (this, SIGNAL (messageChanged (const QString&)),
             this, SLOT (rememberLastMessage (const QString&)));

    /* Remove that ugly border around the statusbar items on every platform */
    setStyleSheet ("QStatusBar::item { border: 0px none black; }");
}

