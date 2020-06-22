/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVM class implementation.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* GUI includes: */
#include "UICommon.h"
#include "UIWizardNewVM.h"
#include "UIWizardNewVMPageBasicUnattended.h"
#include "UIWizardNewVMPageBasicNameType.h"
#include "UIWizardNewVMPageBasicHardware.h"
#include "UIWizardNewVMPageBasicDisk.h"
#include "UIWizardNewVMPageExpert.h"
#include "UIWizardNewVMPageBasicInstallSetup.h"
#include "UIMessageCenter.h"
#include "UIMedium.h"

/* COM includes: */
#include "CAudioAdapter.h"
#include "CGraphicsAdapter.h"
#include "CUSBController.h"
#include "CUSBDeviceFilters.h"
#include "CExtPackManager.h"
#include "CStorageController.h"

/* Namespaces: */
using namespace UIExtraDataDefs;


UIUnattendedInstallData::UIUnattendedInstallData()
    : m_fUnattendedEnabled(false)
    , m_fStartHeadless(false)
{
}

UIWizardNewVM::UIWizardNewVM(QWidget *pParent, const QString &strGroup /* = QString() */)
    : UIWizard(pParent, WizardType_NewVM)
    , m_strGroup(strGroup)
    , m_iIDECount(0)
    , m_iSATACount(0)
    , m_iSCSICount(0)
    , m_iFloppyCount(0)
    , m_iSASCount(0)
    , m_iUSBCount(0)
{
#ifndef VBOX_WS_MAC
    /* Assign watermark: */
    assignWatermark(":/wizard_new_welcome.png");
#else /* VBOX_WS_MAC */
    /* Assign background image: */
    assignBackground(":/wizard_new_welcome_bg.png");
#endif /* VBOX_WS_MAC */
    /* Register classes: */
    qRegisterMetaType<CGuestOSType>();

    connect(this, &UIWizardNewVM::rejected, this, &UIWizardNewVM::sltHandleWizardCancel);
}

void UIWizardNewVM::prepare()
{
    /* Create corresponding pages: */
    switch (mode())
    {
        case WizardMode_Basic:
        {
            setPage(PageUnattended, new UIWizardNewVMPageBasicUnattended);
            setPage(PageNameType, new UIWizardNewVMPageBasicNameType(m_strGroup));
            setPage(PageInstallSetup, new UIWizardNewVMPageBasicInstallSetup);
            setPage(PageHardware, new UIWizardNewVMPageBasicHardware);
            setPage(PageDisk, new UIWizardNewVMPageBasicDisk);
            setStartId(PageUnattended);
            break;
        }
        case WizardMode_Expert:
        {
            setPage(PageExpert, new UIWizardNewVMPageExpert(m_strGroup));
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid mode: %d", mode()));
            break;
        }
    }
    /* Call to base-class: */
    UIWizard::prepare();
}

bool UIWizardNewVM::createVM()
{
    /* Get VBox object: */
    CVirtualBox vbox = uiCommon().virtualBox();

    /* OS type: */
    CGuestOSType type = field("type").value<CGuestOSType>();
    QString strTypeId = type.GetId();

    /* Create virtual machine: */
    if (m_machine.isNull())
    {
        QVector<QString> groups;
        if (!m_strGroup.isEmpty())
            groups << m_strGroup;
        m_machine = vbox.CreateMachine(field("machineFilePath").toString(),
                                       field("name").toString(),
                                       groups, strTypeId, QString());
        if (!vbox.isOk())
        {
            msgCenter().cannotCreateMachine(vbox, this);
            return false;
        }

        /* The First RUN Wizard is to be shown:
         * 1. if we don't attach any virtual hard-drive
         * 2. or attach a new (empty) one.
         * 3. and if the unattended install is not enabled
         * Usually we are assigning extra-data values through UIExtraDataManager,
         * but in that special case VM was not registered yet, so UIExtraDataManager is unaware of it: */
        if (!isUnattendedInstallEnabled() &&
            (field("virtualDiskId").toString().isNull() || !field("virtualDisk").value<CMedium>().isNull()))
            m_machine.SetExtraData(GUI_FirstRun, "yes");
    }

#if 0
    /* Configure the newly created vm here in GUI by several calls to API: */
    configureVM(strTypeId, type);
#else
    /* The newer and less tested way of configuring vms: */
    m_machine.ApplyDefaults(QString());
    /* Apply user preferences again. IMachine::applyDefaults may have overwritten the user setting: */
    m_machine.SetMemorySize(field("baseMemory").toUInt());
    m_machine.SetCPUCount(field("VCPUCount").toUInt());
    /* Correct the VRAM size since API does not take fullscreen memory requirements into account: */
    CGraphicsAdapter comGraphics = m_machine.GetGraphicsAdapter();
    comGraphics.SetVRAMSize(qMax(comGraphics.GetVRAMSize(), (ULONG)(UICommon::requiredVideoMemory(strTypeId) / _1M)));
#endif

    /* Register the VM prior to attaching hard disks: */
    vbox.RegisterMachine(m_machine);
    if (!vbox.isOk())
    {
        msgCenter().cannotRegisterMachine(vbox, m_machine.GetName(), this);
        return false;
    }
    return attachDefaultDevices(type);
}

