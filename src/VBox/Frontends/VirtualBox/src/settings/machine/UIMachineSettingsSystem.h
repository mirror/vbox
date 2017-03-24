/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsSystem class declaration.
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

#ifndef ___UIMachineSettingsSystem_h___
#define ___UIMachineSettingsSystem_h___

/* GUI includes: */
#include "UISettingsPage.h"
#include "UIMachineSettingsSystem.gen.h"

/* Forward declarations: */
struct UIDataSettingsMachineSystem;
typedef UISettingsCache<UIDataSettingsMachineSystem> UISettingsCacheMachineSystem;


/** Machine settings: System page. */
class UIMachineSettingsSystem : public UISettingsPageMachine,
                                public Ui::UIMachineSettingsSystem
{
    Q_OBJECT;

public:

    /** Constructs System settings page. */
    UIMachineSettingsSystem();
    /** Destructs System settings page. */
    ~UIMachineSettingsSystem();

    /* API: Correlation stuff: */
    bool isHWVirtExEnabled() const;
    bool isHIDEnabled() const;
    KChipsetType chipsetType() const;
    void setUSBEnabled(bool fEnabled);

protected:

    /** Returns whether the page content was changed. */
    virtual bool changed() const /* override */;

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

    /** Performs final page polishing. */
    virtual void polishPage() /* override */;

    /** Preprocesses any Qt @a pEvent for passed @a pObject. */
    virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;

private slots:

    /* Handlers: Memory-size stuff: */
    void sltHandleMemorySizeSliderChange();
    void sltHandleMemorySizeEditorChange();

    /* Handler: Boot-table stuff: */
    void sltCurrentBootItemChanged(int iCurrentIndex);

    /* Handlers: CPU stuff: */
    void sltHandleCPUCountSliderChange();
    void sltHandleCPUCountEditorChange();
    void sltHandleCPUExecCapSliderChange();
    void sltHandleCPUExecCapEditorChange();

private:

    /* Helpers: Prepare stuff: */
    void prepare();
    void prepareTabMotherboard();
    void prepareTabProcessor();
    void prepareTabAcceleration();

    /* Helper: Pointing HID type combo stuff: */
    void repopulateComboPointingHIDType();

    /* Helpers: Translation stuff: */
    void retranslateComboChipsetType();
    void retranslateComboPointingHIDType();
    void retranslateComboParavirtProvider();

    /* Helper: Boot-table stuff: */
    void adjustBootOrderTWSize();

    /* Variable: Boot-table stuff: */
    QList<KDeviceType> m_possibleBootItems;

    /* Variables: CPU stuff: */
    uint m_uMinGuestCPU;
    uint m_uMaxGuestCPU;
    uint m_uMinGuestCPUExecCap;
    uint m_uMedGuestCPUExecCap;
    uint m_uMaxGuestCPUExecCap;

    /* Variable: Correlation stuff: */
    bool m_fIsUSBEnabled;

    /** Holds the page data cache instance. */
    UISettingsCacheMachineSystem *m_pCache;
};

#endif /* !___UIMachineSettingsSystem_h___ */

