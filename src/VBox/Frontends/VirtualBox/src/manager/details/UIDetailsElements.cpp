/* $Id$ */
/** @file
 * VBox Qt GUI - UIDetailsElement[Name] classes implementation.
 */

/*
 * Copyright (C) 2012-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QDir>
# include <QTimer>
# include <QGraphicsLinearLayout>

/* GUI includes: */
# include "UIDetailsElements.h"
# include "UIDetailsModel.h"
# include "UIMachinePreview.h"
# include "UIGraphicsRotatorButton.h"
# include "VBoxGlobal.h"
# include "UIIconPool.h"
# include "UIConverter.h"
# include "UIGraphicsTextPane.h"
# include "UIErrorString.h"

/* COM includes: */
# include "COMEnums.h"
# include "CMachine.h"
# include "CSystemProperties.h"
# include "CVRDEServer.h"
# include "CStorageController.h"
# include "CMediumAttachment.h"
# include "CAudioAdapter.h"
# include "CRecordingSettings.h"
# include "CRecordingScreenSettings.h"
# include "CNetworkAdapter.h"
# include "CSerialPort.h"
# include "CUSBController.h"
# include "CUSBDeviceFilters.h"
# include "CUSBDeviceFilter.h"
# include "CSharedFolder.h"
# include "CMedium.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIDetailsUpdateTask::UIDetailsUpdateTask(const CMachine &machine)
    : UITask(UITask::Type_DetailsPopulation)
{
    /* Store machine as property: */
    setProperty("machine", QVariant::fromValue(machine));
}

UIDetailsElementInterface::UIDetailsElementInterface(UIDetailsSet *pParent, DetailsElementType type, bool fOpened)
    : UIDetailsElement(pParent, type, fOpened)
    , m_pTask(0)
{
    /* Listen for the global thread-pool: */
    connect(vboxGlobal().threadPool(), SIGNAL(sigTaskComplete(UITask*)),
            this, SLOT(sltUpdateAppearanceFinished(UITask*)));

    /* Translate finally: */
    retranslateUi();
}

void UIDetailsElementInterface::retranslateUi()
{
    /* Assign corresponding name: */
    setName(gpConverter->toString(elementType()));
}

void UIDetailsElementInterface::updateAppearance()
{
    /* Call to base-class: */
    UIDetailsElement::updateAppearance();

    /* Prepare/start update task: */
    if (!m_pTask)
    {
        /* Prepare update task: */
        m_pTask = createUpdateTask();
        /* Post task into global thread-pool: */
        vboxGlobal().threadPool()->enqueueTask(m_pTask);
    }
}

void UIDetailsElementInterface::sltUpdateAppearanceFinished(UITask *pTask)
{
    /* Make sure that is one of our tasks: */
    if (pTask->type() != UITask::Type_DetailsPopulation)
        return;

    /* Skip unrelated tasks: */
    if (m_pTask != pTask)
        return;

    /* Assign new text if changed: */
    const UITextTable newText = pTask->property("table").value<UITextTable>();
    if (text() != newText)
        setText(newText);

    /* Mark task processed: */
    m_pTask = 0;

    /* Notify listeners about update task complete: */
    emit sigBuildDone();
}


UIDetailsElementPreview::UIDetailsElementPreview(UIDetailsSet *pParent, bool fOpened)
    : UIDetailsElement(pParent, DetailsElementType_Preview, fOpened)
{
    /* Create preview: */
    m_pPreview = new UIMachinePreview(this);
    AssertPtr(m_pPreview);
    {
        /* Configure preview: */
        connect(m_pPreview, SIGNAL(sigSizeHintChanged()),
                this, SLOT(sltPreviewSizeHintChanged()));
    }

    /* Translate finally: */
    retranslateUi();
}

void UIDetailsElementPreview::sltPreviewSizeHintChanged()
{
    /* Recursively update size-hints: */
    updateGeometry();
    /* Update whole model layout: */
    model()->updateLayout();
}

void UIDetailsElementPreview::retranslateUi()
{
    /* Assign corresponding name: */
    setName(gpConverter->toString(elementType()));
}

int UIDetailsElementPreview::minimumWidthHint() const
{
    /* Prepare variables: */
    int iMargin = data(ElementData_Margin).toInt();

    /* Calculating proposed width: */
    int iProposedWidth = 0;

    /* Maximum between header width and preview width: */
    iProposedWidth += qMax(minimumHeaderWidth(), m_pPreview->minimumSizeHint().toSize().width());

    /* Two margins: */
    iProposedWidth += 2 * iMargin;

    /* Return result: */
    return iProposedWidth;
}

int UIDetailsElementPreview::minimumHeightHintForElement(bool fClosed) const
{
    /* Prepare variables: */
    int iMargin = data(ElementData_Margin).toInt();

    /* Calculating proposed height: */
    int iProposedHeight = 0;

    /* Two margins: */
    iProposedHeight += 2 * iMargin;

    /* Header height: */
    iProposedHeight += minimumHeaderHeight();

    /* Element is opened? */
    if (!fClosed)
    {
        iProposedHeight += iMargin;
        iProposedHeight += m_pPreview->minimumSizeHint().toSize().height();
    }
    else
    {
        /* Additional height during animation: */
        if (button()->isAnimationRunning())
            iProposedHeight += additionalHeight();
    }

    /* Return result: */
    return iProposedHeight;
}

