/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxGLSettingsInput class declaration
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxGLSettingsInput_h__
#define __VBoxGLSettingsInput_h__

#include "UISettingsPage.h"
#include "VBoxGLSettingsInput.gen.h"

/* Global settings / Input page / Cache: */
struct UISettingsCacheGlobalInput
{
    int m_iHostKey;
    bool m_fAutoCapture;
};

/* Global settings / Input page: */
class VBoxGLSettingsInput : public UISettingsPageGlobal,
                            public Ui::VBoxGLSettingsInput
{
    Q_OBJECT;

public:

    VBoxGLSettingsInput();

protected:

    /* Load data to cashe from corresponding external object(s),
     * this task COULD be performed in other than GUI thread: */
    void loadToCacheFrom(QVariant &data);
    /* Load data to corresponding widgets from cache,
     * this task SHOULD be performed in GUI thread only: */
    void getFromCache();

    /* Save data from corresponding widgets to cache,
     * this task SHOULD be performed in GUI thread only: */
    void putToCache();
    /* Save data from cache to corresponding external object(s),
     * this task COULD be performed in other than GUI thread: */
    void saveFromCacheTo(QVariant &data);

    void setOrderAfter (QWidget *aWidget);

    void retranslateUi();

private:

    /* Cache: */
    UISettingsCacheGlobalInput m_cache;
};

#endif // __VBoxGLSettingsInput_h__

