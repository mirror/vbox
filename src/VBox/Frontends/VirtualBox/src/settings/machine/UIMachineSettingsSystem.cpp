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
    , m_pTabWidget(0)
    , m_pTabMotherboard(0)
    , m_pLabelBaseMemory(0)
    , m_pEditorBaseMemory(0)
    , m_pLabelBootOrder(0)
    , m_pEditorBootOrder(0)
    , m_pLabelChipset(0)
    , m_pComboChipset(0)
    , m_pLabelPointingHID(0)
    , m_pComboPointingHID(0)
    , m_pLabelExtendedMotherboard(0)
    , m_pCheckBoxAPIC(0)
    , m_pCheckBoxEFI(0)
    , m_pCheckBoxUTC(0)
    , m_pTabProcessor(0)
    , m_pLabelProcessorCount(0)
    , m_pSliderProcessorCount(0)
    , m_pSpinboxProcessorCount(0)
    , m_pLabelProcessorCountMin(0)
    , m_pLabelProcessorCountMax(0)
    , m_pLabelProcessorExecCap(0)
    , m_pSliderProcessorExecCap(0)
    , m_pSpinboxProcessorExecCap(0)
    , m_pLabelProcessorExecCapMin(0)
    , m_pLabelProcessorExecCapMax(0)
    , m_pLabelExtendedProcessor(0)
    , m_pCheckBoxPAE(0)
    , m_pCheckBoxNestedVirtualization(0)
    , m_pTabAcceleration(0)
    , m_pLabelParavirtProvider(0)
    , m_pComboParavirtProvider(0)
    , m_pLabelVirtualization(0)
    , m_pCheckBoxVirtualization(0)
    , m_pCheckBoxNestedPaging(0)
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
    return m_pComboPointingHID->currentData().value<KPointingHIDType>() != KPointingHIDType_PS2Mouse;
}