void UIDetailsElementPreview::updateLayout()
{
    /* Call to base-class: */
    UIDetailsElement::updateLayout();

    /* Show/hide preview: */
    if (isClosed() && m_pPreview->isVisible())
        m_pPreview->hide();
    if (isOpened() && !m_pPreview->isVisible() && !isAnimationRunning())
        m_pPreview->show();

    /* And update preview layout itself: */
    const int iMargin = data(ElementData_Margin).toInt();
    m_pPreview->setPos(iMargin, 2 * iMargin + minimumHeaderHeight());
    m_pPreview->resize(m_pPreview->minimumSizeHint());
}

void UIDetailsElementPreview::updateAppearance()
{
    /* Call to base-class: */
    UIDetailsElement::updateAppearance();

    /* Set new machine attribute directly: */
    m_pPreview->setMachine(machine());
    m_pPreview->resize(m_pPreview->minimumSizeHint());
    emit sigBuildDone();
}


void UIDetailsUpdateTaskGeneral::run()
{
    /* Acquire corresponding machine: */
    CMachine comMachine = property("machine").value<CMachine>();
    if (comMachine.isNull())
        return;

    /* Prepare table: */
    UITextTable table;

    /* Gather information: */
    if (comMachine.GetAccessible())
    {
        /* Name: */
        if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral_Name)
            table << UITextTableLine(QApplication::translate("UIDetails", "Name", "details (general)"), comMachine.GetName());

        /* Operating system: */
        if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral_OS)
            table << UITextTableLine(QApplication::translate("UIDetails", "Operating System", "details (general)"),
                                     vboxGlobal().vmGuestOSTypeDescription(comMachine.GetOSTypeId()));

        /* Settings file location: */
        if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral_Location)
            table << UITextTableLine(QApplication::translate("UIDetails", "Settings File Location", "details (general)"),
                                     QDir::toNativeSeparators(QFileInfo(comMachine.GetSettingsFilePath()).absolutePath()));

        /* Groups: */
        if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeGeneral_Groups)
        {
            QStringList groups = comMachine.GetGroups().toList();
            /* Do not show groups for machine which is in root group only: */
            if (groups.size() == 1)
                groups.removeAll("/");
            /* If group list still not empty: */
            if (!groups.isEmpty())
            {
                /* For every group: */
                for (int i = 0; i < groups.size(); ++i)
                {
                    /* Trim first '/' symbol: */
                    QString &strGroup = groups[i];
                    if (strGroup.startsWith("/") && strGroup != "/")
                        strGroup.remove(0, 1);
                }
                table << UITextTableLine(QApplication::translate("UIDetails", "Groups", "details (general)"), groups.join(", "));
            }
        }
    }
    else
        table << UITextTableLine(QApplication::translate("UIDetails", "Information Inaccessible", "details"), QString());

    /* Save the table as property: */
    setProperty("table", QVariant::fromValue(table));
}

UITask *UIDetailsElementGeneral::createUpdateTask()
{
    return new UIDetailsUpdateTaskGeneral(machine(), model()->optionsGeneral());
}


