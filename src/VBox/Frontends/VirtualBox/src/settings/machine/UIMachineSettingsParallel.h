/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsParallel class declaration.
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

#ifndef ___UIMachineSettingsParallel_h___
#define ___UIMachineSettingsParallel_h___

/* GUI includes: */
#include "UISettingsPage.h"
#include "UIMachineSettingsParallel.gen.h"

/* Forward declarations: */
class QITabWidget;
class UIMachineSettingsParallelPage;
struct UIDataSettingsMachineParallel;
struct UIDataSettingsMachineParallelPort;
typedef UISettingsCache<UIDataSettingsMachineParallelPort> UISettingsCacheMachineParallelPort;
typedef UISettingsCachePool<UIDataSettingsMachineParallel, UISettingsCacheMachineParallelPort> UISettingsCacheMachineParallel;


/** Machine settings: Parallel page. */
class UIMachineSettingsParallelPage : public UISettingsPageMachine
{
    Q_OBJECT;

public:

    /** Constructs Parallel settings page. */
    UIMachineSettingsParallelPage();
    /** Destructs Parallel settings page. */
    ~UIMachineSettingsParallelPage();

protected:

    /** Returns whether the page content was changed. */
    bool changed() const /* override */;

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

    /** Performs validation, updates @a messages list if something is wrong. */
    bool validate(QList<UIValidationMessage> &messages);

    /** Handles translation event. */
    void retranslateUi();

private:

    void polishPage();

    QITabWidget *mTabWidget;

    /** Holds the page data cache instance. */
    UISettingsCacheMachineParallel *m_pCache;
};

#endif /* !___UIMachineSettingsParallel_h___ */

