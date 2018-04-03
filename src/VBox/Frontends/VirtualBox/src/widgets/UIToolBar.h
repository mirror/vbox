/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolBar class declaration.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIToolBar_h___
#define ___UIToolBar_h___

/* Qt includes: */
#include <QToolBar>

/* GUI includes: */
#include "UILibraryDefs.h"

/* Forward declarations: */
class QMainWindow;

/** QToolBar extension
  * with few settings presets. */
class SHARED_LIBRARY_STUFF UIToolBar : public QToolBar
{
    Q_OBJECT;

public:

    /** Constructs tool-bar passing @a pParent to the base-class. */
    UIToolBar(QWidget *pParent = 0);

    /** Defines whether tool-bar should use text-labels. */
    void setUseTextLabels(bool fEnable);

#ifdef VBOX_WS_MAC
    /** Mac OS X: Defines whether native tool-bar should be used. */
    void enableMacToolbar();
    /** Mac OS X: Defines whether native tool-bar button should be shown. */
    void setShowToolBarButton(bool fShow);
    /** Mac OS X: Updates native tool-bar layout. */
    void updateLayout();
#endif /* VBOX_WS_MAC */

private:

    /** Prepares all. */
    void prepare();

    /** Holds the parent main-window isntance. */
    QMainWindow *m_pMainWindow;
};

#endif /* !___UIToolBar_h___ */

