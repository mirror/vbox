/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * COMEnumsWrapper class implementation
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

/* Qt includes: */
#include <QApplication>
#include <QColor>
#include <QPixmap>

/* GUI includes: */
#include "COMEnumsWrapper.h"

/* static */
COMEnumsWrapper* COMEnumsWrapper::m_spInstance = 0;

/* static */
void COMEnumsWrapper::prepare()
{
    /* Make sure instance WAS NOT created yet: */
    if (m_spInstance)
        return;

    /* Prepare instance: */
    m_spInstance = new COMEnumsWrapper;
}

/* static */
void COMEnumsWrapper::cleanup()
{
    /* Make sure instance IS still created: */
    if (!m_spInstance)
        return;

    /* Cleanup instance: */
    delete m_spInstance;
    m_spInstance = 0;
}

/* KMachineState => QString: */
QString COMEnumsWrapper::toString(KMachineState state) const
{
    AssertMsg(!m_machineStateNames.value(state).isNull(), ("No text for %d", state));
    return m_machineStateNames.value(state);
}

/* KMachineState => QColor: */
const QColor& COMEnumsWrapper::toColor(KMachineState state) const
{
    static const QColor none;
    AssertMsg(m_machineStateColors.value(state), ("No color for %d", state));
    return m_machineStateColors.value(state) ? *m_machineStateColors.value(state) : none;
}

/* KMachineState => QPixmap: */
QPixmap COMEnumsWrapper::toIcon(KMachineState state) const
{
    QPixmap *pPixMap = m_machineStateIcons.value(state);
    AssertMsg(pPixMap, ("Icon for VM state %d must be defined", state));
    return pPixMap ? *pPixMap : QPixmap();
}

/* KSessionState => QString: */
QString COMEnumsWrapper::toString(KSessionState state) const
{
    AssertMsg(!m_sessionStateNames.value(state).isNull(), ("No text for %d", state));
    return m_sessionStateNames.value(state);
}

/* KDeviceType => QString: */
QString COMEnumsWrapper::toString(KDeviceType type) const
{
    AssertMsg(!m_deviceTypeNames.value(type).isNull(), ("No text for %d", type));
    return m_deviceTypeNames.value(type);
}

/* QString => KDeviceType: */
KDeviceType COMEnumsWrapper::toDeviceType(const QString &strType) const
{
    UIULongStringHash::const_iterator it = qFind(m_deviceTypeNames.begin(), m_deviceTypeNames.end(), strType);
    AssertMsg(it != m_deviceTypeNames.end(), ("No value for {%s}", strType.toLatin1().constData()));
    return KDeviceType(it.key());
}

/* KClipboardMode => QString: */
QString COMEnumsWrapper::toString(KClipboardMode mode) const
{
    AssertMsg(!m_clipboardTypeNames.value(mode).isNull(), ("No text for %d", mode));
    return m_clipboardTypeNames.value(mode);
}

/* QString => KClipboardMode: */
KClipboardMode COMEnumsWrapper::toClipboardModeType(const QString &strMode) const
{
    UIULongStringHash::const_iterator it = qFind(m_clipboardTypeNames.begin(), m_clipboardTypeNames.end(), strMode);
    AssertMsg(it != m_clipboardTypeNames.end(), ("No value for {%s}", strMode.toLatin1().constData()));
    return KClipboardMode(it.key());
}

/* KMediumType => QString: */
QString COMEnumsWrapper::toString(KMediumType type) const
{
    AssertMsg(!m_mediumTypeNames.value(type).isNull(), ("No text for %d", type));
    return m_mediumTypeNames.value(type);
}

/* KMediumVariant => QString: */
QString COMEnumsWrapper::toString(KMediumVariant variant) const
{
    switch (variant)
    {
        case KMediumVariant_Standard:
            return m_mediumVariantNames[0];
        case (KMediumVariant)(KMediumVariant_Standard | KMediumVariant_Diff):
            return m_mediumVariantNames[1];
        case (KMediumVariant)(KMediumVariant_Standard | KMediumVariant_Fixed):
            return m_mediumVariantNames[2];
        case (KMediumVariant)(KMediumVariant_Standard | KMediumVariant_Fixed | KMediumVariant_Diff):
            return m_mediumVariantNames[3];
        case (KMediumVariant)(KMediumVariant_Standard | KMediumVariant_VmdkSplit2G):
            return m_mediumVariantNames[4];
        case (KMediumVariant)(KMediumVariant_Standard | KMediumVariant_VmdkSplit2G | KMediumVariant_Diff):
            return m_mediumVariantNames[5];
        case (KMediumVariant)(KMediumVariant_Standard | KMediumVariant_Fixed | KMediumVariant_VmdkSplit2G):
            return m_mediumVariantNames[6];
        case (KMediumVariant)(KMediumVariant_Standard | KMediumVariant_Fixed | KMediumVariant_VmdkSplit2G | KMediumVariant_Diff):
            return m_mediumVariantNames[7];
        default:
            AssertMsgFailed(("No text for %d", variant));
            break;
    }
    return QString();
}

/* KNetworkAttachmentType => QString: */
QString COMEnumsWrapper::toString(KNetworkAttachmentType type) const
{
    AssertMsg(!m_networkAttachmentTypeNames.value(type).isNull(), ("No text for %d", type));
    return m_networkAttachmentTypeNames.value(type);
}

