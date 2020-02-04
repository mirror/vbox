/* $Id$ */
/** @file
 * VBox Qt GUI - UIConverterBackendCOM implementation.
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
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
#include "UIIconPool.h"

/* COM includes: */
#include "COMEnums.h"


/* Determines if <Object of type X> can be converted to object of other type.
 * These functions returns 'true' for all allowed conversions. */
template<> bool canConvert<KMachineState>() { return true; }
template<> bool canConvert<KSessionState>() { return true; }
template<> bool canConvert<KParavirtProvider>() { return true; }
template<> bool canConvert<KDeviceType>() { return true; }
template<> bool canConvert<KClipboardMode>() { return true; }
template<> bool canConvert<KDnDMode>() { return true; }
template<> bool canConvert<KPointingHIDType>() { return true; }
template<> bool canConvert<KGraphicsControllerType>() { return true; }
template<> bool canConvert<KMediumType>() { return true; }
template<> bool canConvert<KMediumVariant>() { return true; }
template<> bool canConvert<KNetworkAttachmentType>() { return true; }
template<> bool canConvert<KNetworkAdapterType>() { return true; }
template<> bool canConvert<KNetworkAdapterPromiscModePolicy>() { return true; }
template<> bool canConvert<KPortMode>() { return true; }
template<> bool canConvert<KUSBControllerType>() { return true; }
template<> bool canConvert<KUSBDeviceState>() { return true; }
template<> bool canConvert<KUSBDeviceFilterAction>() { return true; }
template<> bool canConvert<KAudioDriverType>() { return true; }
template<> bool canConvert<KAudioControllerType>() { return true; }
template<> bool canConvert<KAuthType>() { return true; }
template<> bool canConvert<KStorageBus>() { return true; }
template<> bool canConvert<KStorageControllerType>() { return true; }
template<> bool canConvert<KChipsetType>() { return true; }
template<> bool canConvert<KNATProtocol>() { return true; }

/* QColor <= KMachineState: */
template<> QColor toColor(const KMachineState &state)
{
    switch (state)
    {
        case KMachineState_PoweredOff:             return QColor(Qt::gray);
        case KMachineState_Saved:                  return QColor(Qt::yellow);
        case KMachineState_Aborted:                return QColor(Qt::darkRed);
        case KMachineState_Teleported:             return QColor(Qt::red);
        case KMachineState_Running:                return QColor(Qt::green);
        case KMachineState_Paused:                 return QColor(Qt::darkGreen);
        case KMachineState_Stuck:                  return QColor(Qt::darkMagenta);
        case KMachineState_Teleporting:            return QColor(Qt::blue);
        case KMachineState_Snapshotting:           return QColor(Qt::green);
        case KMachineState_OnlineSnapshotting:     return QColor(Qt::green);
        case KMachineState_LiveSnapshotting:       return QColor(Qt::green);
        case KMachineState_Starting:               return QColor(Qt::green);
        case KMachineState_Stopping:               return QColor(Qt::green);
        case KMachineState_Saving:                 return QColor(Qt::green);
        case KMachineState_Restoring:              return QColor(Qt::green);
        case KMachineState_TeleportingPausedVM:    return QColor(Qt::blue);
        case KMachineState_TeleportingIn:          return QColor(Qt::blue);
        case KMachineState_DeletingSnapshotOnline: return QColor(Qt::green);
        case KMachineState_DeletingSnapshotPaused: return QColor(Qt::darkGreen);
        case KMachineState_RestoringSnapshot:      return QColor(Qt::green);
        case KMachineState_DeletingSnapshot:       return QColor(Qt::green);
        case KMachineState_SettingUp:              return QColor(Qt::green);
        // case KMachineState_FirstOnline:
        // case KMachineState_LastOnline:
        // case KMachineState_FirstTransient:
        // case KMachineState_LastTransient:
        default: AssertMsgFailed(("No color for %d", state)); break;
    }
    return QColor();
}