void UIWizardNewVM::configureVM(const QString &strGuestTypeId, const CGuestOSType &comGuestType)
{
    /* Get graphics adapter: */
    CGraphicsAdapter comGraphics = m_machine.GetGraphicsAdapter();

    /* RAM size: */
    m_machine.SetMemorySize(field("baseMemory").toInt());
    /* VCPU count: */
    m_machine.SetCPUCount(field("VCPUCount").toUInt());


    /* Graphics Controller type: */
    comGraphics.SetGraphicsControllerType(comGuestType.GetRecommendedGraphicsController());

    /* VRAM size - select maximum between recommended and minimum for fullscreen: */
    comGraphics.SetVRAMSize(qMax(comGuestType.GetRecommendedVRAM(), (ULONG)(UICommon::requiredVideoMemory(strGuestTypeId) / _1M)));

    /* Selecting recommended chipset type: */
    m_machine.SetChipsetType(comGuestType.GetRecommendedChipset());

    /* Selecting recommended Audio Controller: */
    m_machine.GetAudioAdapter().SetAudioController(comGuestType.GetRecommendedAudioController());
    /* And the Audio Codec: */
    m_machine.GetAudioAdapter().SetAudioCodec(comGuestType.GetRecommendedAudioCodec());
    /* Enabling audio by default: */
    m_machine.GetAudioAdapter().SetEnabled(true);
    m_machine.GetAudioAdapter().SetEnabledOut(true);

    /* Enable the OHCI and EHCI controller by default for new VMs. (new in 2.2): */
    CUSBDeviceFilters usbDeviceFilters = m_machine.GetUSBDeviceFilters();
    bool fOhciEnabled = false;
    if (!usbDeviceFilters.isNull() && comGuestType.GetRecommendedUSB3() && m_machine.GetUSBProxyAvailable())
    {
        /* USB 3.0 is only available if the proper ExtPack is installed: */
        CExtPackManager manager = uiCommon().virtualBox().GetExtensionPackManager();
        if (manager.IsExtPackUsable(GUI_ExtPackName))
        {
            m_machine.AddUSBController("XHCI", KUSBControllerType_XHCI);
            /* xHci includes OHCI */
            fOhciEnabled = true;
        }
    }
    if (   !fOhciEnabled
        && !usbDeviceFilters.isNull() && comGuestType.GetRecommendedUSB() && m_machine.GetUSBProxyAvailable())
    {
        m_machine.AddUSBController("OHCI", KUSBControllerType_OHCI);
        fOhciEnabled = true;
        /* USB 2.0 is only available if the proper ExtPack is installed.
         * Note. Configuring EHCI here and providing messages about
         * the missing extpack isn't exactly clean, but it is a
         * necessary evil to patch over legacy compatability issues
         * introduced by the new distribution model. */
        CExtPackManager manager = uiCommon().virtualBox().GetExtensionPackManager();
        if (manager.IsExtPackUsable(GUI_ExtPackName))
            m_machine.AddUSBController("EHCI", KUSBControllerType_EHCI);
    }

    /* Create a floppy controller if recommended: */
    QString strFloppyName = getNextControllerName(KStorageBus_Floppy);
    if (comGuestType.GetRecommendedFloppy())
    {
        m_machine.AddStorageController(strFloppyName, KStorageBus_Floppy);
        CStorageController flpCtr = m_machine.GetStorageControllerByName(strFloppyName);
        flpCtr.SetControllerType(KStorageControllerType_I82078);
    }

    /* Create recommended DVD storage controller: */
    KStorageBus strDVDBus = comGuestType.GetRecommendedDVDStorageBus();
    QString strDVDName = getNextControllerName(strDVDBus);
    m_machine.AddStorageController(strDVDName, strDVDBus);

    /* Set recommended DVD storage controller type: */
    CStorageController dvdCtr = m_machine.GetStorageControllerByName(strDVDName);
    KStorageControllerType dvdStorageControllerType = comGuestType.GetRecommendedDVDStorageController();
    dvdCtr.SetControllerType(dvdStorageControllerType);

    /* Create recommended HD storage controller if it's not the same as the DVD controller: */
    KStorageBus ctrHDBus = comGuestType.GetRecommendedHDStorageBus();
    KStorageControllerType hdStorageControllerType = comGuestType.GetRecommendedHDStorageController();
    CStorageController hdCtr;
    QString strHDName;
    if (ctrHDBus != strDVDBus || hdStorageControllerType != dvdStorageControllerType)
    {
        strHDName = getNextControllerName(ctrHDBus);
        m_machine.AddStorageController(strHDName, ctrHDBus);
        hdCtr = m_machine.GetStorageControllerByName(strHDName);
        hdCtr.SetControllerType(hdStorageControllerType);
    }
    else
    {
        /* The HD controller is the same as DVD: */
        hdCtr = dvdCtr;
        strHDName = strDVDName;
    }

    /* Limit the AHCI port count if it's used because windows has trouble with
       too many ports and other guest (OS X in particular) may take extra long
       to boot: */
    if (hdStorageControllerType == KStorageControllerType_IntelAhci)
        hdCtr.SetPortCount(1 + (dvdStorageControllerType == KStorageControllerType_IntelAhci));
    else if (dvdStorageControllerType == KStorageControllerType_IntelAhci)
        dvdCtr.SetPortCount(1);

    /* Turn on PAE, if recommended: */
    m_machine.SetCPUProperty(KCPUPropertyType_PAE, comGuestType.GetRecommendedPAE());

    /* Set the recommended triple fault behavior: */
    m_machine.SetCPUProperty(KCPUPropertyType_TripleFaultReset, comGuestType.GetRecommendedTFReset());

    /* Set recommended firmware type: */
    KFirmwareType fwType = comGuestType.GetRecommendedFirmware();
    m_machine.SetFirmwareType(fwType);

    /* Set recommended human interface device types: */
    if (comGuestType.GetRecommendedUSBHID())
    {
        m_machine.SetKeyboardHIDType(KKeyboardHIDType_USBKeyboard);
        m_machine.SetPointingHIDType(KPointingHIDType_USBMouse);
        if (!fOhciEnabled && !usbDeviceFilters.isNull())
            m_machine.AddUSBController("OHCI", KUSBControllerType_OHCI);
    }

    if (comGuestType.GetRecommendedUSBTablet())
    {
        m_machine.SetPointingHIDType(KPointingHIDType_USBTablet);
        if (!fOhciEnabled && !usbDeviceFilters.isNull())
            m_machine.AddUSBController("OHCI", KUSBControllerType_OHCI);
    }

    /* Set HPET flag: */
    m_machine.SetHPETEnabled(comGuestType.GetRecommendedHPET());

    /* Set UTC flags: */
    m_machine.SetRTCUseUTC(comGuestType.GetRecommendedRTCUseUTC());

    /* Set graphic bits: */
    if (comGuestType.GetRecommended2DVideoAcceleration())
        comGraphics.SetAccelerate2DVideoEnabled(comGuestType.GetRecommended2DVideoAcceleration());

    if (comGuestType.GetRecommended3DAcceleration())
        comGraphics.SetAccelerate3DEnabled(comGuestType.GetRecommended3DAcceleration());
}

