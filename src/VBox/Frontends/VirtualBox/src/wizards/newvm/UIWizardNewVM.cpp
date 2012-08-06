/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardNewVM class implementation
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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
#include "UIWizardNewVM.h"
#include "UIWizardNewVMPageBasic1.h"
#include "UIWizardNewVMPageBasic2.h"
#include "UIWizardNewVMPageBasic3.h"
#include "UIWizardNewVMPageExpert.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"

/* COM includes: */
#include "CAudioAdapter.h"
#include "CUSBController.h"
#include "CExtPackManager.h"
#include "CStorageController.h"

UIWizardNewVM::UIWizardNewVM(QWidget *pParent, const QString &strGroup /* = QString() */)
    : UIWizard(pParent, UIWizardType_NewVM)
    , m_strGroup(strGroup)
    , m_iIDECount(0)
    , m_iSATACount(0)
    , m_iSCSICount(0)
    , m_iFloppyCount(0)
    , m_iSASCount(0)
{
#ifndef Q_WS_MAC
    /* Assign watermark: */
    assignWatermark(":/vmw_new_welcome.png");
#else /* Q_WS_MAC */
    /* Assign background image: */
    assignBackground(":/vmw_new_welcome_bg.png");
#endif /* Q_WS_MAC */
}

bool UIWizardNewVM::createVM()
{
    /* Get VBox object: */
    CVirtualBox vbox = vboxGlobal().virtualBox();

    /* OS type: */
    CGuestOSType type = field("type").value<CGuestOSType>();
    QString strTypeId = type.GetId();

    /* Create virtual machine: */
    if (m_machine.isNull())
    {
        QVector<QString> groups;
        if (!m_strGroup.isEmpty())
            groups << m_strGroup;
        m_machine = vbox.CreateMachine(QString(), field("name").toString(), groups, strTypeId, QString(), false);
        if (!vbox.isOk())
        {
            msgCenter().cannotCreateMachine(vbox, this);
            return false;
        }

        /* The FirstRun wizard is to be shown only when we don't attach any virtual hard drive or attach a new (empty) one.
         * Selecting an existing virtual hard drive will cancel the FirstRun wizard. */
        if (field("virtualDiskId").toString().isNull() || !field("virtualDisk").value<CMedium>().isNull())
            m_machine.SetExtraData(GUI_FirstRun, "yes");
    }

    /* RAM size: */
    m_machine.SetMemorySize(field("ram").toInt());

    /* VRAM size - select maximum between recommended and minimum for fullscreen: */
    m_machine.SetVRAMSize(qMax(type.GetRecommendedVRAM(), (ULONG)(VBoxGlobal::requiredVideoMemory(strTypeId) / _1M)));

    /* Selecting recommended chipset type: */
    m_machine.SetChipsetType(type.GetRecommendedChipset());

    /* Selecting recommended Audio Controller: */
    m_machine.GetAudioAdapter().SetAudioController(type.GetRecommendedAudioController());
    /* Enabling audio by default: */
    m_machine.GetAudioAdapter().SetEnabled(true);

    /* Enable the OHCI and EHCI controller by default for new VMs. (new in 2.2): */
    CUSBController usbController = m_machine.GetUSBController();
    if (!usbController.isNull() && type.GetRecommendedUSB() && usbController.GetProxyAvailable())
    {
        usbController.SetEnabled(true);
        /* USB 2.0 is only available if the proper ExtPack is installed.
         * Note. Configuring EHCI here and providing messages about
         * the missing extpack isn't exactly clean, but it is a
         * necessary evil to patch over legacy compatability issues
         * introduced by the new distribution model. */
        CExtPackManager manager = vboxGlobal().virtualBox().GetExtensionPackManager();
        if (manager.IsExtPackUsable(GUI_ExtPackName))
            usbController.SetEnabledEHCI(true);
    }

    /* Create a floppy controller if recommended: */
    QString strFloppyName = getNextControllerName(KStorageBus_Floppy);
    if (type.GetRecommendedFloppy())
    {
        m_machine.AddStorageController(strFloppyName, KStorageBus_Floppy);
        CStorageController flpCtr = m_machine.GetStorageControllerByName(strFloppyName);
        flpCtr.SetControllerType(KStorageControllerType_I82078);
    }

    /* Create recommended DVD storage controller: */
    KStorageBus strDVDBus = type.GetRecommendedDVDStorageBus();
    QString strDVDName = getNextControllerName(strDVDBus);
    m_machine.AddStorageController(strDVDName, strDVDBus);

    /* Set recommended DVD storage controller type: */
    CStorageController dvdCtr = m_machine.GetStorageControllerByName(strDVDName);
    KStorageControllerType dvdStorageControllerType = type.GetRecommendedDVDStorageController();
    dvdCtr.SetControllerType(dvdStorageControllerType);

    /* Create recommended HD storage controller if it's not the same as the DVD controller: */
    KStorageBus ctrHDBus = type.GetRecommendedHDStorageBus();
    KStorageControllerType hdStorageControllerType = type.GetRecommendedHDStorageController();
    CStorageController hdCtr;
    QString strHDName;
    if (ctrHDBus != strDVDBus || hdStorageControllerType != dvdStorageControllerType)
    {
        strHDName = getNextControllerName(ctrHDBus);
        m_machine.AddStorageController(strHDName, ctrHDBus);
        hdCtr = m_machine.GetStorageControllerByName(strHDName);
        hdCtr.SetControllerType(hdStorageControllerType);

        /* Set the port count to 1 if SATA is used. */
        if (hdStorageControllerType == KStorageControllerType_IntelAhci)
            hdCtr.SetPortCount(1);
    }
    else
    {
        /* The HD controller is the same as DVD: */
        hdCtr = dvdCtr;
        strHDName = strDVDName;
    }

    /* Turn on PAE, if recommended: */
    m_machine.SetCPUProperty(KCPUPropertyType_PAE, type.GetRecommendedPAE());

    /* Set recommended firmware type: */
    KFirmwareType fwType = type.GetRecommendedFirmware();
    m_machine.SetFirmwareType(fwType);

    /* Set recommended human interface device types: */
    if (type.GetRecommendedUSBHID())
    {
        m_machine.SetKeyboardHIDType(KKeyboardHIDType_USBKeyboard);
        m_machine.SetPointingHIDType(KPointingHIDType_USBMouse);
        if (!usbController.isNull())
            usbController.SetEnabled(true);
    }

    if (type.GetRecommendedUSBTablet())
    {
        m_machine.SetPointingHIDType(KPointingHIDType_USBTablet);
        if (!usbController.isNull())
            usbController.SetEnabled(true);
    }

    /* Set HPET flag: */
    m_machine.SetHPETEnabled(type.GetRecommendedHPET());

    /* Set UTC flags: */
    m_machine.SetRTCUseUTC(type.GetRecommendedRTCUseUTC());

    /* Set graphic bits: */
    if (type.GetRecommended2DVideoAcceleration())
        m_machine.SetAccelerate2DVideoEnabled(type.GetRecommended2DVideoAcceleration());

    if (type.GetRecommended3DAcceleration())
        m_machine.SetAccelerate3DEnabled(type.GetRecommended3DAcceleration());

    /* Register the VM prior to attaching hard disks: */
    vbox.RegisterMachine(m_machine);
    if (!vbox.isOk())
    {
        msgCenter().cannotCreateMachine(vbox, m_machine, this);
        return false;
    }

    /* Attach default devices: */
    {
        bool success = false;
        QString strMachineId = m_machine.GetId();
        CSession session = vboxGlobal().openSession(strMachineId);
        if (!session.isNull())
        {
            CMachine machine = session.GetMachine();

            QString strId = field("virtualDiskId").toString();
            /* Boot virtual hard drive: */
            if (!strId.isNull())
            {
                UIMedium vmedium = vboxGlobal().findMedium(strId);
                CMedium medium = vmedium.medium();              // @todo r=dj can this be cached somewhere?
                machine.AttachDevice(strHDName, 0, 0, KDeviceType_HardDisk, medium);
                if (!machine.isOk())
                    msgCenter().cannotAttachDevice(machine, UIMediumType_HardDisk, field("virtualDiskLocation").toString(),
                                                   StorageSlot(ctrHDBus, 0, 0), this);
            }

            /* Attach empty CD/DVD ROM Device */
            machine.AttachDevice(strDVDName, 1, 0, KDeviceType_DVD, CMedium());
            if (!machine.isOk())
                msgCenter().cannotAttachDevice(machine, UIMediumType_DVD, QString(), StorageSlot(strDVDBus, 1, 0), this);


            /* Attach an empty floppy drive if recommended */
            if (type.GetRecommendedFloppy()) {
                machine.AttachDevice(strFloppyName, 0, 0, KDeviceType_Floppy, CMedium());
                if (!machine.isOk())
                    msgCenter().cannotAttachDevice(machine, UIMediumType_Floppy, QString(),
                                                   StorageSlot(KStorageBus_Floppy, 0, 0), this);
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
            /* Unregister on failure */
            QVector<CMedium> aMedia = m_machine.Unregister(KCleanupMode_UnregisterOnly);   //  @todo replace with DetachAllReturnHardDisksOnly once a progress dialog is in place below
            if (vbox.isOk())
            {
                CProgress progress = m_machine.Delete(aMedia);
                progress.WaitForCompletion(-1);         // @todo do this nicely with a progress dialog, this can delete lots of files
            }
            return false;
        }
    }

    /* Ensure we don't try to delete a newly created virtual hard drive on success: */
    if (!field("virtualDisk").value<CMedium>().isNull())
        field("virtualDisk").value<CMedium>().detach();

    return true;
}

void UIWizardNewVM::retranslateUi()
{
    /* Call to base-class: */
    UIWizard::retranslateUi();

    /* Translate wizard: */
    setWindowTitle(tr("Create Virtual Machine"));
    setButtonText(QWizard::FinishButton, tr("Create"));
}

void UIWizardNewVM::prepare()
{
    /* Create corresponding pages: */
    switch (mode())
    {
        case UIWizardMode_Basic:
        {
            setPage(Page1, new UIWizardNewVMPageBasic1(m_strGroup));
            setPage(Page2, new UIWizardNewVMPageBasic2);
            setPage(Page3, new UIWizardNewVMPageBasic3);
            break;
        }
        case UIWizardMode_Expert:
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

QString UIWizardNewVM::getNextControllerName(KStorageBus type)
{
    QString strControllerName;
    switch (type)
    {
        case KStorageBus_IDE:
        {
            strControllerName = tr("IDE Controller");
            ++m_iIDECount;
            if (m_iIDECount > 1)
                strControllerName = QString("%1 %2").arg(strControllerName).arg(m_iIDECount);
            break;
        }
        case KStorageBus_SATA:
        {
            strControllerName = tr("SATA Controller");
            ++m_iSATACount;
            if (m_iSATACount > 1)
                strControllerName = QString("%1 %2").arg(strControllerName).arg(m_iSATACount);
            break;
        }
        case KStorageBus_SCSI:
        {
            strControllerName = tr("SCSI Controller");
            ++m_iSCSICount;
            if (m_iSCSICount > 1)
                strControllerName = QString("%1 %2").arg(strControllerName).arg(m_iSCSICount);
            break;
        }
        case KStorageBus_Floppy:
        {
            strControllerName = tr("Floppy Controller");
            ++m_iFloppyCount;
            if (m_iFloppyCount > 1)
                strControllerName = QString("%1 %2").arg(strControllerName).arg(m_iFloppyCount);
            break;
        }
        case KStorageBus_SAS:
        {
            strControllerName = tr("SAS Controller");
            ++m_iSASCount;
            if (m_iSASCount > 1)
                strControllerName = QString("%1 %2").arg(strControllerName).arg(m_iSASCount);
            break;
        }
        default:
            break;
    }
    return strControllerName;
}

