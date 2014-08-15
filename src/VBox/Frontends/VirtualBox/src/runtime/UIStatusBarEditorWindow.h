/** @file
 * VBox Qt GUI - UIStatusBarEditorWindow class declaration.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIStatusBarEditorWindow_h___
#define ___UIStatusBarEditorWindow_h___

/* Qt includes: */
#include "UISlidingToolBar.h"

/* Forward declarations: */
class UIMachineWindow;

/** UISlidingToolBar wrapper
  * providing user with possibility to edit status-bar layout. */
class UIStatusBarEditorWindow : public UISlidingToolBar
{
    Q_OBJECT;

public:

    /** Constructor, passes @a pParent to the UISlidingToolBar constructor. */
    UIStatusBarEditorWindow(UIMachineWindow *pParent);
};

#endif /* !___UIStatusBarEditorWindow_h___ */