/* QIcon <= KMachineState: */
template<> QIcon toIcon(const KMachineState &state)
{
    switch (state)
    {
        case KMachineState_PoweredOff:             return UIIconPool::iconSet(":/state_powered_off_16px.png");
        case KMachineState_Saved:                  return UIIconPool::iconSet(":/state_saved_16px.png");
        case KMachineState_Aborted:                return UIIconPool::iconSet(":/state_aborted_16px.png");
        case KMachineState_Teleported:             return UIIconPool::iconSet(":/state_saved_16px.png");
        case KMachineState_Running:                return UIIconPool::iconSet(":/state_running_16px.png");
        case KMachineState_Paused:                 return UIIconPool::iconSet(":/state_paused_16px.png");
        case KMachineState_Stuck:                  return UIIconPool::iconSet(":/state_stuck_16px.png");
        case KMachineState_Teleporting:            return UIIconPool::iconSet(":/state_running_16px.png");
        case KMachineState_Snapshotting:           return UIIconPool::iconSet(":/state_saving_16px.png");
        case KMachineState_OnlineSnapshotting:     return UIIconPool::iconSet(":/state_running_16px.png");
        case KMachineState_LiveSnapshotting:       return UIIconPool::iconSet(":/state_running_16px.png");
        case KMachineState_Starting:               return UIIconPool::iconSet(":/state_running_16px.png");
        case KMachineState_Stopping:               return UIIconPool::iconSet(":/state_running_16px.png");
        case KMachineState_Saving:                 return UIIconPool::iconSet(":/state_saving_16px.png");
        case KMachineState_Restoring:              return UIIconPool::iconSet(":/state_restoring_16px.png");
        case KMachineState_TeleportingPausedVM:    return UIIconPool::iconSet(":/state_saving_16px.png");
        case KMachineState_TeleportingIn:          return UIIconPool::iconSet(":/state_restoring_16px.png");
        case KMachineState_DeletingSnapshotOnline: return UIIconPool::iconSet(":/state_discarding_16px.png");
        case KMachineState_DeletingSnapshotPaused: return UIIconPool::iconSet(":/state_discarding_16px.png");
        case KMachineState_RestoringSnapshot:      return UIIconPool::iconSet(":/state_discarding_16px.png");
        case KMachineState_DeletingSnapshot:       return UIIconPool::iconSet(":/state_discarding_16px.png");
        case KMachineState_SettingUp:              return UIIconPool::iconSet(":/vm_settings_16px.png"); /// @todo Change icon!
        // case KMachineState_FirstOnline:
        // case KMachineState_LastOnline:
        // case KMachineState_FirstTransient:
        // case KMachineState_LastTransient:
        default: AssertMsgFailed(("No icon for %d", state)); break;
    }
    return QIcon();
}

/* QString <= KMachineState: */
template<> QString toString(const KMachineState &state)
{
    switch (state)
    {
        case KMachineState_PoweredOff:             return QApplication::translate("UICommon", "Powered Off", "MachineState");
        case KMachineState_Saved:                  return QApplication::translate("UICommon", "Saved", "MachineState");
        case KMachineState_Aborted:                return QApplication::translate("UICommon", "Aborted", "MachineState");
        case KMachineState_Teleported:             return QApplication::translate("UICommon", "Teleported", "MachineState");
        case KMachineState_Running:                return QApplication::translate("UICommon", "Running", "MachineState");
        case KMachineState_Paused:                 return QApplication::translate("UICommon", "Paused", "MachineState");
        case KMachineState_Stuck:                  return QApplication::translate("UICommon", "Guru Meditation", "MachineState");
        case KMachineState_Teleporting:            return QApplication::translate("UICommon", "Teleporting", "MachineState");
        case KMachineState_Snapshotting:           return QApplication::translate("UICommon", "Taking Snapshot", "MachineState");
        case KMachineState_OnlineSnapshotting:     return QApplication::translate("UICommon", "Taking Online Snapshot", "MachineState");
        case KMachineState_LiveSnapshotting:       return QApplication::translate("UICommon", "Taking Live Snapshot", "MachineState");
        case KMachineState_Starting:               return QApplication::translate("UICommon", "Starting", "MachineState");
        case KMachineState_Stopping:               return QApplication::translate("UICommon", "Stopping", "MachineState");
        case KMachineState_Saving:                 return QApplication::translate("UICommon", "Saving", "MachineState");
        case KMachineState_Restoring:              return QApplication::translate("UICommon", "Restoring", "MachineState");
        case KMachineState_TeleportingPausedVM:    return QApplication::translate("UICommon", "Teleporting Paused VM", "MachineState");
        case KMachineState_TeleportingIn:          return QApplication::translate("UICommon", "Teleporting", "MachineState");
        case KMachineState_DeletingSnapshotOnline: return QApplication::translate("UICommon", "Deleting Snapshot", "MachineState");
        case KMachineState_DeletingSnapshotPaused: return QApplication::translate("UICommon", "Deleting Snapshot", "MachineState");
        case KMachineState_RestoringSnapshot:      return QApplication::translate("UICommon", "Restoring Snapshot", "MachineState");
        case KMachineState_DeletingSnapshot:       return QApplication::translate("UICommon", "Deleting Snapshot", "MachineState");
        case KMachineState_SettingUp:              return QApplication::translate("UICommon", "Setting Up", "MachineState");
        // case KMachineState_FirstOnline:
        // case KMachineState_LastOnline:
        // case KMachineState_FirstTransient:
        // case KMachineState_LastTransient:
        default: AssertMsgFailed(("No text for %d", state)); break;
    }
    return QString();
}

/* QString <= KSessionState: */
template<> QString toString(const KSessionState &state)
{
    switch (state)
    {
        case KSessionState_Unlocked:  return QApplication::translate("UICommon", "Unlocked", "SessionState");
        case KSessionState_Locked:    return QApplication::translate("UICommon", "Locked", "SessionState");
        case KSessionState_Spawning:  return QApplication::translate("UICommon", "Spawning", "SessionState");
        case KSessionState_Unlocking: return QApplication::translate("UICommon", "Unlocking", "SessionState");
        default: AssertMsgFailed(("No text for %d", state)); break;
    }
    return QString();
}

