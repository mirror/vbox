/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsSystem class implementation.
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

/* Qt includes: */
#include <QCheckBox>
#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIAdvancedSlider.h"
#include "QITabWidget.h"
#include "QIWidgetValidator.h"
#include "UIBaseMemoryEditor.h"
#include "UIBootOrderEditor.h"
#include "UIConverter.h"
#include "UIIconPool.h"
#include "UIMachineSettingsSystem.h"
#include "UIErrorString.h"
#include "UICommon.h"

/* COM includes: */
#include "CBIOSSettings.h"

/* Other VBox includes: */
#include <iprt/cdefs.h>


/** Machine settings: System page data structure. */
struct UIDataSettingsMachineSystem
{
    /** Constructs data. */
    UIDataSettingsMachineSystem()
        /* Support flags: */
        : m_fSupportedPAE(false)
        , m_fSupportedNestedHwVirtEx(false)
        , m_fSupportedHwVirtEx(false)
        , m_fSupportedNestedPaging(false)
        /* Motherboard data: */
        , m_iMemorySize(-1)
        , m_bootItems(UIBootItemDataList())
        , m_chipsetType(KChipsetType_Null)
        , m_pointingHIDType(KPointingHIDType_None)
        , m_fEnabledIoApic(false)
        , m_fEnabledEFI(false)
        , m_fEnabledUTC(false)
        /* CPU data: */
        , m_cCPUCount(-1)
        , m_iCPUExecCap(-1)
        , m_fEnabledPAE(false)
        , m_fEnabledNestedHwVirtEx(false)
        /* Acceleration data: */
        , m_paravirtProvider(KParavirtProvider_None)
        , m_fEnabledHwVirtEx(false)
        , m_fEnabledNestedPaging(false)
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsMachineSystem &other) const
    {
        return true
               /* Support flags: */
               && (m_fSupportedPAE == other.m_fSupportedPAE)
               && (m_fSupportedNestedHwVirtEx == other.m_fSupportedNestedHwVirtEx)
               && (m_fSupportedHwVirtEx == other.m_fSupportedHwVirtEx)
               && (m_fSupportedNestedPaging == other.m_fSupportedNestedPaging)
               /* Motherboard data: */
               && (m_iMemorySize == other.m_iMemorySize)
               && (m_bootItems == other.m_bootItems)
               && (m_chipsetType == other.m_chipsetType)
               && (m_pointingHIDType == other.m_pointingHIDType)
               && (m_fEnabledIoApic == other.m_fEnabledIoApic)
               && (m_fEnabledEFI == other.m_fEnabledEFI)
               && (m_fEnabledUTC == other.m_fEnabledUTC)
               /* CPU data: */
               && (m_cCPUCount == other.m_cCPUCount)
               && (m_iCPUExecCap == other.m_iCPUExecCap)
               && (m_fEnabledPAE == other.m_fEnabledPAE)
               && (m_fEnabledNestedHwVirtEx == other.m_fEnabledNestedHwVirtEx)
               /* Acceleration data: */
               && (m_paravirtProvider == other.m_paravirtProvider)
               && (m_fEnabledHwVirtEx == other.m_fEnabledHwVirtEx)
               && (m_fEnabledNestedPaging == other.m_fEnabledNestedPaging)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsMachineSystem &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsMachineSystem &other) const { return !equal(other); }

    /** Holds whether the PAE is supported. */
    bool  m_fSupportedPAE;
    /** Holds whether the Nested HW Virt Ex is supported. */
    bool  m_fSupportedNestedHwVirtEx;
    /** Holds whether the HW Virt Ex is supported. */
    bool  m_fSupportedHwVirtEx;
    /** Holds whether the Nested Paging is supported. */
    bool  m_fSupportedNestedPaging;

    /** Holds the RAM size. */
    int                 m_iMemorySize;
    /** Holds the boot items. */
    UIBootItemDataList  m_bootItems;
    /** Holds the chipset type. */
    KChipsetType        m_chipsetType;
    /** Holds the pointing HID type. */
    KPointingHIDType    m_pointingHIDType;
    /** Holds whether the IO APIC is enabled. */
    bool                m_fEnabledIoApic;
    /** Holds whether the EFI is enabled. */
    bool                m_fEnabledEFI;
    /** Holds whether the UTC is enabled. */
    bool                m_fEnabledUTC;

    /** Holds the CPU count. */
    int   m_cCPUCount;
    /** Holds the CPU execution cap. */
    int   m_iCPUExecCap;
    /** Holds whether the PAE is enabled. */
    bool  m_fEnabledPAE;
    /** Holds whether the Nested HW Virt Ex is enabled. */
    bool  m_fEnabledNestedHwVirtEx;

    /** Holds the paravirtualization provider. */
    KParavirtProvider  m_paravirtProvider;
    /** Holds whether the HW Virt Ex is enabled. */
    bool               m_fEnabledHwVirtEx;
    /** Holds whether the Nested Paging is enabled. */
    bool               m_fEnabledNestedPaging;
};


UIMachineSettingsSystem::UIMachineSettingsSystem()
    : m_uMinGuestCPU(0), m_uMaxGuestCPU(0)
    , m_uMinGuestCPUExecCap(0), m_uMedGuestCPUExecCap(0), m_uMaxGuestCPUExecCap(0)
    , m_fIsUSBEnabled(false)
    , m_pCache(0)
{
    /* Prepare: */
    prepare();
}

UIMachineSettingsSystem::~UIMachineSettingsSystem()
{
    /* Cleanup: */
    cleanup();
}

bool UIMachineSettingsSystem::isHWVirtExSupported() const
{
    AssertPtrReturn(m_pCache, false);
    return m_pCache->base().m_fSupportedHwVirtEx;
}

bool UIMachineSettingsSystem::isHWVirtExEnabled() const
{
    return m_pCheckBoxVirtualization->isChecked();
}

bool UIMachineSettingsSystem::isNestedPagingSupported() const
{
    AssertPtrReturn(m_pCache, false);
    return m_pCache->base().m_fSupportedNestedPaging;
}

bool UIMachineSettingsSystem::isNestedPagingEnabled() const
{
    return m_pCheckBoxNestedPaging->isChecked();
}

bool UIMachineSettingsSystem::isNestedHWVirtExSupported() const
{
    AssertPtrReturn(m_pCache, false);
    return m_pCache->base().m_fSupportedNestedHwVirtEx;
}

bool UIMachineSettingsSystem::isNestedHWVirtExEnabled() const
{
    return m_pCheckBoxNestedVirtualization->isChecked();
}

bool UIMachineSettingsSystem::isHIDEnabled() const
{
    return m_pComboPointingHIDType->currentData().value<KPointingHIDType>() != KPointingHIDType_PS2Mouse;
}

KChipsetType UIMachineSettingsSystem::chipsetType() const
{
    return m_pComboChipsetType->currentData().value<KChipsetType>();
}

void UIMachineSettingsSystem::setUSBEnabled(bool fEnabled)
{
    /* Make sure USB status has changed: */
    if (m_fIsUSBEnabled == fEnabled)
        return;

    /* Update USB status value: */
    m_fIsUSBEnabled = fEnabled;

    /* Revalidate: */
    revalidate();
}

bool UIMachineSettingsSystem::changed() const
{
    return m_pCache->wasChanged();
}