bool UIWizardNewVM::attachDefaultDevices(const CGuestOSType &comGuestType)
{
    bool success = false;
    QUuid uMachineId = m_machine.GetId();
    CSession session = uiCommon().openSession(uMachineId);
    if (!session.isNull())
    {
        CMachine machine = session.GetMachine();

        QUuid uId = field("virtualDiskId").toUuid();
        /* Boot virtual hard drive: */
        if (!uId.isNull())
        {
            KStorageBus enmHDDBus = comGuestType.GetRecommendedHDStorageBus();
            CStorageController comHDDController = m_machine.GetStorageControllerByInstance(enmHDDBus, 0);
            if (!comHDDController.isNull())
            {
                UIMedium vmedium = uiCommon().medium(uId);
                CMedium medium = vmedium.medium();              /// @todo r=dj can this be cached somewhere?
                machine.AttachDevice(comHDDController.GetName(), 0, 0, KDeviceType_HardDisk, medium);
                if (!machine.isOk())
                    msgCenter().cannotAttachDevice(machine, UIMediumDeviceType_HardDisk, field("virtualDiskLocation").toString(),
                                                   StorageSlot(enmHDDBus, 0, 0), this);
            }
        }

        /* Attach empty optical drive: */
        KStorageBus enmDVDBus = comGuestType.GetRecommendedDVDStorageBus();
        CStorageController comDVDController = m_machine.GetStorageControllerByInstance(enmDVDBus, 0);
        if (!comDVDController.isNull())
        {
            machine.AttachDevice(comDVDController.GetName(), 1, 0, KDeviceType_DVD, CMedium());
            if (!machine.isOk())
                msgCenter().cannotAttachDevice(machine, UIMediumDeviceType_DVD, QString(),
                                               StorageSlot(enmDVDBus, 1, 0), this);

        }

        /* Attach an empty floppy drive if recommended */
        if (comGuestType.GetRecommendedFloppy()) {
            CStorageController comFloppyController = m_machine.GetStorageControllerByInstance(KStorageBus_Floppy, 0);
            if (!comFloppyController.isNull())
            {
                machine.AttachDevice(comFloppyController.GetName(), 0, 0, KDeviceType_Floppy, CMedium());
                if (!machine.isOk())
                    msgCenter().cannotAttachDevice(machine, UIMediumDeviceType_Floppy, QString(),
                                                   StorageSlot(KStorageBus_Floppy, 0, 0), this);
            }
        }

        if (machine.isOk())
        {
            machine.SaveSettings();
            if (machine.isOk())
                success = true;
            else
                msgCenter().cannotSaveMachineSettings(machine, this);
        }

        session.UnlockMachine();
    }
    if (!success)
    {
        CVirtualBox vbox = uiCommon().virtualBox();
        /* Unregister on failure */
        QVector<CMedium> aMedia = m_machine.Unregister(KCleanupMode_UnregisterOnly);   /// @todo replace with DetachAllReturnHardDisksOnly once a progress dialog is in place below
        if (vbox.isOk())
        {
            CProgress progress = m_machine.DeleteConfig(aMedia);
            progress.WaitForCompletion(-1);         /// @todo do this nicely with a progress dialog, this can delete lots of files
        }
        return false;
    }

    /* Ensure we don't try to delete a newly created virtual hard drive on success: */
    if (!field("virtualDisk").value<CMedium>().isNull())
        field("virtualDisk").value<CMedium>().detach();

    return true;
}

