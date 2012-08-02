/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIConverterBackendGlobal implementation
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
#include <QApplication>
#include <QHash>

/* GUI includes: */
#include "UIConverterBackend.h"
#include "VBoxGlobal.h"

/* COM includes: */
#include "CSystemProperties.h"

/* Determines if <Object of type X> can be converted to object of other type.
 * These functions returns 'true' for all allowed conversions. */
template<> bool canConvert<StorageSlot>() { return true; }
template<> bool canConvert<DetailsElementType>() { return true; }

/* QString <= StorageSlot: */
template<> QString toString(const StorageSlot &storageSlot)
{
    QString strResult;
    switch (storageSlot.bus)
    {
        case KStorageBus_IDE:
        {
            int iMaxPort = vboxGlobal().virtualBox().GetSystemProperties().GetMaxPortCountForStorageBus(storageSlot.bus);
            int iMaxDevice = vboxGlobal().virtualBox().GetSystemProperties().GetMaxDevicesPerPortForStorageBus(storageSlot.bus);
            if (storageSlot.port < 0 || storageSlot.port > iMaxPort)
            {
                AssertMsgFailed(("No text for bus=%d & port=%d", storageSlot.bus, storageSlot.port));
                break;
            }
            if (storageSlot.device < 0 || storageSlot.device > iMaxDevice)
            {
                AssertMsgFailed(("No text for bus=%d & port=%d & device=%d", storageSlot.bus, storageSlot.port, storageSlot.device));
                break;
            }
            if (storageSlot.port == 0 && storageSlot.device == 0)
                strResult = QApplication::translate("VBoxGlobal", "IDE Primary Master", "StorageSlot");
            else if (storageSlot.port == 0 && storageSlot.device == 1)
                strResult = QApplication::translate("VBoxGlobal", "IDE Primary Slave", "StorageSlot");
            else if (storageSlot.port == 1 && storageSlot.device == 0)
                strResult = QApplication::translate("VBoxGlobal", "IDE Secondary Master", "StorageSlot");
            else if (storageSlot.port == 1 && storageSlot.device == 1)
                strResult = QApplication::translate("VBoxGlobal", "IDE Secondary Slave", "StorageSlot");
            break;
        }
        case KStorageBus_SATA:
        {
            int iMaxPort = vboxGlobal().virtualBox().GetSystemProperties().GetMaxPortCountForStorageBus(storageSlot.bus);
            if (storageSlot.port < 0 || storageSlot.port > iMaxPort)
            {
                AssertMsgFailed(("No text for bus=%d & port=%d", storageSlot.bus, storageSlot.port));
                break;
            }
            if (storageSlot.device != 0)
            {
                AssertMsgFailed(("No text for bus=%d & port=%d & device=%d", storageSlot.bus, storageSlot.port, storageSlot.device));
                break;
            }
            strResult = QApplication::translate("VBoxGlobal", "SATA Port %1", "StorageSlot").arg(storageSlot.port);
            break;
        }
        case KStorageBus_SCSI:
        {
            int iMaxPort = vboxGlobal().virtualBox().GetSystemProperties().GetMaxPortCountForStorageBus(storageSlot.bus);
            if (storageSlot.port < 0 || storageSlot.port > iMaxPort)
            {
                AssertMsgFailed(("No text for bus=%d & port=%d", storageSlot.bus, storageSlot.port));
                break;
            }
            if (storageSlot.device != 0)
            {
                AssertMsgFailed(("No text for bus=%d & port=%d & device=%d", storageSlot.bus, storageSlot.port, storageSlot.device));
                break;
            }
            strResult = QApplication::translate("VBoxGlobal", "SCSI Port %1", "StorageSlot").arg(storageSlot.port);
            break;
        }
        case KStorageBus_SAS:
        {
            int iMaxPort = vboxGlobal().virtualBox().GetSystemProperties().GetMaxPortCountForStorageBus(storageSlot.bus);
            if (storageSlot.port < 0 || storageSlot.port > iMaxPort)
            {
                AssertMsgFailed(("No text for bus=%d & port=%d", storageSlot.bus, storageSlot.port));
                break;
            }
            if (storageSlot.device != 0)
            {
                AssertMsgFailed(("No text for bus=%d & port=%d & device=%d", storageSlot.bus, storageSlot.port, storageSlot.device));
                break;
            }
            strResult = QApplication::translate("VBoxGlobal", "SAS Port %1", "StorageSlot").arg(storageSlot.port);
            break;
        }
        case KStorageBus_Floppy:
        {
            int iMaxDevice = vboxGlobal().virtualBox().GetSystemProperties().GetMaxDevicesPerPortForStorageBus(storageSlot.bus);
            if (storageSlot.port != 0)
            {
                AssertMsgFailed(("No text for bus=%d & port=%d", storageSlot.bus, storageSlot.port));
                break;
            }
            if (storageSlot.device < 0 || storageSlot.device > iMaxDevice)
            {
                AssertMsgFailed(("No text for bus=%d & port=%d & device=%d", storageSlot.bus, storageSlot.port, storageSlot.device));
                break;
            }
            strResult = QApplication::translate("VBoxGlobal", "Floppy Device %1", "StorageSlot").arg(storageSlot.device);
            break;
        }
        default:
        {
            AssertMsgFailed(("No text for bus=%d & port=% & device=%d", storageSlot.bus, storageSlot.port, storageSlot.device));
            break;
        }
    }
    return strResult;
}