void UIMachineSettingsSystem::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Prepare old system data: */
    UIDataSettingsMachineSystem oldSystemData;

    /* Gather support flags: */
    oldSystemData.m_fSupportedPAE = uiCommon().host().GetProcessorFeature(KProcessorFeature_PAE);
    oldSystemData.m_fSupportedNestedHwVirtEx = uiCommon().host().GetProcessorFeature(KProcessorFeature_NestedHWVirt);
    oldSystemData.m_fSupportedHwVirtEx = uiCommon().host().GetProcessorFeature(KProcessorFeature_HWVirtEx);
    oldSystemData.m_fSupportedNestedPaging = uiCommon().host().GetProcessorFeature(KProcessorFeature_NestedPaging);

    /* Gather old 'Motherboard' data: */
    oldSystemData.m_iMemorySize = m_machine.GetMemorySize();
    oldSystemData.m_bootItems = loadBootItems(m_machine);
    oldSystemData.m_chipsetType = m_machine.GetChipsetType();
    oldSystemData.m_pointingHIDType = m_machine.GetPointingHIDType();
    oldSystemData.m_fEnabledIoApic = m_machine.GetBIOSSettings().GetIOAPICEnabled();
    oldSystemData.m_fEnabledEFI = m_machine.GetFirmwareType() >= KFirmwareType_EFI && m_machine.GetFirmwareType() <= KFirmwareType_EFIDUAL;
    oldSystemData.m_fEnabledUTC = m_machine.GetRTCUseUTC();

    /* Gather old 'Processor' data: */
    oldSystemData.m_cCPUCount = oldSystemData.m_fSupportedHwVirtEx ? m_machine.GetCPUCount() : 1;
    oldSystemData.m_iCPUExecCap = m_machine.GetCPUExecutionCap();
    oldSystemData.m_fEnabledPAE = m_machine.GetCPUProperty(KCPUPropertyType_PAE);
    oldSystemData.m_fEnabledNestedHwVirtEx = m_machine.GetCPUProperty(KCPUPropertyType_HWVirt);

    /* Gather old 'Acceleration' data: */
    oldSystemData.m_paravirtProvider = m_machine.GetParavirtProvider();
    oldSystemData.m_fEnabledHwVirtEx = m_machine.GetHWVirtExProperty(KHWVirtExPropertyType_Enabled);
    oldSystemData.m_fEnabledNestedPaging = m_machine.GetHWVirtExProperty(KHWVirtExPropertyType_NestedPaging);

    /* Cache old system data: */
    m_pCache->cacheInitialData(oldSystemData);

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsSystem::getFromCache()
{
    /* Get old system data from the cache: */
    const UIDataSettingsMachineSystem &oldSystemData = m_pCache->base();

    /* We are doing that *now* because these combos have
     * dynamical content which depends on cashed value: */
    repopulateComboChipsetType();
    repopulateComboPointingHIDType();
    repopulateComboParavirtProviderType();

    /* Load old 'Motherboard' data from the cache: */
    m_pBaseMemoryEditor->setValue(oldSystemData.m_iMemorySize);
    m_pBootOrderEditor->setValue(oldSystemData.m_bootItems);
    const int iChipsetTypePosition = m_pComboChipsetType->findData(oldSystemData.m_chipsetType);
    m_pComboChipsetType->setCurrentIndex(iChipsetTypePosition == -1 ? 0 : iChipsetTypePosition);
    const int iHIDTypePosition = m_pComboPointingHIDType->findData(oldSystemData.m_pointingHIDType);
    m_pComboPointingHIDType->setCurrentIndex(iHIDTypePosition == -1 ? 0 : iHIDTypePosition);
    m_pCheckBoxApic->setChecked(oldSystemData.m_fEnabledIoApic);
    m_pCheckBoxEFI->setChecked(oldSystemData.m_fEnabledEFI);
    m_pCheckBoxUseUTC->setChecked(oldSystemData.m_fEnabledUTC);

    /* Load old 'Processor' data from the cache: */
    m_pSliderCPUCount->setValue(oldSystemData.m_cCPUCount);
    m_pSliderCPUExecCap->setValue(oldSystemData.m_iCPUExecCap);
    m_pCheckBoxPAE->setChecked(oldSystemData.m_fEnabledPAE);
    m_pCheckBoxNestedVirtualization->setChecked(oldSystemData.m_fEnabledNestedHwVirtEx);

    /* Load old 'Acceleration' data from the cache: */
    const int iParavirtProviderPosition = m_pComboParavirtProviderType->findData(oldSystemData.m_paravirtProvider);
    m_pComboParavirtProviderType->setCurrentIndex(iParavirtProviderPosition == -1 ? 0 : iParavirtProviderPosition);
    m_pCheckBoxVirtualization->setChecked(oldSystemData.m_fEnabledHwVirtEx);
    m_pCheckBoxNestedPaging->setChecked(oldSystemData.m_fEnabledNestedPaging);

    /* Polish page finally: */
    polishPage();

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsSystem::putToCache()
{
    /* Prepare new system data: */
    UIDataSettingsMachineSystem newSystemData;

    /* Gather support flags: */
    newSystemData.m_fSupportedPAE = m_pCache->base().m_fSupportedPAE;
    newSystemData.m_fSupportedNestedHwVirtEx = isNestedHWVirtExSupported();
    newSystemData.m_fSupportedHwVirtEx = isHWVirtExSupported();
    newSystemData.m_fSupportedNestedPaging = isNestedPagingSupported();

    /* Gather 'Motherboard' data: */
    newSystemData.m_iMemorySize = m_pBaseMemoryEditor->value();
    newSystemData.m_bootItems = m_pBootOrderEditor->value();
    newSystemData.m_chipsetType = m_pComboChipsetType->currentData().value<KChipsetType>();
    newSystemData.m_pointingHIDType = m_pComboPointingHIDType->currentData().value<KPointingHIDType>();
    newSystemData.m_fEnabledIoApic =    m_pCheckBoxApic->isChecked()
                                     || m_pSliderCPUCount->value() > 1
                                     || m_pComboChipsetType->currentData().value<KChipsetType>() == KChipsetType_ICH9;
    newSystemData.m_fEnabledEFI = m_pCheckBoxEFI->isChecked();
    newSystemData.m_fEnabledUTC = m_pCheckBoxUseUTC->isChecked();

    /* Gather 'Processor' data: */
    newSystemData.m_cCPUCount = m_pSliderCPUCount->value();
    newSystemData.m_iCPUExecCap = m_pSliderCPUExecCap->value();
    newSystemData.m_fEnabledPAE = m_pCheckBoxPAE->isChecked();
    newSystemData.m_fEnabledNestedHwVirtEx = isNestedHWVirtExEnabled();

    /* Gather 'Acceleration' data: */
    newSystemData.m_paravirtProvider = m_pComboParavirtProviderType->currentData().value<KParavirtProvider>();
    /* Enable HW Virt Ex automatically if it's supported and
     * 1. multiple CPUs, 2. Nested Paging or 3. Nested HW Virt Ex is requested. */
    newSystemData.m_fEnabledHwVirtEx =    isHWVirtExEnabled()
                                       || (   isHWVirtExSupported()
                                           && (   m_pSliderCPUCount->value() > 1
                                               || isNestedPagingEnabled()
                                               || isNestedHWVirtExEnabled()));
    /* Enable Nested Paging automatically if it's supported and
     * Nested HW Virt Ex is requested. */
    newSystemData.m_fEnabledNestedPaging =    isNestedPagingEnabled()
                                           || (   isNestedPagingSupported()
                                               && isNestedHWVirtExEnabled());

    /* Cache new system data: */
    m_pCache->cacheCurrentData(newSystemData);
}

void UIMachineSettingsSystem::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Update system data and failing state: */
    setFailed(!saveSystemData());

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

bool UIMachineSettingsSystem::validate(QList<UIValidationMessage> &messages)
{
    /* Pass by default: */
    bool fPass = true;

    /* Motherboard tab: */
    {
        /* Prepare message: */
        UIValidationMessage message;
        message.first = UICommon::removeAccelMark(m_pTabWidgetSystem->tabText(0));

        /* RAM amount test: */
        const ulong uFullSize = uiCommon().host().GetMemorySize();
        if (m_pBaseMemoryEditor->value() > (int)m_pBaseMemoryEditor->maxRAMAlw())
        {
            message.second << tr(
                "More than <b>%1%</b> of the host computer's memory (<b>%2</b>) is assigned to the virtual machine. "
                "Not enough memory is left for the host operating system. Please select a smaller amount.")
                .arg((unsigned)qRound((double)m_pBaseMemoryEditor->maxRAMAlw() / uFullSize * 100.0))
                .arg(uiCommon().formatSize((uint64_t)uFullSize * _1M));
            fPass = false;
        }
        else if (m_pBaseMemoryEditor->value() > (int)m_pBaseMemoryEditor->maxRAMOpt())
        {
            message.second << tr(
                "More than <b>%1%</b> of the host computer's memory (<b>%2</b>) is assigned to the virtual machine. "
                "There might not be enough memory left for the host operating system. Please consider selecting a smaller amount.")
                .arg((unsigned)qRound((double)m_pBaseMemoryEditor->maxRAMOpt() / uFullSize * 100.0))
                .arg(uiCommon().formatSize((uint64_t)uFullSize * _1M));
        }

        /* Chipset type vs IO-APIC test: */
        if (m_pComboChipsetType->currentData().value<KChipsetType>() == KChipsetType_ICH9 && !m_pCheckBoxApic->isChecked())
        {
            message.second << tr(
                "The I/O APIC feature is not currently enabled in the Motherboard section of the System page. "
                "This is needed to support a chipset of type ICH9. "
                "It will be enabled automatically if you confirm your changes.");
        }

        /* HID vs USB test: */
        if (isHIDEnabled() && !m_fIsUSBEnabled)
        {
            message.second << tr(
                "The USB controller emulation is not currently enabled on the USB page. "
                "This is needed to support an emulated USB pointing device. "
                "It will be enabled automatically if you confirm your changes.");
        }

        /* Serialize message: */
        if (!message.second.isEmpty())
            messages << message;
    }

    /* CPU tab: */
    {
        /* Prepare message: */
        UIValidationMessage message;
        message.first = UICommon::removeAccelMark(m_pTabWidgetSystem->tabText(1));

        /* VCPU amount test: */
        const int cTotalCPUs = uiCommon().host().GetProcessorOnlineCoreCount();
        if (m_pSliderCPUCount->value() > 2 * cTotalCPUs)
        {
            message.second << tr(
                "For performance reasons, the number of virtual CPUs attached to the virtual machine may not be more than twice the number "
                "of physical CPUs on the host (<b>%1</b>). Please reduce the number of virtual CPUs.")
                .arg(cTotalCPUs);
            fPass = false;
        }
        else if (m_pSliderCPUCount->value() > cTotalCPUs)
        {
            message.second << tr(
                "More virtual CPUs are assigned to the virtual machine than the number of physical CPUs on the host system (<b>%1</b>). "
                "This is likely to degrade the performance of your virtual machine. Please consider reducing the number of virtual CPUs.")
                .arg(cTotalCPUs);
        }

        /* VCPU vs IO-APIC test: */
        if (m_pSliderCPUCount->value() > 1 && !m_pCheckBoxApic->isChecked())
        {
            message.second << tr(
                "The I/O APIC feature is not currently enabled in the Motherboard section of the System page. "
                "This is needed to support more than one virtual processor. "
                "It will be enabled automatically if you confirm your changes.");
        }

        /* VCPU: */
        if (m_pSliderCPUCount->value() > 1)
        {
            /* HW Virt Ex test: */
            if (isHWVirtExSupported() && !isHWVirtExEnabled())
            {
                message.second << tr(
                    "The hardware virtualization is not currently enabled in the Acceleration section of the System page. "
                    "This is needed to support more than one virtual processor. "
                    "It will be enabled automatically if you confirm your changes.");
            }
        }

        /* CPU execution cap test: */
        if (m_pSliderCPUExecCap->value() < (int)m_uMedGuestCPUExecCap)
        {
            message.second << tr("The processor execution cap is set to a low value. This may make the machine feel slow to respond.");
        }

        /* Warn user about possible performance degradation and suggest lowering # of CPUs assigned to the VM instead: */
        if (m_pSliderCPUExecCap->value() < 100)
        {
            if (m_uMaxGuestCPU > 1 && m_pSliderCPUCount->value() > 1)
            {
                message.second << tr("Please consider lowering the number of CPUs assigned to the virtual machine rather "
                                     "than setting the processor execution cap.");
            }
            else if (m_uMaxGuestCPU > 1)
            {
                message.second << tr("Lowering the processor execution cap may result in a decline in performance.");
            }
        }

        /* Nested HW Virt Ex: */
        if (isNestedHWVirtExEnabled())
        {
            /* HW Virt Ex test: */
            if (isHWVirtExSupported() && !isHWVirtExEnabled())
            {
                message.second << tr(
                    "The hardware virtualization is not currently enabled in the Acceleration section of the System page. "
                    "This is needed to support nested hardware virtualization. "
                    "It will be enabled automatically if you confirm your changes.");
            }

            /* Nested Paging test: */
            if (isHWVirtExSupported() && isNestedPagingSupported() && !isNestedPagingEnabled())
            {
                message.second << tr(
                    "The nested paging is not currently enabled in the Acceleration section of the System page. "
                    "This is needed to support nested hardware virtualization. "
                    "It will be enabled automatically if you confirm your changes.");
            }
        }

        /* Serialize message: */
        if (!message.second.isEmpty())
            messages << message;
    }

    /* Acceleration tab: */
    {
        /* Prepare message: */
        UIValidationMessage message;
        message.first = UICommon::removeAccelMark(m_pTabWidgetSystem->tabText(2));

        /* HW Virt Ex test: */
        if (!isHWVirtExSupported() && isHWVirtExEnabled())
        {
            message.second << tr(
                "The hardware virtualization is enabled in the Acceleration section of the System page although "
                "it is not supported by the host system. It should be disabled in order to start the virtual system.");

            fPass = false;
        }

        /* Nested Paging: */
        if (isNestedPagingEnabled())
        {
            /* HW Virt Ex test: */
            if (isHWVirtExSupported() && !isHWVirtExEnabled())
            {
                message.second << tr(
                    "The hardware virtualization is not currently enabled in the Acceleration section of the System page. "
                    "This is needed for nested paging support. "
                    "It will be enabled automatically if you confirm your changes.");
            }
        }

        /* Serialize message: */
        if (!message.second.isEmpty())
            messages << message;

    }

    /* Return result: */
    return fPass;
}

void UIMachineSettingsSystem::setOrderAfter(QWidget *pWidget)
{
    /* Configure navigation for 'motherboard' tab: */
    setTabOrder(pWidget, m_pTabWidgetSystem->focusProxy());
    setTabOrder(m_pTabWidgetSystem->focusProxy(), m_pBaseMemoryEditor);
    setTabOrder(m_pBaseMemoryEditor, m_pBootOrderEditor);
    setTabOrder(m_pBootOrderEditor, m_pComboChipsetType);
    setTabOrder(m_pComboChipsetType, m_pComboPointingHIDType);
    setTabOrder(m_pComboPointingHIDType, m_pCheckBoxApic);
    setTabOrder(m_pCheckBoxApic, m_pCheckBoxEFI);
    setTabOrder(m_pCheckBoxEFI, m_pCheckBoxUseUTC);

    /* Configure navigation for 'processor' tab: */
    setTabOrder(m_pCheckBoxUseUTC, m_pSliderCPUCount);
    setTabOrder(m_pSliderCPUCount, m_pEditorCPUCount);
    setTabOrder(m_pEditorCPUCount, m_pSliderCPUExecCap);
    setTabOrder(m_pSliderCPUExecCap, m_pEditorCPUExecCap);
    setTabOrder(m_pEditorCPUExecCap, m_pComboParavirtProviderType);

    /* Configure navigation for 'acceleration' tab: */
    setTabOrder(m_pComboParavirtProviderType, m_pCheckBoxPAE);
    setTabOrder(m_pCheckBoxPAE, m_pCheckBoxNestedVirtualization);
    setTabOrder(m_pCheckBoxNestedVirtualization, m_pCheckBoxVirtualization);
    setTabOrder(m_pCheckBoxVirtualization, m_pCheckBoxNestedPaging);
}

void UIMachineSettingsSystem::retranslateUi()
{
    m_pBaseMemoryLabel->setText(QApplication::translate("UIMachineSettingsSystem", "Base &Memory:"));
    m_pBaseMemoryEditor->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "Controls the amount of memory provided "
                                                              "to the virtual machine. If you assign too much, the machine might not start."));
    m_pBootOrderLabel->setText(QApplication::translate("UIMachineSettingsSystem", "&Boot Order:"));
    m_pBootOrderEditor->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "Defines the boot device order. Use the "
                                                             "checkboxes on the left to enable or disable individual boot devices."
                                                             "Move items up and down to change the device order."));
    m_pLabelChipsetType->setText(QApplication::translate("UIMachineSettingsSystem", "&Chipset:"));
    m_pComboChipsetType->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "Selects the chipset to be emulated in "
                                                              "this virtual machine. Note that the ICH9 chipset emulation is experimental "
                                                              "and not recommended except for guest systems (such as Mac OS X) which require it."));
    m_pLabelPointingHIDType->setText(QApplication::translate("UIMachineSettingsSystem", "&Pointing Device:"));
    m_pComboPointingHIDType->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "Determines whether the emulated "
                                                                  "pointing device is a standard PS/2 mouse, a USB tablet or a "
                                                                  "USB multi-touch tablet."));
    m_pLabelMotherboardExtended->setText(QApplication::translate("UIMachineSettingsSystem", "Extended Features:"));
    m_pCheckBoxApic->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "When checked, the virtual machine will "
                                                          "support the Input Output APIC (I/O APIC), which may slightly decrease "
                                                          "performance. <b>Note:</b> don't disable this feature after having "
                                                          "installed a Windows guest operating system!"));
    m_pCheckBoxApic->setText(QApplication::translate("UIMachineSettingsSystem", "Enable &I/O APIC"));
    m_pCheckBoxEFI->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "When checked, the guest will support the "
                                                         "Extended Firmware Interface (EFI), which is required to boot certain "
                                                         "guest OSes. Non-EFI aware OSes will not be able to boot if this option is activated."));
    m_pCheckBoxEFI->setText(QApplication::translate("UIMachineSettingsSystem", "Enable &EFI (special OSes only)"));
    m_pCheckBoxUseUTC->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "When checked, the RTC device will report "
                                                            "the time in UTC, otherwise in local (host) time. Unix usually expects "
                                                            "the hardware clock to be set to UTC."));
    m_pCheckBoxUseUTC->setText(QApplication::translate("UIMachineSettingsSystem", "Hardware Clock in &UTC Time"));
    m_pTabWidgetSystem->setTabText(m_pTabWidgetSystem->indexOf(m_pTabMotherboard), QApplication::translate("UIMachineSettingsSystem", "&Motherboard"));
    m_pLabelCPUCount->setText(QApplication::translate("UIMachineSettingsSystem", "&Processor(s):"));
    m_pSliderCPUCount->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "Controls the number of virtual CPUs in the "
                                                            "virtual machine. You need hardware virtualization support on your host "
                                                            "system to use more than one virtual CPU."));
    m_pEditorCPUCount->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "Controls the number of virtual CPUs in the "
                                                            "virtual machine. You need hardware virtualization support on your host "
                                                            "system to use more than one virtual CPU."));
    m_pLabelCPUExecCap->setText(QApplication::translate("UIMachineSettingsSystem", "&Execution Cap:"));
    m_pSliderCPUExecCap->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "Limits the amount of time that each virtual "
                                                              "CPU is allowed to run for. Each virtual CPU will be allowed to use up "
                                                              "to this percentage of the processing time available on one physical CPU. "
                                                              "The execution cap can be disabled by setting it to 100%. Setting the cap "
                                                              "too low can make the machine feel slow to respond."));
    m_pEditorCPUExecCap->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "Limits the amount of time that each virtual CPU "
                                                              "is allowed to run for. Each virtual CPU will be allowed to use up "
                                                              "to this percentage of the processing time available on one physical "
                                                              "CPU. The execution cap can be disabled by setting it to 100%. Setting "
                                                              "the cap too low can make the machine feel slow to respond."));
    m_pEditorCPUExecCap->setSuffix(QApplication::translate("UIMachineSettingsSystem", "%"));
    m_pLabelCPUExtended->setText(QApplication::translate("UIMachineSettingsSystem", "Extended Features:"));
    m_pCheckBoxPAE->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "When checked, the Physical Address Extension "
                                                         "(PAE) feature of the host CPU will be exposed to the virtual machine."));
    m_pCheckBoxPAE->setText(QApplication::translate("UIMachineSettingsSystem", "Enable PA&E/NX"));
    m_pCheckBoxNestedVirtualization->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "When checked, the nested hardware "
                                                                          "virtualization CPU feature will be exposed to the virtual machine."));
    m_pCheckBoxNestedVirtualization->setText(QApplication::translate("UIMachineSettingsSystem", "Enable Nested &VT-x/AMD-V"));
    m_pTabWidgetSystem->setTabText(m_pTabWidgetSystem->indexOf(m_pTabCPU), QApplication::translate("UIMachineSettingsSystem", "&Processor"));
    m_pLabelParavirtProvider->setText(QApplication::translate("UIMachineSettingsSystem", "&Paravirtualization Interface:"));
    m_pComboParavirtProviderType->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "Selects the paravirtualization "
                                                                       "guest interface provider to be used by this virtual machine."));
    m_pLabelVirtualization->setText(QApplication::translate("UIMachineSettingsSystem", "Hardware Virtualization:"));
    m_pCheckBoxVirtualization->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "When checked, the virtual machine "
                                                                    "will try to make use of the host CPU's hardware virtualization "
                                                                    "extensions such as Intel VT-x and AMD-V."));
    m_pCheckBoxVirtualization->setText(QApplication::translate("UIMachineSettingsSystem", "Enable &VT-x/AMD-V"));
    m_pCheckBoxNestedPaging->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "When checked, the virtual machine will "
                                                                  "try to make use of the nested paging extension of Intel VT-x and AMD-V."));
    m_pCheckBoxNestedPaging->setText(QApplication::translate("UIMachineSettingsSystem", "Enable Nested Pa&ging"));
    m_pTabWidgetSystem->setTabText(m_pTabWidgetSystem->indexOf(m_pTabAcceleration), QApplication::translate("UIMachineSettingsSystem", "Acce&leration"));

    /* Retranslate the cpu slider legend: */
    m_pLabelCPUMin->setText(tr("%1 CPU", "%1 is 1 for now").arg(m_uMinGuestCPU));
    m_pLabelCPUMax->setText(tr("%1 CPUs", "%1 is host cpu count * 2 for now").arg(m_uMaxGuestCPU));

    /* Retranslate the cpu cap slider legend: */
    m_pLabelCPUExecCapMin->setText(tr("%1%").arg(m_uMinGuestCPUExecCap));
    m_pLabelCPUExecCapMax->setText(tr("%1%").arg(m_uMaxGuestCPUExecCap));

    /* Retranslate combo-boxes: */
    retranslateComboChipsetType();
    retranslateComboPointingHIDType();
    retranslateComboParavirtProvider();
}