// int UIWizardNewVM::nextId() const
// {
//     switch (currentId())
//     {
//         case PageUnattended:
//             return PageNameType;
//             break;
//         case PageNameType:
//             if (!isUnattendedInstallEnabled())
//                 return PageHardware;
//             else
//                 return PageInstallSetup;
//             break;
//         case PageInstallSetup:
//                 return PageHardware;
//         case PageHardware:
//             return PageDisk;
//             break;
//         case PageDisk:
//             return UIWizard::nextId();
//         case PageMax:
//         default:
//             return PageUnattended;
//             break;
//     }
//     return UIWizard::nextId();
// }

void UIWizardNewVM::sltHandleWizardCancel()
{
    switch (mode())
    {
        case WizardMode_Basic:
        {
            UIWizardNewVMPageBasicNameType *pPage = qobject_cast<UIWizardNewVMPageBasicNameType*> (page(PageNameType));
            /* Make sure that we were able to find the page that created the folder. */
            Assert(pPage);
            if (pPage)
                pPage->cleanupMachineFolder(true);
            break;
        }
        case WizardMode_Expert:
        {
            UIWizardNewVMPageExpert *pPage = qobject_cast<UIWizardNewVMPageExpert*> (page(PageExpert));
            if (pPage)
                pPage->cleanupMachineFolder(true);
            break;
        }
        default:
            break;
    }
}