KChipsetType UIMachineSettingsSystem::chipsetType() const
{
    return m_pComboChipset->currentData().value<KChipsetType>();
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
    m_pEditorBaseMemory->setValue(oldSystemData.m_iMemorySize);
    m_pEditorBootOrder->setValue(oldSystemData.m_bootItems);
    const int iChipsetTypePosition = m_pComboChipset->findData(oldSystemData.m_chipsetType);
    m_pComboChipset->setCurrentIndex(iChipsetTypePosition == -1 ? 0 : iChipsetTypePosition);
    const int iHIDTypePosition = m_pComboPointingHID->findData(oldSystemData.m_pointingHIDType);
    m_pComboPointingHID->setCurrentIndex(iHIDTypePosition == -1 ? 0 : iHIDTypePosition);
    m_pCheckBoxAPIC->setChecked(oldSystemData.m_fEnabledIoApic);
    m_pCheckBoxEFI->setChecked(oldSystemData.m_fEnabledEFI);
    m_pCheckBoxUTC->setChecked(oldSystemData.m_fEnabledUTC);

    /* Load old 'Processor' data from the cache: */
    m_pSliderProcessorCount->setValue(oldSystemData.m_cCPUCount);
    m_pSliderProcessorExecCap->setValue(oldSystemData.m_iCPUExecCap);
    m_pCheckBoxPAE->setChecked(oldSystemData.m_fEnabledPAE);
    m_pCheckBoxNestedVirtualization->setChecked(oldSystemData.m_fEnabledNestedHwVirtEx);

    /* Load old 'Acceleration' data from the cache: */
    const int iParavirtProviderPosition = m_pComboParavirtProvider->findData(oldSystemData.m_paravirtProvider);
    m_pComboParavirtProvider->setCurrentIndex(iParavirtProviderPosition == -1 ? 0 : iParavirtProviderPosition);
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
    newSystemData.m_iMemorySize = m_pEditorBaseMemory->value();
    newSystemData.m_bootItems = m_pEditorBootOrder->value();
    newSystemData.m_chipsetType = m_pComboChipset->currentData().value<KChipsetType>();
    newSystemData.m_pointingHIDType = m_pComboPointingHID->currentData().value<KPointingHIDType>();
    newSystemData.m_fEnabledIoApic =    m_pCheckBoxAPIC->isChecked()
                                     || m_pSliderProcessorCount->value() > 1
                                     || m_pComboChipset->currentData().value<KChipsetType>() == KChipsetType_ICH9;
    newSystemData.m_fEnabledEFI = m_pCheckBoxEFI->isChecked();
    newSystemData.m_fEnabledUTC = m_pCheckBoxUTC->isChecked();

    /* Gather 'Processor' data: */
    newSystemData.m_cCPUCount = m_pSliderProcessorCount->value();
    newSystemData.m_iCPUExecCap = m_pSliderProcessorExecCap->value();
    newSystemData.m_fEnabledPAE = m_pCheckBoxPAE->isChecked();
    newSystemData.m_fEnabledNestedHwVirtEx = isNestedHWVirtExEnabled();

    /* Gather 'Acceleration' data: */
    newSystemData.m_paravirtProvider = m_pComboParavirtProvider->currentData().value<KParavirtProvider>();
    /* Enable HW Virt Ex automatically if it's supported and
     * 1. multiple CPUs, 2. Nested Paging or 3. Nested HW Virt Ex is requested. */
    newSystemData.m_fEnabledHwVirtEx =    isHWVirtExEnabled()
                                       || (   isHWVirtExSupported()
                                           && (   m_pSliderProcessorCount->value() > 1
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
        message.first = UICommon::removeAccelMark(m_pTabWidget->tabText(0));

        /* RAM amount test: */
        const ulong uFullSize = uiCommon().host().GetMemorySize();
        if (m_pEditorBaseMemory->value() > (int)m_pEditorBaseMemory->maxRAMAlw())
        {
            message.second << tr(
                "More than <b>%1%</b> of the host computer's memory (<b>%2</b>) is assigned to the virtual machine. "
                "Not enough memory is left for the host operating system. Please select a smaller amount.")
                .arg((unsigned)qRound((double)m_pEditorBaseMemory->maxRAMAlw() / uFullSize * 100.0))
                .arg(uiCommon().formatSize((uint64_t)uFullSize * _1M));
            fPass = false;
        }
        else if (m_pEditorBaseMemory->value() > (int)m_pEditorBaseMemory->maxRAMOpt())
        {
            message.second << tr(
                "More than <b>%1%</b> of the host computer's memory (<b>%2</b>) is assigned to the virtual machine. "
                "There might not be enough memory left for the host operating system. Please consider selecting a smaller amount.")
                .arg((unsigned)qRound((double)m_pEditorBaseMemory->maxRAMOpt() / uFullSize * 100.0))
                .arg(uiCommon().formatSize((uint64_t)uFullSize * _1M));
        }

        /* Chipset type vs IO-APIC test: */
        if (m_pComboChipset->currentData().value<KChipsetType>() == KChipsetType_ICH9 && !m_pCheckBoxAPIC->isChecked())
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
        message.first = UICommon::removeAccelMark(m_pTabWidget->tabText(1));

        /* VCPU amount test: */
        const int cTotalCPUs = uiCommon().host().GetProcessorOnlineCoreCount();
        if (m_pSliderProcessorCount->value() > 2 * cTotalCPUs)
        {
            message.second << tr(
                "For performance reasons, the number of virtual CPUs attached to the virtual machine may not be more than twice the number "
                "of physical CPUs on the host (<b>%1</b>). Please reduce the number of virtual CPUs.")
                .arg(cTotalCPUs);
            fPass = false;
        }
        else if (m_pSliderProcessorCount->value() > cTotalCPUs)
        {
            message.second << tr(
                "More virtual CPUs are assigned to the virtual machine than the number of physical CPUs on the host system (<b>%1</b>). "
                "This is likely to degrade the performance of your virtual machine. Please consider reducing the number of virtual CPUs.")
                .arg(cTotalCPUs);
        }

        /* VCPU vs IO-APIC test: */
        if (m_pSliderProcessorCount->value() > 1 && !m_pCheckBoxAPIC->isChecked())
        {
            message.second << tr(
                "The I/O APIC feature is not currently enabled in the Motherboard section of the System page. "
                "This is needed to support more than one virtual processor. "
                "It will be enabled automatically if you confirm your changes.");
        }

        /* VCPU: */
        if (m_pSliderProcessorCount->value() > 1)
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
        if (m_pSliderProcessorExecCap->value() < (int)m_uMedGuestCPUExecCap)
        {
            message.second << tr("The processor execution cap is set to a low value. This may make the machine feel slow to respond.");
        }

        /* Warn user about possible performance degradation and suggest lowering # of CPUs assigned to the VM instead: */
        if (m_pSliderProcessorExecCap->value() < 100)
        {
            if (m_uMaxGuestCPU > 1 && m_pSliderProcessorCount->value() > 1)
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
        message.first = UICommon::removeAccelMark(m_pTabWidget->tabText(2));

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
    setTabOrder(pWidget, m_pTabWidget->focusProxy());
    setTabOrder(m_pTabWidget->focusProxy(), m_pEditorBaseMemory);
    setTabOrder(m_pEditorBaseMemory, m_pEditorBootOrder);
    setTabOrder(m_pEditorBootOrder, m_pComboChipset);
    setTabOrder(m_pComboChipset, m_pComboPointingHID);
    setTabOrder(m_pComboPointingHID, m_pCheckBoxAPIC);
    setTabOrder(m_pCheckBoxAPIC, m_pCheckBoxEFI);
    setTabOrder(m_pCheckBoxEFI, m_pCheckBoxUTC);

    /* Configure navigation for 'processor' tab: */
    setTabOrder(m_pCheckBoxUTC, m_pSliderProcessorCount);
    setTabOrder(m_pSliderProcessorCount, m_pSpinboxProcessorCount);
    setTabOrder(m_pSpinboxProcessorCount, m_pSliderProcessorExecCap);
    setTabOrder(m_pSliderProcessorExecCap, m_pSpinboxProcessorExecCap);
    setTabOrder(m_pSpinboxProcessorExecCap, m_pComboParavirtProvider);

    /* Configure navigation for 'acceleration' tab: */
    setTabOrder(m_pComboParavirtProvider, m_pCheckBoxPAE);
    setTabOrder(m_pCheckBoxPAE, m_pCheckBoxNestedVirtualization);
    setTabOrder(m_pCheckBoxNestedVirtualization, m_pCheckBoxVirtualization);
    setTabOrder(m_pCheckBoxVirtualization, m_pCheckBoxNestedPaging);
}

void UIMachineSettingsSystem::retranslateUi()
{
    m_pLabelBaseMemory->setText(QApplication::translate("UIMachineSettingsSystem", "Base &Memory:"));
    m_pEditorBaseMemory->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "Controls the amount of memory provided "
                                                              "to the virtual machine. If you assign too much, the machine might not start."));
    m_pLabelBootOrder->setText(QApplication::translate("UIMachineSettingsSystem", "&Boot Order:"));
    m_pEditorBootOrder->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "Defines the boot device order. Use the "
                                                             "checkboxes on the left to enable or disable individual boot devices."
                                                             "Move items up and down to change the device order."));
    m_pLabelChipset->setText(QApplication::translate("UIMachineSettingsSystem", "&Chipset:"));
    m_pComboChipset->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "Selects the chipset to be emulated in "
                                                              "this virtual machine. Note that the ICH9 chipset emulation is experimental "
                                                              "and not recommended except for guest systems (such as Mac OS X) which require it."));
    m_pLabelPointingHID->setText(QApplication::translate("UIMachineSettingsSystem", "&Pointing Device:"));
    m_pComboPointingHID->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "Determines whether the emulated "
                                                                  "pointing device is a standard PS/2 mouse, a USB tablet or a "
                                                                  "USB multi-touch tablet."));
    m_pLabelExtendedMotherboard->setText(QApplication::translate("UIMachineSettingsSystem", "Extended Features:"));
    m_pCheckBoxAPIC->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "When checked, the virtual machine will "
                                                          "support the Input Output APIC (I/O APIC), which may slightly decrease "
                                                          "performance. <b>Note:</b> don't disable this feature after having "
                                                          "installed a Windows guest operating system!"));
    m_pCheckBoxAPIC->setText(QApplication::translate("UIMachineSettingsSystem", "Enable &I/O APIC"));
    m_pCheckBoxEFI->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "When checked, the guest will support the "
                                                         "Extended Firmware Interface (EFI), which is required to boot certain "
                                                         "guest OSes. Non-EFI aware OSes will not be able to boot if this option is activated."));
    m_pCheckBoxEFI->setText(QApplication::translate("UIMachineSettingsSystem", "Enable &EFI (special OSes only)"));
    m_pCheckBoxUTC->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "When checked, the RTC device will report "
                                                            "the time in UTC, otherwise in local (host) time. Unix usually expects "
                                                            "the hardware clock to be set to UTC."));
    m_pCheckBoxUTC->setText(QApplication::translate("UIMachineSettingsSystem", "Hardware Clock in &UTC Time"));
    m_pTabWidget->setTabText(m_pTabWidget->indexOf(m_pTabMotherboard), QApplication::translate("UIMachineSettingsSystem", "&Motherboard"));
    m_pLabelProcessorCount->setText(QApplication::translate("UIMachineSettingsSystem", "&Processor(s):"));
    m_pSliderProcessorCount->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "Controls the number of virtual CPUs in the "
                                                            "virtual machine. You need hardware virtualization support on your host "
                                                            "system to use more than one virtual CPU."));
    m_pSpinboxProcessorCount->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "Controls the number of virtual CPUs in the "
                                                            "virtual machine. You need hardware virtualization support on your host "
                                                            "system to use more than one virtual CPU."));
    m_pLabelProcessorExecCap->setText(QApplication::translate("UIMachineSettingsSystem", "&Execution Cap:"));
    m_pSliderProcessorExecCap->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "Limits the amount of time that each virtual "
                                                              "CPU is allowed to run for. Each virtual CPU will be allowed to use up "
                                                              "to this percentage of the processing time available on one physical CPU. "
                                                              "The execution cap can be disabled by setting it to 100%. Setting the cap "
                                                              "too low can make the machine feel slow to respond."));
    m_pSpinboxProcessorExecCap->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "Limits the amount of time that each virtual CPU "
                                                              "is allowed to run for. Each virtual CPU will be allowed to use up "
                                                              "to this percentage of the processing time available on one physical "
                                                              "CPU. The execution cap can be disabled by setting it to 100%. Setting "
                                                              "the cap too low can make the machine feel slow to respond."));
    m_pSpinboxProcessorExecCap->setSuffix(QApplication::translate("UIMachineSettingsSystem", "%"));
    m_pLabelExtendedProcessor->setText(QApplication::translate("UIMachineSettingsSystem", "Extended Features:"));
    m_pCheckBoxPAE->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "When checked, the Physical Address Extension "
                                                         "(PAE) feature of the host CPU will be exposed to the virtual machine."));
    m_pCheckBoxPAE->setText(QApplication::translate("UIMachineSettingsSystem", "Enable PA&E/NX"));
    m_pCheckBoxNestedVirtualization->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "When checked, the nested hardware "
                                                                          "virtualization CPU feature will be exposed to the virtual machine."));
    m_pCheckBoxNestedVirtualization->setText(QApplication::translate("UIMachineSettingsSystem", "Enable Nested &VT-x/AMD-V"));
    m_pTabWidget->setTabText(m_pTabWidget->indexOf(m_pTabProcessor), QApplication::translate("UIMachineSettingsSystem", "&Processor"));
    m_pLabelParavirtProvider->setText(QApplication::translate("UIMachineSettingsSystem", "&Paravirtualization Interface:"));
    m_pComboParavirtProvider->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "Selects the paravirtualization "
                                                                       "guest interface provider to be used by this virtual machine."));
    m_pLabelVirtualization->setText(QApplication::translate("UIMachineSettingsSystem", "Hardware Virtualization:"));
    m_pCheckBoxVirtualization->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "When checked, the virtual machine "
                                                                    "will try to make use of the host CPU's hardware virtualization "
                                                                    "extensions such as Intel VT-x and AMD-V."));
    m_pCheckBoxVirtualization->setText(QApplication::translate("UIMachineSettingsSystem", "Enable &VT-x/AMD-V"));
    m_pCheckBoxNestedPaging->setWhatsThis(QApplication::translate("UIMachineSettingsSystem", "When checked, the virtual machine will "
                                                                  "try to make use of the nested paging extension of Intel VT-x and AMD-V."));
    m_pCheckBoxNestedPaging->setText(QApplication::translate("UIMachineSettingsSystem", "Enable Nested Pa&ging"));
    m_pTabWidget->setTabText(m_pTabWidget->indexOf(m_pTabAcceleration), QApplication::translate("UIMachineSettingsSystem", "Acce&leration"));

    /* Retranslate the cpu slider legend: */
    m_pLabelProcessorCountMin->setText(tr("%1 CPU", "%1 is 1 for now").arg(m_uMinGuestCPU));
    m_pLabelProcessorCountMax->setText(tr("%1 CPUs", "%1 is host cpu count * 2 for now").arg(m_uMaxGuestCPU));

    /* Retranslate the cpu cap slider legend: */
    m_pLabelProcessorExecCapMin->setText(tr("%1%").arg(m_uMinGuestCPUExecCap));
    m_pLabelProcessorExecCapMax->setText(tr("%1%").arg(m_uMaxGuestCPUExecCap));

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
    m_pLabelBaseMemory->setEnabled(isMachineOffline());
    m_pEditorBaseMemory->setEnabled(isMachineOffline());
    m_pLabelBootOrder->setEnabled(isMachineOffline());
    m_pEditorBootOrder->setEnabled(isMachineOffline());
    m_pLabelChipset->setEnabled(isMachineOffline());
    m_pComboChipset->setEnabled(isMachineOffline());
    m_pLabelPointingHID->setEnabled(isMachineOffline());
    m_pComboPointingHID->setEnabled(isMachineOffline());
    m_pLabelExtendedMotherboard->setEnabled(isMachineOffline());
    m_pCheckBoxAPIC->setEnabled(isMachineOffline());
    m_pCheckBoxEFI->setEnabled(isMachineOffline());
    m_pCheckBoxUTC->setEnabled(isMachineOffline());

    /* Polish 'Processor' availability: */
    m_pLabelProcessorCount->setEnabled(isMachineOffline());
    m_pLabelProcessorCountMin->setEnabled(isMachineOffline());
    m_pLabelProcessorCountMax->setEnabled(isMachineOffline());
    m_pSliderProcessorCount->setEnabled(isMachineOffline() && systemData.m_fSupportedHwVirtEx);
    m_pSpinboxProcessorCount->setEnabled(isMachineOffline() && systemData.m_fSupportedHwVirtEx);
    m_pLabelProcessorExecCap->setEnabled(isMachineInValidMode());
    m_pLabelProcessorExecCapMin->setEnabled(isMachineInValidMode());
    m_pLabelProcessorExecCapMax->setEnabled(isMachineInValidMode());
    m_pSliderProcessorExecCap->setEnabled(isMachineInValidMode());
    m_pSpinboxProcessorExecCap->setEnabled(isMachineInValidMode());
    m_pLabelExtendedProcessor->setEnabled(isMachineOffline());
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
    m_pComboParavirtProvider->setEnabled(isMachineOffline());
    m_pLabelVirtualization->setEnabled(isMachineOffline());
}

