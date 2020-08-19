/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsSystem class declaration.
 */

/*
 * Copyright (C) 2008-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsSystem_h
#define FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsSystem_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UISettingsPage.h"

/* Forward declarations: */
struct UIDataSettingsMachineSystem;
typedef UISettingsCache<UIDataSettingsMachineSystem> UISettingsCacheMachineSystem;
class CMachine;
class QCheckBox;
class QComboBox;
class QLabel;
class QSpinBox;
class QIAdvancedSlider;
class QITabWidget;
class UIBaseMemoryEditor;
class UIBootOrderEditor;

/** Machine settings: System page. */
class SHARED_LIBRARY_STUFF UIMachineSettingsSystem : public UISettingsPageMachine
{
    Q_OBJECT;

public:

    /** Constructs System settings page. */
    UIMachineSettingsSystem();
    /** Destructs System settings page. */
    ~UIMachineSettingsSystem();

    /** Returns whether the HW Virt Ex is supported. */
    bool isHWVirtExSupported() const;
    /** Returns whether the HW Virt Ex is enabled. */
    bool isHWVirtExEnabled() const;

    /** Returns whether the Nested Paging is supported. */
    bool isNestedPagingSupported() const;
    /** Returns whether the Nested Paging is enabled. */
    bool isNestedPagingEnabled() const;

    /** Returns whether the Nested HW Virt Ex is supported. */
    bool isNestedHWVirtExSupported() const;
    /** Returns whether the Nested HW Virt Ex is enabled. */
    bool isNestedHWVirtExEnabled() const;

    /** Returns whether the HID is enabled. */
    bool isHIDEnabled() const;

    /** Returns the chipset type. */
    KChipsetType chipsetType() const;

    /** Defines whether the USB is enabled. */
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

    /** Defines TAB order for passed @a pWidget. */
    virtual void setOrderAfter(QWidget *pWidget) /* override */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Performs final page polishing. */
    virtual void polishPage() /* override */;

private slots:

    /** Handles CPU count slider change. */
    void sltHandleCPUCountSliderChange();
    /** Handles CPU count editor change. */
    void sltHandleCPUCountEditorChange();
    /** Handles CPU execution cap slider change. */
    void sltHandleCPUExecCapSliderChange();
    /** Handles CPU execution cap editor change. */
    void sltHandleCPUExecCapEditorChange();

    /** Handles HW Virt Ex check-box toggling. */
    void sltHandleHwVirtExToggle();

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Prepares 'Motherboard' tab. */
    void prepareTabMotherboard();
    /** Prepares 'Processor' tab. */
    void prepareTabProcessor();
    /** Prepares 'Acceleration' tab. */
    void prepareTabAcceleration();
    /** Prepares connections. */
    void prepareConnections();
    /** Cleanups all. */
    void cleanup();

    /** Repopulates Chipset type combo-box. */
    void repopulateComboChipsetType();
    /** Repopulates Pointing HID type combo-box. */
    void repopulateComboPointingHIDType();
    /** Repopulates Paravirtualization Provider type combo-box. */
    void repopulateComboParavirtProviderType();

    /** Retranslates Chipset type combo-box. */
    void retranslateComboChipsetType();
    /** Retranslates Pointing HID type combo-box. */
    void retranslateComboPointingHIDType();
    /** Retranslates Paravirtualization providers combo-box. */
    void retranslateComboParavirtProvider();

    /** Saves existing system data from the cache. */
    bool saveSystemData();
    /** Saves existing 'Motherboard' data from the cache. */
    bool saveMotherboardData();
    /** Saves existing 'Processor' data from the cache. */
    bool saveProcessorData();
    /** Saves existing 'Acceleration' data from the cache. */
    bool saveAccelerationData();

    /** Holds the minimum guest CPU count. */
    uint  m_uMinGuestCPU;
    /** Holds the maximum guest CPU count. */
    uint  m_uMaxGuestCPU;
    /** Holds the minimum guest CPU execution cap. */
    uint  m_uMinGuestCPUExecCap;
    /** Holds the medium guest CPU execution cap. */
    uint  m_uMedGuestCPUExecCap;
    /** Holds the maximum guest CPU execution cap. */
    uint  m_uMaxGuestCPUExecCap;