void UIMachineSettingsSystem::polishPage()
{
    /* Get old system data from the cache: */
    const UIDataSettingsMachineSystem &systemData = m_pCache->base();

    /* Polish 'Motherboard' availability: */
    m_pBaseMemoryLabel->setEnabled(isMachineOffline());
    m_pBaseMemoryEditor->setEnabled(isMachineOffline());
    m_pBootOrderLabel->setEnabled(isMachineOffline());
    m_pBootOrderEditor->setEnabled(isMachineOffline());
    m_pLabelChipsetType->setEnabled(isMachineOffline());
    m_pComboChipsetType->setEnabled(isMachineOffline());
    m_pLabelPointingHIDType->setEnabled(isMachineOffline());
    m_pComboPointingHIDType->setEnabled(isMachineOffline());
    m_pLabelMotherboardExtended->setEnabled(isMachineOffline());
    m_pCheckBoxApic->setEnabled(isMachineOffline());
    m_pCheckBoxEFI->setEnabled(isMachineOffline());
    m_pCheckBoxUseUTC->setEnabled(isMachineOffline());

    /* Polish 'Processor' availability: */
    m_pLabelCPUCount->setEnabled(isMachineOffline());
    m_pLabelCPUMin->setEnabled(isMachineOffline());
    m_pLabelCPUMax->setEnabled(isMachineOffline());
    m_pSliderCPUCount->setEnabled(isMachineOffline() && systemData.m_fSupportedHwVirtEx);
    m_pEditorCPUCount->setEnabled(isMachineOffline() && systemData.m_fSupportedHwVirtEx);
    m_pLabelCPUExecCap->setEnabled(isMachineInValidMode());
    m_pLabelCPUExecCapMin->setEnabled(isMachineInValidMode());
    m_pLabelCPUExecCapMax->setEnabled(isMachineInValidMode());
    m_pSliderCPUExecCap->setEnabled(isMachineInValidMode());
    m_pEditorCPUExecCap->setEnabled(isMachineInValidMode());
    m_pLabelCPUExtended->setEnabled(isMachineOffline());
    m_pCheckBoxPAE->setEnabled(isMachineOffline() && systemData.m_fSupportedPAE);
    m_pCheckBoxNestedVirtualization->setEnabled(   (systemData.m_fSupportedNestedHwVirtEx && isMachineOffline())
                                                || (systemData.m_fEnabledNestedHwVirtEx && isMachineOffline()));

    /* Polish 'Acceleration' availability: */
    m_pCheckBoxVirtualization->setEnabled(   (systemData.m_fSupportedHwVirtEx && isMachineOffline())
                                          || (systemData.m_fEnabledHwVirtEx && isMachineOffline()));
    m_pCheckBoxNestedPaging->setEnabled(   m_pCheckBoxVirtualization->isChecked()
                                        && (   (systemData.m_fSupportedNestedPaging && isMachineOffline())
                                            || (systemData.m_fEnabledNestedPaging && isMachineOffline())));
    m_pLabelParavirtProvider->setEnabled(isMachineOffline());
    m_pComboParavirtProviderType->setEnabled(isMachineOffline());
    m_pLabelVirtualization->setEnabled(isMachineOffline());
}

