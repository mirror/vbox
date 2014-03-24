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
template<> bool canConvert<SizeSuffix>() { return true; }
template<> bool canConvert<StorageSlot>() { return true; }
template<> bool canConvert<RuntimeMenuType>() { return true; }
#ifdef Q_WS_MAC
template<> bool canConvert<RuntimeMenuApplicationActionType>() { return true; }
#endif /* Q_WS_MAC */
template<> bool canConvert<RuntimeMenuMachineActionType>() { return true; }
template<> bool canConvert<RuntimeMenuViewActionType>() { return true; }
template<> bool canConvert<RuntimeMenuDevicesActionType>() { return true; }
#ifdef VBOX_WITH_DEBUGGER_GUI
template<> bool canConvert<RuntimeMenuDebuggerActionType>() { return true; }
#endif /* VBOX_WITH_DEBUGGER_GUI */
template<> bool canConvert<RuntimeMenuHelpActionType>() { return true; }
template<> bool canConvert<UIVisualStateType>() { return true; }
template<> bool canConvert<DetailsElementType>() { return true; }
template<> bool canConvert<GlobalSettingsPageType>() { return true; }
template<> bool canConvert<MachineSettingsPageType>() { return true; }
template<> bool canConvert<IndicatorType>() { return true; }
template<> bool canConvert<MachineCloseAction>() { return true; }

/* QString <= SizeSuffix: */
template<> QString toString(const SizeSuffix &sizeSuffix)
{
    QString strResult;
    switch (sizeSuffix)
    {
        case SizeSuffix_Byte:     strResult = QApplication::translate("VBoxGlobal", "B", "size suffix Bytes"); break;
        case SizeSuffix_KiloByte: strResult = QApplication::translate("VBoxGlobal", "KB", "size suffix KBytes=1024 Bytes"); break;
        case SizeSuffix_MegaByte: strResult = QApplication::translate("VBoxGlobal", "MB", "size suffix MBytes=1024 KBytes"); break;
        case SizeSuffix_GigaByte: strResult = QApplication::translate("VBoxGlobal", "GB", "size suffix GBytes=1024 MBytes"); break;
        case SizeSuffix_TeraByte: strResult = QApplication::translate("VBoxGlobal", "TB", "size suffix TBytes=1024 GBytes"); break;
        case SizeSuffix_PetaByte: strResult = QApplication::translate("VBoxGlobal", "PB", "size suffix PBytes=1024 TBytes"); break;
        default:
        {
            AssertMsgFailed(("No text for size suffix=%d", sizeSuffix));
            break;
        }
    }
    return strResult;
}

/* SizeSuffix <= QString: */
template<> SizeSuffix fromString<SizeSuffix>(const QString &strSizeSuffix)
{
    QHash<QString, SizeSuffix> list;
    list.insert(QApplication::translate("VBoxGlobal", "B", "size suffix Bytes"),               SizeSuffix_Byte);
    list.insert(QApplication::translate("VBoxGlobal", "KB", "size suffix KBytes=1024 Bytes"),  SizeSuffix_KiloByte);
    list.insert(QApplication::translate("VBoxGlobal", "MB", "size suffix MBytes=1024 KBytes"), SizeSuffix_MegaByte);
    list.insert(QApplication::translate("VBoxGlobal", "GB", "size suffix GBytes=1024 MBytes"), SizeSuffix_GigaByte);
    list.insert(QApplication::translate("VBoxGlobal", "TB", "size suffix TBytes=1024 GBytes"), SizeSuffix_TeraByte);
    list.insert(QApplication::translate("VBoxGlobal", "PB", "size suffix PBytes=1024 TBytes"), SizeSuffix_PetaByte);
    if (!list.contains(strSizeSuffix))
    {
        AssertMsgFailed(("No value for '%s'", strSizeSuffix.toAscii().constData()));
    }
    return list.value(strSizeSuffix);
}

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
        case KStorageBus_USB:
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
            strResult = QApplication::translate("VBoxGlobal", "USB Port %1", "StorageSlot").arg(storageSlot.port);
            break;
        }
        default:
        {
            AssertMsgFailed(("No text for bus=%d & port=%d & device=%d", storageSlot.bus, storageSlot.port, storageSlot.device));
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
    list[8] = QApplication::translate("VBoxGlobal", "USB Port %1", "StorageSlot");
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
        case 8:
        {
            KStorageBus bus = KStorageBus_USB;
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
        default:
        {
            AssertMsgFailed(("No storage slot for text='%s'", strStorageSlot.toAscii().constData()));
            break;
        }
    }
    return result;
}

