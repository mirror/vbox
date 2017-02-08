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


/** Global settings: Update page data structure. */
struct UIDataSettingsGlobalUpdate
{
    /** Constructs data. */
    UIDataSettingsGlobalUpdate()
        : m_fCheckEnabled(false)
        , m_periodIndex(VBoxUpdateData::PeriodUndefined)
        , m_branchIndex(VBoxUpdateData::BranchStable)
        , m_strDate(QString())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsGlobalUpdate &other) const
    {
        return true
               && (m_fCheckEnabled == other.m_fCheckEnabled)
               && (m_periodIndex == other.m_periodIndex)
               && (m_branchIndex == other.m_branchIndex)
               && (m_strDate == other.m_strDate)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsGlobalUpdate &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsGlobalUpdate &other) const { return !equal(other); }

    /** Holds whether the update check is enabled. */
    bool m_fCheckEnabled;
    /** Holds the update check period. */
    VBoxUpdateData::PeriodType m_periodIndex;
    /** Holds the update branch type. */
    VBoxUpdateData::BranchType m_branchIndex;
    /** Holds the next update date. */
    QString m_strDate;
};
typedef UISettingsCache<UIDataSettingsGlobalUpdate> UISettingsCacheGlobalUpdate;


/** Global settings: Update page. */
class UIGlobalSettingsUpdate : public UISettingsPageGlobal, public Ui::UIGlobalSettingsUpdate
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGlobalSettingsUpdate();

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

    /* Cache: */
    UISettingsCacheGlobalUpdate m_cache;
};

#endif /* !___UIGlobalSettingsUpdate_h___ */

