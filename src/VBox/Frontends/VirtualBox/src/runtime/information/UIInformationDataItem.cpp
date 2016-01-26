/* $Id$ */
/** @file
 * VBox Qt GUI - UIInformationDataItem class implementation.
 */

/*
 * Copyright (C) 2016 Oracle Corporation
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

/* GUI includes: */
# include "UISession.h"
# include "UIIconPool.h"
# include "VBoxGlobal.h"
# include "UIConverter.h"
# include "UIMessageCenter.h"
# include "UIInformationItem.h"
# include "UIInformationModel.h"
# include "UIInformationDataItem.h"
# include "UIGraphicsRotatorButton.h"

/* COM includes: */
# include "COMEnums.h"
# include "CMedium.h"
# include "CMachine.h"
# include "CVRDEServer.h"
# include "CSerialPort.h"
# include "CAudioAdapter.h"
# include "CParallelPort.h"
# include "CSharedFolder.h"
# include "CUSBController.h"
# include "CNetworkAdapter.h"
# include "CUSBDeviceFilter.h"
# include "CMediumAttachment.h"
# include "CUSBDeviceFilters.h"
# include "CSystemProperties.h"
# include "CStorageController.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UIInformationDataItem::UIInformationDataItem(InformationElementType type, const CMachine &machine, const CConsole &console)
    : m_type(type)
    , m_machine(machine)
    , m_console(console)
{
}

UIInformationDataItem::~UIInformationDataItem()
{
}

QVariant UIInformationDataItem::data(const QModelIndex &index, int role) const
{
    switch (role)
    {
        case Qt::DisplayRole:
        {
            return tr ("General", "details report");
        }
        break;

        case Qt::DecorationRole:
        {
            return QIcon(":/machine_16px.png");
        }
        break;

        default:
        break;
    }
    return QVariant();
}

UIInformationDataGeneral::UIInformationDataGeneral(const CMachine &machine, const CConsole &console)
    : UIInformationDataItem(InformationElementType_General, machine, console)
{
}

QVariant UIInformationDataGeneral::data(const QModelIndex &index, int role) const
{
    switch (role)
    {
        case Qt::DisplayRole:
        {
            return tr ("General", "details report");
        }
        break;

        case Qt::DecorationRole:
        {
            return QString(":/machine_16px.png");
        }
        break;

        case Qt::UserRole+1:
        {
            UITextTable p_text;
            p_text << UITextTableLine(tr("Name", "details report"), m_machine.GetName());
            p_text << UITextTableLine(tr("OS Type", "details report"), vboxGlobal().vmGuestOSTypeDescription(m_machine.GetOSTypeId()));
            return QVariant::fromValue(p_text);
        }
        break;

        case Qt::SizeHintRole:
        {
            return QSize(100,50);
        }
        break;

        default:
        break;
    }
    return QVariant();
}

UIInformationDataSystem::UIInformationDataSystem(const CMachine &machine, const CConsole &console)
    : UIInformationDataItem(InformationElementType_System, machine, console)
{
}

QVariant UIInformationDataSystem::data(const QModelIndex &index, int role) const
{
    switch (role)
    {
        case Qt::DisplayRole:
        {
            return tr ("System", "details report");
        }
        break;

        case Qt::DecorationRole:
        {
            return QString(":/chipset_16px.png");
        }
        break;

        case Qt::UserRole+1:
        {
#ifdef VBOX_WITH_FULL_DETAILS_REPORT
        /* BIOS Settings holder */
        CBIOSSettings biosSettings = aMachine.GetBIOSSettings();

        /* ACPI */
        QString acpi = biosSettings.GetACPIEnabled()
                       ? tr ("Enabled", "details report (ACPI)")
                       : tr ("Disabled", "details report (ACPI)");

                /* I/O APIC */
                QString ioapic = biosSettings.GetIOAPICEnabled()
                    ? tr ("Enabled", "details report (I/O APIC)")
                    : tr ("Disabled", "details report (I/O APIC)");

                /* PAE/NX */
                QString pae = aMachine.GetCpuProperty(KCpuPropertyType_PAE)
                    ? tr ("Enabled", "details report (PAE/NX)")
                    : tr ("Disabled", "details report (PAE/NX)");

                iRowCount += 3; /* Full report rows. */
#endif /* VBOX_WITH_FULL_DETAILS_REPORT */

                /* Boot order */
                QString bootOrder;
                for (ulong i = 1; i <= vboxGlobal().virtualBox().GetSystemProperties().GetMaxBootPosition(); ++ i)
                {
                    KDeviceType device = m_machine.GetBootOrder (i);
                    if (device == KDeviceType_Null)
                        continue;
                    if (!bootOrder.isEmpty())
                        bootOrder += ", ";
                    bootOrder += gpConverter->toString (device);
                }
                if (bootOrder.isEmpty())
                    bootOrder = gpConverter->toString (KDeviceType_Null);

            UITextTable p_text;
            p_text << UITextTableLine(tr("Base Memory", "details report"), QString::number(m_machine.GetMemorySize()));
            p_text << UITextTableLine(tr("Processor(s)", "details report"), QString::number(m_machine.GetCPUCount()));
            p_text << UITextTableLine(tr("Execution Cap", "details report"), QString::number(m_machine.GetCPUExecutionCap()));
            p_text << UITextTableLine(tr("Boot Order", "details report"), bootOrder);

#ifdef VBOX_WITH_FULL_DETAILS_REPORT
            p_text << UITextTableLine(tr("ACPI", "details report"), acpi);
            p_text << UITextTableLine(tr("I/O APIC", "details report"), ioapic);
            p_text << UITextTableLine(tr("PAE/NX", "details report"), pae);
#endif /* VBOX_WITH_FULL_DETAILS_REPORT */

            /* VT-x/AMD-V availability: */
            bool fVTxAMDVSupported = vboxGlobal().host().GetProcessorFeature(KProcessorFeature_HWVirtEx);

            if (fVTxAMDVSupported)
            {
                /* VT-x/AMD-V */
                QString virt = m_machine.GetHWVirtExProperty(KHWVirtExPropertyType_Enabled)
                    ? tr ("Enabled", "details report (VT-x/AMD-V)")
                    : tr ("Disabled", "details report (VT-x/AMD-V)");
                p_text << UITextTableLine(tr("VT-x/AMD-V", "details report"), virt);

                /* Nested Paging */
                QString nested = m_machine.GetHWVirtExProperty(KHWVirtExPropertyType_NestedPaging)
                    ? tr ("Enabled", "details report (Nested Paging)")
                    : tr ("Disabled", "details report (Nested Paging)");
                p_text << UITextTableLine(tr("Nested Paging", "details report"), nested);
            }

            /* Paravirtualization Interface: */
            const QString strParavirtProvider = gpConverter->toString(m_machine.GetParavirtProvider());
            p_text << UITextTableLine(tr("Paravirtualization Interface", "details report"), strParavirtProvider);

            return QVariant::fromValue(p_text);
        }
        break;
        case Qt::SizeHintRole:
        {
            return QSize(100,125);
        }
        break;

        default:
        break;
    }
    return QVariant();
}