void UIDetailsUpdateTaskSystem::run()
{
    /* Acquire corresponding machine: */
    CMachine comMachine = property("machine").value<CMachine>();
    if (comMachine.isNull())
        return;

    /* Prepare table: */
    UITextTable table;

    /* Gather information: */
    if (comMachine.GetAccessible())
    {
        /* Base memory: */
        if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeSystem_RAM)
            table << UITextTableLine(QApplication::translate("UIDetails", "Base Memory", "details (system)"),
                                     QApplication::translate("UIDetails", "%1 MB", "details").arg(comMachine.GetMemorySize()));

        /* Processors: */
        if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeSystem_CPUCount)
        {
            const int cCPU = comMachine.GetCPUCount();
            if (cCPU > 1)
                table << UITextTableLine(QApplication::translate("UIDetails", "Processors", "details (system)"),
                                         QString::number(cCPU));
        }

        /* Execution cap: */
        if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeSystem_CPUExecutionCap)
        {
            const int iCPUExecutionCap = comMachine.GetCPUExecutionCap();
            if (iCPUExecutionCap < 100)
                table << UITextTableLine(QApplication::translate("UIDetails", "Execution Cap", "details (system)"),
                                         QApplication::translate("UIDetails", "%1%", "details").arg(iCPUExecutionCap));
        }

        /* Boot order: */
        if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeSystem_BootOrder)
        {
            QStringList bootOrder;
            for (ulong i = 1; i <= vboxGlobal().virtualBox().GetSystemProperties().GetMaxBootPosition(); ++i)
            {
                const KDeviceType enmDeviceType = comMachine.GetBootOrder(i);
                if (enmDeviceType == KDeviceType_Null)
                    continue;
                bootOrder << gpConverter->toString(enmDeviceType);
            }
            if (bootOrder.isEmpty())
                bootOrder << gpConverter->toString(KDeviceType_Null);
            table << UITextTableLine(QApplication::translate("UIDetails", "Boot Order", "details (system)"), bootOrder.join(", "));
        }

        /* Chipset type: */
        if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeSystem_ChipsetType)
        {
            const KChipsetType enmChipsetType = comMachine.GetChipsetType();
            if (enmChipsetType == KChipsetType_ICH9)
                table << UITextTableLine(QApplication::translate("UIDetails", "Chipset Type", "details (system)"),
                                         gpConverter->toString(enmChipsetType));
        }

        /* EFI: */
        if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeSystem_Firmware)
        {
            switch (comMachine.GetFirmwareType())
            {
                case KFirmwareType_EFI:
                case KFirmwareType_EFI32:
                case KFirmwareType_EFI64:
                case KFirmwareType_EFIDUAL:
                {
                    table << UITextTableLine(QApplication::translate("UIDetails", "EFI", "details (system)"),
                                             QApplication::translate("UIDetails", "Enabled", "details (system/EFI)"));
                    break;
                }
                default:
                {
                    // For NLS purpose:
                    QApplication::translate("UIDetails", "Disabled", "details (system/EFI)");
                    break;
                }
            }
        }

        /* Acceleration: */
        if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeSystem_Acceleration)
        {
            QStringList acceleration;
            if (vboxGlobal().virtualBox().GetHost().GetProcessorFeature(KProcessorFeature_HWVirtEx))
            {
                /* VT-x/AMD-V: */
                if (comMachine.GetHWVirtExProperty(KHWVirtExPropertyType_Enabled))
                {
                    acceleration << QApplication::translate("UIDetails", "VT-x/AMD-V", "details (system)");
                    /* Nested Paging (only when hw virt is enabled): */
                    if (comMachine.GetHWVirtExProperty(KHWVirtExPropertyType_NestedPaging))
                        acceleration << QApplication::translate("UIDetails", "Nested Paging", "details (system)");
                }
            }
            /* PAE/NX: */
            if (comMachine.GetCPUProperty(KCPUPropertyType_PAE))
                acceleration << QApplication::translate("UIDetails", "PAE/NX", "details (system)");
            /* Paravirtualization provider: */
            switch (comMachine.GetEffectiveParavirtProvider())
            {
                case KParavirtProvider_Minimal: acceleration << QApplication::translate("UIDetails", "Minimal Paravirtualization", "details (system)"); break;
                case KParavirtProvider_HyperV:  acceleration << QApplication::translate("UIDetails", "Hyper-V Paravirtualization", "details (system)"); break;
                case KParavirtProvider_KVM:     acceleration << QApplication::translate("UIDetails", "KVM Paravirtualization", "details (system)"); break;
                default: break;
            }
            if (!acceleration.isEmpty())
                table << UITextTableLine(QApplication::translate("UIDetails", "Acceleration", "details (system)"),
                                         acceleration.join(", "));
        }
    }
    else
        table << UITextTableLine(QApplication::translate("UIDetails", "Information Inaccessible", "details"),
                                 QString());

    /* Save the table as property: */
    setProperty("table", QVariant::fromValue(table));
}

UITask *UIDetailsElementSystem::createUpdateTask()
{
    return new UIDetailsUpdateTaskSystem(machine(), model()->optionsSystem());
}