/* StorageSlot <= QString: */
template<> StorageSlot fromString<StorageSlot>(const QString &strStorageSlot)
{
    QHash<int, QString> list;
    list[0] = QApplication::translate("VBoxGlobal", "IDE Primary Master", "StorageSlot");
    list[1] = QApplication::translate("VBoxGlobal", "IDE Primary Slave", "StorageSlot");
    list[2] = QApplication::translate("VBoxGlobal", "IDE Secondary Master", "StorageSlot");
    list[3] = QApplication::translate("VBoxGlobal", "IDE Secondary Slave", "StorageSlot");
    list[4] = QApplication::translate("VBoxGlobal", "SATA Port %1", "StorageSlot");
    list[5] = QApplication::translate("VBoxGlobal", "SCSI Port %1", "StorageSlot");
    list[6] = QApplication::translate("VBoxGlobal", "SAS Port %1", "StorageSlot");
    list[7] = QApplication::translate("VBoxGlobal", "Floppy Device %1", "StorageSlot");
    int index = -1;
    QRegExp regExp;
    for (int i = 0; i < list.size(); ++i)
    {
        regExp = QRegExp(i >= 0 && i <= 3 ? list[i] : list[i].arg("(\\d+)"));
        if (regExp.indexIn(strStorageSlot) != -1)
        {
            index = i;
            break;
        }
    }

    StorageSlot result;
    switch (index)
    {
        case 0:
        case 1:
        case 2:
        case 3:
        {
            KStorageBus bus = KStorageBus_IDE;
            int iMaxPort = vboxGlobal().virtualBox().GetSystemProperties().GetMaxPortCountForStorageBus(bus);
            int iMaxDevice = vboxGlobal().virtualBox().GetSystemProperties().GetMaxDevicesPerPortForStorageBus(bus);
            LONG iPort = index / iMaxPort;
            LONG iDevice = index % iMaxPort;
            if (iPort < 0 || iPort > iMaxPort)
            {
                AssertMsgFailed(("No storage slot for text='%s'", strStorageSlot.toAscii().constData()));
                break;
            }
            if (iDevice < 0 || iDevice > iMaxDevice)
            {
                AssertMsgFailed(("No storage slot for text='%s'", strStorageSlot.toAscii().constData()));
                break;
            }
            result.bus = bus;
            result.port = iPort;
            result.device = iDevice;
            break;
        }
        case 4:
        {
            KStorageBus bus = KStorageBus_SATA;
            int iMaxPort = vboxGlobal().virtualBox().GetSystemProperties().GetMaxPortCountForStorageBus(bus);
            LONG iPort = regExp.cap(1).toInt();
            LONG iDevice = 0;
            if (iPort < 0 || iPort > iMaxPort)
            {
                AssertMsgFailed(("No storage slot for text='%s'", strStorageSlot.toAscii().constData()));
                break;
            }
            result.bus = bus;
            result.port = iPort;
            result.device = iDevice;
            break;
        }
        case 5:
        {
            KStorageBus bus = KStorageBus_SCSI;
            int iMaxPort = vboxGlobal().virtualBox().GetSystemProperties().GetMaxPortCountForStorageBus(bus);
            LONG iPort = regExp.cap(1).toInt();
            LONG iDevice = 0;
            if (iPort < 0 || iPort > iMaxPort)
            {
                AssertMsgFailed(("No storage slot for text='%s'", strStorageSlot.toAscii().constData()));
                break;
            }
            result.bus = bus;
            result.port = iPort;
            result.device = iDevice;
            break;
        }
        case 6:
        {
            KStorageBus bus = KStorageBus_SAS;
            int iMaxPort = vboxGlobal().virtualBox().GetSystemProperties().GetMaxPortCountForStorageBus(bus);
            LONG iPort = regExp.cap(1).toInt();
            LONG iDevice = 0;
            if (iPort < 0 || iPort > iMaxPort)
            {
                AssertMsgFailed(("No storage slot for text='%s'", strStorageSlot.toAscii().constData()));
                break;
            }
            result.bus = bus;
            result.port = iPort;
            result.device = iDevice;
            break;
        }
        case 7:
        {
            KStorageBus bus = KStorageBus_Floppy;
            int iMaxDevice = vboxGlobal().virtualBox().GetSystemProperties().GetMaxDevicesPerPortForStorageBus(bus);
            LONG iPort = 0;
            LONG iDevice = regExp.cap(1).toInt();
            if (iDevice < 0 || iDevice > iMaxDevice)
            {
                AssertMsgFailed(("No storage slot for text='%s'", strStorageSlot.toAscii().constData()));
                break;
            }
            result.bus = bus;
            result.port = iPort;
            result.device = iDevice;
            break;
        }
        default:
        {
            AssertMsgFailed(("No storage slot for text='%s'", strStorageSlot.toAscii().constData()));
            break;
        }
    }
    return result;
}

