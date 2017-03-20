/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsDisplay class declaration.
 */

/*
 * Copyright (C) 2012-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIGlobalSettingsDisplay_h___
#define ___UIGlobalSettingsDisplay_h___

/* GUI includes: */
#include "UISettingsPage.h"
#include "UIGlobalSettingsDisplay.gen.h"


/** Global settings: Display page data structure. */
struct UIDataSettingsGlobalDisplay
{
    /** Constructs data. */
    UIDataSettingsGlobalDisplay()
        : m_strMaxGuestResolution(QString())
        , m_fActivateHoveredMachineWindow(false)
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsGlobalDisplay &other) const
    {
        return true
               && (m_strMaxGuestResolution == other.m_strMaxGuestResolution)
               && (m_fActivateHoveredMachineWindow == other.m_fActivateHoveredMachineWindow)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsGlobalDisplay &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsGlobalDisplay &other) const { return !equal(other); }

    /** Holds the maximum guest resolution or preset name. */
    QString m_strMaxGuestResolution;
    /** Holds whether we should automatically activate machine window under the mouse cursor. */
    bool m_fActivateHoveredMachineWindow;
};
typedef UISettingsCache<UIDataSettingsGlobalDisplay> UISettingsCacheGlobalDisplay;


/** Global settings: Display page. */
class UIGlobalSettingsDisplay : public UISettingsPageGlobal, public Ui::UIGlobalSettingsDisplay
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGlobalSettingsDisplay();

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

protected slots:

    /* Handler: Resolution-combo stuff: */
    void sltMaxResolutionComboActivated();

private:

    /* Helper: Resolution-combo stuff: */
    void populate();

    /* Cache: */
    UISettingsCacheGlobalDisplay m_cache;
};

#endif /* !___UIGlobalSettingsDisplay_h___ */