void UIDetailsUpdateTaskDisplay::run()
{
    /* Acquire corresponding machine: */
    CMachine comMachine = property("machine").value<CMachine>();
    if (comMachine.isNull())
        return;

    /* Prepare table: */
    UITextTable table;

    /* Gather information: */
    if (comMachine.GetAccessible())
    {
        /* Video memory: */
        if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay_VRAM)
            table << UITextTableLine(QApplication::translate("UIDetails", "Video Memory", "details (display)"),
                                     QApplication::translate("UIDetails", "%1 MB", "details").arg(comMachine.GetVRAMSize()));

        /* Screens: */
        if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay_ScreenCount)
        {
            const int cGuestScreens = comMachine.GetMonitorCount();
            if (cGuestScreens > 1)
                table << UITextTableLine(QApplication::translate("UIDetails", "Screens", "details (display)"),
                                         QString::number(cGuestScreens));
        }

        /* Scale-factor: */
        if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay_ScaleFactor)
        {
            const QString strScaleFactor = comMachine.GetExtraData(UIExtraDataDefs::GUI_ScaleFactor);
            {
                /* Try to convert loaded data to double: */
                bool fOk = false;
                double dValue = strScaleFactor.toDouble(&fOk);
                /* Invent the default value: */
                if (!fOk || !dValue)
                    dValue = 1.0;
                /* Append information: */
                if (dValue != 1.0)
                    table << UITextTableLine(QApplication::translate("UIDetails", "Scale-factor", "details (display)"),
                                             QString::number(dValue, 'f', 2));
            }
        }

        /* Graphics Controller: */
        if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay_GraphicsController)
            table << UITextTableLine(QApplication::translate("UIDetails", "Graphics Controller", "details (display)"),
                                     gpConverter->toString(comMachine.GetGraphicsControllerType()));

        /* Acceleration: */
        if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay_Acceleration)
        {
            QStringList acceleration;
#ifdef VBOX_WITH_VIDEOHWACCEL
            /* 2D acceleration: */
            if (comMachine.GetAccelerate2DVideoEnabled())
                acceleration << QApplication::translate("UIDetails", "2D Video", "details (display)");
#endif
            /* 3D acceleration: */
            if (comMachine.GetAccelerate3DEnabled())
                acceleration << QApplication::translate("UIDetails", "3D", "details (display)");
            if (!acceleration.isEmpty())
                table << UITextTableLine(QApplication::translate("UIDetails", "Acceleration", "details (display)"),
                                         acceleration.join(", "));
        }

        /* Remote desktop server: */
        if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay_VRDE)
        {
            const CVRDEServer comServer = comMachine.GetVRDEServer();
            if (!comServer.isNull())
            {
                if (comServer.GetEnabled())
                    table << UITextTableLine(QApplication::translate("UIDetails", "Remote Desktop Server Port", "details (display/vrde)"),
                                             comServer.GetVRDEProperty("TCP/Ports"));
                else
                    table << UITextTableLine(QApplication::translate("UIDetails", "Remote Desktop Server", "details (display/vrde)"),
                                             QApplication::translate("UIDetails", "Disabled", "details (display/vrde/VRDE server)"));
            }
        }

        /* Recording: */
        if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeDisplay_Recording)
        {
            CRecordingSettings comRecordingSettings = comMachine.GetRecordingSettings();
            if (comRecordingSettings.GetEnabled())
            {
                /* For now all screens have the same config: */
                const CRecordingScreenSettings comRecordingScreen0Settings = comRecordingSettings.GetScreenSettings(0);

                /** @todo r=andy Refine these texts (wrt audio and/or video). */
                table << UITextTableLine(QApplication::translate("UIDetails", "Recording File", "details (display/recording)"),
                                         comRecordingScreen0Settings.GetFilename());
                table << UITextTableLine(QApplication::translate("UIDetails", "Recording Attributes", "details (display/recording)"),
                                         QApplication::translate("UIDetails", "Frame Size: %1x%2, Frame Rate: %3fps, Bit Rate: %4kbps")
                                             .arg(comRecordingScreen0Settings.GetVideoWidth()).arg(comRecordingScreen0Settings.GetVideoHeight())
                                             .arg(comRecordingScreen0Settings.GetVideoFPS()).arg(comRecordingScreen0Settings.GetVideoRate()));
            }
            else
            {
                table << UITextTableLine(QApplication::translate("UIDetails", "Recording", "details (display/recording)"),
                                         QApplication::translate("UIDetails", "Disabled", "details (display/recording)"));
            }
        }
    }
    else
        table << UITextTableLine(QApplication::translate("UIDetails", "Information Inaccessible", "details"), QString());

    /* Save the table as property: */
    setProperty("table", QVariant::fromValue(table));
}

UITask *UIDetailsElementDisplay::createUpdateTask()
{
    return new UIDetailsUpdateTaskDisplay(machine(), model()->optionsDisplay());
}


void UIDetailsUpdateTaskStorage::run()
{
    /* Acquire corresponding machine: */
    CMachine comMachine = property("machine").value<CMachine>();
    if (comMachine.isNull())
        return;

    /* Prepare table: */
    UITextTable table;

    /* Gather information: */
    if (comMachine.GetAccessible())
    {
        /* Iterate over all the machine controllers: */
        foreach (const CStorageController &comController, comMachine.GetStorageControllers())
        {
            /* Add controller information: */
            const QString strControllerName = QApplication::translate("UIMachineSettingsStorage", "Controller: %1");
            table << UITextTableLine(strControllerName.arg(comController.GetName()), QString());
            /* Populate map (its sorted!): */
            QMap<StorageSlot, QString> attachmentsMap;
            foreach (const CMediumAttachment &attachment, comMachine.GetMediumAttachmentsOfController(comController.GetName()))
            {
                /* Acquire device type first of all: */
                const KDeviceType enmDeviceType = attachment.GetType();

                /* Ignore restricted device types: */
                if (   (   !(m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeStorage_HardDisks)
                        && enmDeviceType == KDeviceType_HardDisk)
                    || (   !(m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeStorage_OpticalDevices)
                        && enmDeviceType == KDeviceType_DVD)
                    || (   !(m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeStorage_FloppyDevices)
                        && enmDeviceType == KDeviceType_Floppy))
                    continue;

                /* Prepare current storage slot: */
                const StorageSlot attachmentSlot(comController.GetBus(), attachment.GetPort(), attachment.GetDevice());
                AssertMsg(comController.isOk(),
                          ("Unable to acquire controller data: %s\n",
                           UIErrorString::formatRC(comController.lastRC()).toUtf8().constData()));
                if (!comController.isOk())
                    continue;

                /* Prepare attachment information: */
                QString strAttachmentInfo = vboxGlobal().details(attachment.GetMedium(), false, false);
                /* That hack makes sure 'Inaccessible' word is always bold: */
                { // hack
                    const QString strInaccessibleString(VBoxGlobal::tr("Inaccessible", "medium"));
                    const QString strBoldInaccessibleString(QString("<b>%1</b>").arg(strInaccessibleString));
                    strAttachmentInfo.replace(strInaccessibleString, strBoldInaccessibleString);
                } // hack

                /* Append 'device slot name' with 'device type name' for optical devices only: */
                QString strDeviceType = enmDeviceType == KDeviceType_DVD
                                      ? QApplication::translate("UIDetails", "[Optical Drive]", "details (storage)")
                                      : QString();
                if (!strDeviceType.isNull())
                    strDeviceType.append(' ');

                /* Insert that attachment information into the map: */
                if (!strAttachmentInfo.isNull())
                {
                    /* Configure hovering anchors: */
                    const QString strAnchorType = enmDeviceType == KDeviceType_DVD || enmDeviceType == KDeviceType_Floppy ? QString("mount") :
                                                  enmDeviceType == KDeviceType_HardDisk ? QString("attach") : QString();
                    const CMedium medium = attachment.GetMedium();
                    const QString strMediumLocation = medium.isNull() ? QString() : medium.GetLocation();
                    attachmentsMap.insert(attachmentSlot,
                                          QString("<a href=#%1,%2,%3,%4>%5</a>")
                                                  .arg(strAnchorType,
                                                       comController.GetName(),
                                                       gpConverter->toString(attachmentSlot),
                                                       strMediumLocation,
                                                       strDeviceType + strAttachmentInfo));
                }
            }

            /* Iterate over the sorted map: */
            const QList<StorageSlot> storageSlots = attachmentsMap.keys();
            const QList<QString> storageInfo = attachmentsMap.values();
            for (int i = 0; i < storageSlots.size(); ++i)
                table << UITextTableLine(QString("  ") + gpConverter->toString(storageSlots[i]), storageInfo[i]);
        }
        if (table.isEmpty())
            table << UITextTableLine(QApplication::translate("UIDetails", "Not Attached", "details (storage)"), QString());
    }
    else
        table << UITextTableLine(QApplication::translate("UIDetails", "Information Inaccessible", "details"), QString());

    /* Save the table as property: */
    setProperty("table", QVariant::fromValue(table));
}