/* QString => KNetworkAttachmentType: */
KNetworkAttachmentType COMEnumsWrapper::toNetworkAttachmentType(const QString &strType) const
{
    UIULongStringHash::const_iterator it = qFind(m_networkAttachmentTypeNames.begin(), m_networkAttachmentTypeNames.end(), strType);
    AssertMsg(it != m_networkAttachmentTypeNames.end(), ("No value for {%s}", strType.toLatin1().constData()));
    return KNetworkAttachmentType(it.key());
}

/* KNetworkAdapterType => QString: */
QString COMEnumsWrapper::toString(KNetworkAdapterType type) const
{
    AssertMsg(!m_networkAdapterTypeNames.value(type).isNull(), ("No text for %d", type));
    return m_networkAdapterTypeNames.value(type);
}

/* QString => KNetworkAdapterType: */
KNetworkAdapterType COMEnumsWrapper::toNetworkAdapterType(const QString &strType) const
{
    UIULongStringHash::const_iterator it = qFind(m_networkAdapterTypeNames.begin(), m_networkAdapterTypeNames.end(), strType);
    AssertMsg(it != m_networkAdapterTypeNames.end(), ("No value for {%s}", strType.toLatin1().constData()));
    return KNetworkAdapterType(it.key());
}

/* KNetworkAdapterPromiscModePolicy => QString: */
QString COMEnumsWrapper::toString(KNetworkAdapterPromiscModePolicy policy) const
{
    AssertMsg(!m_networkAdapterPromiscModePolicyNames.value(policy).isNull(), ("No text for %d", policy));
    return m_networkAdapterPromiscModePolicyNames.value(policy);
}

/* QString => KNetworkAdapterPromiscModePolicy: */
KNetworkAdapterPromiscModePolicy COMEnumsWrapper::toNetworkAdapterPromiscModePolicyType(const QString &strPolicy) const
{
    UIULongStringHash::const_iterator it = qFind(m_networkAdapterPromiscModePolicyNames.begin(), m_networkAdapterPromiscModePolicyNames.end(), strPolicy);
    AssertMsg(it != m_networkAdapterPromiscModePolicyNames.end(), ("No value for {%s}", strPolicy.toLatin1().constData()));
    return KNetworkAdapterPromiscModePolicy(it.key());
}

/* KPortMode => QString: */
QString COMEnumsWrapper::toString(KPortMode mode) const
{
    AssertMsg(!m_portModeTypeNames.value(mode).isNull(), ("No text for %d", mode));
    return m_portModeTypeNames.value(mode);
}

/* QString => KPortMode: */
KPortMode COMEnumsWrapper::toPortMode(const QString &strMode) const
{
    UIULongStringHash::const_iterator it = qFind(m_portModeTypeNames.begin(), m_portModeTypeNames.end(), strMode);
    AssertMsg(it != m_portModeTypeNames.end(), ("No value for {%s}", strMode.toLatin1().constData()));
    return KPortMode(it.key());
}

/* KUSBDeviceState => QString: */
QString COMEnumsWrapper::toString(KUSBDeviceState state) const
{
    AssertMsg(!m_usbDeviceStateNames.value(state).isNull(), ("No text for %d", state));
    return m_usbDeviceStateNames.value(state);
}

/* KUSBDeviceFilterAction => QString: */
QString COMEnumsWrapper::toString(KUSBDeviceFilterAction action) const
{
    AssertMsg(!m_usbDeviceFilterActionNames.value(action).isNull(), ("No text for %d", action));
    return m_usbDeviceFilterActionNames.value(action);
}

/* QString => KUSBDeviceFilterAction: */
KUSBDeviceFilterAction COMEnumsWrapper::toUSBDevFilterAction(const QString &strActions) const
{
    UIULongStringHash::const_iterator it = qFind(m_usbDeviceFilterActionNames.begin(), m_usbDeviceFilterActionNames.end(), strActions);
    AssertMsg(it != m_usbDeviceFilterActionNames.end(), ("No value for {%s}", strActions.toLatin1().constData()));
    return KUSBDeviceFilterAction(it.key());
}

/* KAudioDriverType => QString: */
QString COMEnumsWrapper::toString(KAudioDriverType type) const
{
    AssertMsg(!m_audioDriverTypeNames.value(type).isNull(), ("No text for %d", type));
    return m_audioDriverTypeNames.value(type);
}

/* QString => KAudioDriverType: */
KAudioDriverType COMEnumsWrapper::toAudioDriverType(const QString &strType) const
{
    UIULongStringHash::const_iterator it = qFind(m_audioDriverTypeNames.begin(), m_audioDriverTypeNames.end(), strType);
    AssertMsg(it != m_audioDriverTypeNames.end(), ("No value for {%s}", strType.toLatin1().constData()));
    return KAudioDriverType(it.key());
}

/* KAudioControllerType => QString: */
QString COMEnumsWrapper::toString(KAudioControllerType type) const
{
    AssertMsg(!m_audioControllerTypeNames.value(type).isNull(), ("No text for %d", type));
    return m_audioControllerTypeNames.value(type);
}

/* QString => KAudioControllerType: */
KAudioControllerType COMEnumsWrapper::toAudioControllerType(const QString &strType) const
{
    UIULongStringHash::const_iterator it = qFind(m_audioControllerTypeNames.begin(), m_audioControllerTypeNames.end(), strType);
    AssertMsg(it != m_audioControllerTypeNames.end(), ("No value for {%s}", strType.toLatin1().constData()));
    return KAudioControllerType(it.key());
}

