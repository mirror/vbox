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
#include <QHeaderView>

/* GUI includes: */
#include "QIWidgetValidator.h"
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
    /* Translate uic generated strings: */
    Ui::UIMachineSettingsSystem::retranslateUi(this);

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
    /* Apply UI decorations: */
    Ui::UIMachineSettingsSystem::setupUi(this);

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
