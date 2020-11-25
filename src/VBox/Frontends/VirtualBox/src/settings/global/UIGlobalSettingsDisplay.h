/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsDisplay class declaration.
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsDisplay_h
#define FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsDisplay_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UISettingsPage.h"

/* Forward declarations: */
class QCheckBox;
class QLabel;
class UIMaximumGuestScreenSizeEditor;
class UIScaleFactorEditor;
struct UIDataSettingsGlobalDisplay;
typedef UISettingsCache<UIDataSettingsGlobalDisplay> UISettingsCacheGlobalDisplay;

/** Global settings: Display page. */
class SHARED_LIBRARY_STUFF UIGlobalSettingsDisplay : public UISettingsPageGlobal
{
    Q_OBJECT;

public:

    /** Constructs Display settings page. */
    UIGlobalSettingsDisplay();
    /** Destructs Display settings page. */
    virtual ~UIGlobalSettingsDisplay() /* override */;

protected:

    /** Loads settings from external object(s) packed inside @a data to cache.
      * @note  This task WILL be performed in other than the GUI thread, no widget interactions! */
    virtual void loadToCacheFrom(QVariant &data) /* override */;
    /** Loads data from cache to corresponding widgets.
      * @note  This task WILL be performed in the GUI thread only, all widget interactions here! */
    virtual void getFromCache() /* override */;

    /** Saves data from corresponding widgets to cache.
      * @note  This task WILL be performed in the GUI thread only, all widget interactions here! */
    virtual void putToCache() /* override */;
    /** Saves settings from cache to external object(s) packed inside @a data.
      * @note  This task WILL be performed in other than the GUI thread, no widget interactions! */
    virtual void saveFromCacheTo(QVariant &data) /* overrride */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Cleanups all. */
    void cleanup();

    /** Saves existing data from the cache. */
    bool saveDisplayData();

    /** Holds the page data cache instance. */
    UISettingsCacheGlobalDisplay *m_pCache;

    /** @name Widgets
     * @{ */
        /** Holds the maximum guest screen size label instance. */
        QLabel                         *m_pLabelMaximumGuestScreenSizePolicy;
        /** Holds the maximum guest screen width label instance. */
        QLabel                         *m_pLabelMaximumGuestScreenWidth;
        /** Holds the maximum guest screen height label instance. */
        QLabel                         *m_pLabelMaximumGuestScreenHeight;
        /** Holds the maximum guest screen size editor instance. */
        UIMaximumGuestScreenSizeEditor *m_pEditorMaximumGuestScreenSize;
        /** Holds the scale-factor label instance. */
        QLabel                         *m_pLabelScaleFactor;
        /** Holds the scale-factor editor instance. */
        UIScaleFactorEditor            *m_pEditorScaleFactor;
        /** Holds the 'machine-windows' label instance. */
        QLabel                         *m_pLabelMachineWindows;
        /** Holds the 'activate on mouse hover' check-box instance. */
        QCheckBox                      *m_pCheckBoxActivateOnMouseHover;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsDisplay_h */