/* KAuthType => QString: */
QString COMEnumsWrapper::toString(KAuthType type) const
{
    AssertMsg(!m_authTypeNames.value(type).isNull(), ("No text for %d", type));
    return m_authTypeNames.value(type);
}

/* QString => KAuthType: */
KAuthType COMEnumsWrapper::toAuthType(const QString &strType) const
{
    UIULongStringHash::const_iterator it = qFind(m_authTypeNames.begin(), m_authTypeNames.end(), strType);
    AssertMsg(it != m_authTypeNames.end(), ("No value for {%s}", strType.toLatin1().constData()));
    return KAuthType(it.key());
}

/* KStorageBus => QString: */
QString COMEnumsWrapper::toString(KStorageBus bus) const
{
    AssertMsg(!m_storageBusNames.value(bus).isNull(), ("No text for %d", bus));
    return m_storageBusNames[bus];
}

/* QString => KStorageBus: */
KStorageBus COMEnumsWrapper::toStorageBusType(const QString &strBus) const
{
    UIULongStringHash::const_iterator it = qFind(m_storageBusNames.begin(), m_storageBusNames.end(), strBus);
    AssertMsg(it != m_storageBusNames.end(), ("No value for {%s}", strBus.toLatin1().constData()));
    return KStorageBus(it.key());
}

/* KStorageBus/LONG => QString: */
QString COMEnumsWrapper::toString(KStorageBus bus, LONG iChannel) const
{
    QString strChannel;

    switch (bus)
    {
        case KStorageBus_IDE:
        {
            if (iChannel == 0 || iChannel == 1)
            {
                strChannel = m_storageBusChannelNames[iChannel];
                break;
            }
            AssertMsgFailed(("Invalid IDE channel %d\n", iChannel));
            break;
        }
        case KStorageBus_SATA:
        case KStorageBus_SCSI:
        {
            strChannel = m_storageBusChannelNames[2].arg(iChannel);
            break;
        }
        case KStorageBus_Floppy:
        {
            AssertMsgFailed(("Floppy have no channels, only devices\n"));
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid bus type %d\n", bus));
            break;
        }
    }

    Assert(!strChannel.isNull());
    return strChannel;
}

/* KStorageBus/QString => LONG: */
LONG COMEnumsWrapper::toStorageChannel(KStorageBus bus, const QString &strChannel) const
{
    LONG iChannel = 0;

    switch (bus)
    {
        case KStorageBus_IDE:
        {
            UILongStringHash::const_iterator it = qFind(m_storageBusChannelNames.begin(), m_storageBusChannelNames.end(), strChannel);
            AssertMsgBreak(it != m_storageBusChannelNames.end(), ("No value for {%s}\n", strChannel.toLatin1().constData()));
            iChannel = it.key();
            break;
        }
        case KStorageBus_SATA:
        case KStorageBus_SCSI:
        {
            QString strTemplate = m_storageBusChannelNames[2].arg("");
            if (strChannel.startsWith(strTemplate))
            {
                iChannel = strChannel.right(strChannel.length() - strTemplate.length()).toLong();
                break;
            }
            AssertMsgFailed(("Invalid channel {%s}\n", strChannel.toLatin1().constData()));
            break;
        }
        case KStorageBus_Floppy:
        {
            iChannel = 0;
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid bus type %d\n", bus));
            break;
        }
    }

    return iChannel;
}

/* KStorageBus/LONG/LONG => QString: */
QString COMEnumsWrapper::toString(KStorageBus bus, LONG iChannel, LONG iDevice) const
{
    NOREF(iChannel);

    QString strDevice;

    switch (bus)
    {
        case KStorageBus_IDE:
        {
            if (iDevice == 0 || iDevice == 1)
            {
                strDevice = m_storageBusDeviceNames[iDevice];
                break;
            }
            AssertMsgFailed(("Invalid device %d\n", iDevice));
            break;
        }
        case KStorageBus_SATA:
        case KStorageBus_SCSI:
        {
            AssertMsgFailed(("SATA & SCSI have no devices, only channels\n"));
            break;
        }
        case KStorageBus_Floppy:
        {
            AssertMsgBreak(iChannel == 0, ("Invalid channel %d\n", iChannel));
            strDevice = m_storageBusDeviceNames[2].arg(iDevice);
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid bus type %d\n", bus));
            break;
        }
    }

    Assert(!strDevice.isNull());
    return strDevice;
}

/* KStorageBus/LONG/QString => LONG: */
LONG COMEnumsWrapper::toStorageDevice(KStorageBus bus, LONG iChannel, const QString &strDevice) const
{
    NOREF (iChannel);

    LONG iDevice = 0;

    switch (bus)
    {
        case KStorageBus_IDE:
        {
            UILongStringHash::const_iterator it = qFind(m_storageBusDeviceNames.begin(), m_storageBusDeviceNames.end(), strDevice);
            AssertMsgBreak(it != m_storageBusDeviceNames.end(), ("No value for {%s}", strDevice.toLatin1().constData()));
            iDevice = it.key();
            break;
        }
        case KStorageBus_SATA:
        case KStorageBus_SCSI:
        {
            iDevice = 0;
            break;
        }
        case KStorageBus_Floppy:
        {
            AssertMsgBreak(iChannel == 0, ("Invalid channel %d\n", iChannel));
            QString strTemplate = m_storageBusDeviceNames[2].arg("");
            if (strDevice.startsWith(strTemplate))
            {
                iDevice = strDevice.right(strDevice.length() - strTemplate.length()).toLong();
                break;
            }
            AssertMsgFailed(("Invalid device {%s}\n", strDevice.toLatin1().constData()));
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid bus type %d\n", bus));
            break;
        }
    }

    return iDevice;
}

