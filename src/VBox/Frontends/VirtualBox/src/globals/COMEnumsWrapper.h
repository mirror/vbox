/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * COMEnumsWrapper class declaration
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

#ifndef __COMEnumExtension_h__
#define __COMEnumExtension_h__

/* Qt includes: */
#include <QObject>
#include <QHash>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* COM includes: */
#include "COMEnums.h"

/* Defines: */
typedef QHash<ulong, QString> UIULongStringHash;
typedef QHash<long, QString> UILongStringHash;
typedef QHash<ulong, QColor*> UIULongColorHash;
typedef QHash<ulong, QPixmap*> UIULongPixmapHash;

/* Describes conversions between different COM enums and other Qt classes: */
class COMEnumsWrapper : public QIWithRetranslateUI3<QObject>
{
    Q_OBJECT;

public:

    /* Singleton instance: */
    static COMEnumsWrapper* instance() { return m_spInstance; }

    /* Prepare/cleanup: */
    static void prepare();
    static void cleanup();

    /* KMachineState => QString: */
    QString toString(KMachineState state) const;
    /* KMachineState => QColor: */
    const QColor& toColor(KMachineState state) const;
    /* KMachineState => QPixmap: */
    QPixmap toIcon(KMachineState state) const;

    /* KSessionState => QString: */
    QString toString(KSessionState state) const;

    /* KDeviceType => QString: */
    QString toString(KDeviceType type) const;
    /* QString => KDeviceType: */
    KDeviceType toDeviceType(const QString &strType) const;

    /* KClipboardMode => QString: */
    QString toString(KClipboardMode mode) const;
    /* QString => KClipboardMode: */
    KClipboardMode toClipboardModeType(const QString &strMode) const;

    /* KMediumType => QString: */
    QString toString(KMediumType type) const;

    /* KMediumVariant => QString: */
    QString toString(KMediumVariant variant) const;

    /* KNetworkAttachmentType => QString: */
    QString toString(KNetworkAttachmentType type) const;
    /* QString => KNetworkAttachmentType: */
    KNetworkAttachmentType toNetworkAttachmentType(const QString &strType) const;

    /* KNetworkAdapterType => QString: */
    QString toString(KNetworkAdapterType type) const;
    /* QString => KNetworkAdapterType: */
    KNetworkAdapterType toNetworkAdapterType(const QString &strType) const;

    /* KNetworkAdapterPromiscModePolicy => QString: */
    QString toString(KNetworkAdapterPromiscModePolicy policy) const;
    /* QString => KNetworkAdapterPromiscModePolicy: */
    KNetworkAdapterPromiscModePolicy toNetworkAdapterPromiscModePolicyType(const QString &strPolicy) const;

    /* KPortMode => QString: */
    QString toString(KPortMode mode) const;
    /* QString => KPortMode: */
    KPortMode toPortMode(const QString &strMode) const;

    /* KPortMode => QString: */
    QString toString(KUSBDeviceState state) const;

    /* KUSBDeviceFilterAction => QString: */
    QString toString(KUSBDeviceFilterAction action) const;
    /* QString => KUSBDeviceFilterAction: */
    KUSBDeviceFilterAction toUSBDevFilterAction(const QString &strActions) const;

    /* KAudioDriverType => QString: */
    QString toString(KAudioDriverType type) const;
    /* QString => KAudioDriverType: */
    KAudioDriverType toAudioDriverType(const QString &strType) const;

    /* KAudioControllerType => QString: */
    QString toString(KAudioControllerType type) const;
    /* QString => KAudioControllerType: */
    KAudioControllerType toAudioControllerType(const QString &strType) const;

    /* KAuthType => QString: */
    QString toString(KAuthType type) const;
    /* QString => KAuthType: */
    KAuthType toAuthType(const QString &strType) const;

    /* KStorageBus => QString: */
    QString toString(KStorageBus bus) const;
    /* QString => KStorageBus: */
    KStorageBus toStorageBusType(const QString &strBus) const;
    /* KStorageBus/LONG => QString: */
    QString toString(KStorageBus bus, LONG iChannel) const;
    /* KStorageBus/QString => LONG: */
    LONG toStorageChannel(KStorageBus bus, const QString &strChannel) const;
    /* KStorageBus/LONG/LONG => QString: */
    QString toString(KStorageBus bus, LONG iChannel, LONG iDevice) const;
    /* KStorageBus/LONG/QString => LONG: */
    LONG toStorageDevice(KStorageBus bus, LONG iChannel, const QString &strDevice) const;

    /* KStorageControllerType => QString: */
    QString toString(KStorageControllerType type) const;
    /* QString => KStorageControllerType: */
    KStorageControllerType toControllerType(const QString &strType) const;
    /* KStorageControllerType => KStorageBus: */
    KStorageBus toStorageBusType(KStorageControllerType type) const;

    /* KChipsetType => QString: */
    QString toString(KChipsetType type) const;
    /* QString => KStorageControllerType: */
    KChipsetType toChipsetType(const QString &strType) const;

    /* KNATProtocol => QString: */
    QString toString(KNATProtocol protocol) const;
    /* QString => KNATProtocol: */
    KNATProtocol toNATProtocolType(const QString &strProtocol) const;

protected:

    /* Constructor/destructor: */
    COMEnumsWrapper();
    ~COMEnumsWrapper();

    /* Translate stuff: */
    void retranslateUi();

private:

    /* Static instance: */
    static COMEnumsWrapper *m_spInstance;

    /* Translate stuff: */
    UIULongStringHash m_machineStateNames;
    UIULongStringHash m_sessionStateNames;
    UIULongStringHash m_deviceTypeNames;
    UIULongStringHash m_clipboardTypeNames;
    UIULongStringHash m_mediumTypeNames;
    UIULongStringHash m_mediumVariantNames;
    UIULongStringHash m_networkAttachmentTypeNames;
    UIULongStringHash m_networkAdapterTypeNames;
    UIULongStringHash m_networkAdapterPromiscModePolicyNames;
    UIULongStringHash m_portModeTypeNames;
    UIULongStringHash m_usbDeviceStateNames;
    UIULongStringHash m_usbDeviceFilterActionNames;
    UIULongStringHash m_audioDriverTypeNames;
    UIULongStringHash m_audioControllerTypeNames;
    UIULongStringHash m_authTypeNames;
    UIULongStringHash m_storageBusNames;
    UILongStringHash  m_storageBusChannelNames;
    UILongStringHash  m_storageBusDeviceNames;
    UIULongStringHash m_storageControllerTypeNames;
    UIULongStringHash m_pchipsetTypeNames;
    UIULongStringHash m_natProtocolNames;

    /* Other stuff: */
    UIULongColorHash  m_machineStateColors;
    UIULongPixmapHash m_machineStateIcons;
};
#define gCOMenum COMEnumsWrapper::instance()

#endif /* __COMEnumExtension_h__ */

