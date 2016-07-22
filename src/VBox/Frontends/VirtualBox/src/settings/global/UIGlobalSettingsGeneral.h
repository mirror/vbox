/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsGeneral class declaration.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIGlobalSettingsGeneral_h__
#define __UIGlobalSettingsGeneral_h__

/* Local includes */
#include "UISettingsPage.h"
#include "UIGlobalSettingsGeneral.gen.h"

/* Global settings / General page / Cache: */
struct UISettingsCacheGlobalGeneral
{
    QString m_strDefaultMachineFolder;
    QString m_strVRDEAuthLibrary;
    bool m_fHostScreenSaverDisabled;
};

/* Global settings / General page: */
class UIGlobalSettingsGeneral : public UISettingsPageGlobal, public Ui::UIGlobalSettingsGeneral
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGlobalSettingsGeneral();

protected:

    /* Load data to cache from corresponding external object(s),
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

    /* Helper: Navigation stuff: */
    void setOrderAfter(QWidget *pWidget);

    /* Helper: Translation stuff: */
    void retranslateUi();

private:

    /* Cache: */
    UISettingsCacheGlobalGeneral m_cache;
};

#endif // __UIGlobalSettingsGeneral_h__