/* QString <= RuntimeMenuType: */
template<> QString toInternalString(const RuntimeMenuType &runtimeMenuType)
{
    QString strResult;
    switch (runtimeMenuType)
    {
        case RuntimeMenuType_Machine: strResult = "Machine"; break;
        case RuntimeMenuType_View:    strResult = "View"; break;
        case RuntimeMenuType_Devices: strResult = "Devices"; break;
        case RuntimeMenuType_Debug:   strResult = "Debug"; break;
        case RuntimeMenuType_Help:    strResult = "Help"; break;
        case RuntimeMenuType_All:     strResult = "All"; break;
        default:
        {
            AssertMsgFailed(("No text for indicator type=%d", runtimeMenuType));
            break;
        }
    }
    return strResult;
}

/* RuntimeMenuType <= QString: */
template<> RuntimeMenuType fromInternalString<RuntimeMenuType>(const QString &strRuntimeMenuType)
{
    /* Here we have some fancy stuff allowing us
     * to search through the keys using 'case-insensitive' rule: */
    QStringList keys;  QList<RuntimeMenuType> values;
    keys << "Machine"; values << RuntimeMenuType_Machine;
    keys << "View";    values << RuntimeMenuType_View;
    keys << "Devices"; values << RuntimeMenuType_Devices;
    keys << "Debug";   values << RuntimeMenuType_Debug;
    keys << "Help";    values << RuntimeMenuType_Help;
    keys << "All";     values << RuntimeMenuType_All;
    /* Invalid type for unknown words: */
    if (!keys.contains(strRuntimeMenuType, Qt::CaseInsensitive))
        return RuntimeMenuType_Invalid;
    /* Corresponding type for known words: */
    return values.at(keys.indexOf(QRegExp(strRuntimeMenuType, Qt::CaseInsensitive)));
}

#ifdef Q_WS_MAC
/* QString <= RuntimeMenuApplicationActionType: */
template<> QString toInternalString(const RuntimeMenuApplicationActionType &runtimeMenuApplicationActionType)
{
    QString strResult;
    switch (runtimeMenuApplicationActionType)
    {
        case RuntimeMenuApplicationActionType_About: strResult = "About"; break;
        case RuntimeMenuApplicationActionType_All:   strResult = "All"; break;
        default:
        {
            AssertMsgFailed(("No text for action type=%d", runtimeMenuApplicationActionType));
            break;
        }
    }
    return strResult;
}

/* RuntimeMenuApplicationActionType <= QString: */
template<> RuntimeMenuApplicationActionType fromInternalString<RuntimeMenuApplicationActionType>(const QString &strRuntimeMenuApplicationActionType)
{
    /* Here we have some fancy stuff allowing us
     * to search through the keys using 'case-insensitive' rule: */
    QStringList keys; QList<RuntimeMenuApplicationActionType> values;
    keys << "About";  values << RuntimeMenuApplicationActionType_About;
    keys << "All";    values << RuntimeMenuApplicationActionType_All;
    /* Invalid type for unknown words: */
    if (!keys.contains(strRuntimeMenuApplicationActionType, Qt::CaseInsensitive))
        return RuntimeMenuApplicationActionType_Invalid;
    /* Corresponding type for known words: */
    return values.at(keys.indexOf(QRegExp(strRuntimeMenuApplicationActionType, Qt::CaseInsensitive)));
}
#endif /* Q_WS_MAC */

/* QString <= RuntimeMenuMachineActionType: */
template<> QString toInternalString(const RuntimeMenuMachineActionType &runtimeMenuMachineActionType)
{
    QString strResult;
    switch (runtimeMenuMachineActionType)
    {
        case RuntimeMenuMachineActionType_SettingsDialog:    strResult = "SettingsDialog"; break;
        case RuntimeMenuMachineActionType_TakeSnapshot:      strResult = "TakeSnapshot"; break;
        case RuntimeMenuMachineActionType_TakeScreenshot:    strResult = "TakeScreenshot"; break;
        case RuntimeMenuMachineActionType_InformationDialog: strResult = "InformationDialog"; break;
        case RuntimeMenuMachineActionType_MouseIntegration:  strResult = "MouseIntegration"; break;
        case RuntimeMenuMachineActionType_TypeCAD:           strResult = "TypeCAD"; break;
#ifdef Q_WS_X11
        case RuntimeMenuMachineActionType_TypeCABS:          strResult = "TypeCABS"; break;
#endif /* Q_WS_X11 */
        case RuntimeMenuMachineActionType_Pause:             strResult = "Pause"; break;
        case RuntimeMenuMachineActionType_Reset:             strResult = "Reset"; break;
        case RuntimeMenuMachineActionType_SaveState:         strResult = "SaveState"; break;
        case RuntimeMenuMachineActionType_Shutdown:          strResult = "Shutdown"; break;
        case RuntimeMenuMachineActionType_PowerOff:          strResult = "PowerOff"; break;
#ifndef Q_WS_MAC
        case RuntimeMenuMachineActionType_Close:             strResult = "Close"; break;
#endif /* !Q_WS_MAC */
        case RuntimeMenuMachineActionType_All:               strResult = "All"; break;
        default:
        {
            AssertMsgFailed(("No text for action type=%d", runtimeMenuMachineActionType));
            break;
        }
    }
    return strResult;
}

