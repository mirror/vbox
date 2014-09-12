/* $Id$ */
/** @file
 * VBox Qt GUI - UIDockIconPreview class declaration.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIDockIconPreview_h___
#define ___UIDockIconPreview_h___

#ifdef QT_MAC_USE_COCOA

#include "UICocoaDockIconPreview.h"
class UIDockIconPreview: public UICocoaDockIconPreview
{
public:
    UIDockIconPreview(UISession *pSession, const QPixmap& overlayImage)
      : UICocoaDockIconPreview(pSession, overlayImage) {}
};

#endif /* QT_MAC_USE_COCOA */

#endif /* !___UIDockIconPreview_h___ */