UITask *UIDetailsElementStorage::createUpdateTask()
{
    return new UIDetailsUpdateTaskStorage(machine(), model()->optionsStorage());
}


void UIDetailsUpdateTaskAudio::run()
{
    /* Acquire corresponding machine: */
    CMachine comMachine = property("machine").value<CMachine>();
    if (comMachine.isNull())
        return;

    /* Prepare table: */
    UITextTable table;

    /* Gather information: */
    if (comMachine.GetAccessible())
    {
        const CAudioAdapter comAudio = comMachine.GetAudioAdapter();
        if (comAudio.GetEnabled())
        {
            /* Host driver: */
            if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeAudio_Driver)
                table << UITextTableLine(QApplication::translate("UIDetails", "Host Driver", "details (audio)"),
                                         gpConverter->toString(comAudio.GetAudioDriver()));

            /* Controller: */
            if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeAudio_Controller)
                table << UITextTableLine(QApplication::translate("UIDetails", "Controller", "details (audio)"),
                                         gpConverter->toString(comAudio.GetAudioController()));

#ifdef VBOX_WITH_AUDIO_INOUT_INFO
            /* Audio I/O: */
            if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeAudio_IO)
            {
                table << UITextTableLine(QApplication::translate("UIDetails", "Audio Input", "details (audio)"),
                                         comAudio.GetEnabledIn() ?
                                         QApplication::translate("UIDetails", "Enabled", "details (audio/input)") :
                                         QApplication::translate("UIDetails", "Disabled", "details (audio/input)"));
                table << UITextTableLine(QApplication::translate("UIDetails", "Audio Output", "details (audio)"),
                                         comAudio.GetEnabledOut() ?
                                         QApplication::translate("UIDetails", "Enabled", "details (audio/output)") :
                                         QApplication::translate("UIDetails", "Disabled", "details (audio/output)"));
            }
#endif /* VBOX_WITH_AUDIO_INOUT_INFO */
        }
        else
            table << UITextTableLine(QApplication::translate("UIDetails", "Disabled", "details (audio)"),
                                     QString());
    }
    else
        table << UITextTableLine(QApplication::translate("UIDetails", "Information Inaccessible", "details"),
                                 QString());

    /* Save the table as property: */
    setProperty("table", QVariant::fromValue(table));
}

UITask *UIDetailsElementAudio::createUpdateTask()
{
    return new UIDetailsUpdateTaskAudio(machine(), model()->optionsAudio());
}


