/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QIGraphicsWidget class declaration.
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_extensions_graphics_QIGraphicsWidget_h
#define FEQT_INCLUDED_SRC_extensions_graphics_QIGraphicsWidget_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QGraphicsWidget>

/* GUI includes: */
#include "UILibraryDefs.h"

/** QGraphicsWidget extension with advanced functionality. */
class SHARED_LIBRARY_STUFF QIGraphicsWidget : public QGraphicsWidget
{
    Q_OBJECT;

public:

    /** Constructs graphics-widget passing @a pParent to the base-class. */
    QIGraphicsWidget(QGraphicsWidget *pParent = 0);

    /** Returns minimum size-hint. */
    virtual QSizeF minimumSizeHint() const;
};

#endif /* !FEQT_INCLUDED_SRC_extensions_graphics_QIGraphicsWidget_h */
