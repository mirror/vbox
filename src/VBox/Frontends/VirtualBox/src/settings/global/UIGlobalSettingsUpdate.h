/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsUpdate class declaration.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsUpdate_h
#define FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsUpdate_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UISettingsPage.h"
#include "UIUpdateDefs.h"

/* Forward declarations: */
class QCheckBox;
class QComboBox;
class QLabel;
class QRadioButton;
struct UIDataSettingsGlobalUpdate;
typedef UISettingsCache<UIDataSettingsGlobalUpdate> UISettingsCacheGlobalUpdate;

/** Global settings: Update page. */
class SHARED_LIBRARY_STUFF UIGlobalSettingsUpdate : public UISettingsPageGlobal
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
    virtual void loadToCacheFrom(QVariant &data) /* override */;
    /** Loads data into corresponding widgets from the cache,
      * this task SHOULD be performed in the GUI thread only. */
    virtual void getFromCache() /* override */;

    /** Saves data from corresponding widgets to the cache,
      * this task SHOULD be performed in the GUI thread only. */
    virtual void putToCache() /* override */;
    /** Saves data from the cache to corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    virtual void saveFromCacheTo(QVariant &data) /* overrride */;

    /** Defines TAB order for passed @a pWidget. */
    virtual void setOrderAfter(QWidget *pWidget) /* override */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** Handles whether update is @a fEnabled. */
    void sltHandleUpdateToggle(bool fEnabled);
    /** Handles update period change. */
    void sltHandleUpdatePeriodChange();

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Prepares connections. */
    void prepareConnections();
    /** Cleanups all. */
    void cleanup();

    /** Returns period type. */
    VBoxUpdateData::PeriodType periodType() const;
    /** Returns branch type. */
    VBoxUpdateData::BranchType branchType() const;

    /** Saves existing update data from the cache. */
    bool saveUpdateData();

    /** Holds the page data cache instance. */
    UISettingsCacheGlobalUpdate *m_pCache;

    /** @name Widgets
     * @{ */
        QCheckBox *m_pCheckBoxUpdate;
        QWidget *m_pWidgetUpdateSettings;
        QLabel *m_pLabelUpdatePeriod;
        QComboBox *m_pComboUpdatePeriod;
        QLabel *m_pLabelUpdateDate;
        QLabel *m_pFieldUpdateDate;
        QLabel *m_pLabelUpdateFilter;
        QRadioButton *m_pRadioUpdateFilterStable;
        QRadioButton *m_pRadioUpdateFilterEvery;
        QRadioButton *m_pRadioUpdateFilterBetas;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsUpdate_h */
