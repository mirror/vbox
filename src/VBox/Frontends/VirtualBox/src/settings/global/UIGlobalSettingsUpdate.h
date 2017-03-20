/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsUpdate class declaration.
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

#ifndef ___UIGlobalSettingsUpdate_h___
#define ___UIGlobalSettingsUpdate_h___

/* GUI includes: */
#include "UISettingsPage.h"
#include "UIGlobalSettingsUpdate.gen.h"
#include "UIUpdateDefs.h"

/* Forward declarations: */
struct UIDataSettingsGlobalUpdate;
typedef UISettingsCache<UIDataSettingsGlobalUpdate> UISettingsCacheGlobalUpdate;


/** Global settings: Update page. */
class UIGlobalSettingsUpdate : public UISettingsPageGlobal, public Ui::UIGlobalSettingsUpdate
{
    Q_OBJECT;

public:

    /** Constructs Update settings page. */
    UIGlobalSettingsUpdate();
    /** Destructs Update settings page. */
    ~UIGlobalSettingsUpdate();

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

private slots:

    /* Handlers: */
    void sltUpdaterToggled(bool fEnabled);
    void sltPeriodActivated();

private:

    /* Helpers: */
    VBoxUpdateData::PeriodType periodType() const;
    VBoxUpdateData::BranchType branchType() const;

    /* Variables: */
    QRadioButton *m_pLastChosenRadio;

    /** Holds the page data cache instance. */
    UISettingsCacheGlobalUpdate *m_pCache;
};

#endif /* !___UIGlobalSettingsUpdate_h___ */