    /** Holds whether the USB is enabled. */
    bool m_fIsUSBEnabled;

    /** Holds the page data cache instance. */
    UISettingsCacheMachineSystem *m_pCache;

    /** @name Widgets
     * @{ */
        /** Holds the tab-widget instance. */
        QITabWidget *m_pTabWidget;

        /** Holds the 'Motherboard' tab instance. */
        QWidget            *m_pTabMotherboard;
        /** Holds the base memory label instance. */
        QLabel             *m_pLabelBaseMemory;
        /** Holds the base memory editor instance. */
        UIBaseMemoryEditor *m_pEditorBaseMemory;
        /** Holds the boot order label instance. */
        QLabel             *m_pLabelBootOrder;
        /** Holds the boot order editor instance. */
        UIBootOrderEditor  *m_pEditorBootOrder;
        /** Holds the chipset label instance. */
        QLabel             *m_pLabelChipset;
        /** Holds the chipset combo instance. */
        QComboBox          *m_pComboChipset;
        /** Holds the pointing HID label instance. */
        QLabel             *m_pLabelPointingHID;
        /** Holds the pointing HID combo instance. */
        QComboBox          *m_pComboPointingHID;
        /** Holds the extended motherboard label instance. */
        QLabel             *m_pLabelExtendedMotherboard;
        /** Holds the APIC check-box instance. */
        QCheckBox          *m_pCheckBoxAPIC;
        /** Holds the EFI check-box instance. */
        QCheckBox          *m_pCheckBoxEFI;
        /** Holds the UTC check-box instance. */
        QCheckBox          *m_pCheckBoxUTC;

        /** Holds the 'Processor' tab instance. */
        QWidget          *m_pTabProcessor;
        /** Holds the processor count label instance. */
        QLabel           *m_pLabelProcessorCount;
        /** Holds the processor count slider instance. */
        QIAdvancedSlider *m_pSliderProcessorCount;
        /** Holds the processor count spinbox instance. */
        QSpinBox         *m_pSpinboxProcessorCount;
        /** Holds the processor count min label instance. */
        QLabel           *m_pLabelProcessorCountMin;
        /** Holds the processor count max label instance. */
        QLabel           *m_pLabelProcessorCountMax;
        /** Holds the processor exec cap label instance. */
        QLabel           *m_pLabelProcessorExecCap;
        /** Holds the processor exec cap slider instance. */
        QIAdvancedSlider *m_pSliderProcessorExecCap;
        /** Holds the processor exec cap spinbox instance. */
        QSpinBox         *m_pSpinboxProcessorExecCap;
        /** Holds the processor exec cap min label instance. */
        QLabel           *m_pLabelProcessorExecCapMin;
        /** Holds the processor exec cap max label instance. */
        QLabel           *m_pLabelProcessorExecCapMax;
        /** Holds the extended processor label instance. */
        QLabel           *m_pLabelExtendedProcessor;
        /** Holds the PAE check-box instance. */
        QCheckBox        *m_pCheckBoxPAE;
        /** Holds the nested virtualization check-box instance. */
        QCheckBox        *m_pCheckBoxNestedVirtualization;

        /** Holds the 'Acceleration' tab instance. */
        QWidget   *m_pTabAcceleration;
        /** Holds the paravirtualization provider label instance. */
        QLabel    *m_pLabelParavirtProvider;
        /** Holds the paravirtualization provider combo instance. */
        QComboBox *m_pComboParavirtProvider;
        /** Holds the virtualization label instance. */
        QLabel *m_pLabelVirtualization;
        /** Holds the virtualization check-box instance. */
        QCheckBox *m_pCheckBoxVirtualization;
        /** Holds the nested paging check-box instance. */
        QCheckBox *m_pCheckBoxNestedPaging;
   /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsSystem_h */