/* RuntimeMenuMachineActionType <= QString: */
template<> RuntimeMenuMachineActionType fromInternalString<RuntimeMenuMachineActionType>(const QString &strRuntimeMenuMachineActionType)
{
    /* Here we have some fancy stuff allowing us
     * to search through the keys using 'case-insensitive' rule: */
    QStringList keys;            QList<RuntimeMenuMachineActionType> values;
    keys << "SettingsDialog";    values << RuntimeMenuMachineActionType_SettingsDialog;
    keys << "TakeSnapshot";      values << RuntimeMenuMachineActionType_TakeSnapshot;
    keys << "TakeScreenshot";    values << RuntimeMenuMachineActionType_TakeScreenshot;
    keys << "InformationDialog"; values << RuntimeMenuMachineActionType_InformationDialog;
    keys << "MouseIntegration";  values << RuntimeMenuMachineActionType_MouseIntegration;
    keys << "TypeCAD";           values << RuntimeMenuMachineActionType_TypeCAD;
#ifdef Q_WS_X11
    keys << "TypeCABS";          values << RuntimeMenuMachineActionType_TypeCABS;
#endif /* Q_WS_X11 */
    keys << "Pause";             values << RuntimeMenuMachineActionType_Pause;
    keys << "Reset";             values << RuntimeMenuMachineActionType_Reset;
    keys << "SaveState";         values << RuntimeMenuMachineActionType_SaveState;
    keys << "Shutdown";          values << RuntimeMenuMachineActionType_Shutdown;
    keys << "PowerOff";          values << RuntimeMenuMachineActionType_PowerOff;
#ifndef Q_WS_MAC
    keys << "Close";             values << RuntimeMenuMachineActionType_Close;
#endif /* !Q_WS_MAC */
    keys << "All";               values << RuntimeMenuMachineActionType_All;
    /* Invalid type for unknown words: */
    if (!keys.contains(strRuntimeMenuMachineActionType, Qt::CaseInsensitive))
        return RuntimeMenuMachineActionType_Invalid;
    /* Corresponding type for known words: */
    return values.at(keys.indexOf(QRegExp(strRuntimeMenuMachineActionType, Qt::CaseInsensitive)));
}

/* QString <= RuntimeMenuViewActionType: */
template<> QString toInternalString(const RuntimeMenuViewActionType &runtimeMenuViewActionType)
{
    QString strResult;
    switch (runtimeMenuViewActionType)
    {
        case RuntimeMenuViewActionType_Fullscreen:      strResult = "Fullscreen"; break;
        case RuntimeMenuViewActionType_Seamless:        strResult = "Seamless"; break;
        case RuntimeMenuViewActionType_Scale:           strResult = "Scale"; break;
        case RuntimeMenuViewActionType_GuestAutoresize: strResult = "GuestAutoresize"; break;
        case RuntimeMenuViewActionType_AdjustWindow:    strResult = "AdjustWindow"; break;
        case RuntimeMenuViewActionType_Multiscreen:     strResult = "Multiscreen"; break;
        case RuntimeMenuViewActionType_All:             strResult = "All"; break;
        default:
        {
            AssertMsgFailed(("No text for action type=%d", runtimeMenuViewActionType));
            break;
        }
    }
    return strResult;
}

