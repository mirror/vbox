/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsSystem class implementation.
 */

/*
 * Copyright (C) 2008-2022 Oracle Corporation
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
#include <QLabel>
#include <QVBoxLayout>

/* GUI includes: */
#include "QITabWidget.h"
#include "UIBaseMemoryEditor.h"
#include "UIBootOrderEditor.h"
#include "UIChipsetEditor.h"
#include "UICommon.h"
#include "UIConverter.h"
#include "UIErrorString.h"
#include "UIExecutionCapEditor.h"
#include "UIMachineSettingsSystem.h"
#include "UIMotherboardFeaturesEditor.h"
#include "UIPointingHIDEditor.h"
#include "UIProcessorFeaturesEditor.h"
#include "UITranslator.h"
#include "UIVirtualCPUEditor.h"

/* COM includes: */
#include "CBIOSSettings.h"


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
    : m_fIsUSBEnabled(false)
    , m_pCache(0)
    , m_pTabWidget(0)
    , m_pTabMotherboard(0)
    , m_pEditorBaseMemory(0)
    , m_pEditorBootOrder(0)
    , m_pEditorChipset(0)
    , m_pEditorPointingHID(0)
    , m_pEditorMotherboardFeatures(0)
    , m_pTabProcessor(0)
    , m_pEditorVCPU(0)
    , m_pEditorExecCap(0)
    , m_pEditorProcessorFeatures(0)
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
    return m_pEditorProcessorFeatures->isEnabledNestedVirtualization();
}

bool UIMachineSettingsSystem::isHIDEnabled() const
{
    return m_pEditorPointingHID->value() != KPointingHIDType_PS2Mouse;
}

KChipsetType UIMachineSettingsSystem::chipsetType() const
{
    return m_pEditorChipset->value();
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
    return m_pCache ? m_pCache->wasChanged() : false;
}

