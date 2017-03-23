/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsInput class declaration.
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

#ifndef ___UIGlobalSettingsInput_h___
#define ___UIGlobalSettingsInput_h___

/* GUI includes: */
#include "UISettingsPage.h"
#include "UIGlobalSettingsInput.gen.h"

/* Forward declartions: */
class QLineEdit;
class QTabWidget;
class UIDataSettingsGlobalInput;
class UIHotKeyTable;
class UIHotKeyTableModel;
typedef UISettingsCache<UIDataSettingsGlobalInput> UISettingsCacheGlobalInput;


/** Global settings: Input page. */
class UIGlobalSettingsInput : public UISettingsPageGlobal,
                              public Ui::UIGlobalSettingsInput
{
    Q_OBJECT;

    /* Hot-key table indexes: */
    enum { UIHotKeyTableIndex_Selector, UIHotKeyTableIndex_Machine };

public:

    /** Constructs Input settings page. */
    UIGlobalSettingsInput();
    /** Destructs Input settings page. */
    ~UIGlobalSettingsInput();

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

    /** Performs validation, updates @a messages list if something is wrong. */
    virtual bool validate(QList<UIValidationMessage> &messages) /* override */;

    /** Defines TAB order. */
    virtual void setOrderAfter(QWidget *pWidget) /* override */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private:

    QTabWidget         *m_pTabWidget;
    QLineEdit          *m_pSelectorFilterEditor;
    UIHotKeyTableModel *m_pSelectorModel;
    UIHotKeyTable      *m_pSelectorTable;
    QLineEdit          *m_pMachineFilterEditor;
    UIHotKeyTableModel *m_pMachineModel;
    UIHotKeyTable      *m_pMachineTable;

    /** Holds the page data cache instance. */
    UISettingsCacheGlobalInput *m_pCache;
};

#endif /* !___UIGlobalSettingsInput_h___ */

