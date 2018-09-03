/* $Id$ */
/** @file
 * VBox Qt GUI - UIDetailsElement[Name] classes implementation.
 */

/*
 * Copyright (C) 2012-2017 Oracle Corporation
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
    CMachine machine = property("machine").value<CMachine>();
    if (machine.isNull())
        return;

    /* Prepare table: */
    UITextTable table;

    /* Gather information: */
    if (machine.GetAccessible())
    {
        /* Machine name: */
        table << UITextTableLine(QApplication::translate("UIDetails", "Name", "details (general)"), machine.GetName());

        /* Operating system type: */
        table << UITextTableLine(QApplication::translate("UIDetails", "Operating System", "details (general)"),
                                 vboxGlobal().vmGuestOSTypeDescription(machine.GetOSTypeId()));

        /* Location of the settings file: */
        const QString strSettingsFilePath = machine.GetSettingsFilePath();
        if (!strSettingsFilePath.isEmpty())
        {
            table << UITextTableLine(QApplication::translate("UIDetails", "Settings File Location", "details (general)"),
                                     QDir::toNativeSeparators(QFileInfo(strSettingsFilePath).absolutePath()));
        }

        /* Get groups: */
        QStringList groups = machine.GetGroups().toList();
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
    else
        table << UITextTableLine(QApplication::translate("UIDetails", "Information Inaccessible", "details"), QString());

    /* Save the table as property: */
    setProperty("table", QVariant::fromValue(table));
}