/* KStorageControllerType => QString: */
QString COMEnumsWrapper::toString(KStorageControllerType type) const
{
    AssertMsg(!m_storageControllerTypeNames.value(type).isNull(), ("No text for %d", type));
    return m_storageControllerTypeNames.value(type);
}

/* QString => KStorageControllerType: */
KStorageControllerType COMEnumsWrapper::toControllerType(const QString &strType) const
{
    UIULongStringHash::const_iterator it = qFind(m_storageControllerTypeNames.begin(), m_storageControllerTypeNames.end(), strType);
    AssertMsg(it != m_storageControllerTypeNames.end(), ("No value for {%s}", strType.toLatin1().constData()));
    return KStorageControllerType(it.key());
}

/* KStorageControllerType => KStorageBus: */
KStorageBus COMEnumsWrapper::toStorageBusType(KStorageControllerType type) const
{
    KStorageBus bus = KStorageBus_Null;
    switch (type)
    {
        case KStorageControllerType_Null: bus = KStorageBus_Null; break;
        case KStorageControllerType_PIIX3:
        case KStorageControllerType_PIIX4:
        case KStorageControllerType_ICH6: bus = KStorageBus_IDE; break;
        case KStorageControllerType_IntelAhci: bus = KStorageBus_SATA; break;
        case KStorageControllerType_LsiLogic:
        case KStorageControllerType_BusLogic: bus = KStorageBus_SCSI; break;
        case KStorageControllerType_I82078: bus = KStorageBus_Floppy; break;
        default:
            AssertMsgFailed(("toStorageBusType: %d not handled\n", type)); break;
    }
    return bus;
}

/* KChipsetType => QString: */
QString COMEnumsWrapper::toString(KChipsetType type) const
{
    AssertMsg(!m_pchipsetTypeNames.value(type).isNull(), ("No text for %d", type));
    return m_pchipsetTypeNames.value(type);
}

/* QString => KStorageControllerType: */
KChipsetType COMEnumsWrapper::toChipsetType(const QString &strType) const
{
    UIULongStringHash::const_iterator it = qFind(m_pchipsetTypeNames.begin(), m_pchipsetTypeNames.end(), strType);
    AssertMsg(it != m_pchipsetTypeNames.end(), ("No value for {%s}", strType.toLatin1().constData()));
    return KChipsetType(it.key());
}

/* KNATProtocol => QString: */
QString COMEnumsWrapper::toString(KNATProtocol protocol) const
{
    AssertMsg(!m_natProtocolNames.value(protocol).isNull(), ("No text for %d", protocol));
    return m_natProtocolNames.value(protocol);
}

/* QString => KNATProtocol: */
KNATProtocol COMEnumsWrapper::toNATProtocolType(const QString &strProtocol) const
{
    UIULongStringHash::const_iterator it = qFind(m_natProtocolNames.begin(), m_natProtocolNames.end(), strProtocol);
    AssertMsg(it != m_natProtocolNames.end(), ("No value for {%s}", strProtocol.toLatin1().constData()));
    return KNATProtocol(it.key());
}

