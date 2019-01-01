/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QITabWidget class declaration.
 */

/*
 * Copyright (C) 2010-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___QITabWidget_h___
#define ___QITabWidget_h___
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QTabWidget>

/* GUI includes: */
#include "UILibraryDefs.h"

/** QTabWidget extension with advanced functionality. */
class SHARED_LIBRARY_STUFF QITabWidget : public QTabWidget
{
    Q_OBJECT;

public:

    /** Constructs tab-widget passing @a pParent to the base-class. */
    QITabWidget(QWidget *pParent = 0);
};

#endif /* !___QITabWidget_h___ */