/* RuntimeMenuViewActionType <= QString: */
template<> RuntimeMenuViewActionType fromInternalString<RuntimeMenuViewActionType>(const QString &strRuntimeMenuViewActionType)
{
    /* Here we have some fancy stuff allowing us
     * to search through the keys using 'case-insensitive' rule: */
    QStringList keys;          QList<RuntimeMenuViewActionType> values;
    keys << "Fullscreen";      values << RuntimeMenuViewActionType_Fullscreen;
    keys << "Seamless";        values << RuntimeMenuViewActionType_Seamless;
    keys << "Scale";           values << RuntimeMenuViewActionType_Scale;
    keys << "GuestAutoresize"; values << RuntimeMenuViewActionType_GuestAutoresize;
    keys << "AdjustWindow";    values << RuntimeMenuViewActionType_AdjustWindow;
    keys << "Multiscreen";     values << RuntimeMenuViewActionType_Multiscreen;
    keys << "All";             values << RuntimeMenuViewActionType_All;
    /* Invalid type for unknown words: */
    if (!keys.contains(strRuntimeMenuViewActionType, Qt::CaseInsensitive))
        return RuntimeMenuViewActionType_Invalid;
    /* Corresponding type for known words: */
    return values.at(keys.indexOf(QRegExp(strRuntimeMenuViewActionType, Qt::CaseInsensitive)));
}

/* QString <= RuntimeMenuDevicesActionType: */
template<> QString toInternalString(const RuntimeMenuDevicesActionType &runtimeMenuDevicesActionType)
{
    QString strResult;
    switch (runtimeMenuDevicesActionType)
    {
        case RuntimeMenuDevicesActionType_OpticalDevices:        strResult = "OpticalDevices"; break;
        case RuntimeMenuDevicesActionType_FloppyDevices:         strResult = "FloppyDevices"; break;
        case RuntimeMenuDevicesActionType_USBDevices:            strResult = "USBDevices"; break;
        case RuntimeMenuDevicesActionType_WebCams:               strResult = "WebCams"; break;
        case RuntimeMenuDevicesActionType_SharedClipboard:       strResult = "SharedClipboard"; break;
        case RuntimeMenuDevicesActionType_DragAndDrop:           strResult = "DragAndDrop"; break;
        case RuntimeMenuDevicesActionType_NetworkSettings:       strResult = "NetworkSettings"; break;
        case RuntimeMenuDevicesActionType_SharedFoldersSettings: strResult = "SharedFoldersSettings"; break;
        case RuntimeMenuDevicesActionType_VRDEServer:            strResult = "VRDEServer"; break;
        case RuntimeMenuDevicesActionType_VideoCapture:          strResult = "VideoCapture"; break;
        case RuntimeMenuDevicesActionType_InstallGuestTools:     strResult = "InstallGuestTools"; break;
        case RuntimeMenuDevicesActionType_All:                   strResult = "All"; break;
        default:
        {
            AssertMsgFailed(("No text for action type=%d", runtimeMenuDevicesActionType));
            break;
        }
    }
    return strResult;
}

/* RuntimeMenuDevicesActionType <= QString: */
template<> RuntimeMenuDevicesActionType fromInternalString<RuntimeMenuDevicesActionType>(const QString &strRuntimeMenuDevicesActionType)
{
    /* Here we have some fancy stuff allowing us
     * to search through the keys using 'case-insensitive' rule: */
    QStringList keys;                QList<RuntimeMenuDevicesActionType> values;
    keys << "OpticalDevices";        values << RuntimeMenuDevicesActionType_OpticalDevices;
    keys << "FloppyDevices";         values << RuntimeMenuDevicesActionType_FloppyDevices;
    keys << "USBDevices";            values << RuntimeMenuDevicesActionType_USBDevices;
    keys << "WebCams";               values << RuntimeMenuDevicesActionType_WebCams;
    keys << "SharedClipboard";       values << RuntimeMenuDevicesActionType_SharedClipboard;
    keys << "DragAndDrop";           values << RuntimeMenuDevicesActionType_DragAndDrop;
    keys << "NetworkSettings";       values << RuntimeMenuDevicesActionType_NetworkSettings;
    keys << "SharedFoldersSettings"; values << RuntimeMenuDevicesActionType_SharedFoldersSettings;
    keys << "VRDEServer";            values << RuntimeMenuDevicesActionType_VRDEServer;
    keys << "VideoCapture";          values << RuntimeMenuDevicesActionType_VideoCapture;
    keys << "InstallGuestTools";     values << RuntimeMenuDevicesActionType_InstallGuestTools;
    keys << "All";                   values << RuntimeMenuDevicesActionType_All;
    /* Invalid type for unknown words: */
    if (!keys.contains(strRuntimeMenuDevicesActionType, Qt::CaseInsensitive))
        return RuntimeMenuDevicesActionType_Invalid;
    /* Corresponding type for known words: */
    return values.at(keys.indexOf(QRegExp(strRuntimeMenuDevicesActionType, Qt::CaseInsensitive)));
}

