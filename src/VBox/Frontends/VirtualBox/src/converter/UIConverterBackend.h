/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIConverterBackend declaration
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

#ifndef __UIConverterBackend_h__
#define __UIConverterBackend_h__

/* Qt includes: */
#include <QString>
#include <QColor>
#include <QPixmap>

/* GUI includes: */
#include "UIDefs.h"

/* Determines if 'Object of type X' can be converted to object of other type.
 * This function always returns 'false' until re-determined for specific object type. */
template<class X> bool canConvert() { return false; }

/* Converts passed 'Object X' to QColor.
 * This function returns null QColor for any object type until re-determined for specific one. */
template<class X> QColor toColor(const X & /* xobject */) { Assert(0); return QColor(); }

/* Converts passed 'Object X' to QPixmap.
 * This function returns null QPixmap for any object type until re-determined for specific one. */
template<class X> QPixmap toPixmap(const X & /* xobject */) { Assert(0); return QPixmap(); }

/* Converts passed 'Object of type X' to QString.
 * This function returns null QString for any object type until re-determined for specific one. */
template<class X> QString toString(const X & /* xobject */) { Assert(0); return QString(); }
/* Converts passed QString to 'Object of type X'.
 * This function returns default constructed object for any object type until re-determined for specific one. */
template<class X> X fromString(const QString & /* strData */) { Assert(0); return X(); }

/* Declare global canConvert specializations: */
template<> bool canConvert<StorageSlot>();

/* Declare COM canConvert specializations: */
template<> bool canConvert<KMachineState>();
template<> bool canConvert<KSessionState>();
template<> bool canConvert<KDeviceType>();
template<> bool canConvert<KClipboardMode>();
template<> bool canConvert<KMediumType>();
template<> bool canConvert<KMediumVariant>();
template<> bool canConvert<KNetworkAttachmentType>();
template<> bool canConvert<KNetworkAdapterType>();
template<> bool canConvert<KNetworkAdapterPromiscModePolicy>();
template<> bool canConvert<KPortMode>();
template<> bool canConvert<KUSBDeviceState>();
template<> bool canConvert<KUSBDeviceFilterAction>();
template<> bool canConvert<KAudioDriverType>();
template<> bool canConvert<KAudioControllerType>();
template<> bool canConvert<KAuthType>();
template<> bool canConvert<KStorageBus>();
template<> bool canConvert<KStorageControllerType>();
template<> bool canConvert<KChipsetType>();
template<> bool canConvert<KNATProtocol>();

/* Declare global conversion specializations: */
template<> QString toString(const StorageSlot &storageSlot);
template<> StorageSlot fromString<StorageSlot>(const QString &strStorageSlot);

/* Declare COM conversion specializations: */
template<> QColor toColor(const KMachineState &state);
template<> QPixmap toPixmap(const KMachineState &state);
template<> QString toString(const KMachineState &state);
template<> QString toString(const KSessionState &state);
template<> QString toString(const KDeviceType &type);
template<> QString toString(const KClipboardMode &mode);
template<> QString toString(const KMediumType &type);
template<> QString toString(const KMediumVariant &variant);
template<> QString toString(const KNetworkAttachmentType &type);
template<> QString toString(const KNetworkAdapterType &type);
template<> QString toString(const KNetworkAdapterPromiscModePolicy &policy);
template<> QString toString(const KPortMode &mode);
template<> QString toString(const KUSBDeviceState &state);
template<> QString toString(const KUSBDeviceFilterAction &action);
template<> QString toString(const KAudioDriverType &type);
template<> QString toString(const KAudioControllerType &type);
template<> QString toString(const KAuthType &type);
template<> QString toString(const KStorageBus &bus);
template<> QString toString(const KStorageControllerType &type);
template<> QString toString(const KChipsetType &type);
template<> QString toString(const KNATProtocol &protocol);
template<> KPortMode fromString<KPortMode>(const QString &strMode);
template<> KUSBDeviceFilterAction fromString<KUSBDeviceFilterAction>(const QString &strAction);
template<> KAudioDriverType fromString<KAudioDriverType>(const QString &strType);
template<> KAudioControllerType fromString<KAudioControllerType>(const QString &strType);
template<> KAuthType fromString<KAuthType>(const QString &strType);
template<> KStorageControllerType fromString<KStorageControllerType>(const QString &strType);

#endif /* __UIConverterBackend_h__ */