void UIDetailsUpdateTaskSystem::run()
{
    /* Acquire corresponding machine: */
    CMachine machine = property("machine").value<CMachine>();
    if (machine.isNull())
        return;

    /* Prepare table: */
    UITextTable table;

    /* Gather information: */
    if (machine.GetAccessible())
    {
        /* Base memory: */
        table << UITextTableLine(QApplication::translate("UIDetails", "Base Memory", "details (system)"),
                                 QApplication::translate("UIDetails", "%1 MB", "details").arg(machine.GetMemorySize()));

        /* CPU count: */
        int cCPU = machine.GetCPUCount();
        if (cCPU > 1)
            table << UITextTableLine(QApplication::translate("UIDetails", "Processors", "details (system)"),
                                     QString::number(cCPU));

        /* CPU execution cap: */
        int iCPUExecCap = machine.GetCPUExecutionCap();
        if (iCPUExecCap < 100)
            table << UITextTableLine(QApplication::translate("UIDetails", "Execution Cap", "details (system)"),
                                     QApplication::translate("UIDetails", "%1%", "details").arg(iCPUExecCap));

        /* Boot-order: */
        QStringList bootOrder;
        for (ulong i = 1; i <= vboxGlobal().virtualBox().GetSystemProperties().GetMaxBootPosition(); ++i)
        {
            KDeviceType device = machine.GetBootOrder(i);
            if (device == KDeviceType_Null)
                continue;
            bootOrder << gpConverter->toString(device);
        }
        if (bootOrder.isEmpty())
            bootOrder << gpConverter->toString(KDeviceType_Null);
        table << UITextTableLine(QApplication::translate("UIDetails", "Boot Order", "details (system)"), bootOrder.join(", "));

        /* Chipset type: */
        const KChipsetType enmChipsetType = machine.GetChipsetType();
        if (enmChipsetType == KChipsetType_ICH9)
            table << UITextTableLine(QApplication::translate("UIDetails", "Chipset Type", "details (system)"),
                                     gpConverter->toString(enmChipsetType));

        /* Firware type: */
        switch (machine.GetFirmwareType())
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

        /* Acceleration: */
        QStringList acceleration;
        if (vboxGlobal().virtualBox().GetHost().GetProcessorFeature(KProcessorFeature_HWVirtEx))
        {
            /* VT-x/AMD-V: */
            if (machine.GetHWVirtExProperty(KHWVirtExPropertyType_Enabled))
            {
                acceleration << QApplication::translate("UIDetails", "VT-x/AMD-V", "details (system)");
                /* Nested Paging (only when hw virt is enabled): */
                if (machine.GetHWVirtExProperty(KHWVirtExPropertyType_NestedPaging))
                    acceleration << QApplication::translate("UIDetails", "Nested Paging", "details (system)");
            }
        }
        if (machine.GetCPUProperty(KCPUPropertyType_PAE))
            acceleration << QApplication::translate("UIDetails", "PAE/NX", "details (system)");
        switch (machine.GetEffectiveParavirtProvider())
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
    else
        table << UITextTableLine(QApplication::translate("UIDetails", "Information Inaccessible", "details"),
                                 QString());

    /* Save the table as property: */
    setProperty("table", QVariant::fromValue(table));
}


void UIDetailsUpdateTaskDisplay::run()
{
    /* Acquire corresponding machine: */
    CMachine machine = property("machine").value<CMachine>();
    if (machine.isNull())
        return;

    /* Prepare table: */
    UITextTable table;

    /* Gather information: */
    if (machine.GetAccessible())
    {
        /* Video memory: */
        table << UITextTableLine(QApplication::translate("UIDetails", "Video Memory", "details (display)"),
                                 QApplication::translate("UIDetails", "%1 MB", "details").arg(machine.GetVRAMSize()));

        /* Screen count: */
        int cGuestScreens = machine.GetMonitorCount();
        if (cGuestScreens > 1)
            table << UITextTableLine(QApplication::translate("UIDetails", "Screens", "details (display)"),
                                     QString::number(cGuestScreens));

        /* Get scale-factor value: */
        const QString strScaleFactor = machine.GetExtraData(UIExtraDataDefs::GUI_ScaleFactor);
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

        QStringList acceleration;
#ifdef VBOX_WITH_VIDEOHWACCEL
        /* 2D acceleration: */
        if (machine.GetAccelerate2DVideoEnabled())
            acceleration << QApplication::translate("UIDetails", "2D Video", "details (display)");
#endif /* VBOX_WITH_VIDEOHWACCEL */
        /* 3D acceleration: */
        if (machine.GetAccelerate3DEnabled())
            acceleration << QApplication::translate("UIDetails", "3D", "details (display)");
        if (!acceleration.isEmpty())
            table << UITextTableLine(QApplication::translate("UIDetails", "Acceleration", "details (display)"),
                                     acceleration.join(", "));

        /* VRDE info: */
        CVRDEServer srv = machine.GetVRDEServer();
        if (!srv.isNull())
        {
            if (srv.GetEnabled())
                table << UITextTableLine(QApplication::translate("UIDetails", "Remote Desktop Server Port", "details (display/vrde)"),
                                         srv.GetVRDEProperty("TCP/Ports"));
            else
                table << UITextTableLine(QApplication::translate("UIDetails", "Remote Desktop Server", "details (display/vrde)"),
                                         QApplication::translate("UIDetails", "Disabled", "details (display/vrde/VRDE server)"));
        }

        /* Video Capture info: */
        if (machine.GetVideoCaptureEnabled())
        {
            table << UITextTableLine(QApplication::translate("UIDetails", "Video Capture File", "details (display/video capture)"),
                                     machine.GetVideoCaptureFile());
            table << UITextTableLine(QApplication::translate("UIDetails", "Video Capture Attributes", "details (display/video capture)"),
                                     QApplication::translate("UIDetails", "Frame Size: %1x%2, Frame Rate: %3fps, Bit Rate: %4kbps")
                                         .arg(machine.GetVideoCaptureWidth()).arg(machine.GetVideoCaptureHeight())
                                         .arg(machine.GetVideoCaptureFPS()).arg(machine.GetVideoCaptureRate()));
        }
        else
        {
            table << UITextTableLine(QApplication::translate("UIDetails", "Video Capture", "details (display/video capture)"),
                                     QApplication::translate("UIDetails", "Disabled", "details (display/video capture)"));
        }
    }
    else
        table << UITextTableLine(QApplication::translate("UIDetails", "Information Inaccessible", "details"), QString());

    /* Save the table as property: */
    setProperty("table", QVariant::fromValue(table));
}


void UIDetailsUpdateTaskStorage::run()
{
    /* Acquire corresponding machine: */
    CMachine machine = property("machine").value<CMachine>();
    if (machine.isNull())
        return;

    /* Prepare table: */
    UITextTable table;

    /* Gather information: */
    if (machine.GetAccessible())
    {
        /* Iterate over all the machine controllers: */
        bool fSomeInfo = false;
        foreach (const CStorageController &controller, machine.GetStorageControllers())
        {
            /* Add controller information: */
            QString strControllerName = QApplication::translate("UIMachineSettingsStorage", "Controller: %1");
            table << UITextTableLine(strControllerName.arg(controller.GetName()), QString());
            fSomeInfo = true;
            /* Populate map (its sorted!): */
            QMap<StorageSlot, QString> attachmentsMap;
            foreach (const CMediumAttachment &attachment, machine.GetMediumAttachmentsOfController(controller.GetName()))
            {
                /* Prepare current storage slot: */
                StorageSlot attachmentSlot(controller.GetBus(), attachment.GetPort(), attachment.GetDevice());
                AssertMsg(controller.isOk(),
                          ("Unable to acquire controller data: %s\n",
                           UIErrorString::formatRC(controller.lastRC()).toUtf8().constData()));
                if (!controller.isOk())
                    continue;
                /* Prepare attachment information: */
                QString strAttachmentInfo = vboxGlobal().details(attachment.GetMedium(), false, false);
                /* That temporary hack makes sure 'Inaccessible' word is always bold: */
                { // hack
                    QString strInaccessibleString(VBoxGlobal::tr("Inaccessible", "medium"));
                    QString strBoldInaccessibleString(QString("<b>%1</b>").arg(strInaccessibleString));
                    strAttachmentInfo.replace(strInaccessibleString, strBoldInaccessibleString);
                } // hack
                /* Append 'device slot name' with 'device type name' for optical devices only: */
                KDeviceType deviceType = attachment.GetType();
                QString strDeviceType = deviceType == KDeviceType_DVD ?
                            QApplication::translate("UIDetails", "[Optical Drive]", "details (storage)") : QString();
                if (!strDeviceType.isNull())
                    strDeviceType.append(' ');
                /* Insert that attachment information into the map: */
                if (!strAttachmentInfo.isNull())
                {
                    /* Configure hovering anchors: */
                    const QString strAnchorType = deviceType == KDeviceType_DVD || deviceType == KDeviceType_Floppy ? QString("mount") :
                                                  deviceType == KDeviceType_HardDisk ? QString("attach") : QString();
                    const CMedium medium = attachment.GetMedium();
                    const QString strMediumLocation = medium.isNull() ? QString() : medium.GetLocation();
                    attachmentsMap.insert(attachmentSlot,
                                          QString("<a href=#%1,%2,%3,%4>%5</a>")
                                                  .arg(strAnchorType,
                                                       controller.GetName(),
                                                       gpConverter->toString(attachmentSlot),
                                                       strMediumLocation,
                                                       strDeviceType + strAttachmentInfo));
                }
            }
            /* Iterate over the sorted map: */
            QList<StorageSlot> storageSlots = attachmentsMap.keys();
            QList<QString> storageInfo = attachmentsMap.values();
            for (int i = 0; i < storageSlots.size(); ++i)
                table << UITextTableLine(QString("  ") + gpConverter->toString(storageSlots[i]), storageInfo[i]);
        }
        if (!fSomeInfo)
            table << UITextTableLine(QApplication::translate("UIDetails", "Not Attached", "details (storage)"), QString());
    }
    else
        table << UITextTableLine(QApplication::translate("UIDetails", "Information Inaccessible", "details"), QString());

    /* Save the table as property: */
    setProperty("table", QVariant::fromValue(table));
}


void UIDetailsUpdateTaskAudio::run()
{
    /* Acquire corresponding machine: */
    CMachine machine = property("machine").value<CMachine>();
    if (machine.isNull())
        return;

    /* Prepare table: */
    UITextTable table;

    /* Gather information: */
    if (machine.GetAccessible())
    {
        const CAudioAdapter &audio = machine.GetAudioAdapter();
        if (audio.GetEnabled())
        {
            /* Driver: */
            table << UITextTableLine(QApplication::translate("UIDetails", "Host Driver", "details (audio)"),
                                     gpConverter->toString(audio.GetAudioDriver()));

            /* Controller: */
            table << UITextTableLine(QApplication::translate("UIDetails", "Controller", "details (audio)"),
                                     gpConverter->toString(audio.GetAudioController()));

#ifdef VBOX_WITH_AUDIO_INOUT_INFO
            /* Output: */
            table << UITextTableLine(QApplication::translate("UIDetails", "Audio Output", "details (audio)"),
                                     audio.GetEnabledOut() ?
                                     QApplication::translate("UIDetails", "Enabled", "details (audio/output)") :
                                     QApplication::translate("UIDetails", "Disabled", "details (audio/output)"));

            /* Input: */
            table << UITextTableLine(QApplication::translate("UIDetails", "Audio Input", "details (audio)"),
                                     audio.GetEnabledIn() ?
                                     QApplication::translate("UIDetails", "Enabled", "details (audio/input)") :
                                     QApplication::translate("UIDetails", "Disabled", "details (audio/input)"));
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


void UIDetailsUpdateTaskNetwork::run()
{
    /* Acquire corresponding machine: */
    CMachine machine = property("machine").value<CMachine>();
    if (machine.isNull())
        return;

    /* Prepare table: */
    UITextTable table;

    /* Gather information: */
    if (machine.GetAccessible())
    {
        /* Iterate over all the adapters: */
        bool fSomeInfo = false;
        ulong uCount = vboxGlobal().virtualBox().GetSystemProperties().GetMaxNetworkAdapters(machine.GetChipsetType());
        for (ulong uSlot = 0; uSlot < uCount; ++uSlot)
        {
            const CNetworkAdapter &adapter = machine.GetNetworkAdapter(uSlot);
            if (adapter.GetEnabled())
            {
                KNetworkAttachmentType type = adapter.GetAttachmentType();
                QString strAttachmentType = gpConverter->toString(adapter.GetAdapterType())
                                            .replace(QRegExp("\\s\\(.+\\)"), " (%1)");
                switch (type)
                {
                    case KNetworkAttachmentType_Bridged:
                    {
                        strAttachmentType = strAttachmentType.arg(QApplication::translate("UIDetails", "Bridged Adapter, %1", "details (network)")
                                                                  .arg(adapter.GetBridgedInterface()));
                        break;
                    }
                    case KNetworkAttachmentType_Internal:
                    {
                        strAttachmentType = strAttachmentType.arg(QApplication::translate("UIDetails", "Internal Network, '%1'", "details (network)")
                                                                  .arg(adapter.GetInternalNetwork()));
                        break;
                    }
                    case KNetworkAttachmentType_HostOnly:
                    {
                        strAttachmentType = strAttachmentType.arg(QApplication::translate("UIDetails", "Host-only Adapter, '%1'", "details (network)")
                                                                  .arg(adapter.GetHostOnlyInterface()));
                        break;
                    }
                    case KNetworkAttachmentType_Generic:
                    {
                        QString strGenericDriverProperties(summarizeGenericProperties(adapter));
                        strAttachmentType = strGenericDriverProperties.isNull() ?
                                  strAttachmentType.arg(QApplication::translate("UIDetails", "Generic Driver, '%1'", "details (network)").arg(adapter.GetGenericDriver())) :
                                  strAttachmentType.arg(QApplication::translate("UIDetails", "Generic Driver, '%1' { %2 }", "details (network)")
                                                        .arg(adapter.GetGenericDriver(), strGenericDriverProperties));
                        break;
                    }
                    case KNetworkAttachmentType_NATNetwork:
                    {
                        strAttachmentType = strAttachmentType.arg(QApplication::translate("UIDetails", "NAT Network, '%1'", "details (network)")
                                                                  .arg(adapter.GetNATNetwork()));
                        break;
                    }
                    default:
                    {
                        strAttachmentType = strAttachmentType.arg(gpConverter->toString(type));
                        break;
                    }
                }
                table << UITextTableLine(QApplication::translate("UIDetails", "Adapter %1", "details (network)").arg(adapter.GetSlot() + 1), strAttachmentType);
                fSomeInfo = true;
            }
        }
        if (!fSomeInfo)
            table << UITextTableLine(QApplication::translate("UIDetails", "Disabled", "details (network/adapter)"), QString());
    }
    else
        table << UITextTableLine(QApplication::translate("UIDetails", "Information Inaccessible", "details"), QString());

    /* Save the table as property: */
    setProperty("table", QVariant::fromValue(table));
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
    CMachine machine = property("machine").value<CMachine>();
    if (machine.isNull())
        return;

    /* Prepare table: */
    UITextTable table;

    /* Gather information: */
    if (machine.GetAccessible())
    {
        /* Iterate over all the ports: */
        bool fSomeInfo = false;
        ulong uCount = vboxGlobal().virtualBox().GetSystemProperties().GetSerialPortCount();
        for (ulong uSlot = 0; uSlot < uCount; ++uSlot)
        {
            const CSerialPort &port = machine.GetSerialPort(uSlot);
            if (port.GetEnabled())
            {
                KPortMode mode = port.GetHostMode();
                QString data = vboxGlobal().toCOMPortName(port.GetIRQ(), port.GetIOBase()) + ", ";
                if (mode == KPortMode_HostPipe || mode == KPortMode_HostDevice ||
                    mode == KPortMode_RawFile || mode == KPortMode_TCP)
                    data += QString("%1 (%2)").arg(gpConverter->toString(mode)).arg(QDir::toNativeSeparators(port.GetPath()));
                else
                    data += gpConverter->toString(mode);
                table << UITextTableLine(QApplication::translate("UIDetails", "Port %1", "details (serial)").arg(port.GetSlot() + 1), data);
                fSomeInfo = true;
            }
        }
        if (!fSomeInfo)
            table << UITextTableLine(QApplication::translate("UIDetails", "Disabled", "details (serial)"), QString());
    }
    else
        table << UITextTableLine(QApplication::translate("UIDetails", "Information Inaccessible", "details"), QString());

    /* Save the table as property: */
    setProperty("table", QVariant::fromValue(table));
}


void UIDetailsUpdateTaskUSB::run()
{
    /* Acquire corresponding machine: */
    CMachine machine = property("machine").value<CMachine>();
    if (machine.isNull())
        return;

    /* Prepare table: */
    UITextTable table;

    /* Gather information: */
    if (machine.GetAccessible())
    {
        /* Iterate over all the USB filters: */
        const CUSBDeviceFilters &filters = machine.GetUSBDeviceFilters();
        if (!filters.isNull() && machine.GetUSBProxyAvailable())
        {
            const CUSBDeviceFilters flts = machine.GetUSBDeviceFilters();
            const CUSBControllerVector controllers = machine.GetUSBControllers();
            if (!flts.isNull() && !controllers.isEmpty())
            {
                /* USB Controllers info: */
                QStringList controllerList;
                foreach (const CUSBController &controller, controllers)
                    controllerList << gpConverter->toString(controller.GetType());
                table << UITextTableLine(QApplication::translate("UIDetails", "USB Controller", "details (usb)"),
                                          controllerList.join(", "));
                /* USB Device Filters info: */
                const CUSBDeviceFilterVector &coll = flts.GetDeviceFilters();
                uint uActive = 0;
                for (int i = 0; i < coll.size(); ++i)
                    if (coll[i].GetActive())
                        ++uActive;
                table << UITextTableLine(QApplication::translate("UIDetails", "Device Filters", "details (usb)"),
                                         QApplication::translate("UIDetails", "%1 (%2 active)", "details (usb)").arg(coll.size()).arg(uActive));
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


void UIDetailsUpdateTaskSF::run()
{
    /* Acquire corresponding machine: */
    CMachine machine = property("machine").value<CMachine>();
    if (machine.isNull())
        return;

    /* Prepare table: */
    UITextTable table;

    /* Gather information: */
    if (machine.GetAccessible())
    {
        /* Iterate over all the shared folders: */
        ulong uCount = machine.GetSharedFolders().size();
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


void UIDetailsUpdateTaskUI::run()
{
    /* Acquire corresponding machine: */
    CMachine machine = property("machine").value<CMachine>();
    if (machine.isNull())
        return;

    /* Prepare table: */
    UITextTable table;

    /* Gather information: */
    if (machine.GetAccessible())
    {
#ifndef VBOX_WS_MAC
        /* Get menu-bar availability status: */
        const QString strMenubarEnabled = machine.GetExtraData(UIExtraDataDefs::GUI_MenuBar_Enabled);
        {
            /* Try to convert loaded data to bool: */
            const bool fEnabled = !(strMenubarEnabled.compare("false", Qt::CaseInsensitive) == 0 ||
                                    strMenubarEnabled.compare("no", Qt::CaseInsensitive) == 0 ||
                                    strMenubarEnabled.compare("off", Qt::CaseInsensitive) == 0 ||
                                    strMenubarEnabled == "0");
            /* Append information: */
            table << UITextTableLine(QApplication::translate("UIDetails", "Menu-bar", "details (user interface)"),
                                     fEnabled ? QApplication::translate("UIDetails", "Enabled", "details (user interface/menu-bar)") :
                                                QApplication::translate("UIDetails", "Disabled", "details (user interface/menu-bar)"));
        }
#endif /* !VBOX_WS_MAC */

        /* Get status-bar availability status: */
        const QString strStatusbarEnabled = machine.GetExtraData(UIExtraDataDefs::GUI_StatusBar_Enabled);
        {
            /* Try to convert loaded data to bool: */
            const bool fEnabled = !(strStatusbarEnabled.compare("false", Qt::CaseInsensitive) == 0 ||
                                    strStatusbarEnabled.compare("no", Qt::CaseInsensitive) == 0 ||
                                    strStatusbarEnabled.compare("off", Qt::CaseInsensitive) == 0 ||
                                    strStatusbarEnabled == "0");
            /* Append information: */
            table << UITextTableLine(QApplication::translate("UIDetails", "Status-bar", "details (user interface)"),
                                     fEnabled ? QApplication::translate("UIDetails", "Enabled", "details (user interface/status-bar)") :
                                                QApplication::translate("UIDetails", "Disabled", "details (user interface/status-bar)"));
        }

#ifndef VBOX_WS_MAC
        /* Get mini-toolbar availability status: */
        const QString strMiniToolbarEnabled = machine.GetExtraData(UIExtraDataDefs::GUI_ShowMiniToolBar);
        {
            /* Try to convert loaded data to bool: */
            const bool fEnabled = !(strMiniToolbarEnabled.compare("false", Qt::CaseInsensitive) == 0 ||
                                    strMiniToolbarEnabled.compare("no", Qt::CaseInsensitive) == 0 ||
                                    strMiniToolbarEnabled.compare("off", Qt::CaseInsensitive) == 0 ||
                                    strMiniToolbarEnabled == "0");
            /* Append information: */
            if (fEnabled)
            {
                /* Get mini-toolbar position: */
                const QString &strMiniToolbarPosition = machine.GetExtraData(UIExtraDataDefs::GUI_MiniToolBarAlignment);
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


void UIDetailsUpdateTaskDescription::run()
{
    /* Acquire corresponding machine: */
    CMachine machine = property("machine").value<CMachine>();
    if (machine.isNull())
        return;

    /* Prepare table: */
    UITextTable table;

    /* Gather information: */
    if (machine.GetAccessible())
    {
        /* Get description: */
        const QString &strDesc = machine.GetDescription();
        if (!strDesc.isEmpty())
            table << UITextTableLine(strDesc, QString());
        else
            table << UITextTableLine(QApplication::translate("UIDetails", "None", "details (description)"), QString());
    }
    else
        table << UITextTableLine(QApplication::translate("UIDetails", "Information Inaccessible", "details"), QString());

    /* Save the table as property: */
    setProperty("table", QVariant::fromValue(table));
}