void UIMachineSettingsSystem::sltHandleCPUCountSliderChange()
{
    /* Apply new memory-size value: */
    m_pSpinboxProcessorCount->blockSignals(true);
    m_pSpinboxProcessorCount->setValue(m_pSliderProcessorCount->value());
    m_pSpinboxProcessorCount->blockSignals(false);

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsSystem::sltHandleCPUCountEditorChange()
{
    /* Apply new memory-size value: */
    m_pSliderProcessorCount->blockSignals(true);
    m_pSliderProcessorCount->setValue(m_pSpinboxProcessorCount->value());
    m_pSliderProcessorCount->blockSignals(false);

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsSystem::sltHandleCPUExecCapSliderChange()
{
    /* Apply new memory-size value: */
    m_pSpinboxProcessorExecCap->blockSignals(true);
    m_pSpinboxProcessorExecCap->setValue(m_pSliderProcessorExecCap->value());
    m_pSpinboxProcessorExecCap->blockSignals(false);

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsSystem::sltHandleCPUExecCapEditorChange()
{
    /* Apply new memory-size value: */
    m_pSliderProcessorExecCap->blockSignals(true);
    m_pSliderProcessorExecCap->setValue(m_pSpinboxProcessorExecCap->value());
    m_pSliderProcessorExecCap->blockSignals(false);

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
    /* Prepare cache: */
    m_pCache = new UISettingsCacheMachineSystem;
    AssertPtrReturnVoid(m_pCache);

    /* Prepare widgets: */
    prepareWidgets();

    /* Apply language settings: */
    retranslateUi();
}

void UIMachineSettingsSystem::prepareWidgets()
{
    /* Prepare main layout: */
    QVBoxLayout *pLayoutMain = new QVBoxLayout(this);
    if (pLayoutMain)
    {
        /* Prepare tab-widget: */
        m_pTabWidget = new QITabWidget();
        if (m_pTabWidget)
        {
            /* Prepare each tab separately: */
            prepareTabMotherboard();
            prepareTabProcessor();
            prepareTabAcceleration();
            prepareConnections();

            pLayoutMain->addWidget(m_pTabWidget);
        }
    }
}

void UIMachineSettingsSystem::prepareTabMotherboard()
{
    /* Prepare Motherboard tab: */
    m_pTabMotherboard = new QWidget;
    if (m_pTabMotherboard)
    {
        /* Prepare Motherboard tab layout: */
        QGridLayout *pLayoutMotherboard = new QGridLayout(m_pTabMotherboard);
        if (pLayoutMotherboard)
        {
            pLayoutMotherboard->setColumnStretch(4, 1);
            pLayoutMotherboard->setRowStretch(9, 1);

            /* Prepare base memory label: */
            m_pLabelBaseMemory = new QLabel(m_pTabMotherboard);
            if (m_pLabelBaseMemory)
            {
                m_pLabelBaseMemory->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                pLayoutMotherboard->addWidget(m_pLabelBaseMemory, 0, 0);
            }
            /* Prepare base memory editor: */
            m_pEditorBaseMemory = new UIBaseMemoryEditor(m_pTabMotherboard);
            if (m_pEditorBaseMemory)
            {
                m_pLabelBaseMemory->setBuddy(m_pEditorBaseMemory->focusProxy());
                m_pEditorBaseMemory->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));

                pLayoutMotherboard->addWidget(m_pEditorBaseMemory, 0, 1, 2, 4);
            }

            /* Prepare boot order label: */
            m_pLabelBootOrder = new QLabel(m_pTabMotherboard);
            if (m_pLabelBootOrder)
            {
                m_pLabelBootOrder->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                pLayoutMotherboard->addWidget(m_pLabelBootOrder, 2, 0);
            }
            /* Prepare boot order editor: */
            m_pEditorBootOrder = new UIBootOrderEditor(m_pTabMotherboard);
            if (m_pEditorBootOrder)
            {
                m_pLabelBootOrder->setBuddy(m_pEditorBootOrder->focusProxy());
                m_pEditorBootOrder->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));

                pLayoutMotherboard->addWidget(m_pEditorBootOrder, 2, 1, 2, 2);
            }

            /* Prepare chipset label: */
            m_pLabelChipset = new QLabel(m_pTabMotherboard);
            if (m_pLabelChipset)
            {
                m_pLabelChipset->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                pLayoutMotherboard->addWidget(m_pLabelChipset, 4, 0);
            }
            /* Prepare chipset combo: */
            m_pComboChipset = new QComboBox(m_pTabMotherboard);
            if (m_pComboChipset)
            {
                m_pLabelChipset->setBuddy(m_pComboChipset);
                m_pComboChipset->setSizeAdjustPolicy(QComboBox::AdjustToContents);
                m_pComboChipset->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));

                pLayoutMotherboard->addWidget(m_pComboChipset, 4, 1);
            }

            /* Prepare pointing HID label: */
            m_pLabelPointingHID = new QLabel(m_pTabMotherboard);
            if (m_pLabelPointingHID)
            {
                m_pLabelPointingHID->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                pLayoutMotherboard->addWidget(m_pLabelPointingHID, 5, 0);
            }
            /* Prepare pointing HID combo: */
            m_pComboPointingHID = new QComboBox(m_pTabMotherboard);
            if (m_pComboPointingHID)
            {
                m_pLabelPointingHID->setBuddy(m_pComboPointingHID);
                m_pComboPointingHID->setSizeAdjustPolicy(QComboBox::AdjustToContents);
                m_pComboPointingHID->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));

                pLayoutMotherboard->addWidget(m_pComboPointingHID, 5, 1);
            }

            /* Prepare extended motherboard label: */
            m_pLabelExtendedMotherboard = new QLabel(m_pTabMotherboard);
            if (m_pLabelExtendedMotherboard)
            {
                m_pLabelExtendedMotherboard->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                pLayoutMotherboard->addWidget(m_pLabelExtendedMotherboard, 6, 0);
            }
            /* Prepare APIC check-box: */
            m_pCheckBoxAPIC = new QCheckBox(m_pTabMotherboard);
            if (m_pCheckBoxAPIC)
            {
                m_pCheckBoxAPIC->setObjectName(QStringLiteral("m_pCheckBoxAPIC"));
                pLayoutMotherboard->addWidget(m_pCheckBoxAPIC, 6, 1, 1, 3);
            }
            /* Prepare EFI check-box: */
            m_pCheckBoxEFI = new QCheckBox(m_pTabMotherboard);
            if (m_pCheckBoxEFI)
            {
                m_pCheckBoxEFI->setObjectName(QStringLiteral("m_pCheckBoxEFI"));
                pLayoutMotherboard->addWidget(m_pCheckBoxEFI, 7, 1, 1, 3);
            }
            /* Prepare UTC check-box: */
            m_pCheckBoxUTC = new QCheckBox(m_pTabMotherboard);
            if (m_pCheckBoxUTC)
            {
                m_pCheckBoxUTC->setObjectName(QStringLiteral("m_pCheckBoxUTC"));
                pLayoutMotherboard->addWidget(m_pCheckBoxUTC, 8, 1, 1, 3);
            }
        }

        m_pTabWidget->addTab(m_pTabMotherboard, QString());
    }
}