UIInformationDataDisplay::UIInformationDataDisplay(const CMachine &machine, const CConsole &console)
    : UIInformationDataItem(InformationElementType_Display, machine, console)
{
}

QVariant UIInformationDataDisplay::data(const QModelIndex &index, int role) const
{
    switch (role)
    {
        case Qt::DisplayRole:
        {
            return tr ("Display", "details report");
        }
        break;

        case Qt::DecorationRole:
        {
            return QString(":/vrdp_16px.png");
        }
        break;

        case Qt::UserRole+1:
        {

            UITextTable p_text;
            /* Video tab */
            p_text << UITextTableLine(tr("Video Memory", "details report"), QString::number(m_machine.GetVRAMSize()));


            int cGuestScreens = m_machine.GetMonitorCount();
            if (cGuestScreens > 1)
                p_text << UITextTableLine(tr("Screens", "details report"), QString::number(cGuestScreens));


            QString acc3d = m_machine.GetAccelerate3DEnabled() && vboxGlobal().is3DAvailable()
                ? tr ("Enabled", "details report (3D Acceleration)")
                : tr ("Disabled", "details report (3D Acceleration)");
            p_text << UITextTableLine(tr("3D Acceleration", "details report"), acc3d);

    #ifdef VBOX_WITH_VIDEOHWACCEL
            QString acc2dVideo = m_machine.GetAccelerate2DVideoEnabled()
                ? tr ("Enabled", "details report (2D Video Acceleration)")
                : tr ("Disabled", "details report (2D Video Acceleration)");
            p_text << UITextTableLine(tr("2D Video Acceleration", "details report"), acc2dVideo);
    #endif

            /* VRDP tab */
            CVRDEServer srv = m_machine.GetVRDEServer();
            if (!srv.isNull())
            {
                if (srv.GetEnabled())
                    p_text << UITextTableLine(tr("Remote Desktop Server Port", "details report (VRDE Server)"), srv.GetVRDEProperty("TCP/Ports"));
                else
                    p_text << UITextTableLine(tr("Remote Desktop Server", "details report (VRDE Server)"), tr("Disabled", "details report (VRDE Server)"));
            }

            return QVariant::fromValue(p_text);

        }
        break;

        case Qt::SizeHintRole:
        {
            return QSize(100,50);
        }
        break;

        default:
        break;
    }
    return QVariant();
}

UIInformationDataStorage::UIInformationDataStorage(const CMachine &machine, const CConsole &console)
    : UIInformationDataItem(InformationElementType_Storage, machine, console)
{
}

QVariant UIInformationDataStorage::data(const QModelIndex &index, int role) const
{
    switch (role)
    {
        case Qt::DisplayRole:
        {
            return tr("Storage Statistics");
        }
        break;

        case Qt::DecorationRole:
        {
            return QString(":/hd_16px.png");
        }
        break;

        case Qt::UserRole+1:
        {
            UITextTable p_text;
            p_text << UITextTableLine(QString("key3"), QString("value3"));
            return QVariant::fromValue(p_text);
        }
        break;

        case Qt::SizeHintRole:
        {
            return QSize(100, 150);
        }
        break;

        default:
        break;
    }
    return QVariant();
}