#ifdef VBOX_WITH_DEBUGGER_GUI
/* QString <= RuntimeMenuDebuggerActionType: */
template<> QString toInternalString(const RuntimeMenuDebuggerActionType &runtimeMenuDebuggerActionType)
{
    QString strResult;
    switch (runtimeMenuDebuggerActionType)
    {
        case RuntimeMenuDebuggerActionType_Statistics:  strResult = "Statistics"; break;
        case RuntimeMenuDebuggerActionType_CommandLine: strResult = "CommandLine"; break;
        case RuntimeMenuDebuggerActionType_Logging:     strResult = "Logging"; break;
        case RuntimeMenuDebuggerActionType_LogDialog:   strResult = "LogDialog"; break;
        case RuntimeMenuDebuggerActionType_All:         strResult = "All"; break;
        default:
        {
            AssertMsgFailed(("No text for action type=%d", runtimeMenuDebuggerActionType));
            break;
        }
    }
    return strResult;
}

/* RuntimeMenuDebuggerActionType <= QString: */
template<> RuntimeMenuDebuggerActionType fromInternalString<RuntimeMenuDebuggerActionType>(const QString &strRuntimeMenuDebuggerActionType)
{
    /* Here we have some fancy stuff allowing us
     * to search through the keys using 'case-insensitive' rule: */
    QStringList keys;      QList<RuntimeMenuDebuggerActionType> values;
    keys << "Statistics";  values << RuntimeMenuDebuggerActionType_Statistics;
    keys << "CommandLine"; values << RuntimeMenuDebuggerActionType_CommandLine;
    keys << "Logging";     values << RuntimeMenuDebuggerActionType_Logging;
    keys << "LogDialog";   values << RuntimeMenuDebuggerActionType_LogDialog;
    keys << "All";         values << RuntimeMenuDebuggerActionType_All;
    /* Invalid type for unknown words: */
    if (!keys.contains(strRuntimeMenuDebuggerActionType, Qt::CaseInsensitive))
        return RuntimeMenuDebuggerActionType_Invalid;
    /* Corresponding type for known words: */
    return values.at(keys.indexOf(QRegExp(strRuntimeMenuDebuggerActionType, Qt::CaseInsensitive)));
}
#endif /* VBOX_WITH_DEBUGGER_GUI */

/* QString <= RuntimeMenuHelpActionType: */
template<> QString toInternalString(const RuntimeMenuHelpActionType &runtimeMenuHelpActionType)
{
    QString strResult;
    switch (runtimeMenuHelpActionType)
    {
        case RuntimeMenuHelpActionType_Contents:             strResult = "Contents"; break;
        case RuntimeMenuHelpActionType_WebSite:              strResult = "WebSite"; break;
        case RuntimeMenuHelpActionType_ResetWarnings:        strResult = "ResetWarnings"; break;
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
        case RuntimeMenuHelpActionType_NetworkAccessManager: strResult = "NetworkAccessManager"; break;
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
#ifndef Q_WS_MAC
        case RuntimeMenuHelpActionType_About:                strResult = "About"; break;
#endif /* !Q_WS_MAC */
        case RuntimeMenuHelpActionType_All:                  strResult = "All"; break;
        default:
        {
            AssertMsgFailed(("No text for action type=%d", runtimeMenuHelpActionType));
            break;
        }
    }
    return strResult;
}

/* RuntimeMenuHelpActionType <= QString: */
template<> RuntimeMenuHelpActionType fromInternalString<RuntimeMenuHelpActionType>(const QString &strRuntimeMenuHelpActionType)
{
    /* Here we have some fancy stuff allowing us
     * to search through the keys using 'case-insensitive' rule: */
    QStringList keys;               QList<RuntimeMenuHelpActionType> values;
    keys << "Contents";             values << RuntimeMenuHelpActionType_Contents;
    keys << "WebSite";              values << RuntimeMenuHelpActionType_WebSite;
    keys << "ResetWarnings";        values << RuntimeMenuHelpActionType_ResetWarnings;
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    keys << "NetworkAccessManager"; values << RuntimeMenuHelpActionType_NetworkAccessManager;
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
#ifndef Q_WS_MAC
    keys << "About";                values << RuntimeMenuHelpActionType_About;
#endif /* !Q_WS_MAC */
    keys << "All";                  values << RuntimeMenuHelpActionType_All;
    /* Invalid type for unknown words: */
    if (!keys.contains(strRuntimeMenuHelpActionType, Qt::CaseInsensitive))
        return RuntimeMenuHelpActionType_Invalid;
    /* Corresponding type for known words: */
    return values.at(keys.indexOf(QRegExp(strRuntimeMenuHelpActionType, Qt::CaseInsensitive)));
}