/* QString <= KParavirtProvider: */
template<> QString toString(const KParavirtProvider &type)
{
    switch (type)
    {
        case KParavirtProvider_None:    return QApplication::translate("UICommon", "None", "ParavirtProvider");
        case KParavirtProvider_Default: return QApplication::translate("UICommon", "Default", "ParavirtProvider");
        case KParavirtProvider_Legacy:  return QApplication::translate("UICommon", "Legacy", "ParavirtProvider");
        case KParavirtProvider_Minimal: return QApplication::translate("UICommon", "Minimal", "ParavirtProvider");
        case KParavirtProvider_HyperV:  return QApplication::translate("UICommon", "Hyper-V", "ParavirtProvider");
        case KParavirtProvider_KVM:     return QApplication::translate("UICommon", "KVM", "ParavirtProvider");
        default: AssertMsgFailed(("No text for %d", type)); break;
    }
    return QString();
}

/* QString <= KDeviceType: */
template<> QString toString(const KDeviceType &type)
{
    switch (type)
    {
        case KDeviceType_Null:         return QApplication::translate("UICommon", "None", "DeviceType");
        case KDeviceType_Floppy:       return QApplication::translate("UICommon", "Floppy", "DeviceType");
        case KDeviceType_DVD:          return QApplication::translate("UICommon", "Optical", "DeviceType");
        case KDeviceType_HardDisk:     return QApplication::translate("UICommon", "Hard Disk", "DeviceType");
        case KDeviceType_Network:      return QApplication::translate("UICommon", "Network", "DeviceType");
        case KDeviceType_USB:          return QApplication::translate("UICommon", "USB", "DeviceType");
        case KDeviceType_SharedFolder: return QApplication::translate("UICommon", "Shared Folder", "DeviceType");
        default: AssertMsgFailed(("No text for %d", type)); break;
    }
    return QString();
}

/* QString <= KClipboardMode: */
template<> QString toString(const KClipboardMode &mode)
{
    switch (mode)
    {
        case KClipboardMode_Disabled:      return QApplication::translate("UICommon", "Disabled", "ClipboardType");
        case KClipboardMode_HostToGuest:   return QApplication::translate("UICommon", "Host To Guest", "ClipboardType");
        case KClipboardMode_GuestToHost:   return QApplication::translate("UICommon", "Guest To Host", "ClipboardType");
        case KClipboardMode_Bidirectional: return QApplication::translate("UICommon", "Bidirectional", "ClipboardType");
        default: AssertMsgFailed(("No text for %d", mode)); break;
    }
    return QString();
}

/* QString <= KDnDMode: */
template<> QString toString(const KDnDMode &mode)
{
    switch (mode)
    {
        case KDnDMode_Disabled:      return QApplication::translate("UICommon", "Disabled", "DragAndDropType");
        case KDnDMode_HostToGuest:   return QApplication::translate("UICommon", "Host To Guest", "DragAndDropType");
        case KDnDMode_GuestToHost:   return QApplication::translate("UICommon", "Guest To Host", "DragAndDropType");
        case KDnDMode_Bidirectional: return QApplication::translate("UICommon", "Bidirectional", "DragAndDropType");
        default: AssertMsgFailed(("No text for %d", mode)); break;
    }
    return QString();
}

/* QString <= KPointingHIDType: */
template<> QString toString(const KPointingHIDType &type)
{
    switch (type)
    {
        case KPointingHIDType_PS2Mouse:      return QApplication::translate("UICommon", "PS/2 Mouse", "PointingHIDType");
        case KPointingHIDType_USBMouse:      return QApplication::translate("UICommon", "USB Mouse", "PointingHIDType");
        case KPointingHIDType_USBTablet:     return QApplication::translate("UICommon", "USB Tablet", "PointingHIDType");
        case KPointingHIDType_ComboMouse:    return QApplication::translate("UICommon", "PS/2 and USB Mouse", "PointingHIDType");
        case KPointingHIDType_USBMultiTouch: return QApplication::translate("UICommon", "USB Multi-Touch Tablet", "PointingHIDType");
        default: AssertMsgFailed(("No text for %d", type)); break;
    }
    return QString();
}

/* QString <= KGraphicsControllerType: */
template<> QString toString(const KGraphicsControllerType &type)
{
    switch (type)
    {
        case KGraphicsControllerType_Null:     return QApplication::translate("UICommon", "None",     "GraphicsControllerType");
        case KGraphicsControllerType_VBoxVGA:  return QApplication::translate("UICommon", "VBoxVGA",  "GraphicsControllerType");
        case KGraphicsControllerType_VMSVGA:   return QApplication::translate("UICommon", "VMSVGA",   "GraphicsControllerType");
        case KGraphicsControllerType_VBoxSVGA: return QApplication::translate("UICommon", "VBoxSVGA", "GraphicsControllerType");
        default: AssertMsgFailed(("No text for %d", type)); break;
    }
    return QString();
}