void UIWizardNewVM::retranslateUi()
{
    /* Call to base-class: */
    UIWizard::retranslateUi();

    /* Translate wizard: */
    setWindowTitle(tr("Create Virtual Machine"));
    setButtonText(QWizard::FinishButton, tr("Create"));
}

QString UIWizardNewVM::getNextControllerName(KStorageBus type)
{
    QString strControllerName;
    switch (type)
    {
        case KStorageBus_IDE:
        {
            strControllerName = "IDE";
            ++m_iIDECount;
            if (m_iIDECount > 1)
                strControllerName = QString("%1 %2").arg(strControllerName).arg(m_iIDECount);
            break;
        }
        case KStorageBus_SATA:
        {
            strControllerName = "SATA";
            ++m_iSATACount;
            if (m_iSATACount > 1)
                strControllerName = QString("%1 %2").arg(strControllerName).arg(m_iSATACount);
            break;
        }
        case KStorageBus_SCSI:
        {
            strControllerName = "SCSI";
            ++m_iSCSICount;
            if (m_iSCSICount > 1)
                strControllerName = QString("%1 %2").arg(strControllerName).arg(m_iSCSICount);
            break;
        }
        case KStorageBus_Floppy:
        {
            strControllerName = "Floppy";
            ++m_iFloppyCount;
            if (m_iFloppyCount > 1)
                strControllerName = QString("%1 %2").arg(strControllerName).arg(m_iFloppyCount);
            break;
        }
        case KStorageBus_SAS:
        {
            strControllerName = "SAS";
            ++m_iSASCount;
            if (m_iSASCount > 1)
                strControllerName = QString("%1 %2").arg(strControllerName).arg(m_iSASCount);
            break;
        }
        case KStorageBus_USB:
        {
            strControllerName = "USB";
            ++m_iUSBCount;
            if (m_iUSBCount > 1)
                strControllerName = QString("%1 %2").arg(strControllerName).arg(m_iUSBCount);
            break;
        }
        default:
            break;
    }
    return strControllerName;
}

QUuid UIWizardNewVM::createdMachineId() const
{
    if (m_machine.isOk())
        return m_machine.GetId();
    return QUuid();
}

void UIWizardNewVM::setDefaultUnattendedInstallData(const UIUnattendedInstallData &unattendedInstallData)
{
    m_unattendedInstallData = unattendedInstallData;
    UIWizardNewVMPageBasicInstallSetup *pPage = qobject_cast<UIWizardNewVMPageBasicInstallSetup *>(page(PageInstallSetup));
    if (pPage)
        pPage->setDefaultUnattendedInstallData(unattendedInstallData);
}

const UIUnattendedInstallData &UIWizardNewVM::unattendedInstallData() const
{
    QVariant fieldValue = field("ISOFilePath");
    if (!fieldValue.isNull() && fieldValue.isValid() && fieldValue.canConvert(QMetaType::QString))
        m_unattendedInstallData.m_strISOPath = fieldValue.toString();

    fieldValue = field("isUnattendedEnabled");
    if (!fieldValue.isNull() && fieldValue.isValid() && fieldValue.canConvert(QMetaType::Bool))
        m_unattendedInstallData.m_fUnattendedEnabled = fieldValue.toBool();

    fieldValue = field("startHeadless");
    if (!fieldValue.isNull() && fieldValue.isValid() && fieldValue.canConvert(QMetaType::Bool))
        m_unattendedInstallData.m_fStartHeadless = fieldValue.toBool();

    fieldValue = field("userName");
    if (!fieldValue.isNull() && fieldValue.isValid() && fieldValue.canConvert(QMetaType::QString))
        m_unattendedInstallData.m_strUserName = fieldValue.toString();

    fieldValue = field("password");
    if (!fieldValue.isNull() && fieldValue.isValid() && fieldValue.canConvert(QMetaType::QString))
        m_unattendedInstallData.m_strPassword = fieldValue.toString();

    m_unattendedInstallData.m_uMachineUid = createdMachineId();

    return m_unattendedInstallData;
}

bool UIWizardNewVM::isUnattendedInstallEnabled() const
{
    QVariant fieldValue = field("isUnattendedEnabled");
    if (fieldValue.isNull() || !fieldValue.isValid() || !fieldValue.canConvert(QMetaType::Bool))
        return false;
    return fieldValue.toBool();
}
