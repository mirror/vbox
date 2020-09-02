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
class QComboBox;
class QLabel;
class QSpinBox;
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
    ~UIGlobalSettingsDisplay();

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

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** Handles maximum guest-screen size policy change. */
    void sltHandleMaximumGuestScreenSizePolicyChange();

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Prepares connections. */
    void prepareConnections();
    /** Cleanups all. */
    void cleanup();

    /** Reloads maximum guest-screen size policy combo-box. */
    void reloadMaximumGuestScreenSizePolicyComboBox();

    /** Saves existing display data from the cache. */
    bool saveDisplayData();

    /** Holds the page data cache instance. */
    UISettingsCacheGlobalDisplay *m_pCache;

    /** @name Widgets
     * @{ */
        /** Holds the max guest screen size label instance. */
        QLabel              *m_pLabelMaxGuestScreenSize;
        /** Holds the max guest screen size combo instance. */
        QComboBox           *m_pComboMaxGuestScreenSize;
        /** Holds the max guest screen width label instance. */
        QLabel              *m_pLabelMaxGuestScreenWidth;
        /** Holds the max guest screen width spinbox instance. */
        QSpinBox            *m_pSpinboxMaxGuestScreenWidth;
        /** Holds the max guest screen height label instance. */
        QLabel              *m_pLabelMaxGuestScreenHeight;
        /** Holds the max guest screen height spinbox instance. */
        QSpinBox            *m_pSpinboxMaxGuestScreenHeight;
        /** Holds the scale-factor label instance. */
        QLabel              *m_pLabelScaleFactor;
        /** Holds the scale-factor editor instance. */
        UIScaleFactorEditor *m_pEditorScaleFactor;
        /** Holds the 'machine-windows' label instance. */
        QLabel              *m_pLabelMachineWindows;
        /** Holds the 'activate on mouse hover' check-box instance. */
        QCheckBox           *m_pCheckBoxActivateOnMouseHover;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsDisplay_h */
