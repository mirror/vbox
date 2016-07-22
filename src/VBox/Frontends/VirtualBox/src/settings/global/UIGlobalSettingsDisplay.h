/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsDisplay class declaration.
 */

/*
 * Copyright (C) 2012-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIGlobalSettingsDisplay_h__
#define __UIGlobalSettingsDisplay_h__

/* Local includes: */
#include "UISettingsPage.h"
#include "UIGlobalSettingsDisplay.gen.h"

/* Global settings / Display page / Cache: */
struct UISettingsCacheGlobalDisplay
{
    QString m_strMaxGuestResolution;
    bool m_fActivateHoveredMachineWindow;
};

/* Global settings / Display page: */
class UIGlobalSettingsDisplay : public UISettingsPageGlobal, public Ui::UIGlobalSettingsDisplay
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGlobalSettingsDisplay();

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

protected slots:

    /* Handler: Resolution-combo stuff: */
    void sltMaxResolutionComboActivated();

private:

    /* Helper: Resolution-combo stuff: */
    void populate();

    /* Cache: */
    UISettingsCacheGlobalDisplay m_cache;
};

#endif // __UIGlobalSettingsDisplay_h__

