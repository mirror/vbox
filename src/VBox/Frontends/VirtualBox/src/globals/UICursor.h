/* $Id$ */
/** @file
 * VBox Qt GUI - UICursor namespace declaration.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_globals_UICursor_h
#define FEQT_INCLUDED_SRC_globals_UICursor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UILibraryDefs.h"

/* Forward declarations: */
class QGraphicsWidget;
class QWidget;

/** QObject subclass containing common GUI functionality. */
namespace UICursor
{
    /** Does some checks on certain platforms before calling QWidget::setCursor(...). */
    SHARED_LIBRARY_STUFF void setCursor(QWidget *pWidget, const QCursor &cursor);
    /** Does some checks on certain platforms before calling QGraphicsWidget::setCursor(...). */
    SHARED_LIBRARY_STUFF void setCursor(QGraphicsWidget *pWidget, const QCursor &cursor);
    /** Does some checks on certain platforms before calling QWidget::unsetCursor(). */
    SHARED_LIBRARY_STUFF void unsetCursor(QWidget *pWidget);
    /** Does some checks on certain platforms before calling QGraphicsWidget::unsetCursor(). */
    SHARED_LIBRARY_STUFF void unsetCursor(QGraphicsWidget *pWidget);
}

#endif /* !FEQT_INCLUDED_SRC_globals_UICursor_h */
