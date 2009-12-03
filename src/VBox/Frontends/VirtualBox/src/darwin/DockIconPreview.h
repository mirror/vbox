/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxDockIconPreview class declaration
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

#ifndef ___VBoxDockIconPreview_h___
#define ___VBoxDockIconPreview_h___

#ifdef QT_MAC_USE_COCOA

#include "CocoaDockIconPreview.h"
class VBoxDockIconPreview: public CocoaDockIconPreview
{
public:
    VBoxDockIconPreview (VBoxConsoleWnd *aMainWnd, const QPixmap& aOverlayImage)
      : CocoaDockIconPreview (aMainWnd, aOverlayImage) {}
};

#else /* QT_MAC_USE_COCOA */

#include "CarbonDockIconPreview.h"
class VBoxDockIconPreview: public CarbonDockIconPreview
{
public:
    VBoxDockIconPreview (VBoxConsoleWnd *aMainWnd, const QPixmap& aOverlayImage)
      : CarbonDockIconPreview (aMainWnd, aOverlayImage) {}
};

#endif /* QT_MAC_USE_COCOA */

#endif /* !___VBoxDockIconPreview_h___ */