/* QString <= UIVisualStateType: */
template<> QString toInternalString(const UIVisualStateType &visualStateType)
{
    QString strResult;
    switch (visualStateType)
    {
        case UIVisualStateType_Normal:     strResult = "Normal"; break;
        case UIVisualStateType_Fullscreen: strResult = "Fullscreen"; break;
        case UIVisualStateType_Seamless:   strResult = "Seamless"; break;
        case UIVisualStateType_Scale:      strResult = "Scale"; break;
        case UIVisualStateType_All:        strResult = "All"; break;
        default:
        {
            AssertMsgFailed(("No text for visual state type=%d", visualStateType));
            break;
        }
    }
    return strResult;
}

/* UIVisualStateType <= QString: */
template<> UIVisualStateType fromInternalString<UIVisualStateType>(const QString &strVisualStateType)
{
    /* Here we have some fancy stuff allowing us
     * to search through the keys using 'case-insensitive' rule: */
    QStringList keys;     QList<UIVisualStateType> values;
    keys << "Normal";     values << UIVisualStateType_Normal;
    keys << "Fullscreen"; values << UIVisualStateType_Fullscreen;
    keys << "Seamless";   values << UIVisualStateType_Seamless;
    keys << "Scale";      values << UIVisualStateType_Scale;
    keys << "All";        values << UIVisualStateType_All;
    /* Invalid type for unknown words: */
    if (!keys.contains(strVisualStateType, Qt::CaseInsensitive))
        return UIVisualStateType_Invalid;
    /* Corresponding type for known words: */
    return values.at(keys.indexOf(QRegExp(strVisualStateType, Qt::CaseInsensitive)));
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

/* QString <= GlobalSettingsPageType: */
template<> QString toInternalString(const GlobalSettingsPageType &globalSettingsPageType)
{
    QString strResult;
    switch (globalSettingsPageType)
    {
        case GlobalSettingsPageType_General:    strResult = "General"; break;
        case GlobalSettingsPageType_Input:      strResult = "Input"; break;
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
        case GlobalSettingsPageType_Update:     strResult = "Update"; break;
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
        case GlobalSettingsPageType_Language:   strResult = "Language"; break;
        case GlobalSettingsPageType_Display:    strResult = "Display"; break;
        case GlobalSettingsPageType_Network:    strResult = "Network"; break;
        case GlobalSettingsPageType_Extensions: strResult = "Extensions"; break;
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
        case GlobalSettingsPageType_Proxy:      strResult = "Proxy"; break;
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
        default:
        {
            AssertMsgFailed(("No text for settings page type=%d", globalSettingsPageType));
            break;
        }
    }
    return strResult;
}

/* GlobalSettingsPageType <= QString: */
template<> GlobalSettingsPageType fromInternalString<GlobalSettingsPageType>(const QString &strGlobalSettingsPageType)
{
    /* Here we have some fancy stuff allowing us
     * to search through the keys using 'case-insensitive' rule: */
    QStringList keys;    QList<GlobalSettingsPageType> values;
    keys << "General";    values << GlobalSettingsPageType_General;
    keys << "Input";      values << GlobalSettingsPageType_Input;
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    keys << "Update";     values << GlobalSettingsPageType_Update;
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
    keys << "Language";   values << GlobalSettingsPageType_Language;
    keys << "Display";    values << GlobalSettingsPageType_Display;
    keys << "Network";    values << GlobalSettingsPageType_Network;
    keys << "Extensions"; values << GlobalSettingsPageType_Extensions;
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    keys << "Proxy";      values << GlobalSettingsPageType_Proxy;
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
    /* Invalid type for unknown words: */
    if (!keys.contains(strGlobalSettingsPageType, Qt::CaseInsensitive))
        return GlobalSettingsPageType_Invalid;
    /* Corresponding type for known words: */
    return values.at(keys.indexOf(QRegExp(strGlobalSettingsPageType, Qt::CaseInsensitive)));
}

/* QPixmap <= GlobalSettingsPageType: */
template<> QPixmap toWarningPixmap(const GlobalSettingsPageType &type)
{
    switch (type)
    {
        case GlobalSettingsPageType_General:    return QPixmap(":/machine_warning_16px.png");
        case GlobalSettingsPageType_Input:      return QPixmap(":/hostkey_warning_16px.png");
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
        case GlobalSettingsPageType_Update:     return QPixmap(":/refresh_warning_16px.png");
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
        case GlobalSettingsPageType_Language:   return QPixmap(":/site_warning_16px.png");
        case GlobalSettingsPageType_Display:    return QPixmap(":/vrdp_warning_16px.png");
        case GlobalSettingsPageType_Network:    return QPixmap(":/nw_warning_16px.png");
        case GlobalSettingsPageType_Extensions: return QPixmap(":/extension_pack_warning_16px.png");
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
        case GlobalSettingsPageType_Proxy:      return QPixmap(":/proxy_warning_16px.png");
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
        default: AssertMsgFailed(("No pixmap for %d", type)); break;
    }
    return QPixmap();
}

/* QString <= MachineSettingsPageType: */
template<> QString toInternalString(const MachineSettingsPageType &machineSettingsPageType)
{
    QString strResult;
    switch (machineSettingsPageType)
    {
        case MachineSettingsPageType_General:  strResult = "General"; break;
        case MachineSettingsPageType_System:   strResult = "System"; break;
        case MachineSettingsPageType_Display:  strResult = "Display"; break;
        case MachineSettingsPageType_Storage:  strResult = "Storage"; break;
        case MachineSettingsPageType_Audio:    strResult = "Audio"; break;
        case MachineSettingsPageType_Network:  strResult = "Network"; break;
        case MachineSettingsPageType_Ports:    strResult = "Ports"; break;
        case MachineSettingsPageType_Serial:   strResult = "Serial"; break;
        case MachineSettingsPageType_Parallel: strResult = "Parallel"; break;
        case MachineSettingsPageType_USB:      strResult = "USB"; break;
        case MachineSettingsPageType_SF:       strResult = "SharedFolders"; break;
        default:
        {
            AssertMsgFailed(("No text for settings page type=%d", machineSettingsPageType));
            break;
        }
    }
    return strResult;
}

/* MachineSettingsPageType <= QString: */
template<> MachineSettingsPageType fromInternalString<MachineSettingsPageType>(const QString &strMachineSettingsPageType)
{
    /* Here we have some fancy stuff allowing us
     * to search through the keys using 'case-insensitive' rule: */
    QStringList keys;        QList<MachineSettingsPageType> values;
    keys << "General";       values << MachineSettingsPageType_General;
    keys << "System";        values << MachineSettingsPageType_System;
    keys << "Display";       values << MachineSettingsPageType_Display;
    keys << "Storage";       values << MachineSettingsPageType_Storage;
    keys << "Audio";         values << MachineSettingsPageType_Audio;
    keys << "Network";       values << MachineSettingsPageType_Network;
    keys << "Ports";         values << MachineSettingsPageType_Ports;
    keys << "Serial";        values << MachineSettingsPageType_Serial;
    keys << "Parallel";      values << MachineSettingsPageType_Parallel;
    keys << "USB";           values << MachineSettingsPageType_USB;
    keys << "SharedFolders"; values << MachineSettingsPageType_SF;
    /* Invalid type for unknown words: */
    if (!keys.contains(strMachineSettingsPageType, Qt::CaseInsensitive))
        return MachineSettingsPageType_Invalid;
    /* Corresponding type for known words: */
    return values.at(keys.indexOf(QRegExp(strMachineSettingsPageType, Qt::CaseInsensitive)));
}

/* QPixmap <= MachineSettingsPageType: */
template<> QPixmap toWarningPixmap(const MachineSettingsPageType &type)
{
    switch (type)
    {
        case MachineSettingsPageType_General:  return QPixmap(":/machine_warning_16px.png");
        case MachineSettingsPageType_System:   return QPixmap(":/chipset_warning_16px.png");
        case MachineSettingsPageType_Display:  return QPixmap(":/vrdp_warning_16px.png");
        case MachineSettingsPageType_Storage:  return QPixmap(":/hd_warning_16px.png");
        case MachineSettingsPageType_Audio:    return QPixmap(":/sound_warning_16px.png");
        case MachineSettingsPageType_Network:  return QPixmap(":/nw_warning_16px.png");
        case MachineSettingsPageType_Ports:    return QPixmap(":/serial_port_warning_16px.png");
        case MachineSettingsPageType_Serial:   return QPixmap(":/serial_port_warning_16px.png");
        case MachineSettingsPageType_Parallel: return QPixmap(":/parallel_port_warning_16px.png");
        case MachineSettingsPageType_USB:      return QPixmap(":/usb_warning_16px.png");
        case MachineSettingsPageType_SF:       return QPixmap(":/sf_warning_16px.png");
        default: AssertMsgFailed(("No pixmap for %d", type)); break;
    }
    return QPixmap();
}

/* QString <= IndicatorType: */
template<> QString toInternalString(const IndicatorType &indicatorType)
{
    QString strResult;
    switch (indicatorType)
    {
        case IndicatorType_HardDisks:     strResult = "HardDisks"; break;
        case IndicatorType_OpticalDisks:  strResult = "OpticalDisks"; break;
        case IndicatorType_FloppyDisks:   strResult = "FloppyDisks"; break;
        case IndicatorType_Network:       strResult = "Network"; break;
        case IndicatorType_USB:           strResult = "USB"; break;
        case IndicatorType_SharedFolders: strResult = "SharedFolders"; break;
        case IndicatorType_VideoCapture:  strResult = "VideoCapture"; break;
        case IndicatorType_Features:      strResult = "Features"; break;
        case IndicatorType_Mouse:         strResult = "Mouse"; break;
        case IndicatorType_Keyboard:      strResult = "Keyboard"; break;
        default:
        {
            AssertMsgFailed(("No text for indicator type=%d", indicatorType));
            break;
        }
    }
    return strResult;
}

/* IndicatorType <= QString: */
template<> IndicatorType fromInternalString<IndicatorType>(const QString &strIndicatorType)
{
    /* Here we have some fancy stuff allowing us
     * to search through the keys using 'case-insensitive' rule: */
    QStringList keys;        QList<IndicatorType> values;
    keys << "HardDisks";     values << IndicatorType_HardDisks;
    keys << "OpticalDisks";  values << IndicatorType_OpticalDisks;
    keys << "FloppyDisks";   values << IndicatorType_FloppyDisks;
    keys << "Network";       values << IndicatorType_Network;
    keys << "USB";           values << IndicatorType_USB;
    keys << "SharedFolders"; values << IndicatorType_SharedFolders;
    keys << "VideoCapture";  values << IndicatorType_VideoCapture;
    keys << "Features";      values << IndicatorType_Features;
    keys << "Mouse";         values << IndicatorType_Mouse;
    keys << "Keyboard";      values << IndicatorType_Keyboard;
    /* Invalid type for unknown words: */
    if (!keys.contains(strIndicatorType, Qt::CaseInsensitive))
        return IndicatorType_Invalid;
    /* Corresponding type for known words: */
    return values.at(keys.indexOf(QRegExp(strIndicatorType, Qt::CaseInsensitive)));
}

/* QString <= MachineCloseAction: */
template<> QString toInternalString(const MachineCloseAction &machineCloseAction)
{
    QString strResult;
    switch (machineCloseAction)
    {
        case MachineCloseAction_SaveState:                  strResult = "SaveState"; break;
        case MachineCloseAction_Shutdown:                   strResult = "Shutdown"; break;
        case MachineCloseAction_PowerOff:                   strResult = "PowerOff"; break;
        case MachineCloseAction_PowerOff_RestoringSnapshot: strResult = "PowerOffRestoringSnapshot"; break;
        default:
        {
            AssertMsgFailed(("No text for indicator type=%d", machineCloseAction));
            break;
        }
    }
    return strResult;
}

/* MachineCloseAction <= QString: */
template<> MachineCloseAction fromInternalString<MachineCloseAction>(const QString &strMachineCloseAction)
{
    /* Here we have some fancy stuff allowing us
     * to search through the keys using 'case-insensitive' rule: */
    QStringList keys;                    QList<MachineCloseAction> values;
    keys << "SaveState";                 values << MachineCloseAction_SaveState;
    keys << "Shutdown";                  values << MachineCloseAction_Shutdown;
    keys << "PowerOff";                  values << MachineCloseAction_PowerOff;
    keys << "PowerOffRestoringSnapshot"; values << MachineCloseAction_PowerOff_RestoringSnapshot;
    /* Invalid type for unknown words: */
    if (!keys.contains(strMachineCloseAction, Qt::CaseInsensitive))
        return MachineCloseAction_Invalid;
    /* Corresponding type for known words: */
    return values.at(keys.indexOf(QRegExp(strMachineCloseAction, Qt::CaseInsensitive)));
}