/* KGraphicsControllerType <= QString: */
template<> KGraphicsControllerType fromString<KGraphicsControllerType>(const QString &strType)
{
    QHash<QString, KGraphicsControllerType> list;
    list.insert(QApplication::translate("UICommon", "None",     "GraphicsControllerType"), KGraphicsControllerType_Null);
    list.insert(QApplication::translate("UICommon", "VBoxVGA",  "GraphicsControllerType"), KGraphicsControllerType_VBoxVGA);
    list.insert(QApplication::translate("UICommon", "VMSVGA",   "GraphicsControllerType"), KGraphicsControllerType_VMSVGA);
    list.insert(QApplication::translate("UICommon", "VBoxSVGA", "GraphicsControllerType"), KGraphicsControllerType_VBoxSVGA);
    if (!list.contains(strType))
    {
        AssertMsgFailed(("No value for '%s'", strType.toUtf8().constData()));
    }
    return list.value(strType, KGraphicsControllerType_Null);
}

/* QString <= KMediumType: */
template<> QString toString(const KMediumType &type)
{
    switch (type)
    {
        case KMediumType_Normal:       return QApplication::translate("UICommon", "Normal", "MediumType");
        case KMediumType_Immutable:    return QApplication::translate("UICommon", "Immutable", "MediumType");
        case KMediumType_Writethrough: return QApplication::translate("UICommon", "Writethrough", "MediumType");
        case KMediumType_Shareable:    return QApplication::translate("UICommon", "Shareable", "MediumType");
        case KMediumType_Readonly:     return QApplication::translate("UICommon", "Readonly", "MediumType");
        case KMediumType_MultiAttach:  return QApplication::translate("UICommon", "Multi-attach", "MediumType");
        default: AssertMsgFailed(("No text for %d", type)); break;
    }
    return QString();
}

/* QString <= KMediumVariant: */
template<> QString toString(const KMediumVariant &variant)
{
    /* Note: KMediumVariant_Diff and KMediumVariant_Fixed are so far mutually exclusive: */
    switch ((int)variant)
    {
        case KMediumVariant_Standard:
            return QApplication::translate("UICommon", "Dynamically allocated storage", "MediumVariant");
        case (KMediumVariant)(KMediumVariant_Standard | KMediumVariant_VdiZeroExpand):
            return QApplication::translate("UICommon", "New dynamically allocated storage", "MediumVariant");
        case (KMediumVariant)(KMediumVariant_Standard | KMediumVariant_Diff):
            return QApplication::translate("UICommon", "Dynamically allocated differencing storage", "MediumVariant");
        case (KMediumVariant)(KMediumVariant_Standard | KMediumVariant_Fixed):
            return QApplication::translate("UICommon", "Fixed size storage", "MediumVariant");
        case (KMediumVariant)(KMediumVariant_Standard | KMediumVariant_VmdkSplit2G):
            return QApplication::translate("UICommon", "Dynamically allocated storage split into files of less than 2GB", "MediumVariant");
        case (KMediumVariant)(KMediumVariant_Standard | KMediumVariant_VmdkSplit2G | KMediumVariant_Diff):
            return QApplication::translate("UICommon", "Dynamically allocated differencing storage split into files of less than 2GB", "MediumVariant");
        case (KMediumVariant)(KMediumVariant_Standard | KMediumVariant_Fixed | KMediumVariant_VmdkSplit2G):
            return QApplication::translate("UICommon", "Fixed size storage split into files of less than 2GB", "MediumVariant");
        case (KMediumVariant)(KMediumVariant_Standard | KMediumVariant_VmdkStreamOptimized):
            return QApplication::translate("UICommon", "Dynamically allocated compressed storage", "MediumVariant");
        case (KMediumVariant)(KMediumVariant_Standard | KMediumVariant_VmdkStreamOptimized | KMediumVariant_Diff):
            return QApplication::translate("UICommon", "Dynamically allocated differencing compressed storage", "MediumVariant");
        case (KMediumVariant)(KMediumVariant_Standard | KMediumVariant_Fixed | KMediumVariant_VmdkESX):
            return QApplication::translate("UICommon", "Fixed size ESX storage", "MediumVariant");
        case (KMediumVariant)(KMediumVariant_Standard | KMediumVariant_Fixed | KMediumVariant_VmdkRawDisk):
            return QApplication::translate("UICommon", "Fixed size storage on raw disk", "MediumVariant");
        default:
            AssertMsgFailed(("No text for %d", variant)); break;
    }
    return QString();
}

/* QString <= KNetworkAttachmentType: */
template<> QString toString(const KNetworkAttachmentType &type)
{
    switch (type)
    {
        case KNetworkAttachmentType_Null:       return QApplication::translate("UICommon", "Not attached", "NetworkAttachmentType");
        case KNetworkAttachmentType_NAT:        return QApplication::translate("UICommon", "NAT", "NetworkAttachmentType");
        case KNetworkAttachmentType_Bridged:    return QApplication::translate("UICommon", "Bridged Adapter", "NetworkAttachmentType");
        case KNetworkAttachmentType_Internal:   return QApplication::translate("UICommon", "Internal Network", "NetworkAttachmentType");
        case KNetworkAttachmentType_HostOnly:   return QApplication::translate("UICommon", "Host-only Adapter", "NetworkAttachmentType");
        case KNetworkAttachmentType_Generic:    return QApplication::translate("UICommon", "Generic Driver", "NetworkAttachmentType");
        case KNetworkAttachmentType_NATNetwork: return QApplication::translate("UICommon", "NAT Network", "NetworkAttachmentType");
#ifdef VBOX_WITH_CLOUD_NET
        case KNetworkAttachmentType_Cloud:      return QApplication::translate("UICommon", "Cloud Network", "NetworkAttachmentType");
#endif /* VBOX_WITH_CLOUD_NET */
        default: AssertMsgFailed(("No text for %d", type)); break;
    }
    return QString();
}

