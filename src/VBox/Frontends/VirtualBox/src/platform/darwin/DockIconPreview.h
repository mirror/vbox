/* $Id$ */
/** @file
 * VBox Qt GUI - UIDockIconPreview class declaration.
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_platform_darwin_DockIconPreview_h
#define FEQT_INCLUDED_SRC_platform_darwin_DockIconPreview_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UICocoaDockIconPreview.h"


/** UICocoaDockIconPreview extension to be used for VM. */
class UIDockIconPreview : public UICocoaDockIconPreview
{
public:

    /** Constructor taking passed @a pSession and @a overlayImage. */
    UIDockIconPreview(UISession *pSession, const QPixmap& overlayImage)
        : UICocoaDockIconPreview(pSession, overlayImage) {}
};

#endif /* !FEQT_INCLUDED_SRC_platform_darwin_DockIconPreview_h */