/* Constructor: */
COMEnumsWrapper::COMEnumsWrapper()
{
    /* Initialize machine-state colors: */
    m_machineStateColors.insert(KMachineState_Null,                   new QColor(Qt::red));
    m_machineStateColors.insert(KMachineState_PoweredOff,             new QColor(Qt::gray));
    m_machineStateColors.insert(KMachineState_Saved,                  new QColor(Qt::yellow));
    m_machineStateColors.insert(KMachineState_Aborted,                new QColor(Qt::darkRed));
    m_machineStateColors.insert(KMachineState_Teleported,             new QColor(Qt::red));
    m_machineStateColors.insert(KMachineState_Running,                new QColor(Qt::green));
    m_machineStateColors.insert(KMachineState_Paused,                 new QColor(Qt::darkGreen));
    m_machineStateColors.insert(KMachineState_Stuck,                  new QColor(Qt::darkMagenta));
    m_machineStateColors.insert(KMachineState_Teleporting,            new QColor(Qt::blue));
    m_machineStateColors.insert(KMachineState_LiveSnapshotting,       new QColor(Qt::green));
    m_machineStateColors.insert(KMachineState_Starting,               new QColor(Qt::green));
    m_machineStateColors.insert(KMachineState_Stopping,               new QColor(Qt::green));
    m_machineStateColors.insert(KMachineState_Saving,                 new QColor(Qt::green));
    m_machineStateColors.insert(KMachineState_Restoring,              new QColor(Qt::green));
    m_machineStateColors.insert(KMachineState_TeleportingPausedVM,    new QColor(Qt::blue));
    m_machineStateColors.insert(KMachineState_TeleportingIn,          new QColor(Qt::blue));
    m_machineStateColors.insert(KMachineState_RestoringSnapshot,      new QColor(Qt::green));
    m_machineStateColors.insert(KMachineState_DeletingSnapshot,       new QColor(Qt::green));
    m_machineStateColors.insert(KMachineState_DeletingSnapshotOnline, new QColor(Qt::green));
    m_machineStateColors.insert(KMachineState_DeletingSnapshotPaused, new QColor(Qt::darkGreen));
    m_machineStateColors.insert(KMachineState_SettingUp,              new QColor(Qt::green));

    /* Initialize machine state icons: */
    static const struct
    {
        KMachineState state;
        const char *pName;
    }
    kVMStateIcons[] =
    {
        {KMachineState_Null, NULL},
        {KMachineState_PoweredOff, ":/state_powered_off_16px.png"},
        {KMachineState_Saved, ":/state_saved_16px.png"},
        {KMachineState_Aborted, ":/state_aborted_16px.png"},
        {KMachineState_Teleported, ":/state_saved_16px.png"},
        {KMachineState_Running, ":/state_running_16px.png"},
        {KMachineState_Paused, ":/state_paused_16px.png"},
        {KMachineState_Teleporting, ":/state_running_16px.png"},
        {KMachineState_LiveSnapshotting, ":/state_running_16px.png"},
        {KMachineState_Stuck, ":/state_stuck_16px.png"},
        {KMachineState_Starting, ":/state_running_16px.png"},
        {KMachineState_Stopping, ":/state_running_16px.png"},
        {KMachineState_Saving, ":/state_saving_16px.png"},
        {KMachineState_Restoring, ":/state_restoring_16px.png"},
        {KMachineState_TeleportingPausedVM, ":/state_saving_16px.png"},
        {KMachineState_TeleportingIn, ":/state_restoring_16px.png"},
        {KMachineState_RestoringSnapshot, ":/state_discarding_16px.png"},
        {KMachineState_DeletingSnapshot, ":/state_discarding_16px.png"},
        {KMachineState_DeletingSnapshotOnline, ":/state_discarding_16px.png"},
        {KMachineState_DeletingSnapshotPaused, ":/state_discarding_16px.png"},
        {KMachineState_SettingUp, ":/settings_16px.png"},
    };
    for (uint i = 0; i < SIZEOF_ARRAY(kVMStateIcons); ++i)
        m_machineStateIcons.insert(kVMStateIcons[i].state, new QPixmap(kVMStateIcons[i].pName));

    /* Translate finally: */
    retranslateUi();
}

/* Constructor: */
COMEnumsWrapper::~COMEnumsWrapper()
{
    qDeleteAll(m_machineStateIcons);
    qDeleteAll(m_machineStateColors);
}

