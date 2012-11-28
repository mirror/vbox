/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGDetailsDetails class implementation
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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
#include <QGraphicsLinearLayout>
#include <QTimer>
#include <QDir>

/* GUI includes: */
#include "UIGDetailsElements.h"
#include "UIGDetailsModel.h"
#include "UIGMachinePreview.h"
#include "UIGraphicsRotatorButton.h"
#include "VBoxGlobal.h"
#include "UIIconPool.h"
#include "UIConverter.h"

/* COM includes: */
#include "CSystemProperties.h"
#include "CVRDEServer.h"
#include "CStorageController.h"
#include "CMediumAttachment.h"
#include "CAudioAdapter.h"
#include "CNetworkAdapter.h"
#include "CSerialPort.h"
#include "CParallelPort.h"
#include "CUSBController.h"
#include "CUSBDeviceFilter.h"
#include "CSharedFolder.h"

/* Constructor: */
UIGDetailsUpdateThread::UIGDetailsUpdateThread(const CMachine &machine)
    : m_machine(machine)
{
    qRegisterMetaType<UITextTable>();
}

UIGDetailsElementInterface::UIGDetailsElementInterface(UIGDetailsSet *pParent, DetailsElementType elementType, bool fOpened)
    : UIGDetailsElement(pParent, elementType, fOpened)
    , m_pThread(0)
{
}

UIGDetailsElementInterface::~UIGDetailsElementInterface()
{
    cleanupThread();
}

void UIGDetailsElementInterface::updateAppearance()
{
    if (!m_pThread)
    {
        m_pThread = createUpdateThread();
        connect(m_pThread, SIGNAL(sigComplete(const UITextTable&)),
                this, SLOT(sltUpdateAppearanceFinished(const UITextTable&)));
        m_pThread->start();
    }
}

void UIGDetailsElementInterface::sltUpdateAppearanceFinished(const UITextTable &newText)
{
    if (text() != newText)
        setText(newText);
    cleanupThread();
    emit sigElementUpdateDone();
}

void UIGDetailsElementInterface::cleanupThread()
{
    if (m_pThread)
    {
        m_pThread->wait();
        delete m_pThread;
        m_pThread = 0;
    }
}


UIGDetailsUpdateThreadGeneral::UIGDetailsUpdateThreadGeneral(const CMachine &machine)
    : UIGDetailsUpdateThread(machine)
{
}

void UIGDetailsUpdateThreadGeneral::run()
{
    COMBase::InitializeCOM(false);

    if (!machine().isNull())
    {
        /* Prepare table: */
        UITextTable m_text;

        /* Gather information: */
        if (machine().GetAccessible())
        {
            /* Machine name: */
            m_text << UITextTableLine(QApplication::translate("UIGDetails", "Name", "details (general)"), machine().GetName());

            /* Operating system type: */
            m_text << UITextTableLine(QApplication::translate("UIGDetails", "Operating System", "details (general)"),
                                       vboxGlobal().vmGuestOSTypeDescription(machine().GetOSTypeId()));

            /* Get groups: */
            QStringList groups = machine().GetGroups().toList();
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
                m_text << UITextTableLine(QApplication::translate("UIGDetails", "Groups", "details (general)"), groups.join(", "));
            }
        }
        else
            m_text << UITextTableLine(QApplication::translate("UIGDetails", "Information Inaccessible", "details"), QString());

        /* Send information into GUI thread: */
        emit sigComplete(m_text);
    }

    COMBase::CleanupCOM();
}

UIGDetailsElementGeneral::UIGDetailsElementGeneral(UIGDetailsSet *pParent, bool fOpened)
    : UIGDetailsElementInterface(pParent, DetailsElementType_General, fOpened)
{
    /* Name/icon: */
    setName(gpConverter->toString(DetailsElementType_General));
    setIcon(UIIconPool::iconSet(":/machine_16px.png"));
}

UIGDetailsUpdateThread* UIGDetailsElementGeneral::createUpdateThread()
{
    return new UIGDetailsUpdateThreadGeneral(machine());
}


