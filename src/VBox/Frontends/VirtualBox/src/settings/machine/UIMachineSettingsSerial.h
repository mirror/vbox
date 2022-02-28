/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsSerial class declaration.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsSerial_h
#define FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsSerial_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UISettingsPage.h"

/* Forward declarations: */
class QITabWidget;
class UIMachineSettingsSerialPage;
struct UIDataSettingsMachineSerial;
struct UIDataSettingsMachineSerialPort;
typedef UISettingsCache<UIDataSettingsMachineSerialPort> UISettingsCacheMachineSerialPort;
typedef UISettingsCachePool<UIDataSettingsMachineSerial, UISettingsCacheMachineSerialPort> UISettingsCacheMachineSerial;

/** Machine settings: Serial page. */
class SHARED_LIBRARY_STUFF UIMachineSettingsSerialPage : public UISettingsPageMachine
{
    Q_OBJECT;

public:

    /** Constructs Serial settings page. */
    UIMachineSettingsSerialPage();
    /** Destructs Serial settings page. */
    ~UIMachineSettingsSerialPage();

protected:

    /** Returns whether the page content was changed. */
    virtual bool changed() const RT_OVERRIDE;

    /** Loads settings from external object(s) packed inside @a data to cache.
      * @note  This task WILL be performed in other than the GUI thread, no widget interactions! */
    virtual void loadToCacheFrom(QVariant &data) RT_OVERRIDE;
    /** Loads data from cache to corresponding widgets.
      * @note  This task WILL be performed in the GUI thread only, all widget interactions here! */
    virtual void getFromCache() RT_OVERRIDE;

    /** Saves data from corresponding widgets to cache.
      * @note  This task WILL be performed in the GUI thread only, all widget interactions here! */
    virtual void putToCache() RT_OVERRIDE;
    /** Saves settings from cache to external object(s) packed inside @a data.
      * @note  This task WILL be performed in other than the GUI thread, no widget interactions! */
    virtual void saveFromCacheTo(QVariant &data) /* overrride */;

    /** Performs validation, updates @a messages list if something is wrong. */
    virtual bool validate(QList<UIValidationMessage> &messages) RT_OVERRIDE;

    /** Handles translation event. */
    virtual void retranslateUi() RT_OVERRIDE;

    /** Performs final page polishing. */
    virtual void polishPage() RT_OVERRIDE;

private:

    /** Prepares all. */
    void prepare();
    /** Cleanups all. */
    void cleanup();

    /** Saves existing serial data from the cache. */
    bool saveSerialData();
    /** Saves existing port data from the cache. */
    bool savePortData(int iSlot);

    /** Holds the tab-widget instance. */
    QITabWidget *m_pTabWidget;

    /** Holds the page data cache instance. */
    UISettingsCacheMachineSerial *m_pCache;
};

#endif /* !FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsSerial_h */