/* QString <= DetailsElementType: */
template<> QString toString(const DetailsElementType &detailsElementType)
{
    QString strResult;
    switch (detailsElementType)
    {
        case DetailsElementType_General:     strResult = QApplication::translate("VBoxGlobal", "General", "DetailsElementType"); break;
        case DetailsElementType_Preview:     strResult = QApplication::translate("VBoxGlobal", "Preview", "DetailsElementType"); break;
        case DetailsElementType_System:      strResult = QApplication::translate("VBoxGlobal", "System", "DetailsElementType"); break;
        case DetailsElementType_Display:     strResult = QApplication::translate("VBoxGlobal", "Display", "DetailsElementType"); break;
        case DetailsElementType_Storage:     strResult = QApplication::translate("VBoxGlobal", "Storage", "DetailsElementType"); break;
        case DetailsElementType_Audio:       strResult = QApplication::translate("VBoxGlobal", "Audio", "DetailsElementType"); break;
        case DetailsElementType_Network:     strResult = QApplication::translate("VBoxGlobal", "Network", "DetailsElementType"); break;
        case DetailsElementType_Serial:      strResult = QApplication::translate("VBoxGlobal", "Serial ports", "DetailsElementType"); break;
#ifdef VBOX_WITH_PARALLEL_PORTS
        case DetailsElementType_Parallel:    strResult = QApplication::translate("VBoxGlobal", "Parallel ports", "DetailsElementType"); break;
#endif /* VBOX_WITH_PARALLEL_PORTS */
        case DetailsElementType_USB:         strResult = QApplication::translate("VBoxGlobal", "USB", "DetailsElementType"); break;
        case DetailsElementType_SF:          strResult = QApplication::translate("VBoxGlobal", "Shared folders", "DetailsElementType"); break;
        case DetailsElementType_Description: strResult = QApplication::translate("VBoxGlobal", "Description", "DetailsElementType"); break;
        default:
        {
            AssertMsgFailed(("No text for details element type=%d", detailsElementType));
            break;
        }
    }
    return strResult;
}

