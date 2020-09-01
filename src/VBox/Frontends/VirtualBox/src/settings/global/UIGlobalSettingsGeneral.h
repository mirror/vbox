/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsGeneral class declaration.
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

#ifndef FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsGeneral_h
#define FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsGeneral_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UISettingsPage.h"

/* Forward declarations: */
class QCheckBox;
class QLabel;
class UIFilePathSelector;
struct UIDataSettingsGlobalGeneral;
typedef UISettingsCache<UIDataSettingsGlobalGeneral> UISettingsCacheGlobalGeneral;

/** Global settings: General page. */
class SHARED_LIBRARY_STUFF UIGlobalSettingsGeneral : public UISettingsPageGlobal
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

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Cleanups all. */
    void cleanup();

    /** Saves existing general data from the cache. */
    bool saveGeneralData();

    /** Holds the page data cache instance. */
    UISettingsCacheGlobalGeneral *m_pCache;

    /** @name Widgets
     * @{ */
        /** Holds machine folder label instance. */
        QLabel             *m_pLabelMachineFolder;
        /** Holds machine folder selector instance. */
        UIFilePathSelector *m_pSelectorMachineFolder;
        /** Holds VRDP library name label instance. */
        QLabel             *m_pLabelVRDPLibName;
        /** Holds VRDP library name selector instance. */
        UIFilePathSelector *m_pSelectorVRDPLibName;
        /** Holds screen-saver label instance. */
        QLabel             *m_pLabelHostScreenSaver;
        /** Holds disable screen-saver check-box instance. */
        QCheckBox          *m_pCheckBoxHostScreenSaver;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsGeneral_h */