void UIMachineSettingsSystem::prepareTabProcessor()
{
    /* Prepare common variables: */
    const CSystemProperties comProperties = uiCommon().virtualBox().GetSystemProperties();
    const uint uHostCPUs = uiCommon().host().GetProcessorOnlineCoreCount();
    m_uMinGuestCPU = comProperties.GetMinGuestCPUCount();
    m_uMaxGuestCPU = qMin(2 * uHostCPUs, (uint)comProperties.GetMaxGuestCPUCount());
    m_uMinGuestCPUExecCap = 1;
    m_uMedGuestCPUExecCap = 40;
    m_uMaxGuestCPUExecCap = 100;

    /* Prepare Processor tab: */
    m_pTabProcessor = new QWidget;
    if (m_pTabProcessor)
    {
        /* Prepare Processor tab layout: */
        QGridLayout *pLayoutProcessor = new QGridLayout(m_pTabProcessor);
        if (pLayoutProcessor)
        {
            pLayoutProcessor->setRowStretch(6, 1);

            /* Prepare processor count label: */
            m_pLabelProcessorCount = new QLabel(m_pTabProcessor);
            if (m_pLabelProcessorCount)
            {
                m_pLabelProcessorCount->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                pLayoutProcessor->addWidget(m_pLabelProcessorCount, 0, 0);
            }
            /* Prepare processor count layout: */
            QVBoxLayout *pLayoutProcessorCount = new QVBoxLayout;
            if (pLayoutProcessorCount)
            {
                pLayoutProcessorCount->setContentsMargins(0, 0, 0, 0);

                /* Prepare processor count slider: */
                m_pSliderProcessorCount = new QIAdvancedSlider(m_pTabProcessor);
                if (m_pSliderProcessorCount)
                {
                    m_pSliderProcessorCount->setOrientation(Qt::Horizontal);
                    m_pSliderProcessorCount->setPageStep(1);
                    m_pSliderProcessorCount->setSingleStep(1);
                    m_pSliderProcessorCount->setTickInterval(1);
                    m_pSliderProcessorCount->setMinimum(m_uMinGuestCPU);
                    m_pSliderProcessorCount->setMaximum(m_uMaxGuestCPU);
                    m_pSliderProcessorCount->setOptimalHint(1, uHostCPUs);
                    m_pSliderProcessorCount->setWarningHint(uHostCPUs, m_uMaxGuestCPU);

                    pLayoutProcessorCount->addWidget(m_pSliderProcessorCount);
                }
                /* Prepare processor count scale layout: */
                QHBoxLayout *m_pLayoutProcessorCountScale = new QHBoxLayout;
                if (m_pLayoutProcessorCountScale)
                {
                    m_pLayoutProcessorCountScale->setSpacing(0);

                    /* Prepare processor count min label: */
                    m_pLabelProcessorCountMin = new QLabel(m_pTabProcessor);
                    if (m_pLabelProcessorCountMin)
                        m_pLayoutProcessorCountScale->addWidget(m_pLabelProcessorCountMin);
                    m_pLayoutProcessorCountScale->addStretch();
                    /* Prepare processor count max label: */
                    m_pLabelProcessorCountMax = new QLabel(m_pTabProcessor);
                    if (m_pLabelProcessorCountMax)
                        m_pLayoutProcessorCountScale->addWidget(m_pLabelProcessorCountMax);

                    pLayoutProcessorCount->addLayout(m_pLayoutProcessorCountScale);
                }

                pLayoutProcessor->addLayout(pLayoutProcessorCount, 0, 1, 2, 1);
            }
            /* Prepare processor count spinbox: */
            m_pSpinboxProcessorCount = new QSpinBox(m_pTabProcessor);
            if (m_pSpinboxProcessorCount)
            {
                m_pLabelProcessorCount->setBuddy(m_pSpinboxProcessorCount);
                m_pSpinboxProcessorCount->setMinimum(m_uMinGuestCPU);
                m_pSpinboxProcessorCount->setMaximum(m_uMaxGuestCPU);
                uiCommon().setMinimumWidthAccordingSymbolCount(m_pSpinboxProcessorCount, 4);

                pLayoutProcessor->addWidget(m_pSpinboxProcessorCount, 0, 2);
            }

            /* Prepare processor exec cap label: */
            m_pLabelProcessorExecCap = new QLabel(m_pTabProcessor);
            if (m_pLabelProcessorExecCap)
            {
                m_pLabelProcessorExecCap->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                pLayoutProcessor->addWidget(m_pLabelProcessorExecCap, 2, 0);
            }
            /* Prepare processor exec cap layout: */
            QVBoxLayout *pLayoutProcessorExecCap = new QVBoxLayout;
            if (pLayoutProcessorExecCap)
            {
                pLayoutProcessorExecCap->setContentsMargins(0, 0, 0, 0);

                /* Prepare processor exec cap slider: */
                m_pSliderProcessorExecCap = new QIAdvancedSlider(m_pTabProcessor);
                if (m_pSliderProcessorExecCap)
                {
                    m_pSliderProcessorExecCap->setOrientation(Qt::Horizontal);
                    m_pSliderProcessorExecCap->setPageStep(10);
                    m_pSliderProcessorExecCap->setSingleStep(1);
                    m_pSliderProcessorExecCap->setTickInterval(10);
                    m_pSliderProcessorExecCap->setMinimum(m_uMinGuestCPUExecCap);
                    m_pSliderProcessorExecCap->setMaximum(m_uMaxGuestCPUExecCap);
                    m_pSliderProcessorExecCap->setWarningHint(m_uMinGuestCPUExecCap, m_uMedGuestCPUExecCap);
                    m_pSliderProcessorExecCap->setOptimalHint(m_uMedGuestCPUExecCap, m_uMaxGuestCPUExecCap);

                    pLayoutProcessorExecCap->addWidget(m_pSliderProcessorExecCap);
                }
                /* Prepare processor exec cap scale layout: */
                QHBoxLayout *m_pLayoutProcessorExecCapScale = new QHBoxLayout;
                if (m_pLayoutProcessorExecCapScale)
                {
                    m_pLayoutProcessorExecCapScale->setSpacing(0);

                    /* Prepare processor exec cap min label: */
                    m_pLabelProcessorExecCapMin = new QLabel(m_pTabProcessor);
                    if (m_pLabelProcessorExecCapMin)
                        m_pLayoutProcessorExecCapScale->addWidget(m_pLabelProcessorExecCapMin);
                    m_pLayoutProcessorExecCapScale->addStretch();
                    /* Prepare processor exec cap max label: */
                    m_pLabelProcessorExecCapMax = new QLabel(m_pTabProcessor);
                    if (m_pLabelProcessorExecCapMax)
                        m_pLayoutProcessorExecCapScale->addWidget(m_pLabelProcessorExecCapMax);

                    pLayoutProcessorExecCap->addLayout(m_pLayoutProcessorExecCapScale);
                }

                pLayoutProcessor->addLayout(pLayoutProcessorExecCap, 2, 1, 2, 1);
            }
            /* Prepare processor exec cap spinbox: */
            m_pSpinboxProcessorExecCap = new QSpinBox(m_pTabProcessor);
            if (m_pSpinboxProcessorExecCap)
            {
                m_pLabelProcessorExecCap->setBuddy(m_pSpinboxProcessorExecCap);
                m_pSpinboxProcessorExecCap->setMinimum(m_uMinGuestCPUExecCap);
                m_pSpinboxProcessorExecCap->setMaximum(m_uMaxGuestCPUExecCap);
                uiCommon().setMinimumWidthAccordingSymbolCount(m_pSpinboxProcessorExecCap, 4);

                pLayoutProcessor->addWidget(m_pSpinboxProcessorExecCap, 2, 2);
            }

            /* Prepare extended processor label: */
            m_pLabelExtendedProcessor = new QLabel(m_pTabProcessor);
            if (m_pLabelExtendedProcessor)
            {
                m_pLabelExtendedProcessor->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                pLayoutProcessor->addWidget(m_pLabelExtendedProcessor, 4, 0);
            }
            /* Prepare PAE check-box: */
            m_pCheckBoxPAE = new QCheckBox(m_pTabProcessor);
            if (m_pCheckBoxPAE)
                pLayoutProcessor->addWidget(m_pCheckBoxPAE, 4, 1, 1, 2);
            /* Prepare nested virtualization check-box: */
            m_pCheckBoxNestedVirtualization = new QCheckBox(m_pTabProcessor);
            if (m_pCheckBoxNestedVirtualization)
                pLayoutProcessor->addWidget(m_pCheckBoxNestedVirtualization, 5, 1, 1, 2);
        }

        m_pTabWidget->addTab(m_pTabProcessor, QString());
    }
}

