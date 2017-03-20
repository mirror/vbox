/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsGeneral class declaration.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIGlobalSettingsGeneral_h___
#define ___UIGlobalSettingsGeneral_h___

/* GUI includes: */
#include "UISettingsPage.h"
#include "UIGlobalSettingsGeneral.gen.h"

/* Forward declarations: */
struct UIDataSettingsGlobalGeneral;
typedef UISettingsCache<UIDataSettingsGlobalGeneral> UISettingsCacheGlobalGeneral;


/** Global settings: General page. */
class UIGlobalSettingsGeneral : public UISettingsPageGlobal, public Ui::UIGlobalSettingsGeneral
{
    Q_OBJECT;

public:

    /** Constructs General settings page. */
    UIGlobalSettingsGeneral();
    /** Destructs General settings page. */
    ~UIGlobalSettingsGeneral();

protected:

    /** Loads data into the cache from corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    void loadToCacheFrom(QVariant &data);
    /** Loads data into corresponding widgets from the cache,
      * this task SHOULD be performed in the GUI thread only. */
    void getFromCache();

    /** Saves data from corresponding widgets to the cache,
      * this task SHOULD be performed in the GUI thread only. */
    void putToCache();
    /** Saves data from the cache to corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    void saveFromCacheTo(QVariant &data);

    /** Defines TAB order. */
    void setOrderAfter(QWidget *pWidget);

    /** Handles translation event. */
    void retranslateUi();

private:

    /** Holds the page data cache instance. */
    UISettingsCacheGlobalGeneral *m_pCache;
};

#endif /* !___UIGlobalSettingsGeneral_h___ */