/* QString <= KNetworkAdapterType: */
template<> QString toString(const KNetworkAdapterType &type)
{
    switch (type)
    {
        case KNetworkAdapterType_Am79C970A:  return QApplication::translate("UICommon", "PCnet-PCI II (Am79C970A)", "NetworkAdapterType");
        case KNetworkAdapterType_Am79C973:   return QApplication::translate("UICommon", "PCnet-FAST III (Am79C973)", "NetworkAdapterType");
        case KNetworkAdapterType_I82540EM:   return QApplication::translate("UICommon", "Intel PRO/1000 MT Desktop (82540EM)", "NetworkAdapterType");
        case KNetworkAdapterType_I82543GC:   return QApplication::translate("UICommon", "Intel PRO/1000 T Server (82543GC)", "NetworkAdapterType");
        case KNetworkAdapterType_I82545EM:   return QApplication::translate("UICommon", "Intel PRO/1000 MT Server (82545EM)", "NetworkAdapterType");
#ifdef VBOX_WITH_VIRTIO
        case KNetworkAdapterType_Virtio:     return QApplication::translate("UICommon", "Paravirtualized Network (virtio-net)", "NetworkAdapterType");
#endif /* VBOX_WITH_VIRTIO */
        case KNetworkAdapterType_Virtio_1_0: return QApplication::translate("UICommon", "Paravirtualized Network (virtio-net 1.0)", "NetworkAdapterType");
        case KNetworkAdapterType_Am79C960:   return QApplication::translate("UICommon", "PCnet-ISA (Am79C960)", "NetworkAdapterType");
        default: AssertMsgFailed(("No text for %d", type)); break;
    }
    return QString();
}

/* QString <= KNetworkAdapterPromiscModePolicy: */
template<> QString toString(const KNetworkAdapterPromiscModePolicy &policy)
{
    switch (policy)
    {
        case KNetworkAdapterPromiscModePolicy_Deny:
            return QApplication::translate("UICommon", "Deny", "NetworkAdapterPromiscModePolicy");
        case KNetworkAdapterPromiscModePolicy_AllowNetwork:
            return QApplication::translate("UICommon", "Allow VMs", "NetworkAdapterPromiscModePolicy");
        case KNetworkAdapterPromiscModePolicy_AllowAll:
            return QApplication::translate("UICommon", "Allow All", "NetworkAdapterPromiscModePolicy");
        default:
            AssertMsgFailed(("No text for %d", policy)); break;
    }
    return QString();
}

/* QString <= KPortMode: */
template<> QString toString(const KPortMode &mode)
{
    switch (mode)
    {
        case KPortMode_Disconnected: return QApplication::translate("UICommon", "Disconnected", "PortMode");
        case KPortMode_HostPipe:     return QApplication::translate("UICommon", "Host Pipe", "PortMode");
        case KPortMode_HostDevice:   return QApplication::translate("UICommon", "Host Device", "PortMode");
        case KPortMode_RawFile:      return QApplication::translate("UICommon", "Raw File", "PortMode");
        case KPortMode_TCP:          return QApplication::translate("UICommon", "TCP", "PortMode");
        default: AssertMsgFailed(("No text for %d", mode)); break;
    }
    return QString();
}

/* KPortMode <= QString: */
template<> KPortMode fromString<KPortMode>(const QString &strMode)
{
    QHash<QString, KPortMode> list;
    list.insert(QApplication::translate("UICommon", "Disconnected", "PortMode"), KPortMode_Disconnected);
    list.insert(QApplication::translate("UICommon", "Host Pipe", "PortMode"),    KPortMode_HostPipe);
    list.insert(QApplication::translate("UICommon", "Host Device", "PortMode"),  KPortMode_HostDevice);
    list.insert(QApplication::translate("UICommon", "Raw File", "PortMode"),     KPortMode_RawFile);
    list.insert(QApplication::translate("UICommon", "TCP", "PortMode"),          KPortMode_TCP);
    if (!list.contains(strMode))
    {
        AssertMsgFailed(("No value for '%s'", strMode.toUtf8().constData()));
    }
    return list.value(strMode, KPortMode_Disconnected);
}

/* QString <= KUSBControllerType: */
template<> QString toString(const KUSBControllerType &type)
{
    switch (type)
    {
        case KUSBControllerType_OHCI: return QApplication::translate("UICommon", "OHCI", "USBControllerType");
        case KUSBControllerType_EHCI: return QApplication::translate("UICommon", "EHCI", "USBControllerType");
        case KUSBControllerType_XHCI: return QApplication::translate("UICommon", "xHCI", "USBControllerType");
        default: AssertMsgFailed(("No text for %d", type)); break;
    }
    return QString();
}