/* DetailsElementType <= QString: */
template<> DetailsElementType fromString<DetailsElementType>(const QString &strDetailsElementType)
{
    QHash<QString, DetailsElementType> list;
    list.insert(QApplication::translate("VBoxGlobal", "General", "DetailsElementType"),        DetailsElementType_General);
    list.insert(QApplication::translate("VBoxGlobal", "Preview", "DetailsElementType"),        DetailsElementType_Preview);
    list.insert(QApplication::translate("VBoxGlobal", "System", "DetailsElementType"),         DetailsElementType_System);
    list.insert(QApplication::translate("VBoxGlobal", "Display", "DetailsElementType"),        DetailsElementType_Display);
    list.insert(QApplication::translate("VBoxGlobal", "Storage", "DetailsElementType"),        DetailsElementType_Storage);
    list.insert(QApplication::translate("VBoxGlobal", "Audio", "DetailsElementType"),          DetailsElementType_Audio);
    list.insert(QApplication::translate("VBoxGlobal", "Network", "DetailsElementType"),        DetailsElementType_Network);
    list.insert(QApplication::translate("VBoxGlobal", "Serial ports", "DetailsElementType"),   DetailsElementType_Serial);
#ifdef VBOX_WITH_PARALLEL_PORTS
    list.insert(QApplication::translate("VBoxGlobal", "Parallel ports", "DetailsElementType"), DetailsElementType_Parallel);
#endif /* VBOX_WITH_PARALLEL_PORTS */
    list.insert(QApplication::translate("VBoxGlobal", "USB", "DetailsElementType"),            DetailsElementType_USB);
    list.insert(QApplication::translate("VBoxGlobal", "Shared folders", "DetailsElementType"), DetailsElementType_SF);
    list.insert(QApplication::translate("VBoxGlobal", "Description", "DetailsElementType"),    DetailsElementType_Description);
    if (!list.contains(strDetailsElementType))
    {
        AssertMsgFailed(("No value for '%s'", strDetailsElementType.toAscii().constData()));
    }
    return list.value(strDetailsElementType);
}

/* QString <= DetailsElementType: */
template<> QString toInternalString(const DetailsElementType &detailsElementType)
{
    QString strResult;
    switch (detailsElementType)
    {
        case DetailsElementType_General:     strResult = "general"; break;
        case DetailsElementType_Preview:     strResult = "preview"; break;
        case DetailsElementType_System:      strResult = "system"; break;
        case DetailsElementType_Display:     strResult = "display"; break;
        case DetailsElementType_Storage:     strResult = "storage"; break;
        case DetailsElementType_Audio:       strResult = "audio"; break;
        case DetailsElementType_Network:     strResult = "network"; break;
        case DetailsElementType_Serial:      strResult = "serialPorts"; break;
#ifdef VBOX_WITH_PARALLEL_PORTS
        case DetailsElementType_Parallel:    strResult = "parallelPorts"; break;
#endif /* VBOX_WITH_PARALLEL_PORTS */
        case DetailsElementType_USB:         strResult = "usb"; break;
        case DetailsElementType_SF:          strResult = "sharedFolders"; break;
        case DetailsElementType_Description: strResult = "description"; break;
        default:
        {
            AssertMsgFailed(("No text for details element type=%d", detailsElementType));
            break;
        }
    }
    return strResult;
}

/* DetailsElementType <= QString: */
template<> DetailsElementType fromInternalString<DetailsElementType>(const QString &strDetailsElementType)
{
    QHash<QString, DetailsElementType> list;
    list.insert("general",       DetailsElementType_General);
    list.insert("preview",       DetailsElementType_Preview);
    list.insert("system",        DetailsElementType_System);
    list.insert("display",       DetailsElementType_Display);
    list.insert("storage",       DetailsElementType_Storage);
    list.insert("audio",         DetailsElementType_Audio);
    list.insert("network",       DetailsElementType_Network);
    list.insert("serialPorts",   DetailsElementType_Serial);
#ifdef VBOX_WITH_PARALLEL_PORTS
    list.insert("parallelPorts", DetailsElementType_Parallel);
#endif /* VBOX_WITH_PARALLEL_PORTS */
    list.insert("usb",           DetailsElementType_USB);
    list.insert("sharedFolders", DetailsElementType_SF);
    list.insert("description",   DetailsElementType_Description);
    if (!list.contains(strDetailsElementType))
    {
        AssertMsgFailed(("No value for '%s'", strDetailsElementType.toAscii().constData()));
    }
    return list.value(strDetailsElementType);
}