void UIMachineSettingsSystem::prepareTabAcceleration()
{
    /* Prepare Acceleration tab: */
    m_pTabAcceleration = new QWidget;
    if (m_pTabAcceleration)
    {
        /* Prepare Acceleration tab layout: */
        QGridLayout *pLayoutAcceleration = new QGridLayout(m_pTabAcceleration);
        if (pLayoutAcceleration)
        {
            pLayoutAcceleration->setColumnStretch(2, 1);
            pLayoutAcceleration->setRowStretch(3, 1);

            /* Prepare paravirtualization provider label: */
            m_pLabelParavirtProvider = new QLabel(m_pTabAcceleration);
            if (m_pLabelParavirtProvider)
            {
                m_pLabelParavirtProvider->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                pLayoutAcceleration->addWidget(m_pLabelParavirtProvider, 0, 0);
            }
            /* Prepare paravirtualization provider combo: */
            m_pComboParavirtProvider = new QComboBox(m_pTabAcceleration);
            if (m_pComboParavirtProvider)
            {
                m_pLabelParavirtProvider->setBuddy(m_pComboParavirtProvider);
                m_pComboParavirtProvider->setSizeAdjustPolicy(QComboBox::AdjustToContents);
                m_pComboParavirtProvider->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));

                pLayoutAcceleration->addWidget(m_pComboParavirtProvider, 0, 1);
            }

            /* Prepare virtualization label layout: */
            QVBoxLayout *pLayoutVirtualizationLabel = new QVBoxLayout;
            if (pLayoutVirtualizationLabel)
            {
                /* Prepare virtualization label: */
                m_pLabelVirtualization = new QLabel(m_pTabAcceleration);
                if (m_pLabelVirtualization)
                {
                    m_pLabelVirtualization->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    pLayoutVirtualizationLabel->addWidget(m_pLabelVirtualization);
                }
                /* Prepare placeholder: */
                QWidget *pWidgetPlaceholder = new QWidget(m_pTabAcceleration);
                if (pWidgetPlaceholder)
                {
#ifndef VBOX_WITH_RAW_MODE
                    /* Hide placeholder when raw-mode is not supported: */
                    pWidgetPlaceholder->setVisible(false);
#endif

                    pLayoutVirtualizationLabel->addWidget(pWidgetPlaceholder);
                }

                pLayoutAcceleration->addLayout(pLayoutVirtualizationLabel, 1, 0);
            }
            /* Prepare virtualization stuff layout: */
            QVBoxLayout *pLayoutVirtualizationStuff = new QVBoxLayout;
            if (pLayoutVirtualizationStuff)
            {
                /* Prepare virtualization check-box: */
                m_pCheckBoxVirtualization = new QCheckBox(m_pTabAcceleration);
                if (m_pCheckBoxVirtualization)
                {
#ifndef VBOX_WITH_RAW_MODE
                    /* Hide HW Virt Ex checkbox when raw-mode is not supported: */
                    m_pCheckBoxVirtualization->setVisible(false);
#endif

                    pLayoutVirtualizationStuff->addWidget(m_pCheckBoxVirtualization);
                }
                /* Prepare nested paging check-box: */
                m_pCheckBoxNestedPaging = new QCheckBox(m_pTabAcceleration);
                if (m_pCheckBoxNestedPaging)
                    pLayoutVirtualizationStuff->addWidget(m_pCheckBoxNestedPaging);

                pLayoutAcceleration->addLayout(pLayoutVirtualizationStuff, 1, 1);
            }

            m_pTabWidget->addTab(m_pTabAcceleration, QString());
        }
    }
}