/* QString <= KUSBDeviceState: */
template<> QString toString(const KUSBDeviceState &state)
{
    switch (state)
    {
        case KUSBDeviceState_NotSupported: return QApplication::translate("UICommon", "Not supported", "USBDeviceState");
        case KUSBDeviceState_Unavailable:  return QApplication::translate("UICommon", "Unavailable", "USBDeviceState");
        case KUSBDeviceState_Busy:         return QApplication::translate("UICommon", "Busy", "USBDeviceState");
        case KUSBDeviceState_Available:    return QApplication::translate("UICommon", "Available", "USBDeviceState");
        case KUSBDeviceState_Held:         return QApplication::translate("UICommon", "Held", "USBDeviceState");
        case KUSBDeviceState_Captured:     return QApplication::translate("UICommon", "Captured", "USBDeviceState");
        default: AssertMsgFailed(("No text for %d", state)); break;
    }
    return QString();
}

/* QString <= KUSBDeviceFilterAction: */
template<> QString toString(const KUSBDeviceFilterAction &action)
{
    switch (action)
    {
        case KUSBDeviceFilterAction_Ignore: return QApplication::translate("UICommon", "Ignore", "USBDeviceFilterAction");
        case KUSBDeviceFilterAction_Hold:   return QApplication::translate("UICommon", "Hold", "USBDeviceFilterAction");
        default: AssertMsgFailed(("No text for %d", action)); break;
    }
    return QString();
}

/* KUSBDeviceFilterAction <= QString: */
template<> KUSBDeviceFilterAction fromString<KUSBDeviceFilterAction>(const QString &strAction)
{
    QHash<QString, KUSBDeviceFilterAction> list;
    list.insert(QApplication::translate("UICommon", "Ignore", "USBDeviceFilterAction"), KUSBDeviceFilterAction_Ignore);
    list.insert(QApplication::translate("UICommon", "Hold", "USBDeviceFilterAction"),   KUSBDeviceFilterAction_Hold);
    if (!list.contains(strAction))
    {
        AssertMsgFailed(("No value for '%s'", strAction.toUtf8().constData()));
    }
    return list.value(strAction, KUSBDeviceFilterAction_Null);
}

/* QString <= KAudioDriverType: */
template<> QString toString(const KAudioDriverType &type)
{
    switch (type)
    {
        case KAudioDriverType_Null:        return QApplication::translate("UICommon", "Null Audio Driver", "AudioDriverType");
        case KAudioDriverType_WinMM:       return QApplication::translate("UICommon", "Windows Multimedia", "AudioDriverType");
        case KAudioDriverType_OSS:         return QApplication::translate("UICommon", "OSS Audio Driver", "AudioDriverType");
        case KAudioDriverType_ALSA:        return QApplication::translate("UICommon", "ALSA Audio Driver", "AudioDriverType");
        case KAudioDriverType_DirectSound: return QApplication::translate("UICommon", "Windows DirectSound", "AudioDriverType");
        case KAudioDriverType_CoreAudio:   return QApplication::translate("UICommon", "CoreAudio", "AudioDriverType");
        // case KAudioDriverType_MMPM:
        case KAudioDriverType_Pulse:       return QApplication::translate("UICommon", "PulseAudio", "AudioDriverType");
        case KAudioDriverType_SolAudio:    return QApplication::translate("UICommon", "Solaris Audio", "AudioDriverType");
        default: AssertMsgFailed(("No text for %d", type)); break;
    }
    return QString();
}

/* KAudioDriverType <= QString: */
template<> KAudioDriverType fromString<KAudioDriverType>(const QString &strType)
{
    QHash<QString, KAudioDriverType> list;
    list.insert(QApplication::translate("UICommon", "Null Audio Driver", "AudioDriverType"),   KAudioDriverType_Null);
    list.insert(QApplication::translate("UICommon", "Windows Multimedia", "AudioDriverType"),  KAudioDriverType_WinMM);
    list.insert(QApplication::translate("UICommon", "OSS Audio Driver", "AudioDriverType"),    KAudioDriverType_OSS);
    list.insert(QApplication::translate("UICommon", "ALSA Audio Driver", "AudioDriverType"),   KAudioDriverType_ALSA);
    list.insert(QApplication::translate("UICommon", "Windows DirectSound", "AudioDriverType"), KAudioDriverType_DirectSound);
    list.insert(QApplication::translate("UICommon", "CoreAudio", "AudioDriverType"),           KAudioDriverType_CoreAudio);
    // list.insert(..., KAudioDriverType_MMPM);
    list.insert(QApplication::translate("UICommon", "PulseAudio", "AudioDriverType"),          KAudioDriverType_Pulse);
    list.insert(QApplication::translate("UICommon", "Solaris Audio", "AudioDriverType"),       KAudioDriverType_SolAudio);
    if (!list.contains(strType))
    {
        AssertMsgFailed(("No value for '%s'", strType.toUtf8().constData()));
    }
    return list.value(strType, KAudioDriverType_Null);
}

