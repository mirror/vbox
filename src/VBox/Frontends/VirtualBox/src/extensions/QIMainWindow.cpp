/* $Id$ */
/** @file
 * VBox Qt GUI - QIMainWindow class implementation.
 */

/*
 * Copyright (C) 2016-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* GUI includes: */
# include "QIMainWindow.h"
# include "VBoxGlobal.h"
# include "UIDesktopWidgetWatchdog.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


QIMainWindow::QIMainWindow(QWidget *pParent /* = 0 */, Qt::WindowFlags enmFlags /* = 0 */)
    : QMainWindow(pParent, enmFlags)
{
}

void QIMainWindow::restoreGeometry()
{
#if defined(VBOX_WS_MAC) || defined(VBOX_WS_WIN)
    /* Use the old approach for OSX/Win: */
    move(m_geometry.topLeft());
    resize(m_geometry.size());
#else /* !VBOX_WS_MAC && !VBOX_WS_WIN */
    /* Use the new approach for X11: */
    VBoxGlobal::setTopLevelGeometry(this, m_geometry);
#endif /* !VBOX_WS_MAC && !VBOX_WS_WIN */

    /* Maximize (if necessary): */
    if (shouldBeMaximized())
        showMaximized();
}