void UIMachineSettingsSystem::prepareConnections()
{
    /* Configure 'Motherboard' connections: */
    connect(m_pComboChipset, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIMachineSettingsSystem::revalidate);
    connect(m_pComboPointingHID, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIMachineSettingsSystem::revalidate);
    connect(m_pEditorBaseMemory, &UIBaseMemoryEditor::sigValidChanged, this, &UIMachineSettingsSystem::revalidate);
    connect(m_pCheckBoxAPIC, &QCheckBox::stateChanged, this, &UIMachineSettingsSystem::revalidate);

    /* Configure 'Processor' connections: */
    connect(m_pSliderProcessorCount, &QIAdvancedSlider::valueChanged, this, &UIMachineSettingsSystem::sltHandleCPUCountSliderChange);
    connect(m_pSpinboxProcessorCount, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &UIMachineSettingsSystem::sltHandleCPUCountEditorChange);
    connect(m_pSliderProcessorExecCap, &QIAdvancedSlider::valueChanged, this, &UIMachineSettingsSystem::sltHandleCPUExecCapSliderChange);
    connect(m_pSpinboxProcessorExecCap, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
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
    AssertPtrReturnVoid(m_pComboChipset);
    {
        /* Clear combo first of all: */
        m_pComboChipset->clear();

        /* Load currently supported chipset types: */
        CSystemProperties comProperties = uiCommon().virtualBox().GetSystemProperties();
        QVector<KChipsetType> chipsetTypes = comProperties.GetSupportedChipsetTypes();
        /* Take into account currently cached value: */
        const KChipsetType enmCachedValue = m_pCache->base().m_chipsetType;
        if (!chipsetTypes.contains(enmCachedValue))
            chipsetTypes.prepend(enmCachedValue);

        /* Populate combo finally: */
        foreach (const KChipsetType &enmType, chipsetTypes)
            m_pComboChipset->addItem(gpConverter->toString(enmType), QVariant::fromValue(enmType));
    }
}

void UIMachineSettingsSystem::repopulateComboPointingHIDType()
{
    /* Pointing HID Type combo-box created in the .ui file. */
    AssertPtrReturnVoid(m_pComboPointingHID);
    {
        /* Clear combo first of all: */
        m_pComboPointingHID->clear();

        /* Load currently supported pointing HID types: */
        CSystemProperties comProperties = uiCommon().virtualBox().GetSystemProperties();
        QVector<KPointingHIDType> pointingHidTypes = comProperties.GetSupportedPointingHIDTypes();
        /* Take into account currently cached value: */
        const KPointingHIDType enmCachedValue = m_pCache->base().m_pointingHIDType;
        if (!pointingHidTypes.contains(enmCachedValue))
            pointingHidTypes.prepend(enmCachedValue);

        /* Populate combo finally: */
        foreach (const KPointingHIDType &enmType, pointingHidTypes)
            m_pComboPointingHID->addItem(gpConverter->toString(enmType), QVariant::fromValue(enmType));
    }
}

void UIMachineSettingsSystem::repopulateComboParavirtProviderType()
{
    /* Paravirtualization Provider Type combo-box created in the .ui file. */
    AssertPtrReturnVoid(m_pComboParavirtProvider);
    {
        /* Clear combo first of all: */
        m_pComboParavirtProvider->clear();

        /* Load currently supported paravirtualization provider types: */
        CSystemProperties comProperties = uiCommon().virtualBox().GetSystemProperties();
        QVector<KParavirtProvider> supportedProviderTypes = comProperties.GetSupportedParavirtProviders();
        /* Take into account currently cached value: */
        const KParavirtProvider enmCachedValue = m_pCache->base().m_paravirtProvider;
        if (!supportedProviderTypes.contains(enmCachedValue))
            supportedProviderTypes.prepend(enmCachedValue);

        /* Populate combo finally: */
        foreach (const KParavirtProvider &enmProvider, supportedProviderTypes)
            m_pComboParavirtProvider->addItem(gpConverter->toString(enmProvider), QVariant::fromValue(enmProvider));
    }
}

void UIMachineSettingsSystem::retranslateComboChipsetType()
{
    /* For each the element in m_pComboChipset: */
    for (int iIndex = 0; iIndex < m_pComboChipset->count(); ++iIndex)
    {
        /* Apply retranslated text: */
        const KChipsetType enmType = m_pComboChipset->currentData().value<KChipsetType>();
        m_pComboChipset->setItemText(iIndex, gpConverter->toString(enmType));
    }
}

void UIMachineSettingsSystem::retranslateComboPointingHIDType()
{
    /* For each the element in m_pComboPointingHID: */
    for (int iIndex = 0; iIndex < m_pComboPointingHID->count(); ++iIndex)
    {
        /* Apply retranslated text: */
        const KPointingHIDType enmType = m_pComboPointingHID->currentData().value<KPointingHIDType>();
        m_pComboPointingHID->setItemText(iIndex, gpConverter->toString(enmType));
    }
}

void UIMachineSettingsSystem::retranslateComboParavirtProvider()
{
    /* For each the element in m_pComboParavirtProvider: */
    for (int iIndex = 0; iIndex < m_pComboParavirtProvider->count(); ++iIndex)
    {
        /* Apply retranslated text: */
        const KParavirtProvider enmType = m_pComboParavirtProvider->currentData().value<KParavirtProvider>();
        m_pComboParavirtProvider->setItemText(iIndex, gpConverter->toString(enmType));
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
