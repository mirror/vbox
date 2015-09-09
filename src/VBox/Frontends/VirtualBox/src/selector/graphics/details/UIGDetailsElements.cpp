/* $Id$ */
/** @file
 * VBox Qt GUI - UIGDetailsDetails class implementation.
 */

/*
 * Copyright (C) 2012-2014 Oracle Corporation
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
# include <QTimer>
# include <QDir>

/* GUI includes: */
# include "UIGDetailsElements.h"
# include "UIGDetailsModel.h"
# include "UIGMachinePreview.h"
# include "UIGraphicsRotatorButton.h"
# include "VBoxGlobal.h"
# include "UIIconPool.h"
# include "UIConverter.h"
# include "UIGraphicsTextPane.h"
# include "UIMessageCenter.h"

/* COM includes: */
# include "CSystemProperties.h"
# include "CVRDEServer.h"
# include "CStorageController.h"
# include "CMediumAttachment.h"
# include "CAudioAdapter.h"
# include "CNetworkAdapter.h"
# include "CSerialPort.h"
# include "CParallelPort.h"
# include "CUSBController.h"
# include "CUSBDeviceFilters.h"
# include "CUSBDeviceFilter.h"
# include "CSharedFolder.h"
# include "CMedium.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

#include <QGraphicsLinearLayout>


/* Constructor: */
UIGDetailsUpdateThread::UIGDetailsUpdateThread(const CMachine &machine)
    : m_machine(machine)
{
    qRegisterMetaType<UITextTable>();
}

UIGDetailsElementInterface::UIGDetailsElementInterface(UIGDetailsSet *pParent, DetailsElementType type, bool fOpened)
    : UIGDetailsElement(pParent, type, fOpened)
    , m_pThread(0)
{
    /* Assign corresponding icon: */
    setIcon(gpConverter->toIcon(elementType()));

    /* Translate finally: */
    retranslateUi();
}

UIGDetailsElementInterface::~UIGDetailsElementInterface()
{
    cleanupThread();
}

void UIGDetailsElementInterface::retranslateUi()
{
    /* Assign corresponding name: */
    setName(gpConverter->toString(elementType()));
}

