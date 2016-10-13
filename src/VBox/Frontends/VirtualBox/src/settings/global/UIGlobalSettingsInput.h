/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsInput class declaration.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIGlobalSettingsInput_h__
#define __UIGlobalSettingsInput_h__

/* Qt includes: */
#include <QAbstractTableModel>

/* GUI includes: */
#include "UISettingsPage.h"
#include "UIGlobalSettingsInput.gen.h"
#include "UIActionPool.h"

/* Forward declartions: */
class QTabWidget;
class QLineEdit;
class UISettingsCacheGlobalInput;
class UIHotKeyTableModel;
class UIHotKeyTable;


/* Global settings / Input page: */
class UIGlobalSettingsInput : public UISettingsPageGlobal, public Ui::UIGlobalSettingsInput
{
    Q_OBJECT;

    /* Hot-key table indexes: */
    enum UIHotKeyTableIndex
    {
        UIHotKeyTableIndex_Selector = 0,
        UIHotKeyTableIndex_Machine = 1
    };

public:

    /* Constructor: */
    UIGlobalSettingsInput();

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

    /* API: Validation stuff: */
    bool validate(QList<UIValidationMessage> &messages);

    /* Helper: Navigation stuff: */
    void setOrderAfter(QWidget *pWidget);

    /* Helper: Translation stuff: */
    void retranslateUi();

private:

    /* Helper: Prepare stuff: */
    void prepareValidation();

    QTabWidget *m_pTabWidget;
    QLineEdit *m_pSelectorFilterEditor;
    UIHotKeyTableModel *m_pSelectorModel;
    UIHotKeyTable *m_pSelectorTable;
    QLineEdit *m_pMachineFilterEditor;
    UIHotKeyTableModel *m_pMachineModel;
    UIHotKeyTable *m_pMachineTable;

    /** Holds the cache instance. */
    UISettingsCacheGlobalInput *m_pCache;
};

#endif // __UIGlobalSettingsInput_h__