void UIDetailsUpdateTaskNetwork::run()
{
    /* Acquire corresponding machine: */
    CMachine comMachine = property("machine").value<CMachine>();
    if (comMachine.isNull())
        return;

    /* Prepare table: */
    UITextTable table;

    /* Gather information: */
    if (comMachine.GetAccessible())
    {
        /* Iterate over all the adapters: */
        const ulong uCount = vboxGlobal().virtualBox().GetSystemProperties().GetMaxNetworkAdapters(comMachine.GetChipsetType());
        for (ulong uSlot = 0; uSlot < uCount; ++uSlot)
        {
            const CNetworkAdapter comAdapter = comMachine.GetNetworkAdapter(uSlot);

            /* Skip disabled adapters: */
            if (!comAdapter.GetEnabled())
                continue;

            /* Gather adapter information: */
            const KNetworkAttachmentType enmType = comAdapter.GetAttachmentType();
            const QString strAttachmentTemplate = gpConverter->toString(comAdapter.GetAdapterType()).replace(QRegExp("\\s\\(.+\\)"), " (%1)");
            QString strAttachmentType;
            switch (enmType)
            {
                case KNetworkAttachmentType_Bridged:
                {
                    if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork_BridgetAdapter)
                        strAttachmentType = strAttachmentTemplate.arg(QApplication::translate("UIDetails", "Bridged Adapter, %1", "details (network)")
                                                                      .arg(comAdapter.GetBridgedInterface()));
                    break;
                }
                case KNetworkAttachmentType_Internal:
                {
                    if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork_InternalNetwork)
                        strAttachmentType = strAttachmentTemplate.arg(QApplication::translate("UIDetails", "Internal Network, '%1'", "details (network)")
                                                                      .arg(comAdapter.GetInternalNetwork()));
                    break;
                }
                case KNetworkAttachmentType_HostOnly:
                {
                    if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork_HostOnlyAdapter)
                        strAttachmentType = strAttachmentTemplate.arg(QApplication::translate("UIDetails", "Host-only Adapter, '%1'", "details (network)")
                                                                      .arg(comAdapter.GetHostOnlyInterface()));
                    break;
                }
                case KNetworkAttachmentType_Generic:
                {
                    if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork_GenericDriver)
                    {
                        const QString strGenericDriverProperties(summarizeGenericProperties(comAdapter));
                        strAttachmentType = strGenericDriverProperties.isNull() ?
                            strAttachmentTemplate.arg(QApplication::translate("UIDetails", "Generic Driver, '%1'", "details (network)")
                                                      .arg(comAdapter.GetGenericDriver())) :
                            strAttachmentTemplate.arg(QApplication::translate("UIDetails", "Generic Driver, '%1' { %2 }", "details (network)")
                                                      .arg(comAdapter.GetGenericDriver(), strGenericDriverProperties));
                    }
                    break;
                }
                case KNetworkAttachmentType_NATNetwork:
                {
                    if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork_NAT)
                        strAttachmentType = strAttachmentTemplate.arg(QApplication::translate("UIDetails", "NAT Network, '%1'", "details (network)")
                                                                      .arg(comAdapter.GetNATNetwork()));
                    break;
                }
                default:
                {
                    if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork_NotAttached)
                        strAttachmentType = strAttachmentTemplate.arg(gpConverter->toString(enmType));
                    break;
                }
            }
            if (!strAttachmentType.isNull())
                table << UITextTableLine(QApplication::translate("UIDetails", "Adapter %1", "details (network)").arg(comAdapter.GetSlot() + 1), strAttachmentType);
        }
        if (table.isEmpty())
            table << UITextTableLine(QApplication::translate("UIDetails", "Disabled", "details (network/adapter)"), QString());
    }
    else
        table << UITextTableLine(QApplication::translate("UIDetails", "Information Inaccessible", "details"), QString());

    /* Save the table as property: */
    setProperty("table", QVariant::fromValue(table));
}

UITask *UIDetailsElementNetwork::createUpdateTask()
{
    return new UIDetailsUpdateTaskNetwork(machine(), model()->optionsNetwork());
}

/* static */
QString UIDetailsUpdateTaskNetwork::summarizeGenericProperties(const CNetworkAdapter &adapter)
{
    QVector<QString> names;
    QVector<QString> props;
    props = adapter.GetProperties(QString(), names);
    QString strResult;
    for (int i = 0; i < names.size(); ++i)
    {
        strResult += names[i] + "=" + props[i];
        if (i < names.size() - 1)
            strResult += ", ";
    }
    return strResult;
}


void UIDetailsUpdateTaskSerial::run()
{
    /* Acquire corresponding machine: */
    CMachine comMachine = property("machine").value<CMachine>();
    if (comMachine.isNull())
        return;

    /* Prepare table: */
    UITextTable table;

    /* Gather information: */
    if (comMachine.GetAccessible())
    {
        /* Iterate over all the ports: */
        const ulong uCount = vboxGlobal().virtualBox().GetSystemProperties().GetSerialPortCount();
        for (ulong uSlot = 0; uSlot < uCount; ++uSlot)
        {
            const CSerialPort comPort = comMachine.GetSerialPort(uSlot);

            /* Skip disabled adapters: */
            if (!comPort.GetEnabled())
                continue;

            /* Gather port information: */
            const KPortMode enmMode = comPort.GetHostMode();
            const QString strModeTemplate = vboxGlobal().toCOMPortName(comPort.GetIRQ(), comPort.GetIOBase()) + ", ";
            QString strModeType;
            switch (enmMode)
            {
                case KPortMode_HostPipe:
                {
                    if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeSerial_HostPipe)
                        strModeType = strModeTemplate + QString("%1 (%2)").arg(gpConverter->toString(enmMode)).arg(QDir::toNativeSeparators(comPort.GetPath()));
                    break;
                }
                case KPortMode_HostDevice:
                {
                    if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeSerial_HostDevice)
                        strModeType = strModeTemplate + QString("%1 (%2)").arg(gpConverter->toString(enmMode)).arg(QDir::toNativeSeparators(comPort.GetPath()));
                    break;
                }
                case KPortMode_RawFile:
                {
                    if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeSerial_RawFile)
                        strModeType = strModeTemplate + QString("%1 (%2)").arg(gpConverter->toString(enmMode)).arg(QDir::toNativeSeparators(comPort.GetPath()));
                    break;
                }
                case KPortMode_TCP:
                {
                    if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeSerial_TCP)
                        strModeType = strModeTemplate + QString("%1 (%2)").arg(gpConverter->toString(enmMode)).arg(QDir::toNativeSeparators(comPort.GetPath()));
                    break;
                }
                default:
                {
                    if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeSerial_Disconnected)
                        strModeType = strModeTemplate + gpConverter->toString(enmMode);
                    break;
                }
            }
            if (!strModeType.isNull())
                table << UITextTableLine(QApplication::translate("UIDetails", "Port %1", "details (serial)").arg(comPort.GetSlot() + 1), strModeType);
        }
        if (table.isEmpty())
            table << UITextTableLine(QApplication::translate("UIDetails", "Disabled", "details (serial)"), QString());
    }
    else
        table << UITextTableLine(QApplication::translate("UIDetails", "Information Inaccessible", "details"), QString());

    /* Save the table as property: */
    setProperty("table", QVariant::fromValue(table));
}