void UIGDetailsElementInterface::updateAppearance()
{
    /* Call for base class: */
    UIGDetailsElement::updateAppearance();

    /* Create/start update thread in necessary: */
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
    emit sigBuildDone();
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

UIGDetailsUpdateThread* UIGDetailsElementGeneral::createUpdateThread()
{
    return new UIGDetailsUpdateThreadGeneral(machine());
}


UIGDetailsElementPreview::UIGDetailsElementPreview(UIGDetailsSet *pParent, bool fOpened)
    : UIGDetailsElement(pParent, DetailsElementType_Preview, fOpened)
{
    /* Icon: */
    setIcon(UIIconPool::iconSet(":/machine_16px.png"));

    /* Prepare variables: */
    int iMargin = data(ElementData_Margin).toInt();
    /* Prepare layout: */
    QGraphicsLinearLayout *pLayout = new QGraphicsLinearLayout;
    pLayout->setContentsMargins(iMargin, 2 * iMargin + minimumHeaderHeight(), iMargin, iMargin);
    setLayout(pLayout);

    /* Create preview: */
    m_pPreview = new UIGMachinePreview(this);
    connect(m_pPreview, SIGNAL(sigSizeHintChanged()),
            this, SLOT(sltPreviewSizeHintChanged()));
    pLayout->addItem(m_pPreview);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    /* Translate finally: */
    retranslateUi();
}

void UIGDetailsElementPreview::sltPreviewSizeHintChanged()
{
    /* Recursively update size-hints: */
    updateGeometry();
    /* Update whole model layout: */
    model()->updateLayout();
}

void UIGDetailsElementPreview::retranslateUi()
{
    /* Assign corresponding name: */
    setName(gpConverter->toString(elementType()));
}

int UIGDetailsElementPreview::minimumWidthHint() const
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

int UIGDetailsElementPreview::minimumHeightHint(bool fClosed) const
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

void UIGDetailsElementPreview::updateAppearance()
{
    /* Call for base class: */
    UIGDetailsElement::updateAppearance();

    /* Set new machine attribute: */
    m_pPreview->setMachine(machine());
    emit sigBuildDone();
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
            switch (machine().GetEffectiveParavirtProvider())
            {
                case KParavirtProvider_Minimal: acceleration << QApplication::translate("UIGDetails", "Minimal Paravirtualization", "details (system)"); break;
                case KParavirtProvider_HyperV:  acceleration << QApplication::translate("UIGDetails", "Hyper-V Paravirtualization", "details (system)"); break;
                case KParavirtProvider_KVM:     acceleration << QApplication::translate("UIGDetails", "KVM Paravirtualization", "details (system)"); break;
                default: break;
            }
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
            /* Damn GetExtraData should be const already :( */
            CMachine localMachine = machine();

            /* Video memory: */
            m_text << UITextTableLine(QApplication::translate("UIGDetails", "Video Memory", "details (display)"),
                                      QApplication::translate("UIGDetails", "%1 MB", "details").arg(localMachine.GetVRAMSize()));

            /* Screen count: */
            int cGuestScreens = localMachine.GetMonitorCount();
            if (cGuestScreens > 1)
                m_text << UITextTableLine(QApplication::translate("UIGDetails", "Screens", "details (display)"),
                                          QString::number(cGuestScreens));

            /* Get scale-factor value: */
            const QString strScaleFactor = localMachine.GetExtraData(UIExtraDataDefs::GUI_ScaleFactor);
            {
                /* Try to convert loaded data to double: */
                bool fOk = false;
                double dValue = strScaleFactor.toDouble(&fOk);
                /* Invent the default value: */
                if (!fOk || !dValue)
                    dValue = 1.0;
                /* Append information: */
                if (dValue != 1.0)
                    m_text << UITextTableLine(QApplication::translate("UIGDetails", "Scale-factor", "details (display)"),
                                              QString::number(dValue, 'f', 2));
            }

#ifdef Q_WS_MAC
            /* Get 'Unscaled HiDPI Video Output' mode value: */
            const QString strUnscaledHiDPIMode = localMachine.GetExtraData(UIExtraDataDefs::GUI_HiDPI_UnscaledOutput);
            {
                /* Try to convert loaded data to bool: */
                const bool fEnabled  = strUnscaledHiDPIMode.compare("true", Qt::CaseInsensitive) == 0 ||
                                       strUnscaledHiDPIMode.compare("yes", Qt::CaseInsensitive) == 0 ||
                                       strUnscaledHiDPIMode.compare("on", Qt::CaseInsensitive) == 0 ||
                                       strUnscaledHiDPIMode == "1";
                /* Append information: */
                if (fEnabled)
                    m_text << UITextTableLine(QApplication::translate("UIGDetails", "Unscaled HiDPI Video Output", "details (display)"),
                                              QApplication::translate("UIGDetails", "Enabled", "details (display/Unscaled HiDPI Video Output)"));
            }
#endif /* Q_WS_MAC */

            QStringList acceleration;
#ifdef VBOX_WITH_VIDEOHWACCEL
            /* 2D acceleration: */
            if (localMachine.GetAccelerate2DVideoEnabled())
                acceleration << QApplication::translate("UIGDetails", "2D Video", "details (display)");
#endif /* VBOX_WITH_VIDEOHWACCEL */
            /* 3D acceleration: */
            if (localMachine.GetAccelerate3DEnabled())
                acceleration << QApplication::translate("UIGDetails", "3D", "details (display)");
            if (!acceleration.isEmpty())
                m_text << UITextTableLine(QApplication::translate("UIGDetails", "Acceleration", "details (display)"),
                                          acceleration.join(", "));

            /* VRDE info: */
            CVRDEServer srv = localMachine.GetVRDEServer();
            if (!srv.isNull())
            {
                if (srv.GetEnabled())
                    m_text << UITextTableLine(QApplication::translate("UIGDetails", "Remote Desktop Server Port", "details (display/vrde)"),
                                              srv.GetVRDEProperty("TCP/Ports"));
                else
                    m_text << UITextTableLine(QApplication::translate("UIGDetails", "Remote Desktop Server", "details (display/vrde)"),
                                              QApplication::translate("UIGDetails", "Disabled", "details (display/vrde/VRDE server)"));
            }

            /* Video Capture info: */
            if (localMachine.GetVideoCaptureEnabled())
            {
                m_text << UITextTableLine(QApplication::translate("UIGDetails", "Video Capture File", "details (display/video capture)"),
                                          localMachine.GetVideoCaptureFile());
                m_text << UITextTableLine(QApplication::translate("UIGDetails", "Video Capture Attributes", "details (display/video capture)"),
                                          QApplication::translate("UIGDetails", "Frame Size: %1x%2, Frame Rate: %3fps, Bit Rate: %4kbps")
                                             .arg(localMachine.GetVideoCaptureWidth()).arg(localMachine.GetVideoCaptureHeight())
                                             .arg(localMachine.GetVideoCaptureFPS()).arg(localMachine.GetVideoCaptureRate()));
            }
            else
            {
                m_text << UITextTableLine(QApplication::translate("UIGDetails", "Video Capture", "details (display/video capture)"),
                                          QApplication::translate("UIGDetails", "Disabled", "details (display/video capture)"));
            }
        }
        else
            m_text << UITextTableLine(QApplication::translate("UIGDetails", "Information Inaccessible", "details"), QString());

        /* Send information into GUI thread: */
        emit sigComplete(m_text);
    }

    COMBase::CleanupCOM();
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
                    AssertMsg(controller.isOk(),
                              ("Unable to acquire controller data: %s\n",
                               msgCenter().formatRC(controller.lastRC()).toAscii().constData()));
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
                                QApplication::translate("UIGDetails", "[Optical Drive]", "details (storage)") : QString();
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
                                      strAttachmentType.arg(QApplication::translate("UIGDetails", "Generic Driver, '%1' { %2 }", "details (network)")
                                                            .arg(adapter.GetGenericDriver(), strGenericDriverProperties));
                            break;
                        }
                        case KNetworkAttachmentType_NATNetwork:
                        {
                            strAttachmentType = strAttachmentType.arg(QApplication::translate("UIGDetails", "NAT Network, '%1'", "details (network)")
                                                                      .arg(adapter.GetNATNetwork()));
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
                    if (mode == KPortMode_HostPipe || mode == KPortMode_HostDevice ||
                        mode == KPortMode_RawFile || mode == KPortMode_TCP)
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
            const CUSBDeviceFilters &filters = machine().GetUSBDeviceFilters();
            if (!filters.isNull() && machine().GetUSBProxyAvailable())
            {
                const CUSBDeviceFilters flts = machine().GetUSBDeviceFilters();
                const CUSBControllerVector controllers = machine().GetUSBControllers();
                if (!flts.isNull() && !controllers.isEmpty())
                {
                    /* USB Controllers info: */
                    QStringList controllerList;
                    foreach (const CUSBController &controller, controllers)
                        controllerList << gpConverter->toString(controller.GetType());
                    m_text << UITextTableLine(QApplication::translate("UIGDetails", "USB Controller", "details (usb)"),
                                              controllerList.join(", "));
                    /* USB Device Filters info: */
                    const CUSBDeviceFilterVector &coll = flts.GetDeviceFilters();
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

UIGDetailsUpdateThread* UIGDetailsElementSF::createUpdateThread()
{
    return new UIGDetailsUpdateThreadSF(machine());
}


UIGDetailsUpdateThreadUI::UIGDetailsUpdateThreadUI(const CMachine &machine)
    : UIGDetailsUpdateThread(machine)
{
}

void UIGDetailsUpdateThreadUI::run()
{
    COMBase::InitializeCOM(false);

    if (!machine().isNull())
    {
        /* Prepare table: */
        UITextTable m_text;

        /* Gather information: */
        if (machine().GetAccessible())
        {
            /* Damn GetExtraData should be const already :( */
            CMachine localMachine = machine();

#ifndef Q_WS_MAC
            /* Get menu-bar availability status: */
            const QString strMenubarEnabled = localMachine.GetExtraData(UIExtraDataDefs::GUI_MenuBar_Enabled);
            {
                /* Try to convert loaded data to bool: */
                const bool fEnabled = !(strMenubarEnabled.compare("false", Qt::CaseInsensitive) == 0 ||
                                        strMenubarEnabled.compare("no", Qt::CaseInsensitive) == 0 ||
                                        strMenubarEnabled.compare("off", Qt::CaseInsensitive) == 0 ||
                                        strMenubarEnabled == "0");
                /* Append information: */
                m_text << UITextTableLine(QApplication::translate("UIGDetails", "Menu-bar", "details (user interface)"),
                                          fEnabled ? QApplication::translate("UIGDetails", "Enabled", "details (user interface/menu-bar)") :
                                                     QApplication::translate("UIGDetails", "Disabled", "details (user interface/menu-bar)"));
            }
#endif /* !Q_WS_MAC */

            /* Get status-bar availability status: */
            const QString strStatusbarEnabled = localMachine.GetExtraData(UIExtraDataDefs::GUI_StatusBar_Enabled);
            {
                /* Try to convert loaded data to bool: */
                const bool fEnabled = !(strStatusbarEnabled.compare("false", Qt::CaseInsensitive) == 0 ||
                                        strStatusbarEnabled.compare("no", Qt::CaseInsensitive) == 0 ||
                                        strStatusbarEnabled.compare("off", Qt::CaseInsensitive) == 0 ||
                                        strStatusbarEnabled == "0");
                /* Append information: */
                m_text << UITextTableLine(QApplication::translate("UIGDetails", "Status-bar", "details (user interface)"),
                                          fEnabled ? QApplication::translate("UIGDetails", "Enabled", "details (user interface/status-bar)") :
                                                     QApplication::translate("UIGDetails", "Disabled", "details (user interface/status-bar)"));
            }

#ifndef Q_WS_MAC
            /* Get mini-toolbar availability status: */
            const QString strMiniToolbarEnabled = localMachine.GetExtraData(UIExtraDataDefs::GUI_ShowMiniToolBar);
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
                    const QString &strMiniToolbarPosition = localMachine.GetExtraData(UIExtraDataDefs::GUI_MiniToolBarAlignment);
                    {
                        /* Try to convert loaded data to alignment: */
                        switch (gpConverter->fromInternalString<MiniToolbarAlignment>(strMiniToolbarPosition))
                        {
                            /* Append information: */
                            case MiniToolbarAlignment_Top:
                                m_text << UITextTableLine(QApplication::translate("UIGDetails", "Mini-toolbar Position", "details (user interface)"),
                                                          QApplication::translate("UIGDetails", "Top", "details (user interface/mini-toolbar position)"));
                                break;
                            /* Append information: */
                            case MiniToolbarAlignment_Bottom:
                                m_text << UITextTableLine(QApplication::translate("UIGDetails", "Mini-toolbar Position", "details (user interface)"),
                                                          QApplication::translate("UIGDetails", "Bottom", "details (user interface/mini-toolbar position)"));
                                break;
                        }
                    }
                }
                /* Append information: */
                else
                    m_text << UITextTableLine(QApplication::translate("UIGDetails", "Mini-toolbar", "details (user interface)"),
                                              QApplication::translate("UIGDetails", "Disabled", "details (user interface/mini-toolbar)"));
            }
#endif /* !Q_WS_MAC */
        }
        else
            m_text << UITextTableLine(QApplication::translate("UIGDetails", "Information Inaccessible", "details"), QString());

        /* Send information into GUI thread: */
        emit sigComplete(m_text);
    }

    COMBase::CleanupCOM();
}

UIGDetailsUpdateThread* UIGDetailsElementUI::createUpdateThread()
{
    return new UIGDetailsUpdateThreadUI(machine());
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

UIGDetailsUpdateThread* UIGDetailsElementDescription::createUpdateThread()
{
    return new UIGDetailsUpdateThreadDescription(machine());
}

