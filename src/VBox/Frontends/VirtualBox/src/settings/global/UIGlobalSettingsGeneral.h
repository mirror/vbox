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
        QLabel             *m_pLabelVRDPLibraryName;
        /** Holds VRDP library name selector instance. */
        UIFilePathSelector *m_pSelectorVRDPLibraryName;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsGeneral_h */