UITask *UIDetailsElementSerial::createUpdateTask()
{
    return new UIDetailsUpdateTaskSerial(machine(), model()->optionsSerial());
}


void UIDetailsUpdateTaskUSB::run()
{
    /* Acquire corresponding machine: */
    CMachine comMachine = property("machine").value<CMachine>();
    if (comMachine.isNull())
        return;

    /* Prepare table: */
    UITextTable table;

    /* Gather information: */
    if (comMachine.GetAccessible())
    {
        /* Iterate over all the USB filters: */
        const CUSBDeviceFilters comFilterObject = comMachine.GetUSBDeviceFilters();
        if (!comFilterObject.isNull() && comMachine.GetUSBProxyAvailable())
        {
            const CUSBControllerVector controllers = comMachine.GetUSBControllers();
            if (!controllers.isEmpty())
            {
                /* USB controllers: */
                if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeUsb_Controller)
                {
                    QStringList controllersReadable;
                    foreach (const CUSBController &comController, controllers)
                        controllersReadable << gpConverter->toString(comController.GetType());
                    table << UITextTableLine(QApplication::translate("UIDetails", "USB Controller", "details (usb)"),
                                             controllersReadable.join(", "));
                }

                /* Device filters: */
                if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeUsb_DeviceFilters)
                {
                    const CUSBDeviceFilterVector filters = comFilterObject.GetDeviceFilters();
                    uint uActive = 0;
                    for (int i = 0; i < filters.size(); ++i)
                        if (filters.at(i).GetActive())
                            ++uActive;
                    table << UITextTableLine(QApplication::translate("UIDetails", "Device Filters", "details (usb)"),
                                             QApplication::translate("UIDetails", "%1 (%2 active)", "details (usb)").arg(filters.size()).arg(uActive));
                }
            }
            else
                table << UITextTableLine(QApplication::translate("UIDetails", "Disabled", "details (usb)"), QString());
        }
        else
            table << UITextTableLine(QApplication::translate("UIDetails", "USB Controller Inaccessible", "details (usb)"), QString());
    }
    else
        table << UITextTableLine(QApplication::translate("UIDetails", "Information Inaccessible", "details"), QString());

    /* Save the table as property: */
    setProperty("table", QVariant::fromValue(table));
}

UITask *UIDetailsElementUSB::createUpdateTask()
{
    return new UIDetailsUpdateTaskUSB(machine(), model()->optionsUsb());
}


void UIDetailsUpdateTaskSF::run()
{
    /* Acquire corresponding machine: */
    CMachine comMachine = property("machine").value<CMachine>();
    if (comMachine.isNull())
        return;

    /* Prepare table: */
    UITextTable table;

    /* Gather information: */
    if (comMachine.GetAccessible())
    {
        /* Summary: */
        const ulong uCount = comMachine.GetSharedFolders().size();
        if (uCount > 0)
            table << UITextTableLine(QApplication::translate("UIDetails", "Shared Folders", "details (shared folders)"), QString::number(uCount));
        else
            table << UITextTableLine(QApplication::translate("UIDetails", "None", "details (shared folders)"), QString());
    }
    else
        table << UITextTableLine(QApplication::translate("UIDetails", "Information Inaccessible", "details"), QString());

    /* Save the table as property: */
    setProperty("table", QVariant::fromValue(table));
}

UITask *UIDetailsElementSF::createUpdateTask()
{
    return new UIDetailsUpdateTaskSF(machine(), model()->optionsSharedFolders());
}