void UIMachineSettingsSystem::loadToCacheFrom(QVariant &data)
{
    /* Sanity check: */
    if (!m_pCache)
        return;

    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Prepare old data: */
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

    /* Cache old data: */
    m_pCache->cacheInitialData(oldSystemData);

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsSystem::getFromCache()
{
    /* Sanity check: */
    if (!m_pCache)
        return;

    /* Get old data from cache: */
    const UIDataSettingsMachineSystem &oldSystemData = m_pCache->base();

    /* We are doing that *now* because these combos have
     * dynamical content which depends on cashed value: */
    repopulateComboParavirtProviderType();

    /* Load old 'Motherboard' data from cache: */
    if (m_pEditorBaseMemory)
        m_pEditorBaseMemory->setValue(oldSystemData.m_iMemorySize);
    if (m_pEditorBootOrder)
        m_pEditorBootOrder->setValue(oldSystemData.m_bootItems);
    if (m_pEditorChipset)
        m_pEditorChipset->setValue(oldSystemData.m_chipsetType);
    if (m_pEditorPointingHID)
        m_pEditorPointingHID->setValue(oldSystemData.m_pointingHIDType);
    if (m_pEditorMotherboardFeatures)
    {
        m_pEditorMotherboardFeatures->setEnableIoApic(oldSystemData.m_fEnabledIoApic);
        m_pEditorMotherboardFeatures->setEnableEfi(oldSystemData.m_fEnabledEFI);
        m_pEditorMotherboardFeatures->setEnableUtcTime(oldSystemData.m_fEnabledUTC);
    }

    /* Load old 'Processor' data from cache: */
    if (m_pEditorVCPU)
        m_pEditorVCPU->setValue(oldSystemData.m_cCPUCount);
    if (m_pEditorExecCap)
        m_pEditorExecCap->setValue(oldSystemData.m_iCPUExecCap);
    if (m_pEditorProcessorFeatures)
    {
        m_pEditorProcessorFeatures->setEnablePae(oldSystemData.m_fEnabledPAE);
        m_pEditorProcessorFeatures->setEnableNestedVirtualization(oldSystemData.m_fEnabledNestedHwVirtEx);
    }

    /* Load old 'Acceleration' data from cache: */
    if (m_pComboParavirtProvider)
    {
        const int iParavirtProviderPosition = m_pComboParavirtProvider->findData(oldSystemData.m_paravirtProvider);
        m_pComboParavirtProvider->setCurrentIndex(iParavirtProviderPosition == -1 ? 0 : iParavirtProviderPosition);
    }
    if (m_pCheckBoxVirtualization)
        m_pCheckBoxVirtualization->setChecked(oldSystemData.m_fEnabledHwVirtEx);
    if (m_pCheckBoxNestedPaging)
        m_pCheckBoxNestedPaging->setChecked(oldSystemData.m_fEnabledNestedPaging);

    /* Polish page finally: */
    polishPage();

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsSystem::putToCache()
{
    /* Sanity check: */
    if (!m_pCache)
        return;

    /* Prepare new data: */
    UIDataSettingsMachineSystem newSystemData;

    /* Gather support flags: */
    newSystemData.m_fSupportedPAE = m_pCache->base().m_fSupportedPAE;
    newSystemData.m_fSupportedNestedHwVirtEx = isNestedHWVirtExSupported();
    newSystemData.m_fSupportedHwVirtEx = isHWVirtExSupported();
    newSystemData.m_fSupportedNestedPaging = isNestedPagingSupported();

    /* Gather 'Motherboard' data: */
    if (m_pEditorBaseMemory)
        newSystemData.m_iMemorySize = m_pEditorBaseMemory->value();
    if (m_pEditorBootOrder)
        newSystemData.m_bootItems = m_pEditorBootOrder->value();
    if (m_pEditorChipset)
        newSystemData.m_chipsetType = m_pEditorChipset->value();
    if (m_pEditorPointingHID)
        newSystemData.m_pointingHIDType = m_pEditorPointingHID->value();
    if (   m_pEditorMotherboardFeatures
        && m_pEditorVCPU
        && m_pEditorChipset)
        newSystemData.m_fEnabledIoApic =    m_pEditorMotherboardFeatures->isEnabledIoApic()
                                         || m_pEditorVCPU->value() > 1
                                         || m_pEditorChipset->value() == KChipsetType_ICH9;
    if (m_pEditorMotherboardFeatures)
        newSystemData.m_fEnabledEFI = m_pEditorMotherboardFeatures->isEnabledEfi();
    if (m_pEditorMotherboardFeatures)
        newSystemData.m_fEnabledUTC = m_pEditorMotherboardFeatures->isEnabledUtcTime();

    /* Gather 'Processor' data: */
    if (m_pEditorVCPU)
        newSystemData.m_cCPUCount = m_pEditorVCPU->value();
    if (m_pEditorExecCap)
        newSystemData.m_iCPUExecCap = m_pEditorExecCap->value();
    if (m_pEditorProcessorFeatures)
        newSystemData.m_fEnabledPAE = m_pEditorProcessorFeatures->isEnabledPae();
    newSystemData.m_fEnabledNestedHwVirtEx = isNestedHWVirtExEnabled();

    /* Gather 'Acceleration' data: */
    if (m_pComboParavirtProvider)
        newSystemData.m_paravirtProvider = m_pComboParavirtProvider->currentData().value<KParavirtProvider>();
    /* Enable HW Virt Ex automatically if it's supported and
     * 1. multiple CPUs, 2. Nested Paging or 3. Nested HW Virt Ex is requested. */
    if (m_pEditorVCPU)
        newSystemData.m_fEnabledHwVirtEx =    isHWVirtExEnabled()
                                           || (   isHWVirtExSupported()
                                               && (   m_pEditorVCPU->value() > 1
                                                   || isNestedPagingEnabled()
                                                   || isNestedHWVirtExEnabled()));
    /* Enable Nested Paging automatically if it's supported and
     * Nested HW Virt Ex is requested. */
    newSystemData.m_fEnabledNestedPaging =    isNestedPagingEnabled()
                                     || (   isNestedPagingSupported()
                                         && isNestedHWVirtExEnabled());

    /* Cache new data: */
    m_pCache->cacheCurrentData(newSystemData);
}

void UIMachineSettingsSystem::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Update data and failing state: */
    setFailed(!saveData());

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
        message.first = UITranslator::removeAccelMark(m_pTabWidget->tabText(0));

        /* RAM amount test: */
        const ulong uFullSize = uiCommon().host().GetMemorySize();
        if (m_pEditorBaseMemory->value() > (int)m_pEditorBaseMemory->maxRAMAlw())
        {
            message.second << tr(
                "More than <b>%1%</b> of the host computer's memory (<b>%2</b>) is assigned to the virtual machine. "
                "Not enough memory is left for the host operating system. Please select a smaller amount.")
                .arg((unsigned)qRound((double)m_pEditorBaseMemory->maxRAMAlw() / uFullSize * 100.0))
                .arg(UITranslator::formatSize((uint64_t)uFullSize * _1M));
            fPass = false;
        }
        else if (m_pEditorBaseMemory->value() > (int)m_pEditorBaseMemory->maxRAMOpt())
        {
            message.second << tr(
                "More than <b>%1%</b> of the host computer's memory (<b>%2</b>) is assigned to the virtual machine. "
                "There might not be enough memory left for the host operating system. Please consider selecting a smaller amount.")
                .arg((unsigned)qRound((double)m_pEditorBaseMemory->maxRAMOpt() / uFullSize * 100.0))
                .arg(UITranslator::formatSize((uint64_t)uFullSize * _1M));
        }

        /* Chipset type vs IO-APIC test: */
        if (m_pEditorChipset->value() == KChipsetType_ICH9 && !m_pEditorMotherboardFeatures->isEnabledIoApic())
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
        message.first = UITranslator::removeAccelMark(m_pTabWidget->tabText(1));

        /* VCPU amount test: */
        const int cTotalCPUs = uiCommon().host().GetProcessorOnlineCoreCount();
        if (m_pEditorVCPU->value() > 2 * cTotalCPUs)
        {
            message.second << tr(
                "For performance reasons, the number of virtual CPUs attached to the virtual machine may not be more than twice the number "
                "of physical CPUs on the host (<b>%1</b>). Please reduce the number of virtual CPUs.")
                .arg(cTotalCPUs);
            fPass = false;
        }
        else if (m_pEditorVCPU->value() > cTotalCPUs)
        {
            message.second << tr(
                "More virtual CPUs are assigned to the virtual machine than the number of physical CPUs on the host system (<b>%1</b>). "
                "This is likely to degrade the performance of your virtual machine. Please consider reducing the number of virtual CPUs.")
                .arg(cTotalCPUs);
        }

        /* VCPU vs IO-APIC test: */
        if (m_pEditorVCPU->value() > 1 && !m_pEditorMotherboardFeatures->isEnabledIoApic())
        {
            message.second << tr(
                "The I/O APIC feature is not currently enabled in the Motherboard section of the System page. "
                "This is needed to support more than one virtual processor. "
                "It will be enabled automatically if you confirm your changes.");
        }

        /* VCPU: */
        if (m_pEditorVCPU->value() > 1)
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
        if (m_pEditorExecCap->value() < m_pEditorExecCap->medExecCap())
        {
            message.second << tr("The processor execution cap is set to a low value. This may make the machine feel slow to respond.");
        }

        /* Warn user about possible performance degradation and suggest lowering # of CPUs assigned to the VM instead: */
        if (m_pEditorExecCap->value() < 100)
        {
            if (m_pEditorVCPU->maxVCPUCount() > 1 && m_pEditorVCPU->value() > 1)
            {
                message.second << tr("Please consider lowering the number of CPUs assigned to the virtual machine rather "
                                     "than setting the processor execution cap.");
            }
            else if (m_pEditorVCPU->maxVCPUCount() > 1)
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
        message.first = UITranslator::removeAccelMark(m_pTabWidget->tabText(2));

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
    setTabOrder(m_pEditorBootOrder, m_pEditorChipset);
    setTabOrder(m_pEditorChipset, m_pEditorPointingHID);
    setTabOrder(m_pEditorPointingHID, m_pEditorMotherboardFeatures);
    setTabOrder(m_pEditorMotherboardFeatures, m_pEditorVCPU);

    /* Configure navigation for 'processor' tab: */
    setTabOrder(m_pEditorVCPU, m_pEditorExecCap);
    setTabOrder(m_pEditorExecCap, m_pEditorProcessorFeatures);
    setTabOrder(m_pEditorProcessorFeatures, m_pComboParavirtProvider);

    /* Configure navigation for 'acceleration' tab: */
    setTabOrder(m_pComboParavirtProvider, m_pCheckBoxVirtualization);
    setTabOrder(m_pCheckBoxVirtualization, m_pCheckBoxNestedPaging);
}

void UIMachineSettingsSystem::retranslateUi()
{
    m_pEditorBaseMemory->setToolTip(tr("Controls the amount of memory provided to the virtual machine. "
                                       "If you assign too much, the machine might not start."));
    m_pEditorBootOrder->setWhatsThis(tr("Defines the boot device order. Use the "
                                        "checkboxes on the left to enable or disable individual boot devices. "
                                        "Move items up and down to change the device order."));
    m_pTabWidget->setTabText(m_pTabWidget->indexOf(m_pTabMotherboard), tr("&Motherboard"));
    m_pTabWidget->setTabText(m_pTabWidget->indexOf(m_pTabProcessor), tr("&Processor"));
    m_pLabelParavirtProvider->setText(tr("&Paravirtualization Interface:"));
    m_pComboParavirtProvider->setToolTip(tr("Selects the paravirtualization guest interface provider to be used by this "
                                            "virtual machine."));
    m_pLabelVirtualization->setText(tr("Hardware Virtualization:"));
    m_pCheckBoxVirtualization->setToolTip(tr("When checked, the virtual machine will try to make use of the host CPU's hardware "
                                             "virtualization extensions such as Intel VT-x and AMD-V."));
    m_pCheckBoxVirtualization->setText(tr("Enable &VT-x/AMD-V"));
    m_pCheckBoxNestedPaging->setToolTip(tr("When checked, the virtual machine will try to make use of the nested paging "
                                           "extension of Intel VT-x and AMD-V."));
    m_pCheckBoxNestedPaging->setText(tr("Enable Nested Pa&ging"));
    m_pTabWidget->setTabText(m_pTabWidget->indexOf(m_pTabAcceleration), tr("Acce&leration"));

    /* Retranslate combo-boxes: */
    retranslateComboParavirtProvider();

    /* These editors have own labels, but we want them to be properly layouted according to each other: */
    int iMinimumLayoutHint = 0;
    iMinimumLayoutHint = qMax(iMinimumLayoutHint, m_pEditorBaseMemory->minimumLabelHorizontalHint());
    iMinimumLayoutHint = qMax(iMinimumLayoutHint, m_pEditorBootOrder->minimumLabelHorizontalHint());
    iMinimumLayoutHint = qMax(iMinimumLayoutHint, m_pEditorChipset->minimumLabelHorizontalHint());
    iMinimumLayoutHint = qMax(iMinimumLayoutHint, m_pEditorPointingHID->minimumLabelHorizontalHint());
    iMinimumLayoutHint = qMax(iMinimumLayoutHint, m_pEditorMotherboardFeatures->minimumLabelHorizontalHint());
    m_pEditorBaseMemory->setMinimumLayoutIndent(iMinimumLayoutHint);
    m_pEditorBootOrder->setMinimumLayoutIndent(iMinimumLayoutHint);
    m_pEditorChipset->setMinimumLayoutIndent(iMinimumLayoutHint);
    m_pEditorPointingHID->setMinimumLayoutIndent(iMinimumLayoutHint);
    m_pEditorMotherboardFeatures->setMinimumLayoutIndent(iMinimumLayoutHint);
    iMinimumLayoutHint = 0;
    iMinimumLayoutHint = qMax(iMinimumLayoutHint, m_pEditorVCPU->minimumLabelHorizontalHint());
    iMinimumLayoutHint = qMax(iMinimumLayoutHint, m_pEditorExecCap->minimumLabelHorizontalHint());
    iMinimumLayoutHint = qMax(iMinimumLayoutHint, m_pEditorProcessorFeatures->minimumLabelHorizontalHint());
    m_pEditorVCPU->setMinimumLayoutIndent(iMinimumLayoutHint);
    m_pEditorExecCap->setMinimumLayoutIndent(iMinimumLayoutHint);
    m_pEditorProcessorFeatures->setMinimumLayoutIndent(iMinimumLayoutHint);
}

void UIMachineSettingsSystem::polishPage()
{
    /* Get old data from cache: */
    const UIDataSettingsMachineSystem &systemData = m_pCache->base();

    /* Polish 'Motherboard' availability: */
    m_pEditorBaseMemory->setEnabled(isMachineOffline());
    m_pEditorBootOrder->setEnabled(isMachineOffline());
    m_pEditorChipset->setEnabled(isMachineOffline());
    m_pEditorPointingHID->setEnabled(isMachineOffline());
    m_pEditorMotherboardFeatures->setEnabled(isMachineOffline());

    /* Polish 'Processor' availability: */
    m_pEditorVCPU->setEnabled(isMachineOffline() && systemData.m_fSupportedHwVirtEx);
    m_pEditorExecCap->setEnabled(isMachineInValidMode());
    m_pEditorProcessorFeatures->setEnablePaeAvailable(isMachineOffline() && systemData.m_fSupportedPAE);
    m_pEditorProcessorFeatures->setEnableNestedVirtualizationAvailable(   isMachineOffline()
                                                                       && (   systemData.m_fSupportedNestedHwVirtEx
                                                                           || systemData.m_fEnabledNestedHwVirtEx));

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

    /* Prepare everything: */
    prepareWidgets();
    prepareConnections();

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
        m_pTabWidget = new QITabWidget(this);
        if (m_pTabWidget)
        {
            /* Prepare each tab separately: */
            prepareTabMotherboard();
            prepareTabProcessor();
            prepareTabAcceleration();

            pLayoutMain->addWidget(m_pTabWidget);
        }
    }
}

void UIMachineSettingsSystem::prepareTabMotherboard()
{
    /* Prepare 'Motherboard' tab: */
    m_pTabMotherboard = new QWidget;
    if (m_pTabMotherboard)
    {
        /* Prepare 'Motherboard' tab layout: */
        QGridLayout *pLayoutMotherboard = new QGridLayout(m_pTabMotherboard);
        if (pLayoutMotherboard)
        {
            pLayoutMotherboard->setColumnStretch(1, 1);
            pLayoutMotherboard->setRowStretch(5, 1);

            /* Prepare base memory editor: */
            m_pEditorBaseMemory = new UIBaseMemoryEditor(m_pTabMotherboard);
            if (m_pEditorBaseMemory)
                pLayoutMotherboard->addWidget(m_pEditorBaseMemory, 0, 0, 1, 2);

            /* Prepare boot order editor: */
            m_pEditorBootOrder = new UIBootOrderEditor(m_pTabMotherboard);
            if (m_pEditorBootOrder)
                pLayoutMotherboard->addWidget(m_pEditorBootOrder, 1, 0);

            /* Prepare chipset editor: */
            m_pEditorChipset = new UIChipsetEditor(m_pTabMotherboard);
            if (m_pEditorChipset)
                pLayoutMotherboard->addWidget(m_pEditorChipset, 2, 0);

            /* Prepare pointing HID editor: */
            m_pEditorPointingHID = new UIPointingHIDEditor(m_pTabMotherboard);
            if (m_pEditorPointingHID)
                pLayoutMotherboard->addWidget(m_pEditorPointingHID, 3, 0);

            /* Prepare motherboard features editor: */
            m_pEditorMotherboardFeatures = new UIMotherboardFeaturesEditor(m_pTabMotherboard);
            if (m_pEditorMotherboardFeatures)
                pLayoutMotherboard->addWidget(m_pEditorMotherboardFeatures, 4, 0);
        }

        m_pTabWidget->addTab(m_pTabMotherboard, QString());
    }
}

void UIMachineSettingsSystem::prepareTabProcessor()
{
    /* Prepare 'Processor' tab: */
    m_pTabProcessor = new QWidget;
    if (m_pTabProcessor)
    {
        /* Prepare 'Processor' tab layout: */
        QGridLayout *pLayoutProcessor = new QGridLayout(m_pTabProcessor);
        if (pLayoutProcessor)
        {
            pLayoutProcessor->setColumnStretch(1, 1);
            pLayoutProcessor->setRowStretch(3, 1);

            /* Prepare VCPU editor : */
            m_pEditorVCPU = new UIVirtualCPUEditor(m_pTabProcessor);
            if (m_pEditorVCPU)
                pLayoutProcessor->addWidget(m_pEditorVCPU, 0, 0, 1, 2);

            /* Prepare exec cap editor : */
            m_pEditorExecCap = new UIExecutionCapEditor(m_pTabProcessor);
            if (m_pEditorExecCap)
                pLayoutProcessor->addWidget(m_pEditorExecCap, 1, 0, 1, 2);

            /* Prepare processor features editor: */
            m_pEditorProcessorFeatures = new UIProcessorFeaturesEditor(m_pTabProcessor);
            if (m_pEditorProcessorFeatures)
                pLayoutProcessor->addWidget(m_pEditorProcessorFeatures, 2, 0);
        }

        m_pTabWidget->addTab(m_pTabProcessor, QString());
    }
}

void UIMachineSettingsSystem::prepareTabAcceleration()
{
    /* Prepare 'Acceleration' tab: */
    m_pTabAcceleration = new QWidget;
    if (m_pTabAcceleration)
    {
        /* Prepare 'Acceleration' tab layout: */
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
                if (m_pLabelParavirtProvider)
                    m_pLabelParavirtProvider->setBuddy(m_pComboParavirtProvider);
                m_pComboParavirtProvider->setSizeAdjustPolicy(QComboBox::AdjustToContents);
                m_pComboParavirtProvider->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));

                pLayoutAcceleration->addWidget(m_pComboParavirtProvider, 0, 1, 1, 2);
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
    connect(m_pEditorChipset, &UIChipsetEditor::sigValueChanged,
            this, &UIMachineSettingsSystem::revalidate);
    connect(m_pEditorPointingHID, &UIPointingHIDEditor::sigValueChanged,
            this, &UIMachineSettingsSystem::revalidate);
    connect(m_pEditorBaseMemory, &UIBaseMemoryEditor::sigValidChanged,
            this, &UIMachineSettingsSystem::revalidate);
    connect(m_pEditorMotherboardFeatures, &UIMotherboardFeaturesEditor::sigChangedIoApic,
            this, &UIMachineSettingsSystem::revalidate);

    /* Configure 'Processor' connections: */
    connect(m_pEditorVCPU, &UIVirtualCPUEditor::sigValueChanged,
            this, &UIMachineSettingsSystem::revalidate);
    connect(m_pEditorExecCap, &UIExecutionCapEditor::sigValueChanged,
            this, &UIMachineSettingsSystem::revalidate);
    connect(m_pEditorProcessorFeatures, &UIProcessorFeaturesEditor::sigChangedNestedVirtualization,
            this, &UIMachineSettingsSystem::revalidate);

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

void UIMachineSettingsSystem::repopulateComboParavirtProviderType()
{
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

bool UIMachineSettingsSystem::saveData()
{
    /* Sanity check: */
    if (!m_pCache)
        return false;

    /* Prepare result: */
    bool fSuccess = true;
    /* Save general settings from cache: */
    if (fSuccess && isMachineInValidMode() && m_pCache->wasChanged())
    {
        /* Save 'Motherboard' data from cache: */
        if (fSuccess)
            fSuccess = saveMotherboardData();
        /* Save 'Processor' data from cache: */
        if (fSuccess)
            fSuccess = saveProcessorData();
        /* Save 'Acceleration' data from cache: */
        if (fSuccess)
            fSuccess = saveAccelerationData();
    }
    /* Return result: */
    return fSuccess;
}

bool UIMachineSettingsSystem::saveMotherboardData()
{
    /* Sanity check: */
    if (!m_pCache)
        return false;

    /* Prepare result: */
    bool fSuccess = true;
    /* Save 'Motherboard' settings from cache: */
    if (fSuccess)
    {
        /* Get old data from cache: */
        const UIDataSettingsMachineSystem &oldSystemData = m_pCache->base();
        /* Get new data from cache: */
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
    /* Sanity check: */
    if (!m_pCache)
        return false;

    /* Prepare result: */
    bool fSuccess = true;
    /* Save 'Processor' settings from cache: */
    if (fSuccess)
    {
        /* Get old data from cache: */
        const UIDataSettingsMachineSystem &oldSystemData = m_pCache->base();
        /* Get new data from cache: */
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
    /* Sanity check: */
    if (!m_pCache)
        return false;

    /* Prepare result: */
    bool fSuccess = true;
    /* Save 'Acceleration' settings from cache: */
    if (fSuccess)
    {
        /* Get old data from cache: */
        const UIDataSettingsMachineSystem &oldSystemData = m_pCache->base();
        /* Get new data from cache: */
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
