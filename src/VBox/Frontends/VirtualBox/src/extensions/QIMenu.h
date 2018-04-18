/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QIMenu class declaration.
 */

/*
 * Copyright (C) 2010-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___QIMenu_h___
#define ___QIMenu_h___

/* Qt includes: */
#include <QMenu>

/* GUI includes: */
#include "UILibraryDefs.h"

/** QMenu extension with advanced functionality.
  * Allows to highlight first menu item for popped up menu. */
class SHARED_LIBRARY_STUFF QIMenu : public QMenu
{
    Q_OBJECT;

public:

    /** Constructs menu passing @a pParent to the base-class. */
    QIMenu(QWidget *pParent = 0);

private slots:

    /** Highlights first menu action for popped up menu. */
    void sltHighlightFirstAction();
};

#endif /* !___QIMenu_h___ */