void UIMachineSettingsSystem::sltHandleCPUCountSliderChange()
{
    /* Apply new memory-size value: */
    m_pEditorCPUCount->blockSignals(true);
    m_pEditorCPUCount->setValue(m_pSliderCPUCount->value());
    m_pEditorCPUCount->blockSignals(false);

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsSystem::sltHandleCPUCountEditorChange()
{
    /* Apply new memory-size value: */
    m_pSliderCPUCount->blockSignals(true);
    m_pSliderCPUCount->setValue(m_pEditorCPUCount->value());
    m_pSliderCPUCount->blockSignals(false);

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsSystem::sltHandleCPUExecCapSliderChange()
{
    /* Apply new memory-size value: */
    m_pEditorCPUExecCap->blockSignals(true);
    m_pEditorCPUExecCap->setValue(m_pSliderCPUExecCap->value());
    m_pEditorCPUExecCap->blockSignals(false);

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsSystem::sltHandleCPUExecCapEditorChange()
{
    /* Apply new memory-size value: */
    m_pSliderCPUExecCap->blockSignals(true);
    m_pSliderCPUExecCap->setValue(m_pEditorCPUExecCap->value());
    m_pSliderCPUExecCap->blockSignals(false);

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsSystem::sltHandleHwVirtExToggle()
{
    /* Update Nested Paging checkbox: */
    AssertPtrReturnVoid(m_pCache);
    m_pCheckBoxNestedPaging->setEnabled(   m_pCheckBoxVirtualization->isChecked()
                                        && (   (m_pCache->base().m_fSupportedNestedPaging && isMachineOffline())
                                            || (m_pCache->base().m_fEnabledNestedPaging && isMachineOffline())));

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsSystem::prepare()
{
    prepareWidgets();

    /* Prepare cache: */
    m_pCache = new UISettingsCacheMachineSystem;
    AssertPtrReturnVoid(m_pCache);

    /* Tree-widget created in the .ui file. */
    {
        /* Prepare 'Motherboard' tab: */
        prepareTabMotherboard();
        /* Prepare 'Processor' tab: */
        prepareTabProcessor();
        /* Prepare 'Acceleration' tab: */
        prepareTabAcceleration();
        /* Prepare connections: */
        prepareConnections();
    }

    /* Apply language settings: */
    retranslateUi();
}

void UIMachineSettingsSystem::prepareWidgets()
{
    // QVBoxLayout *pVBoxLayout1;
    // QHBoxLayout *pHBoxLayout2;
    // QSpacerItem *pSpacerHorizontal5;
    // QVBoxLayout *pVBoxLayout2;
    // QHBoxLayout *pHBoxLayout3;
    // QSpacerItem *pSpacerHorizontal6;
    // QSpacerItem *pSpacerVertical3;
    // QGridLayout *pGridLayout2;
    // QHBoxLayout *pHBoxLayout4;
    // QSpacerItem *pSpacerHorizontal7;
    // QVBoxLayout *pVerticalLayout1;
    // QVBoxLayout *pVerticalLayout2;
    // QSpacerItem *pSpacerVertical4;

    if (objectName().isEmpty())
        setObjectName(QStringLiteral("UIMachineSettingsSystem"));
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    pMainLayout->setObjectName(QStringLiteral("pMainLayout"));
    m_pTabWidgetSystem = new QITabWidget();
    m_pTabWidgetSystem->setObjectName(QStringLiteral("m_pTabWidgetSystem"));
    m_pTabMotherboard = new QWidget();
    m_pTabMotherboard->setObjectName(QStringLiteral("m_pTabMotherboard"));
    QGridLayout *pGridLayout = new QGridLayout(m_pTabMotherboard);
    pGridLayout->setObjectName(QStringLiteral("gridLayout"));
    m_pBaseMemoryLabel = new QLabel(m_pTabMotherboard);
    m_pBaseMemoryLabel->setObjectName(QStringLiteral("m_pBaseMemoryLabel"));
    m_pBaseMemoryLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    pGridLayout->addWidget(m_pBaseMemoryLabel, 0, 0, 1, 1);

    m_pBaseMemoryEditor = new UIBaseMemoryEditor(m_pTabMotherboard);
    m_pBaseMemoryEditor->setObjectName(QStringLiteral("m_pBaseMemoryEditor"));
    QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    sizePolicy.setHorizontalStretch(1);
    sizePolicy.setVerticalStretch(0);
    sizePolicy.setHeightForWidth(m_pBaseMemoryEditor->sizePolicy().hasHeightForWidth());
    m_pBaseMemoryEditor->setSizePolicy(sizePolicy);
    pGridLayout->addWidget(m_pBaseMemoryEditor, 0, 1, 2, 3);

    m_pBootOrderLabel = new QLabel(m_pTabMotherboard);
    m_pBootOrderLabel->setObjectName(QStringLiteral("m_pBootOrderLabel"));
    m_pBootOrderLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    pGridLayout->addWidget(m_pBootOrderLabel, 2, 0, 1, 1);

    m_pBootOrderEditor = new UIBootOrderEditor(m_pTabMotherboard);
    m_pBootOrderEditor->setObjectName(QStringLiteral("m_pBootOrderEditor"));
    QSizePolicy sizePolicy1(QSizePolicy::Fixed, QSizePolicy::Fixed);
    sizePolicy1.setHorizontalStretch(0);
    sizePolicy1.setVerticalStretch(0);
    sizePolicy1.setHeightForWidth(m_pBootOrderEditor->sizePolicy().hasHeightForWidth());
    m_pBootOrderEditor->setSizePolicy(sizePolicy1);
    pGridLayout->addWidget(m_pBootOrderEditor, 2, 1, 3, 3);

    m_pLabelChipsetType = new QLabel(m_pTabMotherboard);
    m_pLabelChipsetType->setObjectName(QStringLiteral("m_pLabelChipsetType"));
    m_pLabelChipsetType->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    pGridLayout->addWidget(m_pLabelChipsetType, 5, 0, 1, 1);

    QHBoxLayout *pHBoxLayout = new QHBoxLayout();
    pHBoxLayout->setObjectName(QStringLiteral("hboxLayout"));
    m_pComboChipsetType = new QComboBox(m_pTabMotherboard);
    m_pComboChipsetType->setObjectName(QStringLiteral("m_pComboChipsetType"));
    QSizePolicy sizePolicy2(QSizePolicy::Minimum, QSizePolicy::Fixed);
    sizePolicy2.setHorizontalStretch(0);
    sizePolicy2.setVerticalStretch(0);
    sizePolicy2.setHeightForWidth(m_pComboChipsetType->sizePolicy().hasHeightForWidth());
    m_pComboChipsetType->setSizePolicy(sizePolicy2);
    pHBoxLayout->addWidget(m_pComboChipsetType);
    QSpacerItem *pSpacerHorizontal3 = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);

    pHBoxLayout->addItem(pSpacerHorizontal3);
    pGridLayout->addLayout(pHBoxLayout, 5, 1, 1, 3);

    m_pLabelPointingHIDType = new QLabel(m_pTabMotherboard);
    m_pLabelPointingHIDType->setObjectName(QStringLiteral("m_pLabelPointingHIDType"));
    m_pLabelPointingHIDType->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    pGridLayout->addWidget(m_pLabelPointingHIDType, 6, 0, 1, 1);

    QHBoxLayout *hboxLayout1 = new QHBoxLayout();
    hboxLayout1->setObjectName(QStringLiteral("hboxLayout1"));
    m_pComboPointingHIDType = new QComboBox(m_pTabMotherboard);
    m_pComboPointingHIDType->setObjectName(QStringLiteral("m_pComboPointingHIDType"));
    sizePolicy2.setHeightForWidth(m_pComboPointingHIDType->sizePolicy().hasHeightForWidth());
    m_pComboPointingHIDType->setSizePolicy(sizePolicy2);
    hboxLayout1->addWidget(m_pComboPointingHIDType);
    QSpacerItem *pSpacerHorizontal4 = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
    hboxLayout1->addItem(pSpacerHorizontal4);
    pGridLayout->addLayout(hboxLayout1, 6, 1, 1, 3);

    m_pLabelMotherboardExtended = new QLabel(m_pTabMotherboard);
    m_pLabelMotherboardExtended->setObjectName(QStringLiteral("m_pLabelMotherboardExtended"));
    m_pLabelMotherboardExtended->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    pGridLayout->addWidget(m_pLabelMotherboardExtended, 7, 0, 1, 1);

    m_pCheckBoxApic = new QCheckBox(m_pTabMotherboard);
    m_pCheckBoxApic->setObjectName(QStringLiteral("m_pCheckBoxApic"));
    pGridLayout->addWidget(m_pCheckBoxApic, 7, 1, 1, 3);

    m_pCheckBoxEFI = new QCheckBox(m_pTabMotherboard);
    m_pCheckBoxEFI->setObjectName(QStringLiteral("m_pCheckBoxEFI"));
    pGridLayout->addWidget(m_pCheckBoxEFI, 8, 1, 1, 3);

    m_pCheckBoxUseUTC = new QCheckBox(m_pTabMotherboard);
    m_pCheckBoxUseUTC->setObjectName(QStringLiteral("m_pCheckBoxUseUTC"));
    pGridLayout->addWidget(m_pCheckBoxUseUTC, 9, 1, 1, 3);

    QSpacerItem *pSpacerVertical2 = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    pGridLayout->addItem(pSpacerVertical2, 10, 0, 1, 4);

    m_pTabWidgetSystem->addTab(m_pTabMotherboard, QString());
    m_pTabCPU = new QWidget();
    m_pTabCPU->setObjectName(QStringLiteral("m_pTabCPU"));
    QGridLayout *pGridLayout1 = new QGridLayout(m_pTabCPU);
    pGridLayout1->setObjectName(QStringLiteral("pGridLayout1"));
    m_pLabelCPUCount = new QLabel(m_pTabCPU);
    m_pLabelCPUCount->setObjectName(QStringLiteral("m_pLabelCPUCount"));
    m_pLabelCPUCount->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    pGridLayout1->addWidget(m_pLabelCPUCount, 0, 0, 1, 1);

    QWidget *pContainerWidget = new QWidget(m_pTabCPU);
    pContainerWidget->setObjectName(QStringLiteral("widget"));
    sizePolicy.setHeightForWidth(pContainerWidget->sizePolicy().hasHeightForWidth());
    pContainerWidget->setSizePolicy(sizePolicy);
    QVBoxLayout *pVBoxLayout1 = new QVBoxLayout(pContainerWidget);
    pVBoxLayout1->setSpacing(0);
    pVBoxLayout1->setObjectName(QStringLiteral("pVBoxLayout1"));
    pVBoxLayout1->setContentsMargins(0, 0, 0, 0);
    m_pSliderCPUCount = new QIAdvancedSlider(pContainerWidget);
    m_pSliderCPUCount->setObjectName(QStringLiteral("m_pSliderCPUCount"));
    m_pSliderCPUCount->setOrientation(Qt::Horizontal);
    pVBoxLayout1->addWidget(m_pSliderCPUCount);

    QHBoxLayout *pHBoxLayout2 = new QHBoxLayout();
    pHBoxLayout2->setSpacing(0);
    pHBoxLayout2->setObjectName(QStringLiteral("pHBoxLayout2"));
    m_pLabelCPUMin = new QLabel(pContainerWidget);
    m_pLabelCPUMin->setObjectName(QStringLiteral("m_pLabelCPUMin"));
    pHBoxLayout2->addWidget(m_pLabelCPUMin);

    QSpacerItem *pSpacerHorizontal5 = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
    pHBoxLayout2->addItem(pSpacerHorizontal5);

    m_pLabelCPUMax = new QLabel(pContainerWidget);
    m_pLabelCPUMax->setObjectName(QStringLiteral("m_pLabelCPUMax"));
    pHBoxLayout2->addWidget(m_pLabelCPUMax);

    pVBoxLayout1->addLayout(pHBoxLayout2);
    pGridLayout1->addWidget(pContainerWidget, 0, 1, 2, 1);

    m_pEditorCPUCount = new QSpinBox(m_pTabCPU);
    m_pEditorCPUCount->setObjectName(QStringLiteral("m_pEditorCPUCount"));
    pGridLayout1->addWidget(m_pEditorCPUCount, 0, 2, 1, 1);

    m_pLabelCPUExecCap = new QLabel(m_pTabCPU);
    m_pLabelCPUExecCap->setObjectName(QStringLiteral("m_pLabelCPUExecCap"));
    m_pLabelCPUExecCap->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    pGridLayout1->addWidget(m_pLabelCPUExecCap, 2, 0, 1, 1);

    QVBoxLayout *pVBoxLayout2 = new QVBoxLayout();
    pVBoxLayout2->setSpacing(0);
    pVBoxLayout2->setObjectName(QStringLiteral("pVBoxLayout2"));
    m_pSliderCPUExecCap = new QIAdvancedSlider(m_pTabCPU);
    m_pSliderCPUExecCap->setObjectName(QStringLiteral("m_pSliderCPUExecCap"));
    m_pSliderCPUExecCap->setOrientation(Qt::Horizontal);
    pVBoxLayout2->addWidget(m_pSliderCPUExecCap);

    QHBoxLayout *pHBoxLayout3 = new QHBoxLayout();
    pHBoxLayout3->setSpacing(0);
    pHBoxLayout3->setObjectName(QStringLiteral("pHBoxLayout3"));
    m_pLabelCPUExecCapMin = new QLabel(m_pTabCPU);
    m_pLabelCPUExecCapMin->setObjectName(QStringLiteral("m_pLabelCPUExecCapMin"));
    pHBoxLayout3->addWidget(m_pLabelCPUExecCapMin);
    QSpacerItem *pSpacerHorizontal6 = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
    pHBoxLayout3->addItem(pSpacerHorizontal6);

    m_pLabelCPUExecCapMax = new QLabel(m_pTabCPU);
    m_pLabelCPUExecCapMax->setObjectName(QStringLiteral("m_pLabelCPUExecCapMax"));
    pHBoxLayout3->addWidget(m_pLabelCPUExecCapMax);
    pVBoxLayout2->addLayout(pHBoxLayout3);
    pGridLayout1->addLayout(pVBoxLayout2, 2, 1, 2, 1);

    m_pEditorCPUExecCap = new QSpinBox(m_pTabCPU);
    m_pEditorCPUExecCap->setObjectName(QStringLiteral("m_pEditorCPUExecCap"));
    pGridLayout1->addWidget(m_pEditorCPUExecCap, 2, 2, 1, 1);

    m_pLabelCPUExtended = new QLabel(m_pTabCPU);
    m_pLabelCPUExtended->setObjectName(QStringLiteral("m_pLabelCPUExtended"));
    m_pLabelCPUExtended->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    pGridLayout1->addWidget(m_pLabelCPUExtended, 4, 0, 1, 1);

    m_pCheckBoxPAE = new QCheckBox(m_pTabCPU);
    m_pCheckBoxPAE->setObjectName(QStringLiteral("m_pCheckBoxPAE"));
    pGridLayout1->addWidget(m_pCheckBoxPAE, 4, 1, 1, 2);

    m_pCheckBoxNestedVirtualization = new QCheckBox(m_pTabCPU);
    m_pCheckBoxNestedVirtualization->setObjectName(QStringLiteral("m_pCheckBoxNestedVirtualization"));
    pGridLayout1->addWidget(m_pCheckBoxNestedVirtualization, 5, 1, 1, 2);
    QSpacerItem *pSpacerVertical3 = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    pGridLayout1->addItem(pSpacerVertical3, 6, 0, 1, 3);

    m_pTabWidgetSystem->addTab(m_pTabCPU, QString());
    m_pTabAcceleration = new QWidget();
    m_pTabAcceleration->setObjectName(QStringLiteral("m_pTabAcceleration"));
    QGridLayout *pGridLayout2 = new QGridLayout(m_pTabAcceleration);
    pGridLayout2->setObjectName(QStringLiteral("pGridLayout2"));
    m_pLabelParavirtProvider = new QLabel(m_pTabAcceleration);
    m_pLabelParavirtProvider->setObjectName(QStringLiteral("m_pLabelParavirtProvider"));
    m_pLabelParavirtProvider->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    pGridLayout2->addWidget(m_pLabelParavirtProvider, 0, 0, 1, 1);

    QHBoxLayout *pHBoxLayout4 = new QHBoxLayout();
    pHBoxLayout4->setObjectName(QStringLiteral("pHBoxLayout4"));
    m_pComboParavirtProviderType = new QComboBox(m_pTabAcceleration);
    m_pComboParavirtProviderType->setObjectName(QStringLiteral("m_pComboParavirtProviderType"));
    sizePolicy2.setHeightForWidth(m_pComboParavirtProviderType->sizePolicy().hasHeightForWidth());
    m_pComboParavirtProviderType->setSizePolicy(sizePolicy2);
    pHBoxLayout4->addWidget(m_pComboParavirtProviderType);
    QSpacerItem *pSpacerHorizontal7 = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
    pHBoxLayout4->addItem(pSpacerHorizontal7);
    pGridLayout2->addLayout(pHBoxLayout4, 0, 1, 1, 1);

    QVBoxLayout *pVerticalLayout1 = new QVBoxLayout();
    pVerticalLayout1->setObjectName(QStringLiteral("pVerticalLayout1"));
    m_pLabelVirtualization = new QLabel(m_pTabAcceleration);
    m_pLabelVirtualization->setObjectName(QStringLiteral("m_pLabelVirtualization"));
    m_pLabelVirtualization->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    pVerticalLayout1->addWidget(m_pLabelVirtualization);

    m_pWidgetPlaceholder = new QWidget(m_pTabAcceleration);
    m_pWidgetPlaceholder->setObjectName(QStringLiteral("m_pWidgetPlaceholder"));
    pVerticalLayout1->addWidget(m_pWidgetPlaceholder);
    pGridLayout2->addLayout(pVerticalLayout1, 1, 0, 1, 1);

    QVBoxLayout *pVerticalLayout2 = new QVBoxLayout();
    pVerticalLayout2->setObjectName(QStringLiteral("pVerticalLayout2"));
    m_pCheckBoxVirtualization = new QCheckBox(m_pTabAcceleration);
    m_pCheckBoxVirtualization->setObjectName(QStringLiteral("m_pCheckBoxVirtualization"));
    QSizePolicy sizePolicy3(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    sizePolicy3.setHorizontalStretch(1);
    sizePolicy3.setVerticalStretch(0);
    sizePolicy3.setHeightForWidth(m_pCheckBoxVirtualization->sizePolicy().hasHeightForWidth());
    m_pCheckBoxVirtualization->setSizePolicy(sizePolicy3);
    pVerticalLayout2->addWidget(m_pCheckBoxVirtualization);

    m_pCheckBoxNestedPaging = new QCheckBox(m_pTabAcceleration);
    m_pCheckBoxNestedPaging->setObjectName(QStringLiteral("m_pCheckBoxNestedPaging"));
    QSizePolicy sizePolicy4(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    sizePolicy4.setHorizontalStretch(0);
    sizePolicy4.setVerticalStretch(0);
    sizePolicy4.setHeightForWidth(m_pCheckBoxNestedPaging->sizePolicy().hasHeightForWidth());
    m_pCheckBoxNestedPaging->setSizePolicy(sizePolicy4);
    pVerticalLayout2->addWidget(m_pCheckBoxNestedPaging);
    pGridLayout2->addLayout(pVerticalLayout2, 1, 1, 1, 1);

    QSpacerItem *pSpacerVertical4 = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    pGridLayout2->addItem(pSpacerVertical4, 2, 0, 1, 2);

    m_pTabWidgetSystem->addTab(m_pTabAcceleration, QString());
    pMainLayout->addWidget(m_pTabWidgetSystem);

    m_pBootOrderLabel->setBuddy(m_pBootOrderEditor);
    m_pLabelChipsetType->setBuddy(m_pComboChipsetType);
    m_pLabelPointingHIDType->setBuddy(m_pComboPointingHIDType);
    m_pLabelCPUCount->setBuddy(m_pEditorCPUCount);
    m_pLabelCPUExecCap->setBuddy(m_pEditorCPUExecCap);
    m_pLabelParavirtProvider->setBuddy(m_pComboParavirtProviderType);
}

void UIMachineSettingsSystem::prepareTabMotherboard()
{
    /* Tab and it's layout created in the .ui file. */
    {
        /* Base-memory label and editor created in the .ui file. */
        AssertPtrReturnVoid(m_pBaseMemoryLabel);
        AssertPtrReturnVoid(m_pBaseMemoryEditor);
        {
            /* Configure label & editor: */
            m_pBaseMemoryLabel->setBuddy(m_pBaseMemoryEditor->focusProxy());
        }

        /* Boot-order label and editor created in the .ui file. */
        AssertPtrReturnVoid(m_pBootOrderLabel);
        AssertPtrReturnVoid(m_pBootOrderEditor);
        {
            /* Configure label & editor: */
            m_pBootOrderLabel->setBuddy(m_pBootOrderEditor->focusProxy());
        }

        /* Chipset Type combo-box created in the .ui file. */
        AssertPtrReturnVoid(m_pComboChipsetType);
        {
            /* Configure combo-box: */
            m_pComboChipsetType->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        }

        /* Pointing HID Type combo-box created in the .ui file. */
        AssertPtrReturnVoid(m_pComboPointingHIDType);
        {
            /* Configure combo-box: */
            m_pComboPointingHIDType->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        }
    }
}

void UIMachineSettingsSystem::prepareTabProcessor()
{
    /* Prepare common variables: */
    const CSystemProperties properties = uiCommon().virtualBox().GetSystemProperties();
    const uint uHostCPUs = uiCommon().host().GetProcessorOnlineCoreCount();
    m_uMinGuestCPU = properties.GetMinGuestCPUCount();
    m_uMaxGuestCPU = qMin(2 * uHostCPUs, (uint)properties.GetMaxGuestCPUCount());
    m_uMinGuestCPUExecCap = 1;
    m_uMedGuestCPUExecCap = 40;
    m_uMaxGuestCPUExecCap = 100;

    /* Tab and it's layout created in the .ui file. */
    {
        /* CPU-count slider created in the .ui file. */
        AssertPtrReturnVoid(m_pSliderCPUCount);
        {
            /* Configure slider: */
            m_pSliderCPUCount->setPageStep(1);
            m_pSliderCPUCount->setSingleStep(1);
            m_pSliderCPUCount->setTickInterval(1);
            m_pSliderCPUCount->setMinimum(m_uMinGuestCPU);
            m_pSliderCPUCount->setMaximum(m_uMaxGuestCPU);
            m_pSliderCPUCount->setOptimalHint(1, uHostCPUs);
            m_pSliderCPUCount->setWarningHint(uHostCPUs, m_uMaxGuestCPU);
        }

        /* CPU-count editor created in the .ui file. */
        AssertPtrReturnVoid(m_pEditorCPUCount);
        {
            /* Configure editor: */
            m_pEditorCPUCount->setMinimum(m_uMinGuestCPU);
            m_pEditorCPUCount->setMaximum(m_uMaxGuestCPU);
            uiCommon().setMinimumWidthAccordingSymbolCount(m_pEditorCPUCount, 4);
        }

        /* CPU-execution-cap slider created in the .ui file. */
        AssertPtrReturnVoid(m_pSliderCPUExecCap);
        {
            /* Configure slider: */
            m_pSliderCPUExecCap->setPageStep(10);
            m_pSliderCPUExecCap->setSingleStep(1);
            m_pSliderCPUExecCap->setTickInterval(10);
            /* Setup the scale so that ticks are at page step boundaries: */
            m_pSliderCPUExecCap->setMinimum(m_uMinGuestCPUExecCap);
            m_pSliderCPUExecCap->setMaximum(m_uMaxGuestCPUExecCap);
            m_pSliderCPUExecCap->setWarningHint(m_uMinGuestCPUExecCap, m_uMedGuestCPUExecCap);
            m_pSliderCPUExecCap->setOptimalHint(m_uMedGuestCPUExecCap, m_uMaxGuestCPUExecCap);
        }

        /* CPU-execution-cap editor created in the .ui file. */
        AssertPtrReturnVoid(m_pEditorCPUExecCap);
        {
            /* Configure editor: */
            m_pEditorCPUExecCap->setMinimum(m_uMinGuestCPUExecCap);
            m_pEditorCPUExecCap->setMaximum(m_uMaxGuestCPUExecCap);
            uiCommon().setMinimumWidthAccordingSymbolCount(m_pEditorCPUExecCap, 4);
        }
    }
}

void UIMachineSettingsSystem::prepareTabAcceleration()
{
    /* Tab and it's layout created in the .ui file. */
    {
        /* Other widgets created in the .ui file. */
        AssertPtrReturnVoid(m_pWidgetPlaceholder);
        AssertPtrReturnVoid(m_pCheckBoxVirtualization);
        {
            /* Configure widgets: */
#ifndef VBOX_WITH_RAW_MODE
            /* Hide HW Virt Ex checkbox when raw-mode is not supported: */
            m_pWidgetPlaceholder->setVisible(false);
            m_pCheckBoxVirtualization->setVisible(false);
#endif
        }
    }
}

void UIMachineSettingsSystem::prepareConnections()
{
    /* Configure 'Motherboard' connections: */
    connect(m_pComboChipsetType, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIMachineSettingsSystem::revalidate);
    connect(m_pComboPointingHIDType, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIMachineSettingsSystem::revalidate);
    connect(m_pBaseMemoryEditor, &UIBaseMemoryEditor::sigValidChanged, this, &UIMachineSettingsSystem::revalidate);
    connect(m_pCheckBoxApic, &QCheckBox::stateChanged, this, &UIMachineSettingsSystem::revalidate);

    /* Configure 'Processor' connections: */
    connect(m_pSliderCPUCount, &QIAdvancedSlider::valueChanged, this, &UIMachineSettingsSystem::sltHandleCPUCountSliderChange);
    connect(m_pEditorCPUCount, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &UIMachineSettingsSystem::sltHandleCPUCountEditorChange);
    connect(m_pSliderCPUExecCap, &QIAdvancedSlider::valueChanged, this, &UIMachineSettingsSystem::sltHandleCPUExecCapSliderChange);
    connect(m_pEditorCPUExecCap, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &UIMachineSettingsSystem::sltHandleCPUExecCapEditorChange);
    connect(m_pCheckBoxNestedVirtualization, &QCheckBox::stateChanged, this, &UIMachineSettingsSystem::revalidate);

    /* Configure 'Acceleration' connections: */
    connect(m_pCheckBoxVirtualization, &QCheckBox::stateChanged,
            this, &UIMachineSettingsSystem::sltHandleHwVirtExToggle);
    connect(m_pCheckBoxNestedPaging, &QCheckBox::stateChanged,
            this, &UIMachineSettingsSystem::revalidate);
}

void UIMachineSettingsSystem::cleanup()
{
    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

void UIMachineSettingsSystem::repopulateComboChipsetType()
{
    /* Chipset Type combo-box created in the .ui file. */
    AssertPtrReturnVoid(m_pComboChipsetType);
    {
        /* Clear combo first of all: */
        m_pComboChipsetType->clear();

        /* Load currently supported chipset types: */
        CSystemProperties comProperties = uiCommon().virtualBox().GetSystemProperties();
        QVector<KChipsetType> chipsetTypes = comProperties.GetSupportedChipsetTypes();
        /* Take into account currently cached value: */
        const KChipsetType enmCachedValue = m_pCache->base().m_chipsetType;
        if (!chipsetTypes.contains(enmCachedValue))
            chipsetTypes.prepend(enmCachedValue);

        /* Populate combo finally: */
        foreach (const KChipsetType &enmType, chipsetTypes)
            m_pComboChipsetType->addItem(gpConverter->toString(enmType), QVariant::fromValue(enmType));
    }
}

void UIMachineSettingsSystem::repopulateComboPointingHIDType()
{
    /* Pointing HID Type combo-box created in the .ui file. */
    AssertPtrReturnVoid(m_pComboPointingHIDType);
    {
        /* Clear combo first of all: */
        m_pComboPointingHIDType->clear();

        /* Load currently supported pointing HID types: */
        CSystemProperties comProperties = uiCommon().virtualBox().GetSystemProperties();
        QVector<KPointingHIDType> pointingHidTypes = comProperties.GetSupportedPointingHIDTypes();
        /* Take into account currently cached value: */
        const KPointingHIDType enmCachedValue = m_pCache->base().m_pointingHIDType;
        if (!pointingHidTypes.contains(enmCachedValue))
            pointingHidTypes.prepend(enmCachedValue);

        /* Populate combo finally: */
        foreach (const KPointingHIDType &enmType, pointingHidTypes)
            m_pComboPointingHIDType->addItem(gpConverter->toString(enmType), QVariant::fromValue(enmType));
    }
}

void UIMachineSettingsSystem::repopulateComboParavirtProviderType()
{
    /* Paravirtualization Provider Type combo-box created in the .ui file. */
    AssertPtrReturnVoid(m_pComboParavirtProviderType);
    {
        /* Clear combo first of all: */
        m_pComboParavirtProviderType->clear();

        /* Load currently supported paravirtualization provider types: */
        CSystemProperties comProperties = uiCommon().virtualBox().GetSystemProperties();
        QVector<KParavirtProvider> supportedProviderTypes = comProperties.GetSupportedParavirtProviders();
        /* Take into account currently cached value: */
        const KParavirtProvider enmCachedValue = m_pCache->base().m_paravirtProvider;
        if (!supportedProviderTypes.contains(enmCachedValue))
            supportedProviderTypes.prepend(enmCachedValue);

        /* Populate combo finally: */
        foreach (const KParavirtProvider &enmProvider, supportedProviderTypes)
            m_pComboParavirtProviderType->addItem(gpConverter->toString(enmProvider), QVariant::fromValue(enmProvider));
    }
}

void UIMachineSettingsSystem::retranslateComboChipsetType()
{
    /* For each the element in m_pComboChipsetType: */
    for (int iIndex = 0; iIndex < m_pComboChipsetType->count(); ++iIndex)
    {
        /* Apply retranslated text: */
        const KChipsetType enmType = m_pComboChipsetType->currentData().value<KChipsetType>();
        m_pComboChipsetType->setItemText(iIndex, gpConverter->toString(enmType));
    }
}

void UIMachineSettingsSystem::retranslateComboPointingHIDType()
{
    /* For each the element in m_pComboPointingHIDType: */
    for (int iIndex = 0; iIndex < m_pComboPointingHIDType->count(); ++iIndex)
    {
        /* Apply retranslated text: */
        const KPointingHIDType enmType = m_pComboPointingHIDType->currentData().value<KPointingHIDType>();
        m_pComboPointingHIDType->setItemText(iIndex, gpConverter->toString(enmType));
    }
}

void UIMachineSettingsSystem::retranslateComboParavirtProvider()
{
    /* For each the element in m_pComboParavirtProviderType: */
    for (int iIndex = 0; iIndex < m_pComboParavirtProviderType->count(); ++iIndex)
    {
        /* Apply retranslated text: */
        const KParavirtProvider enmType = m_pComboParavirtProviderType->currentData().value<KParavirtProvider>();
        m_pComboParavirtProviderType->setItemText(iIndex, gpConverter->toString(enmType));
    }
}

bool UIMachineSettingsSystem::saveSystemData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save general settings from the cache: */
    if (fSuccess && isMachineInValidMode() && m_pCache->wasChanged())
    {
        /* Save 'Motherboard' data from the cache: */
        if (fSuccess)
            fSuccess = saveMotherboardData();
        /* Save 'Processor' data from the cache: */
        if (fSuccess)
            fSuccess = saveProcessorData();
        /* Save 'Acceleration' data from the cache: */
        if (fSuccess)
            fSuccess = saveAccelerationData();
    }
    /* Return result: */
    return fSuccess;
}

bool UIMachineSettingsSystem::saveMotherboardData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save 'Motherboard' settings from the cache: */
    if (fSuccess)
    {
        /* Get old system data from the cache: */
        const UIDataSettingsMachineSystem &oldSystemData = m_pCache->base();
        /* Get new system data from the cache: */
        const UIDataSettingsMachineSystem &newSystemData = m_pCache->data();

        /* Save memory size: */
        if (fSuccess && isMachineOffline() && newSystemData.m_iMemorySize != oldSystemData.m_iMemorySize)
        {
            m_machine.SetMemorySize(newSystemData.m_iMemorySize);
            fSuccess = m_machine.isOk();
        }
        /* Save boot items: */
        if (fSuccess && isMachineOffline() && newSystemData.m_bootItems != oldSystemData.m_bootItems)
        {
            saveBootItems(newSystemData.m_bootItems, m_machine);
            fSuccess = m_machine.isOk();
        }
        /* Save chipset type: */
        if (fSuccess && isMachineOffline() && newSystemData.m_chipsetType != oldSystemData.m_chipsetType)
        {
            m_machine.SetChipsetType(newSystemData.m_chipsetType);
            fSuccess = m_machine.isOk();
        }
        /* Save pointing HID type: */
        if (fSuccess && isMachineOffline() && newSystemData.m_pointingHIDType != oldSystemData.m_pointingHIDType)
        {
            m_machine.SetPointingHIDType(newSystemData.m_pointingHIDType);
            fSuccess = m_machine.isOk();
        }
        /* Save whether IO APIC is enabled: */
        if (fSuccess && isMachineOffline() && newSystemData.m_fEnabledIoApic != oldSystemData.m_fEnabledIoApic)
        {
            m_machine.GetBIOSSettings().SetIOAPICEnabled(newSystemData.m_fEnabledIoApic);
            fSuccess = m_machine.isOk();
        }
        /* Save firware type (whether EFI is enabled): */
        if (fSuccess && isMachineOffline() && newSystemData.m_fEnabledEFI != oldSystemData.m_fEnabledEFI)
        {
            m_machine.SetFirmwareType(newSystemData.m_fEnabledEFI ? KFirmwareType_EFI : KFirmwareType_BIOS);
            fSuccess = m_machine.isOk();
        }
        /* Save whether UTC is enabled: */
        if (fSuccess && isMachineOffline() && newSystemData.m_fEnabledUTC != oldSystemData.m_fEnabledUTC)
        {
            m_machine.SetRTCUseUTC(newSystemData.m_fEnabledUTC);
            fSuccess = m_machine.isOk();
        }

        /* Show error message if necessary: */
        if (!fSuccess)
            notifyOperationProgressError(UIErrorString::formatErrorInfo(m_machine));
    }
    /* Return result: */
    return fSuccess;
}

bool UIMachineSettingsSystem::saveProcessorData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save 'Processor' settings from the cache: */
    if (fSuccess)
    {
        /* Get old system data from the cache: */
        const UIDataSettingsMachineSystem &oldSystemData = m_pCache->base();
        /* Get new system data from the cache: */
        const UIDataSettingsMachineSystem &newSystemData = m_pCache->data();

        /* Save CPU count: */
        if (fSuccess && isMachineOffline() && newSystemData.m_cCPUCount != oldSystemData.m_cCPUCount)
        {
            m_machine.SetCPUCount(newSystemData.m_cCPUCount);
            fSuccess = m_machine.isOk();
        }
        /* Save whether PAE is enabled: */
        if (fSuccess && isMachineOffline() && newSystemData.m_fEnabledPAE != oldSystemData.m_fEnabledPAE)
        {
            m_machine.SetCPUProperty(KCPUPropertyType_PAE, newSystemData.m_fEnabledPAE);
            fSuccess = m_machine.isOk();
        }
        /* Save whether Nested HW Virt Ex is enabled: */
        if (fSuccess && isMachineOffline() && newSystemData.m_fEnabledNestedHwVirtEx != oldSystemData.m_fEnabledNestedHwVirtEx)
        {
            m_machine.SetCPUProperty(KCPUPropertyType_HWVirt, newSystemData.m_fEnabledNestedHwVirtEx);
            fSuccess = m_machine.isOk();
        }
        /* Save CPU execution cap: */
        if (fSuccess && newSystemData.m_iCPUExecCap != oldSystemData.m_iCPUExecCap)
        {
            m_machine.SetCPUExecutionCap(newSystemData.m_iCPUExecCap);
            fSuccess = m_machine.isOk();
        }

        /* Show error message if necessary: */
        if (!fSuccess)
            notifyOperationProgressError(UIErrorString::formatErrorInfo(m_machine));
    }
    /* Return result: */
    return fSuccess;
}

bool UIMachineSettingsSystem::saveAccelerationData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save 'Acceleration' settings from the cache: */
    if (fSuccess)
    {
        /* Get old system data from the cache: */
        const UIDataSettingsMachineSystem &oldSystemData = m_pCache->base();
        /* Get new system data from the cache: */
        const UIDataSettingsMachineSystem &newSystemData = m_pCache->data();

        /* Save paravirtualization provider: */
        if (fSuccess && isMachineOffline() && newSystemData.m_paravirtProvider != oldSystemData.m_paravirtProvider)
        {
            m_machine.SetParavirtProvider(newSystemData.m_paravirtProvider);
            fSuccess = m_machine.isOk();
        }
        /* Save whether the hardware virtualization extension is enabled: */
        if (fSuccess && isMachineOffline() && newSystemData.m_fEnabledHwVirtEx != oldSystemData.m_fEnabledHwVirtEx)
        {
            m_machine.SetHWVirtExProperty(KHWVirtExPropertyType_Enabled, newSystemData.m_fEnabledHwVirtEx);
            fSuccess = m_machine.isOk();
        }
        /* Save whether the nested paging is enabled: */
        if (fSuccess && isMachineOffline() && newSystemData.m_fEnabledNestedPaging != oldSystemData.m_fEnabledNestedPaging)
        {
            m_machine.SetHWVirtExProperty(KHWVirtExPropertyType_NestedPaging, newSystemData.m_fEnabledNestedPaging);
            fSuccess = m_machine.isOk();
        }

        /* Show error message if necessary: */
        if (!fSuccess)
            notifyOperationProgressError(UIErrorString::formatErrorInfo(m_machine));
    }
    /* Return result: */
    return fSuccess;
}