void UIDetailsUpdateTaskUI::run()
{
    /* Acquire corresponding machine: */
    CMachine comMachine = property("machine").value<CMachine>();
    if (comMachine.isNull())
        return;

    /* Prepare table: */
    UITextTable table;

    /* Gather information: */
    if (comMachine.GetAccessible())
    {
#ifndef VBOX_WS_MAC
        /* Menu-bar: */
        if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface_MenuBar)
        {
            const QString strMenubarEnabled = comMachine.GetExtraData(UIExtraDataDefs::GUI_MenuBar_Enabled);
            /* Try to convert loaded data to bool: */
            const bool fEnabled = !(   strMenubarEnabled.compare("false", Qt::CaseInsensitive) == 0
                                    || strMenubarEnabled.compare("no", Qt::CaseInsensitive) == 0
                                    || strMenubarEnabled.compare("off", Qt::CaseInsensitive) == 0
                                    || strMenubarEnabled == "0");
            /* Append information: */
            table << UITextTableLine(QApplication::translate("UIDetails", "Menu-bar", "details (user interface)"),
                                     fEnabled ? QApplication::translate("UIDetails", "Enabled", "details (user interface/menu-bar)")
                                              : QApplication::translate("UIDetails", "Disabled", "details (user interface/menu-bar)"));
        }
#endif /* !VBOX_WS_MAC */

        /* Status-bar: */
        if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface_StatusBar)
        {
            const QString strStatusbarEnabled = comMachine.GetExtraData(UIExtraDataDefs::GUI_StatusBar_Enabled);
            /* Try to convert loaded data to bool: */
            const bool fEnabled = !(   strStatusbarEnabled.compare("false", Qt::CaseInsensitive) == 0
                                    || strStatusbarEnabled.compare("no", Qt::CaseInsensitive) == 0
                                    || strStatusbarEnabled.compare("off", Qt::CaseInsensitive) == 0
                                    || strStatusbarEnabled == "0");
            /* Append information: */
            table << UITextTableLine(QApplication::translate("UIDetails", "Status-bar", "details (user interface)"),
                                     fEnabled ? QApplication::translate("UIDetails", "Enabled", "details (user interface/status-bar)")
                                              : QApplication::translate("UIDetails", "Disabled", "details (user interface/status-bar)"));
        }

#ifndef VBOX_WS_MAC
        /* Mini-toolbar: */
        if (m_fOptions & UIExtraDataMetaDefs::DetailsElementOptionTypeUserInterface_MiniToolbar)
        {
            const QString strMiniToolbarEnabled = comMachine.GetExtraData(UIExtraDataDefs::GUI_ShowMiniToolBar);
            /* Try to convert loaded data to bool: */
            const bool fEnabled = !(   strMiniToolbarEnabled.compare("false", Qt::CaseInsensitive) == 0
                                    || strMiniToolbarEnabled.compare("no", Qt::CaseInsensitive) == 0
                                    || strMiniToolbarEnabled.compare("off", Qt::CaseInsensitive) == 0
                                    || strMiniToolbarEnabled == "0");
            /* Append information: */
            if (fEnabled)
            {
                /* Get mini-toolbar position: */
                const QString strMiniToolbarPosition = comMachine.GetExtraData(UIExtraDataDefs::GUI_MiniToolBarAlignment);
                {
                    /* Try to convert loaded data to alignment: */
                    switch (gpConverter->fromInternalString<MiniToolbarAlignment>(strMiniToolbarPosition))
                    {
                        /* Append information: */
                        case MiniToolbarAlignment_Top:
                            table << UITextTableLine(QApplication::translate("UIDetails", "Mini-toolbar Position", "details (user interface)"),
                                                     QApplication::translate("UIDetails", "Top", "details (user interface/mini-toolbar position)"));
                            break;
                        /* Append information: */
                        case MiniToolbarAlignment_Bottom:
                            table << UITextTableLine(QApplication::translate("UIDetails", "Mini-toolbar Position", "details (user interface)"),
                                                     QApplication::translate("UIDetails", "Bottom", "details (user interface/mini-toolbar position)"));
                            break;
                    }
                }
            }
            /* Append information: */
            else
                table << UITextTableLine(QApplication::translate("UIDetails", "Mini-toolbar", "details (user interface)"),
                                         QApplication::translate("UIDetails", "Disabled", "details (user interface/mini-toolbar)"));
        }
#endif /* !VBOX_WS_MAC */
    }
    else
        table << UITextTableLine(QApplication::translate("UIDetails", "Information Inaccessible", "details"), QString());

    /* Save the table as property: */
    setProperty("table", QVariant::fromValue(table));
}

UITask *UIDetailsElementUI::createUpdateTask()
{
    return new UIDetailsUpdateTaskUI(machine(), model()->optionsUserInterface());
}


void UIDetailsUpdateTaskDescription::run()
{
    /* Acquire corresponding machine: */
    CMachine comMachine = property("machine").value<CMachine>();
    if (comMachine.isNull())
        return;

    /* Prepare table: */
    UITextTable table;

    /* Gather information: */
    if (comMachine.GetAccessible())
    {
        /* Summary: */
        const QString strDescription = comMachine.GetDescription();
        if (!strDescription.isEmpty())
            table << UITextTableLine(strDescription, QString());
        else
            table << UITextTableLine(QApplication::translate("UIDetails", "None", "details (description)"), QString());
    }
    else
        table << UITextTableLine(QApplication::translate("UIDetails", "Information Inaccessible", "details"), QString());

    /* Save the table as property: */
    setProperty("table", QVariant::fromValue(table));
}

UITask *UIDetailsElementDescription::createUpdateTask()
{
    return new UIDetailsUpdateTaskDescription(machine(), model()->optionsDescription());
}