UIGDetailsElementPreview::UIGDetailsElementPreview(UIGDetailsSet *pParent, bool fOpened)
    : UIGDetailsElement(pParent, DetailsElementType_Preview, fOpened)
{
    /* Name/icon: */
    setName(gpConverter->toString(DetailsElementType_Preview));
    setIcon(UIIconPool::iconSet(":/machine_16px.png"));

    /* Prepare variables: */
    int iMargin = data(ElementData_Margin).toInt();
    int iHeaderHeight = data(ElementData_HeaderSize).toSize().height();
    /* Prepare layout: */
    QGraphicsLinearLayout *pLayout = new QGraphicsLinearLayout;
    pLayout->setContentsMargins(iMargin, 2 * iMargin + iHeaderHeight, iMargin, iMargin);
    setLayout(pLayout);

    /* Create preview: */
    m_pPreview = new UIGMachinePreview(this);
    pLayout->addItem(m_pPreview);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

int UIGDetailsElementPreview::minimumWidthHint() const
{
    /* Prepare variables: */
    int iMargin = data(ElementData_Margin).toInt();
    int iHeaderWidth = data(ElementData_HeaderSize).toSize().width();

    /* Calculating proposed width: */
    int iProposedWidth = 0;

    /* Maximum between header width and preview width: */
    iProposedWidth += qMax(iHeaderWidth, m_pPreview->minimumSizeHint().toSize().width());

    /* Two margins: */
    iProposedWidth += 2 * iMargin;

    /* Return result: */
    return iProposedWidth;
}

int UIGDetailsElementPreview::minimumHeightHint(bool fClosed) const
{
    /* Prepare variables: */
    int iMargin = data(ElementData_Margin).toInt();
    int iHeaderHeight = data(ElementData_HeaderSize).toSize().height();

    /* Calculating proposed height: */
    int iProposedHeight = 0;

    /* Two margins: */
    iProposedHeight += 2 * iMargin;

    /* Header height: */
    iProposedHeight += iHeaderHeight;

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

void UIGDetailsElementPreview::updateAppearance()
{
    m_pPreview->setMachine(machine());
    emit sigElementUpdateDone();
}

void UIGDetailsElementPreview::updateLayout()
{
    /* Call to base-class: */
    UIGDetailsElement::updateLayout();

    /* Show/hide preview: */
    if (closed() && m_pPreview->isVisible())
        m_pPreview->hide();
    if (opened() && !m_pPreview->isVisible() && !isAnimationRunning())
        m_pPreview->show();
}


UIGDetailsUpdateThreadSystem::UIGDetailsUpdateThreadSystem(const CMachine &machine)
    : UIGDetailsUpdateThread(machine)
{
}

void UIGDetailsUpdateThreadSystem::run()
{
    COMBase::InitializeCOM(false);

    if (!machine().isNull())
    {
        /* Prepare table: */
        UITextTable m_text;

        /* Gather information: */
        if (machine().GetAccessible())
        {
            /* Base memory: */
            m_text << UITextTableLine(QApplication::translate("UIGDetails", "Base Memory", "details (system)"),
                                      QApplication::translate("UIGDetails", "%1 MB", "details").arg(machine().GetMemorySize()));

            /* CPU count: */
            int cCPU = machine().GetCPUCount();
            if (cCPU > 1)
                m_text << UITextTableLine(QApplication::translate("UIGDetails", "Processors", "details (system)"),
                                          QString::number(cCPU));

            /* CPU execution cap: */
            int iCPUExecCap = machine().GetCPUExecutionCap();
            if (iCPUExecCap < 100)
                m_text << UITextTableLine(QApplication::translate("UIGDetails", "Execution Cap", "details (system)"),
                                          QApplication::translate("UIGDetails", "%1%", "details").arg(iCPUExecCap));

            /* Boot-order: */
            QStringList bootOrder;
            for (ulong i = 1; i <= vboxGlobal().virtualBox().GetSystemProperties().GetMaxBootPosition(); ++i)
            {
                KDeviceType device = machine().GetBootOrder(i);
                if (device == KDeviceType_Null)
                    continue;
                bootOrder << gpConverter->toString(device);
            }
            if (bootOrder.isEmpty())
                bootOrder << gpConverter->toString(KDeviceType_Null);
            m_text << UITextTableLine(QApplication::translate("UIGDetails", "Boot Order", "details (system)"), bootOrder.join(", "));

            /* Acceleration: */
            QStringList acceleration;
            if (vboxGlobal().virtualBox().GetHost().GetProcessorFeature(KProcessorFeature_HWVirtEx))
            {
                /* VT-x/AMD-V: */
                if (machine().GetHWVirtExProperty(KHWVirtExPropertyType_Enabled))
                {
                    acceleration << QApplication::translate("UIGDetails", "VT-x/AMD-V", "details (system)");
                    /* Nested Paging (only when hw virt is enabled): */
                    if (machine().GetHWVirtExProperty(KHWVirtExPropertyType_NestedPaging))
                        acceleration << QApplication::translate("UIGDetails", "Nested Paging", "details (system)");
                }
            }
            if (machine().GetCPUProperty(KCPUPropertyType_PAE))
                acceleration << QApplication::translate("UIGDetails", "PAE/NX", "details (system)");
            if (!acceleration.isEmpty())
                m_text << UITextTableLine(QApplication::translate("UIGDetails", "Acceleration", "details (system)"),
                                          acceleration.join(", "));
        }
        else
            m_text << UITextTableLine(QApplication::translate("UIGDetails", "Information Inaccessible", "details"),
                                      QString());

        /* Send information into GUI thread: */
        emit sigComplete(m_text);
    }

    COMBase::CleanupCOM();
}

UIGDetailsElementSystem::UIGDetailsElementSystem(UIGDetailsSet *pParent, bool fOpened)
    : UIGDetailsElementInterface(pParent, DetailsElementType_System, fOpened)
{
    /* Name/icon: */
    setName(gpConverter->toString(DetailsElementType_System));
    setIcon(UIIconPool::iconSet(":/chipset_16px.png"));
}

UIGDetailsUpdateThread* UIGDetailsElementSystem::createUpdateThread()
{
    return new UIGDetailsUpdateThreadSystem(machine());
}


UIGDetailsUpdateThreadDisplay::UIGDetailsUpdateThreadDisplay(const CMachine &machine)
    : UIGDetailsUpdateThread(machine)
{
}

void UIGDetailsUpdateThreadDisplay::run()
{
    COMBase::InitializeCOM(false);

    if (!machine().isNull())
    {
        /* Prepare table: */
        UITextTable m_text;

        /* Gather information: */
        if (machine().GetAccessible())
        {
            /* Video memory: */
            m_text << UITextTableLine(QApplication::translate("UIGDetails", "Video Memory", "details (display)"),
                                      QApplication::translate("UIGDetails", "%1 MB", "details").arg(machine().GetVRAMSize()));

            /* Screen count: */
            int cGuestScreens = machine().GetMonitorCount();
            if (cGuestScreens > 1)
                m_text << UITextTableLine(QApplication::translate("UIGDetails", "Screens", "details (display)"),
                                          QString::number(cGuestScreens));

            QStringList acceleration;
#ifdef VBOX_WITH_VIDEOHWACCEL
            /* 2D acceleration: */
            if (machine().GetAccelerate2DVideoEnabled())
                acceleration << QApplication::translate("UIGDetails", "2D Video", "details (display)");
#endif /* VBOX_WITH_VIDEOHWACCEL */
            /* 3D acceleration: */
            if (machine().GetAccelerate3DEnabled())
                acceleration << QApplication::translate("UIGDetails", "3D", "details (display)");
            if (!acceleration.isEmpty())
                m_text << UITextTableLine(QApplication::translate("UIGDetails", "Acceleration", "details (display)"),
                                          acceleration.join(", "));

            /* VRDE info: */
            CVRDEServer srv = machine().GetVRDEServer();
            if (!srv.isNull())
            {
                if (srv.GetEnabled())
                    m_text << UITextTableLine(QApplication::translate("UIGDetails", "Remote Desktop Server Port", "details (display/vrde)"),
                                              srv.GetVRDEProperty("TCP/Ports"));
                else
                    m_text << UITextTableLine(QApplication::translate("UIGDetails", "Remote Desktop Server", "details (display/vrde)"),
                                              QApplication::translate("UIGDetails", "Disabled", "details (display/vrde/VRDE server)"));
            }
        }
        else
            m_text << UITextTableLine(QApplication::translate("UIGDetails", "Information Inaccessible", "details"), QString());

        /* Send information into GUI thread: */
        emit sigComplete(m_text);
    }

    COMBase::CleanupCOM();
}

UIGDetailsElementDisplay::UIGDetailsElementDisplay(UIGDetailsSet *pParent, bool fOpened)
    : UIGDetailsElementInterface(pParent, DetailsElementType_Display, fOpened)
{
    /* Name/icon: */
    setName(gpConverter->toString(DetailsElementType_Display));
    setIcon(UIIconPool::iconSet(":/vrdp_16px.png"));
}

UIGDetailsUpdateThread* UIGDetailsElementDisplay::createUpdateThread()
{
    return new UIGDetailsUpdateThreadDisplay(machine());
}


UIGDetailsUpdateThreadStorage::UIGDetailsUpdateThreadStorage(const CMachine &machine)
    : UIGDetailsUpdateThread(machine)
{
}

void UIGDetailsUpdateThreadStorage::run()
{
    COMBase::InitializeCOM(false);

    if (!machine().isNull())
    {
        /* Prepare table: */
        UITextTable m_text;

        /* Gather information: */
        if (machine().GetAccessible())
        {
            /* Iterate over all the machine controllers: */
            bool fSomeInfo = false;
            foreach (const CStorageController &controller, machine().GetStorageControllers())
            {
                /* Add controller information: */
                QString strControllerName = QApplication::translate("UIMachineSettingsStorage", "Controller: %1");
                m_text << UITextTableLine(strControllerName.arg(controller.GetName()), QString());
                fSomeInfo = true;
                /* Populate map (its sorted!): */
                QMap<StorageSlot, QString> attachmentsMap;
                foreach (const CMediumAttachment &attachment, machine().GetMediumAttachmentsOfController(controller.GetName()))
                {
                    /* Prepare current storage slot: */
                    StorageSlot attachmentSlot(controller.GetBus(), attachment.GetPort(), attachment.GetDevice());
                    /* Prepare attachment information: */
                    QString strAttachmentInfo = vboxGlobal().details(attachment.GetMedium(), false, false);
                    /* That temporary hack makes sure 'Inaccessible' word is always bold: */
                    { // hack
                        QString strInaccessibleString(VBoxGlobal::tr("Inaccessible", "medium"));
                        QString strBoldInaccessibleString(QString("<b>%1</b>").arg(strInaccessibleString));
                        strAttachmentInfo.replace(strInaccessibleString, strBoldInaccessibleString);
                    } // hack
                    /* Append 'device slot name' with 'device type name' for CD/DVD devices only: */
                    QString strDeviceType = attachment.GetType() == KDeviceType_DVD ?
                                QApplication::translate("UIGDetails", "[CD/DVD]", "details (storage)") : QString();
                    if (!strDeviceType.isNull())
                        strDeviceType.append(' ');
                    /* Insert that attachment information into the map: */
                    if (!strAttachmentInfo.isNull())
                        attachmentsMap.insert(attachmentSlot, strDeviceType + strAttachmentInfo);
                }
                /* Iterate over the sorted map: */
                QList<StorageSlot> storageSlots = attachmentsMap.keys();
                QList<QString> storageInfo = attachmentsMap.values();
                for (int i = 0; i < storageSlots.size(); ++i)
                    m_text << UITextTableLine(QString("  ") + gpConverter->toString(storageSlots[i]), storageInfo[i]);
            }
            if (!fSomeInfo)
                m_text << UITextTableLine(QApplication::translate("UIGDetails", "Not Attached", "details (storage)"), QString());
        }
        else
            m_text << UITextTableLine(QApplication::translate("UIGDetails", "Information Inaccessible", "details"), QString());

        /* Send information into GUI thread: */
        emit sigComplete(m_text);
    }

    COMBase::CleanupCOM();
}

UIGDetailsElementStorage::UIGDetailsElementStorage(UIGDetailsSet *pParent, bool fOpened)
    : UIGDetailsElementInterface(pParent, DetailsElementType_Storage, fOpened)
{
    /* Name/icon: */
    setName(gpConverter->toString(DetailsElementType_Storage));
    setIcon(UIIconPool::iconSet(":/attachment_16px.png"));
}

UIGDetailsUpdateThread* UIGDetailsElementStorage::createUpdateThread()
{
    return new UIGDetailsUpdateThreadStorage(machine());
}


UIGDetailsUpdateThreadAudio::UIGDetailsUpdateThreadAudio(const CMachine &machine)
    : UIGDetailsUpdateThread(machine)
{
}

void UIGDetailsUpdateThreadAudio::run()
{
    COMBase::InitializeCOM(false);

    if (!machine().isNull())
    {
        /* Prepare table: */
        UITextTable m_text;

        /* Gather information: */
        if (machine().GetAccessible())
        {
            const CAudioAdapter &audio = machine().GetAudioAdapter();
            if (audio.GetEnabled())
            {
                /* Driver: */
                m_text << UITextTableLine(QApplication::translate("UIGDetails", "Host Driver", "details (audio)"),
                                          gpConverter->toString(audio.GetAudioDriver()));

                /* Controller: */
                m_text << UITextTableLine(QApplication::translate("UIGDetails", "Controller", "details (audio)"),
                                          gpConverter->toString(audio.GetAudioController()));
            }
            else
                m_text << UITextTableLine(QApplication::translate("UIGDetails", "Disabled", "details (audio)"),
                                          QString());
        }
        else
            m_text << UITextTableLine(QApplication::translate("UIGDetails", "Information Inaccessible", "details"),
                                      QString());

        /* Send information into GUI thread: */
        emit sigComplete(m_text);
    }

    COMBase::CleanupCOM();
}

UIGDetailsElementAudio::UIGDetailsElementAudio(UIGDetailsSet *pParent, bool fOpened)
    : UIGDetailsElementInterface(pParent, DetailsElementType_Audio, fOpened)
{
    /* Name/icon: */
    setName(gpConverter->toString(DetailsElementType_Audio));
    setIcon(UIIconPool::iconSet(":/sound_16px.png"));
}

UIGDetailsUpdateThread* UIGDetailsElementAudio::createUpdateThread()
{
    return new UIGDetailsUpdateThreadAudio(machine());
}


UIGDetailsUpdateThreadNetwork::UIGDetailsUpdateThreadNetwork(const CMachine &machine)
    : UIGDetailsUpdateThread(machine)
{
}

void UIGDetailsUpdateThreadNetwork::run()
{
    COMBase::InitializeCOM(false);

    if (!machine().isNull())
    {
        /* Prepare table: */
        UITextTable m_text;

        /* Gather information: */
        if (machine().GetAccessible())
        {
            /* Iterate over all the adapters: */
            bool fSomeInfo = false;
            ulong uSount = vboxGlobal().virtualBox().GetSystemProperties().GetMaxNetworkAdapters(KChipsetType_PIIX3);
            for (ulong uSlot = 0; uSlot < uSount; ++uSlot)
            {
                const CNetworkAdapter &adapter = machine().GetNetworkAdapter(uSlot);
                if (adapter.GetEnabled())
                {
                    KNetworkAttachmentType type = adapter.GetAttachmentType();
                    QString strAttachmentType = gpConverter->toString(adapter.GetAdapterType())
                                                .replace(QRegExp("\\s\\(.+\\)"), " (%1)");
                    switch (type)
                    {
                        case KNetworkAttachmentType_Bridged:
                        {
                            strAttachmentType = strAttachmentType.arg(QApplication::translate("UIGDetails", "Bridged Adapter, %1", "details (network)")
                                                                      .arg(adapter.GetBridgedInterface()));
                            break;
                        }
                        case KNetworkAttachmentType_Internal:
                        {
                            strAttachmentType = strAttachmentType.arg(QApplication::translate("UIGDetails", "Internal Network, '%1'", "details (network)")
                                                                      .arg(adapter.GetInternalNetwork()));
                            break;
                        }
                        case KNetworkAttachmentType_HostOnly:
                        {
                            strAttachmentType = strAttachmentType.arg(QApplication::translate("UIGDetails", "Host-only Adapter, '%1'", "details (network)")
                                                                      .arg(adapter.GetHostOnlyInterface()));
                            break;
                        }
                        case KNetworkAttachmentType_Generic:
                        {
                            QString strGenericDriverProperties(summarizeGenericProperties(adapter));
                            strAttachmentType = strGenericDriverProperties.isNull() ?
                                      strAttachmentType.arg(QApplication::translate("UIGDetails", "Generic Driver, '%1'", "details (network)").arg(adapter.GetGenericDriver())) :
                                      strAttachmentType.arg(QApplication::translate("UIGDetails", "Generic Driver, '%1' {&nbsp;%2&nbsp;}", "details (network)")
                                                            .arg(adapter.GetGenericDriver(), strGenericDriverProperties));
                            break;
                        }
                        default:
                        {
                            strAttachmentType = strAttachmentType.arg(gpConverter->toString(type));
                            break;
                        }
                    }
                    m_text << UITextTableLine(QApplication::translate("UIGDetails", "Adapter %1", "details (network)").arg(adapter.GetSlot() + 1), strAttachmentType);
                    fSomeInfo = true;
                }
            }
            if (!fSomeInfo)
                m_text << UITextTableLine(QApplication::translate("UIGDetails", "Disabled", "details (network/adapter)"), QString());
        }
        else
            m_text << UITextTableLine(QApplication::translate("UIGDetails", "Information Inaccessible", "details"), QString());

        /* Send information into GUI thread: */
        emit sigComplete(m_text);
    }

    COMBase::CleanupCOM();
}

/* static */
QString UIGDetailsUpdateThreadNetwork::summarizeGenericProperties(const CNetworkAdapter &adapter)
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

UIGDetailsElementNetwork::UIGDetailsElementNetwork(UIGDetailsSet *pParent, bool fOpened)
    : UIGDetailsElementInterface(pParent, DetailsElementType_Network, fOpened)
{
    /* Name/icon: */
    setName(gpConverter->toString(DetailsElementType_Network));
    setIcon(UIIconPool::iconSet(":/nw_16px.png"));
}

UIGDetailsUpdateThread* UIGDetailsElementNetwork::createUpdateThread()
{
    return new UIGDetailsUpdateThreadNetwork(machine());
}


UIGDetailsUpdateThreadSerial::UIGDetailsUpdateThreadSerial(const CMachine &machine)
    : UIGDetailsUpdateThread(machine)
{
}

void UIGDetailsUpdateThreadSerial::run()
{
    COMBase::InitializeCOM(false);

    if (!machine().isNull())
    {
        /* Prepare table: */
        UITextTable m_text;

        /* Gather information: */
        if (machine().GetAccessible())
        {
            /* Iterate over all the ports: */
            bool fSomeInfo = false;
            ulong uCount = vboxGlobal().virtualBox().GetSystemProperties().GetSerialPortCount();
            for (ulong uSlot = 0; uSlot < uCount; ++uSlot)
            {
                const CSerialPort &port = machine().GetSerialPort(uSlot);
                if (port.GetEnabled())
                {
                    KPortMode mode = port.GetHostMode();
                    QString data = vboxGlobal().toCOMPortName(port.GetIRQ(), port.GetIOBase()) + ", ";
                    if (mode == KPortMode_HostPipe || mode == KPortMode_HostDevice || mode == KPortMode_RawFile)
                        data += QString("%1 (%2)").arg(gpConverter->toString(mode)).arg(QDir::toNativeSeparators(port.GetPath()));
                    else
                        data += gpConverter->toString(mode);
                    m_text << UITextTableLine(QApplication::translate("UIGDetails", "Port %1", "details (serial)").arg(port.GetSlot() + 1), data);
                    fSomeInfo = true;
                }
            }
            if (!fSomeInfo)
                m_text << UITextTableLine(QApplication::translate("UIGDetails", "Disabled", "details (serial)"), QString());
        }
        else
            m_text << UITextTableLine(QApplication::translate("UIGDetails", "Information Inaccessible", "details"), QString());

        /* Send information into GUI thread: */
        emit sigComplete(m_text);
    }

    COMBase::CleanupCOM();
}

UIGDetailsElementSerial::UIGDetailsElementSerial(UIGDetailsSet *pParent, bool fOpened)
    : UIGDetailsElementInterface(pParent, DetailsElementType_Serial, fOpened)
{
    /* Name/icon: */
    setName(gpConverter->toString(DetailsElementType_Serial));
    setIcon(UIIconPool::iconSet(":/serial_port_16px.png"));
}

UIGDetailsUpdateThread* UIGDetailsElementSerial::createUpdateThread()
{
    return new UIGDetailsUpdateThreadSerial(machine());
}


#ifdef VBOX_WITH_PARALLEL_PORTS
UIGDetailsUpdateThreadParallel::UIGDetailsUpdateThreadParallel(const CMachine &machine)
    : UIGDetailsUpdateThread(machine)
{
}

void UIGDetailsUpdateThreadParallel::run()
{
    COMBase::InitializeCOM(false);

    if (!machine().isNull())
    {
        /* Prepare table: */
        UITextTable m_text;

        /* Gather information: */
        if (machine().GetAccessible())
        {
            bool fSomeInfo = false;
            ulong uCount = vboxGlobal().virtualBox().GetSystemProperties().GetParallelPortCount();
            for (ulong uSlot = 0; uSlot < uCount; ++uSlot)
            {
                const CParallelPort &port = machine().GetParallelPort(uSlot);
                if (port.GetEnabled())
                {
                    QString data = vboxGlobal().toLPTPortName(port.GetIRQ(), port.GetIOBase()) +
                                   QString(" (<nobr>%1</nobr>)").arg(QDir::toNativeSeparators(port.GetPath()));
                    m_text << UITextTableLine(QApplication::translate("UIGDetails", "Port %1", "details (parallel)").arg(port.GetSlot() + 1), data);
                    fSomeInfo = true;
                }
            }
            if (!fSomeInfo)
                m_text << UITextTableLine(QApplication::translate("UIGDetails", "Disabled", "details (parallel)"), QString());
        }
        else
            m_text << UITextTableLine(QApplication::translate("UIGDetails", "Information Inaccessible", "details"), QString());

        /* Send information into GUI thread: */
        emit sigComplete(m_text);
    }

    COMBase::CleanupCOM();
}

UIGDetailsElementParallel::UIGDetailsElementParallel(UIGDetailsSet *pParent, bool fOpened)
    : UIGDetailsElementInterface(pParent, DetailsElementType_Parallel, fOpened)
{
    /* Name/icon: */
    setName(gpConverter->toString(DetailsElementType_Parallel));
    setIcon(UIIconPool::iconSet(":/parallel_port_16px.png"));
}

UIGDetailsUpdateThread* UIGDetailsElementParallel::createUpdateThread()
{
    return new UIGDetailsUpdateThreadParallel(machine());
}
#endif /* VBOX_WITH_PARALLEL_PORTS */


UIGDetailsUpdateThreadUSB::UIGDetailsUpdateThreadUSB(const CMachine &machine)
    : UIGDetailsUpdateThread(machine)
{
}

void UIGDetailsUpdateThreadUSB::run()
{
    COMBase::InitializeCOM(false);

    if (!machine().isNull())
    {
        /* Prepare table: */
        UITextTable m_text;

        /* Gather information: */
        if (machine().GetAccessible())
        {
            /* Iterate over all the USB filters: */
            const CUSBController &ctl = machine().GetUSBController();
            if (!ctl.isNull() && ctl.GetProxyAvailable())
            {
                if (ctl.GetEnabled())
                {
                    const CUSBDeviceFilterVector &coll = ctl.GetDeviceFilters();
                    uint uActive = 0;
                    for (int i = 0; i < coll.size(); ++i)
                        if (coll[i].GetActive())
                            ++uActive;
                    m_text << UITextTableLine(QApplication::translate("UIGDetails", "Device Filters", "details (usb)"),
                                              QApplication::translate("UIGDetails", "%1 (%2 active)", "details (usb)").arg(coll.size()).arg(uActive));
                }
                else
                    m_text << UITextTableLine(QApplication::translate("UIGDetails", "Disabled", "details (usb)"), QString());
            }
            else
                m_text << UITextTableLine(QApplication::translate("UIGDetails", "USB Controller Inaccessible", "details (usb)"), QString());
        }
        else
            m_text << UITextTableLine(QApplication::translate("UIGDetails", "Information Inaccessible", "details"), QString());

        /* Send information into GUI thread: */
        emit sigComplete(m_text);
    }

    COMBase::CleanupCOM();
}

UIGDetailsElementUSB::UIGDetailsElementUSB(UIGDetailsSet *pParent, bool fOpened)
    : UIGDetailsElementInterface(pParent, DetailsElementType_USB, fOpened)
{
    /* Name/icon: */
    setName(gpConverter->toString(DetailsElementType_USB));
    setIcon(UIIconPool::iconSet(":/usb_16px.png"));
}

UIGDetailsUpdateThread* UIGDetailsElementUSB::createUpdateThread()
{
    return new UIGDetailsUpdateThreadUSB(machine());
}


UIGDetailsUpdateThreadSF::UIGDetailsUpdateThreadSF(const CMachine &machine)
    : UIGDetailsUpdateThread(machine)
{
}

void UIGDetailsUpdateThreadSF::run()
{
    COMBase::InitializeCOM(false);

    if (!machine().isNull())
    {
        /* Prepare table: */
        UITextTable m_text;

        /* Gather information: */
        if (machine().GetAccessible())
        {
            /* Iterate over all the shared folders: */
            ulong uCount = machine().GetSharedFolders().size();
            if (uCount > 0)
                m_text << UITextTableLine(QApplication::translate("UIGDetails", "Shared Folders", "details (shared folders)"), QString::number(uCount));
            else
                m_text << UITextTableLine(QApplication::translate("UIGDetails", "None", "details (shared folders)"), QString());
        }
        else
            m_text << UITextTableLine(QApplication::translate("UIGDetails", "Information Inaccessible", "details"), QString());

        /* Send information into GUI thread: */
        emit sigComplete(m_text);
    }

    COMBase::CleanupCOM();
}

UIGDetailsElementSF::UIGDetailsElementSF(UIGDetailsSet *pParent, bool fOpened)
    : UIGDetailsElementInterface(pParent, DetailsElementType_SF, fOpened)
{
    /* Name/icon: */
    setName(gpConverter->toString(DetailsElementType_SF));
    setIcon(UIIconPool::iconSet(":/shared_folder_16px.png"));
}

UIGDetailsUpdateThread* UIGDetailsElementSF::createUpdateThread()
{
    return new UIGDetailsUpdateThreadSF(machine());
}


UIGDetailsUpdateThreadDescription::UIGDetailsUpdateThreadDescription(const CMachine &machine)
    : UIGDetailsUpdateThread(machine)
{
}

void UIGDetailsUpdateThreadDescription::run()
{
    COMBase::InitializeCOM(false);

    if (!machine().isNull())
    {
        /* Prepare table: */
        UITextTable m_text;

        /* Gather information: */
        if (machine().GetAccessible())
        {
            /* Get description: */
            const QString &strDesc = machine().GetDescription();
            if (!strDesc.isEmpty())
                m_text << UITextTableLine(strDesc, QString());
            else
                m_text << UITextTableLine(QApplication::translate("UIGDetails", "None", "details (description)"), QString());
        }
        else
            m_text << UITextTableLine(QApplication::translate("UIGDetails", "Information Inaccessible", "details"), QString());

        /* Send information into GUI thread: */
        emit sigComplete(m_text);
    }

    COMBase::CleanupCOM();
}

UIGDetailsElementDescription::UIGDetailsElementDescription(UIGDetailsSet *pParent, bool fOpened)
    : UIGDetailsElementInterface(pParent, DetailsElementType_Description, fOpened)
{
    /* Name/icon: */
    setName(gpConverter->toString(DetailsElementType_Description));
    setIcon(UIIconPool::iconSet(":/description_16px.png"));
}

UIGDetailsUpdateThread* UIGDetailsElementDescription::createUpdateThread()
{
    return new UIGDetailsUpdateThreadDescription(machine());
}

