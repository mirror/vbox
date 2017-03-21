/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsInterface class declaration.
 */

/*
 * Copyright (C) 2008-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIMachineSettingsInterface_h___
#define ___UIMachineSettingsInterface_h___

/* GUI includes: */
#include "UISettingsPage.h"
#include "UIMachineSettingsInterface.gen.h"

/* Forward declarations: */
class UIActionPool;
struct UIDataSettingsMachineInterface;
typedef UISettingsCache<UIDataSettingsMachineInterface> UISettingsCacheMachineInterface;


/** Machine settings: User Interface page. */
class UIMachineSettingsInterface : public UISettingsPageMachine,
                                   public Ui::UIMachineSettingsInterface
{
    Q_OBJECT;

public:

    /** Constructs User Interface settings page. */
    UIMachineSettingsInterface(const QString strMachineID);
    /** Destructs User Interface settings page. */
    ~UIMachineSettingsInterface();

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

    /** Defines TAB order. */
    void setOrderAfter(QWidget *pWidget);

    /** Handles translation event. */
    void retranslateUi();

    /** Performs final page polishing. */
    void polishPage();

private:

    /** Prepare routine. */
    void prepare();

    /** Cleanup routine. */
    void cleanup();

    /** Holds the machine ID copy. */
    const QString m_strMachineID;
    /** Holds the action-pool instance. */
    UIActionPool *m_pActionPool;

    /** Holds the page data cache instance. */
    UISettingsCacheMachineInterface *m_pCache;
};

#endif /* !___UIMachineSettingsInterface_h___ */

