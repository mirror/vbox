/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIActionPoolSelector class declaration
 */

/*
 * Copyright (C) 2010-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIActionPoolSelector_h__
#define __UIActionPoolSelector_h__

/* Local includes: */
#include "UIActionPool.h"

/* Action keys: */
enum UIActionIndexOffline
{
    /* Maximum index: */
    UIActionIndexOffline_Max = UIActionIndex_Max + 1
};

/* Singleton runtime action pool: */
class UIActionPoolSelector : public UIActionPool
{
    Q_OBJECT;

public:

    /* Singleton methods: */
    static void create();
    static void destroy();

private:

    /* Constructor: */
    UIActionPoolSelector() : UIActionPool(UIActionPoolType_Offline) {}

    /* Virtual helping stuff: */
    void createActions();
    void createMenus();
};

#endif // __UIActionPoolSelector_h__