/* QString <= KAudioControllerType: */
template<> QString toString(const KAudioControllerType &type)
{
    switch (type)
    {
        case KAudioControllerType_AC97: return QApplication::translate("UICommon", "ICH AC97", "AudioControllerType");
        case KAudioControllerType_SB16: return QApplication::translate("UICommon", "SoundBlaster 16", "AudioControllerType");
        case KAudioControllerType_HDA:  return QApplication::translate("UICommon", "Intel HD Audio", "AudioControllerType");
        default: AssertMsgFailed(("No text for %d", type)); break;
    }
    return QString();
}

/* KAudioControllerType <= QString: */
template<> KAudioControllerType fromString<KAudioControllerType>(const QString &strType)
{
    QHash<QString, KAudioControllerType> list;
    list.insert(QApplication::translate("UICommon", "ICH AC97", "AudioControllerType"),        KAudioControllerType_AC97);
    list.insert(QApplication::translate("UICommon", "SoundBlaster 16", "AudioControllerType"), KAudioControllerType_SB16);
    list.insert(QApplication::translate("UICommon", "Intel HD Audio", "AudioControllerType"),  KAudioControllerType_HDA);
    if (!list.contains(strType))
    {
        AssertMsgFailed(("No value for '%s'", strType.toUtf8().constData()));
    }
    return list.value(strType, KAudioControllerType_AC97);
}

/* QString <= KAuthType: */
template<> QString toString(const KAuthType &type)
{
    switch (type)
    {
        case KAuthType_Null:     return QApplication::translate("UICommon", "Null", "AuthType");
        case KAuthType_External: return QApplication::translate("UICommon", "External", "AuthType");
        case KAuthType_Guest:    return QApplication::translate("UICommon", "Guest", "AuthType");
        default: AssertMsgFailed(("No text for %d", type)); break;
    }
    return QString();
}

/* KAuthType <= QString: */
template<> KAuthType fromString<KAuthType>(const QString &strType)
{
    QHash<QString, KAuthType> list;
    list.insert(QApplication::translate("UICommon", "Null", "AuthType"),     KAuthType_Null);
    list.insert(QApplication::translate("UICommon", "External", "AuthType"), KAuthType_External);
    list.insert(QApplication::translate("UICommon", "Guest", "AuthType"),    KAuthType_Guest);
    if (!list.contains(strType))
    {
        AssertMsgFailed(("No value for '%s'", strType.toUtf8().constData()));
    }
    return list.value(strType, KAuthType_Null);
}

/* QString <= KStorageBus: */
template<> QString toString(const KStorageBus &bus)
{
    switch (bus)
    {
        case KStorageBus_IDE:        return QApplication::translate("UICommon", "IDE", "StorageBus");
        case KStorageBus_SATA:       return QApplication::translate("UICommon", "SATA", "StorageBus");
        case KStorageBus_SCSI:       return QApplication::translate("UICommon", "SCSI", "StorageBus");
        case KStorageBus_Floppy:     return QApplication::translate("UICommon", "Floppy", "StorageBus");
        case KStorageBus_SAS:        return QApplication::translate("UICommon", "SAS", "StorageBus");
        case KStorageBus_USB:        return QApplication::translate("UICommon", "USB", "StorageBus");
        case KStorageBus_PCIe:       return QApplication::translate("UICommon", "PCIe", "StorageBus");
        case KStorageBus_VirtioSCSI: return QApplication::translate("UICommon", "virtio-scsi", "StorageBus");
        default:                     AssertMsgFailed(("No text for %d", bus)); break;
    }
    return QString();
}

/* KStorageBus <= QString: */
template<> KStorageBus fromString<KStorageBus>(const QString &strType)
{
    QHash<QString, KStorageBus> list;
    list.insert(QApplication::translate("UICommon", "IDE", "StorageBus"),         KStorageBus_IDE);
    list.insert(QApplication::translate("UICommon", "SATA", "StorageBus"),        KStorageBus_SATA);
    list.insert(QApplication::translate("UICommon", "SCSI", "StorageBus"),        KStorageBus_SCSI);
    list.insert(QApplication::translate("UICommon", "Floppy", "StorageBus"),      KStorageBus_Floppy);
    list.insert(QApplication::translate("UICommon", "SAS", "StorageBus"),         KStorageBus_SAS);
    list.insert(QApplication::translate("UICommon", "USB", "StorageBus"),         KStorageBus_USB);
    list.insert(QApplication::translate("UICommon", "PCIe", "StorageBus"),        KStorageBus_PCIe);
    list.insert(QApplication::translate("UICommon", "virtio-scsi", "StorageBus"), KStorageBus_VirtioSCSI);
    if (!list.contains(strType))
    {
        AssertMsgFailed(("No value for '%s'", strType.toUtf8().constData()));
    }
    return list.value(strType, KStorageBus_Null);
}