/* Translate stuff: */
void COMEnumsWrapper::retranslateUi()
{
    /* KMachineState => QString: */
    m_machineStateNames[KMachineState_PoweredOff] =             QApplication::translate("VBoxGlobal", "Powered Off", "MachineState");
    m_machineStateNames[KMachineState_Saved] =                  QApplication::translate("VBoxGlobal", "Saved", "MachineState");
    m_machineStateNames[KMachineState_Teleported] =             QApplication::translate("VBoxGlobal", "Teleported", "MachineState");
    m_machineStateNames[KMachineState_Aborted] =                QApplication::translate("VBoxGlobal", "Aborted", "MachineState");
    m_machineStateNames[KMachineState_Running] =                QApplication::translate("VBoxGlobal", "Running", "MachineState");
    m_machineStateNames[KMachineState_Paused] =                 QApplication::translate("VBoxGlobal", "Paused", "MachineState");
    m_machineStateNames[KMachineState_Stuck] =                  QApplication::translate("VBoxGlobal", "Guru Meditation", "MachineState");
    m_machineStateNames[KMachineState_Teleporting] =            QApplication::translate("VBoxGlobal", "Teleporting", "MachineState");
    m_machineStateNames[KMachineState_LiveSnapshotting] =       QApplication::translate("VBoxGlobal", "Taking Live Snapshot", "MachineState");
    m_machineStateNames[KMachineState_Starting] =               QApplication::translate("VBoxGlobal", "Starting", "MachineState");
    m_machineStateNames[KMachineState_Stopping] =               QApplication::translate("VBoxGlobal", "Stopping", "MachineState");
    m_machineStateNames[KMachineState_Saving] =                 QApplication::translate("VBoxGlobal", "Saving", "MachineState");
    m_machineStateNames[KMachineState_Restoring] =              QApplication::translate("VBoxGlobal", "Restoring", "MachineState");
    m_machineStateNames[KMachineState_TeleportingPausedVM] =    QApplication::translate("VBoxGlobal", "Teleporting Paused VM", "MachineState");
    m_machineStateNames[KMachineState_TeleportingIn] =          QApplication::translate("VBoxGlobal", "Teleporting", "MachineState");
    m_machineStateNames[KMachineState_RestoringSnapshot] =      QApplication::translate("VBoxGlobal", "Restoring Snapshot", "MachineState");
    m_machineStateNames[KMachineState_DeletingSnapshot] =       QApplication::translate("VBoxGlobal", "Deleting Snapshot", "MachineState");
    m_machineStateNames[KMachineState_DeletingSnapshotOnline] = QApplication::translate("VBoxGlobal", "Deleting Snapshot", "MachineState");
    m_machineStateNames[KMachineState_DeletingSnapshotPaused] = QApplication::translate("VBoxGlobal", "Deleting Snapshot", "MachineState");
    m_machineStateNames[KMachineState_SettingUp] =              QApplication::translate("VBoxGlobal", "Setting Up", "MachineState");
    m_machineStateNames[KMachineState_FaultTolerantSyncing] =   QApplication::translate("VBoxGlobal", "Fault Tolerant Syncing", "MachineState");

    /* KSessionState => QString: */
    m_sessionStateNames[KSessionState_Unlocked] =  QApplication::translate("VBoxGlobal", "Unlocked", "SessionState");
    m_sessionStateNames[KSessionState_Locked] =    QApplication::translate("VBoxGlobal", "Locked", "SessionState");
    m_sessionStateNames[KSessionState_Spawning] =  QApplication::translate("VBoxGlobal", "Spawning", "SessionState");
    m_sessionStateNames[KSessionState_Unlocking] = QApplication::translate("VBoxGlobal", "Unlocking", "SessionState");

    /* KDeviceType => QString: */
    m_deviceTypeNames[KDeviceType_Null] =         QApplication::translate("VBoxGlobal", "None", "DeviceType");
    m_deviceTypeNames[KDeviceType_Floppy] =       QApplication::translate("VBoxGlobal", "Floppy", "DeviceType");
    m_deviceTypeNames[KDeviceType_DVD] =          QApplication::translate("VBoxGlobal", "CD/DVD-ROM", "DeviceType");
    m_deviceTypeNames[KDeviceType_HardDisk] =     QApplication::translate("VBoxGlobal", "Hard Disk", "DeviceType");
    m_deviceTypeNames[KDeviceType_Network] =      QApplication::translate("VBoxGlobal", "Network", "DeviceType");
    m_deviceTypeNames[KDeviceType_USB] =          QApplication::translate("VBoxGlobal", "USB", "DeviceType");
    m_deviceTypeNames[KDeviceType_SharedFolder] = QApplication::translate("VBoxGlobal", "Shared Folder", "DeviceType");

    /* KClipboardMode => QString: */
    m_clipboardTypeNames[KClipboardMode_Disabled] =      QApplication::translate("VBoxGlobal", "Disabled", "ClipboardType");
    m_clipboardTypeNames[KClipboardMode_HostToGuest] =   QApplication::translate("VBoxGlobal", "Host To Guest", "ClipboardType");
    m_clipboardTypeNames[KClipboardMode_GuestToHost] =   QApplication::translate("VBoxGlobal", "Guest To Host", "ClipboardType");
    m_clipboardTypeNames[KClipboardMode_Bidirectional] = QApplication::translate("VBoxGlobal", "Bidirectional", "ClipboardType");

    /* KMediumType => QString: */
    m_mediumTypeNames[KMediumType_Normal] =       QApplication::translate("VBoxGlobal", "Normal", "MediumType");
    m_mediumTypeNames[KMediumType_Immutable] =    QApplication::translate("VBoxGlobal", "Immutable", "MediumType");
    m_mediumTypeNames[KMediumType_Writethrough] = QApplication::translate("VBoxGlobal", "Writethrough", "MediumType");
    m_mediumTypeNames[KMediumType_Shareable] =    QApplication::translate("VBoxGlobal", "Shareable", "MediumType");
    m_mediumTypeNames[KMediumType_Readonly] =     QApplication::translate("VBoxGlobal", "Readonly", "MediumType");
    m_mediumTypeNames[KMediumType_MultiAttach] =  QApplication::translate("VBoxGlobal", "Multi-attach", "MediumType");

    /* KMediumVariant => QString: */
    m_mediumVariantNames[0] = QApplication::translate("VBoxGlobal", "Dynamically allocated storage", "MediumVariant");
    m_mediumVariantNames[1] = QApplication::translate("VBoxGlobal", "Dynamically allocated differencing storage", "MediumVariant");
    m_mediumVariantNames[2] = QApplication::translate("VBoxGlobal", "Fixed size storage", "MediumVariant");
    m_mediumVariantNames[3] = QApplication::translate("VBoxGlobal", "Fixed size differencing storage", "MediumVariant");
    m_mediumVariantNames[4] = QApplication::translate("VBoxGlobal", "Dynamically allocated storage split into files of less than 2GB", "MediumVariant");
    m_mediumVariantNames[5] = QApplication::translate("VBoxGlobal", "Dynamically allocated differencing storage split into files of less than 2GB", "MediumVariant");
    m_mediumVariantNames[6] = QApplication::translate("VBoxGlobal", "Fixed size storage split into files of less than 2GB", "MediumVariant");
    m_mediumVariantNames[7] = QApplication::translate("VBoxGlobal", "Fixed size differencing storage split into files of less than 2GB", "MediumVariant");

    /* KNetworkAttachmentType => QString: */
    m_networkAttachmentTypeNames[KNetworkAttachmentType_Null] =     QApplication::translate("VBoxGlobal", "Not attached", "NetworkAttachmentType");
    m_networkAttachmentTypeNames[KNetworkAttachmentType_NAT] =      QApplication::translate("VBoxGlobal", "NAT", "NetworkAttachmentType");
    m_networkAttachmentTypeNames[KNetworkAttachmentType_Bridged] =  QApplication::translate("VBoxGlobal", "Bridged Adapter", "NetworkAttachmentType");
    m_networkAttachmentTypeNames[KNetworkAttachmentType_Internal] = QApplication::translate("VBoxGlobal", "Internal Network", "NetworkAttachmentType");
    m_networkAttachmentTypeNames[KNetworkAttachmentType_HostOnly] = QApplication::translate("VBoxGlobal", "Host-only Adapter", "NetworkAttachmentType");
    m_networkAttachmentTypeNames[KNetworkAttachmentType_Generic] =  QApplication::translate("VBoxGlobal", "Generic Driver", "NetworkAttachmentType");

    /* KNetworkAdapterType => QString: */
    m_networkAdapterTypeNames[KNetworkAdapterType_Am79C970A] = QApplication::translate("VBoxGlobal", "PCnet-PCI II (Am79C970A)", "NetworkAdapterType");
    m_networkAdapterTypeNames[KNetworkAdapterType_Am79C973] =  QApplication::translate("VBoxGlobal", "PCnet-FAST III (Am79C973)", "NetworkAdapterType");
    m_networkAdapterTypeNames[KNetworkAdapterType_I82540EM] =  QApplication::translate("VBoxGlobal", "Intel PRO/1000 MT Desktop (82540EM)", "NetworkAdapterType");
    m_networkAdapterTypeNames[KNetworkAdapterType_I82543GC] =  QApplication::translate("VBoxGlobal", "Intel PRO/1000 T Server (82543GC)", "NetworkAdapterType");
    m_networkAdapterTypeNames[KNetworkAdapterType_I82545EM] =  QApplication::translate("VBoxGlobal", "Intel PRO/1000 MT Server (82545EM)", "NetworkAdapterType");
#ifdef VBOX_WITH_VIRTIO
    m_networkAdapterTypeNames[KNetworkAdapterType_Virtio] =    QApplication::translate("VBoxGlobal", "Paravirtualized Network (virtio-net)", "NetworkAdapterType");
#endif /* VBOX_WITH_VIRTIO */

    /* KNetworkAdapterPromiscModePolicy => QString: */
    m_networkAdapterPromiscModePolicyNames[KNetworkAdapterPromiscModePolicy_Deny] =         QApplication::translate("VBoxGlobal", "Deny", "NetworkAdapterPromiscModePolicy");
    m_networkAdapterPromiscModePolicyNames[KNetworkAdapterPromiscModePolicy_AllowNetwork] = QApplication::translate("VBoxGlobal", "Allow VMs", "NetworkAdapterPromiscModePolicy");
    m_networkAdapterPromiscModePolicyNames[KNetworkAdapterPromiscModePolicy_AllowAll] =     QApplication::translate("VBoxGlobal", "Allow All", "NetworkAdapterPromiscModePolicy");

    /* KPortMode => QString: */
    m_portModeTypeNames[KPortMode_Disconnected] = QApplication::translate("VBoxGlobal", "Disconnected", "PortMode");
    m_portModeTypeNames[KPortMode_HostPipe] =     QApplication::translate("VBoxGlobal", "Host Pipe", "PortMode");
    m_portModeTypeNames[KPortMode_HostDevice] =   QApplication::translate("VBoxGlobal", "Host Device", "PortMode");
    m_portModeTypeNames[KPortMode_RawFile] =      QApplication::translate("VBoxGlobal", "Raw File", "PortMode");

    /* KUSBDeviceState => QString: */
    m_usbDeviceStateNames[KUSBDeviceState_NotSupported] = QApplication::translate("VBoxGlobal", "Not supported", "USBDeviceState");
    m_usbDeviceStateNames[KUSBDeviceState_Unavailable] =  QApplication::translate("VBoxGlobal", "Unavailable", "USBDeviceState");
    m_usbDeviceStateNames[KUSBDeviceState_Busy] =         QApplication::translate("VBoxGlobal", "Busy", "USBDeviceState");
    m_usbDeviceStateNames[KUSBDeviceState_Available] =    QApplication::translate("VBoxGlobal", "Available", "USBDeviceState");
    m_usbDeviceStateNames[KUSBDeviceState_Held] =         QApplication::translate("VBoxGlobal", "Held", "USBDeviceState");
    m_usbDeviceStateNames[KUSBDeviceState_Captured] =     QApplication::translate("VBoxGlobal", "Captured", "USBDeviceState");

    /* KUSBDeviceFilterAction => QString: */
    m_usbDeviceFilterActionNames[KUSBDeviceFilterAction_Ignore] = QApplication::translate("VBoxGlobal", "Ignore", "USBDeviceFilterAction");
    m_usbDeviceFilterActionNames[KUSBDeviceFilterAction_Hold] =   QApplication::translate("VBoxGlobal", "Hold", "USBDeviceFilterAction");

    /* KAudioDriverType => QString: */
    m_audioDriverTypeNames[KAudioDriverType_Null] =        QApplication::translate("VBoxGlobal", "Null Audio Driver", "AudioDriverType");
    m_audioDriverTypeNames[KAudioDriverType_WinMM] =       QApplication::translate("VBoxGlobal", "Windows Multimedia", "AudioDriverType");
    m_audioDriverTypeNames[KAudioDriverType_SolAudio] =    QApplication::translate("VBoxGlobal", "Solaris Audio", "AudioDriverType");
    m_audioDriverTypeNames[KAudioDriverType_OSS] =         QApplication::translate("VBoxGlobal", "OSS Audio Driver", "AudioDriverType");
    m_audioDriverTypeNames[KAudioDriverType_ALSA] =        QApplication::translate("VBoxGlobal", "ALSA Audio Driver", "AudioDriverType");
    m_audioDriverTypeNames[KAudioDriverType_DirectSound] = QApplication::translate("VBoxGlobal", "Windows DirectSound", "AudioDriverType");
    m_audioDriverTypeNames[KAudioDriverType_CoreAudio] =   QApplication::translate("VBoxGlobal", "CoreAudio", "AudioDriverType");
    m_audioDriverTypeNames[KAudioDriverType_Pulse] =       QApplication::translate("VBoxGlobal", "PulseAudio", "AudioDriverType");

    /* KAudioControllerType => QString: */
    m_audioControllerTypeNames[KAudioControllerType_AC97] = QApplication::translate("VBoxGlobal", "ICH AC97", "AudioControllerType");
    m_audioControllerTypeNames[KAudioControllerType_SB16] = QApplication::translate("VBoxGlobal", "SoundBlaster 16", "AudioControllerType");
    m_audioControllerTypeNames[KAudioControllerType_HDA] =  QApplication::translate("VBoxGlobal", "Intel HD Audio", "AudioControllerType");

    /* KAuthType => QString: */
    m_authTypeNames[KAuthType_Null] =     QApplication::translate("VBoxGlobal", "Null", "AuthType");
    m_authTypeNames[KAuthType_External] = QApplication::translate("VBoxGlobal", "External", "AuthType");
    m_authTypeNames[KAuthType_Guest] =    QApplication::translate("VBoxGlobal", "Guest", "AuthType");

    /* KStorageBus => QString: */
    m_storageBusNames[KStorageBus_IDE] =    QApplication::translate("VBoxGlobal", "IDE", "StorageBus");
    m_storageBusNames[KStorageBus_SATA] =   QApplication::translate("VBoxGlobal", "SATA", "StorageBus");
    m_storageBusNames[KStorageBus_SCSI] =   QApplication::translate("VBoxGlobal", "SCSI", "StorageBus");
    m_storageBusNames[KStorageBus_Floppy] = QApplication::translate("VBoxGlobal", "Floppy", "StorageBus");
    m_storageBusNames[KStorageBus_SAS] =    QApplication::translate("VBoxGlobal", "SAS", "StorageBus");

    /* KStorageBus/LONG => QString: */
    m_storageBusChannelNames[0] = QApplication::translate("VBoxGlobal", "Primary", "StorageBusChannel");
    m_storageBusChannelNames[1] = QApplication::translate("VBoxGlobal", "Secondary", "StorageBusChannel");
    m_storageBusChannelNames[2] = QApplication::translate("VBoxGlobal", "Port %1", "StorageBusChannel");

    /* KStorageBus/LONG/LONG => QString: */
    m_storageBusDeviceNames[0] = QApplication::translate("VBoxGlobal", "Master", "StorageBusDevice");
    m_storageBusDeviceNames[1] = QApplication::translate("VBoxGlobal", "Slave", "StorageBusDevice");
    m_storageBusDeviceNames[2] = QApplication::translate("VBoxGlobal", "Device %1", "StorageBusDevice");

    /* KStorageControllerType => QString: */
    m_storageControllerTypeNames[KStorageControllerType_PIIX3] =       QApplication::translate("VBoxGlobal", "PIIX3", "StorageControllerType");
    m_storageControllerTypeNames[KStorageControllerType_PIIX4] =       QApplication::translate("VBoxGlobal", "PIIX4", "StorageControllerType");
    m_storageControllerTypeNames[KStorageControllerType_ICH6] =        QApplication::translate("VBoxGlobal", "ICH6", "StorageControllerType");
    m_storageControllerTypeNames[KStorageControllerType_IntelAhci] =   QApplication::translate("VBoxGlobal", "AHCI", "StorageControllerType");
    m_storageControllerTypeNames[KStorageControllerType_LsiLogic] =    QApplication::translate("VBoxGlobal", "Lsilogic", "StorageControllerType");
    m_storageControllerTypeNames[KStorageControllerType_BusLogic] =    QApplication::translate("VBoxGlobal", "BusLogic", "StorageControllerType");
    m_storageControllerTypeNames[KStorageControllerType_I82078] =      QApplication::translate("VBoxGlobal", "I82078", "StorageControllerType");
    m_storageControllerTypeNames[KStorageControllerType_LsiLogicSas] = QApplication::translate("VBoxGlobal", "LsiLogic SAS", "StorageControllerType");

    /* KChipsetType => QString: */
    m_pchipsetTypeNames[KChipsetType_PIIX3] = QApplication::translate("VBoxGlobal", "PIIX3", "ChipsetType");
    m_pchipsetTypeNames[KChipsetType_ICH9] =  QApplication::translate("VBoxGlobal", "ICH9", "ChipsetType");

    /* KNATProtocol => QString: */
    m_natProtocolNames[KNATProtocol_UDP] = QApplication::translate("VBoxGlobal", "UDP", "NATProtocol");
    m_natProtocolNames[KNATProtocol_TCP] = QApplication::translate("VBoxGlobal", "TCP", "NATProtocol");
}