/* QString <= KStorageControllerType: */
template<> QString toString(const KStorageControllerType &type)
{
    switch (type)
    {
        case KStorageControllerType_LsiLogic:    return QApplication::translate("UICommon", "LsiLogic", "StorageControllerType");
        case KStorageControllerType_BusLogic:    return QApplication::translate("UICommon", "BusLogic", "StorageControllerType");
        case KStorageControllerType_IntelAhci:   return QApplication::translate("UICommon", "AHCI", "StorageControllerType");
        case KStorageControllerType_PIIX3:       return QApplication::translate("UICommon", "PIIX3", "StorageControllerType");
        case KStorageControllerType_PIIX4:       return QApplication::translate("UICommon", "PIIX4", "StorageControllerType");
        case KStorageControllerType_ICH6:        return QApplication::translate("UICommon", "ICH6", "StorageControllerType");
        case KStorageControllerType_I82078:      return QApplication::translate("UICommon", "I82078", "StorageControllerType");
        case KStorageControllerType_LsiLogicSas: return QApplication::translate("UICommon", "LsiLogic SAS", "StorageControllerType");
        case KStorageControllerType_USB:         return QApplication::translate("UICommon", "USB", "StorageControllerType");
        case KStorageControllerType_NVMe:        return QApplication::translate("UICommon", "NVMe", "StorageControllerType");
        case KStorageControllerType_VirtioSCSI:  return QApplication::translate("UICommon", "virtio-scsi", "StorageControllerType");
        default: AssertMsgFailed(("No text for %d", type)); break;
    }
    return QString();
}

/* KStorageControllerType <= QString: */
template<> KStorageControllerType fromString<KStorageControllerType>(const QString &strType)
{
    QHash<QString, KStorageControllerType> list;
    list.insert(QApplication::translate("UICommon", "LsiLogic", "StorageControllerType"),     KStorageControllerType_LsiLogic);
    list.insert(QApplication::translate("UICommon", "BusLogic", "StorageControllerType"),     KStorageControllerType_BusLogic);
    list.insert(QApplication::translate("UICommon", "AHCI", "StorageControllerType"),         KStorageControllerType_IntelAhci);
    list.insert(QApplication::translate("UICommon", "PIIX3", "StorageControllerType"),        KStorageControllerType_PIIX3);
    list.insert(QApplication::translate("UICommon", "PIIX4", "StorageControllerType"),        KStorageControllerType_PIIX4);
    list.insert(QApplication::translate("UICommon", "ICH6", "StorageControllerType"),         KStorageControllerType_ICH6);
    list.insert(QApplication::translate("UICommon", "I82078", "StorageControllerType"),       KStorageControllerType_I82078);
    list.insert(QApplication::translate("UICommon", "LsiLogic SAS", "StorageControllerType"), KStorageControllerType_LsiLogicSas);
    list.insert(QApplication::translate("UICommon", "USB", "StorageControllerType"),          KStorageControllerType_USB);
    list.insert(QApplication::translate("UICommon", "NVMe", "StorageControllerType"),         KStorageControllerType_NVMe);
    list.insert(QApplication::translate("UICommon", "virtio-scsi", "StorageControllerType"),  KStorageControllerType_VirtioSCSI);
    if (!list.contains(strType))
    {
        AssertMsgFailed(("No value for '%s'", strType.toUtf8().constData()));
    }
    return list.value(strType, KStorageControllerType_Null);
}

/* QString <= KChipsetType: */
template<> QString toString(const KChipsetType &type)
{
    switch (type)
    {
        case KChipsetType_PIIX3: return QApplication::translate("UICommon", "PIIX3", "ChipsetType");
        case KChipsetType_ICH9:  return QApplication::translate("UICommon", "ICH9", "ChipsetType");
        default: AssertMsgFailed(("No text for %d", type)); break;
    }
    return QString();
}

/* QString <= KNATProtocol: */
template<> QString toString(const KNATProtocol &protocol)
{
    switch (protocol)
    {
        case KNATProtocol_UDP: return QApplication::translate("UICommon", "UDP", "NATProtocol");
        case KNATProtocol_TCP: return QApplication::translate("UICommon", "TCP", "NATProtocol");
        default: AssertMsgFailed(("No text for %d", protocol)); break;
    }
    return QString();
}

/* QString <= KNATProtocol: */
template<> QString toInternalString(const KNATProtocol &protocol)
{
    QString strResult;
    switch (protocol)
    {
        case KNATProtocol_UDP: strResult = "udp"; break;
        case KNATProtocol_TCP: strResult = "tcp"; break;
        default: AssertMsgFailed(("No text for protocol type=%d", protocol)); break;
    }
    return strResult;
}

/* KNATProtocol <= QString: */
template<> KNATProtocol fromInternalString<KNATProtocol>(const QString &strProtocol)
{
    /* Here we have some fancy stuff allowing us
     * to search through the keys using 'case-insensitive' rule: */
    QStringList keys; QList<KNATProtocol> values;
    keys << "udp";    values << KNATProtocol_UDP;
    keys << "tcp";    values << KNATProtocol_TCP;
    /* Invalid type for unknown words: */
    if (!keys.contains(strProtocol, Qt::CaseInsensitive))
    {
        AssertMsgFailed(("No value for '%s'", strProtocol.toUtf8().constData()));
        return KNATProtocol_UDP;
    }
    /* Corresponding type for known words: */
    return values.at(keys.indexOf(QRegExp(strProtocol, Qt::CaseInsensitive)));
}
