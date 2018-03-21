/* $Id$ */
/** @file
 * VBox Qt GUI - Global definitions.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIDefs_h___
#define ___UIDefs_h___

/* Qt includes: */
#include <QEvent>
#include <QStringList>

/* COM includes: */
#include "COMEnums.h"

/* Define GUI log group: */
// WORKAROUND:
// This define should go *before* VBox/log.h include!
#ifndef VBOX_WITH_PRECOMPILED_HEADERS
# define LOG_GROUP LOG_GROUP_GUI
#endif

/* Other VBox includes: */
#include <VBox/log.h>
#include <VBox/com/defs.h>

/* Defines: */
#ifdef DEBUG
# define AssertWrapperOk(w)      \
    AssertMsg (w.isOk(), (#w " is not okay (RC=0x%08X)", w.lastRC()))
# define AssertWrapperOkMsg(w, m)      \
    AssertMsg (w.isOk(), (#w ": " m " (RC=0x%08X)", w.lastRC()))
#else /* !DEBUG */
# define AssertWrapperOk(w)          do {} while (0)
# define AssertWrapperOkMsg(w, m)    do {} while (0)
#endif /* !DEBUG */

#ifndef SIZEOF_ARRAY
# define SIZEOF_ARRAY(a) (sizeof(a) / sizeof(a[0]))
#endif


/** Global namespace. */
namespace UIDefs
{
    /** Additional Qt event types. */
    enum UIEventType
    {
        ActivateActionEventType = QEvent::User + 101,
#ifdef VBOX_WS_MAC
        ShowWindowEventType,
#endif
#ifdef VBOX_GUI_USE_QGL
        VHWACommandProcessType,
#endif
    };

    /** Size formatting types. */
    enum FormatSize
    {
        FormatSize_Round,
        FormatSize_RoundDown,
        FormatSize_RoundUp
    };

    /** Default guest additions image name. */
    extern const char* GUI_GuestAdditionsName;
    /** Default extension pack name. */
    extern const char* GUI_ExtPackName;

    /** Allowed VBox file extensions. */
    extern QStringList VBoxFileExts;
    /** Allowed VBox Extension Pack file extensions. */
    extern QStringList VBoxExtPackFileExts;
    /** Allowed OVF file extensions. */
    extern QStringList OVFFileExts;
    /** Allowed OPC file extensions. */
    extern QStringList OPCFileExts;
}
using namespace UIDefs /* if header included */;


#ifdef VBOX_WS_MAC
/** Known macOS releases. */
enum MacOSXRelease
{
    MacOSXRelease_Old,
    MacOSXRelease_SnowLeopard,
    MacOSXRelease_Lion,
    MacOSXRelease_MountainLion,
    MacOSXRelease_Mavericks,
    MacOSXRelease_Yosemite,
    MacOSXRelease_ElCapitan,
    MacOSXRelease_New,
};
#endif /* VBOX_WS_MAC */


/** Size suffixes. */
enum SizeSuffix
{
    SizeSuffix_Byte = 0,
    SizeSuffix_KiloByte,
    SizeSuffix_MegaByte,
    SizeSuffix_GigaByte,
    SizeSuffix_TeraByte,
    SizeSuffix_PetaByte,
    SizeSuffix_Max
};


/** Storage-slot struct. */
struct StorageSlot
{
    StorageSlot() : bus(KStorageBus_Null), port(0), device(0) {}
    StorageSlot(const StorageSlot &other) : bus(other.bus), port(other.port), device(other.device) {}
    StorageSlot(KStorageBus otherBus, LONG iPort, LONG iDevice) : bus(otherBus), port(iPort), device(iDevice) {}
    StorageSlot& operator=(const StorageSlot &other) { bus = other.bus; port = other.port; device = other.device; return *this; }
    bool operator==(const StorageSlot &other) const { return bus == other.bus && port == other.port && device == other.device; }
    bool operator!=(const StorageSlot &other) const { return bus != other.bus || port != other.port || device != other.device; }
    bool operator<(const StorageSlot &other) const { return (bus <  other.bus) ||
                                                            (bus == other.bus && port <  other.port) ||
                                                            (bus == other.bus && port == other.port && device < other.device); }
    bool operator>(const StorageSlot &other) const { return (bus >  other.bus) ||
                                                            (bus == other.bus && port >  other.port) ||
                                                            (bus == other.bus && port == other.port && device > other.device); }
    bool isNull() const { return bus == KStorageBus_Null; }
    KStorageBus bus; LONG port; LONG device;
};
Q_DECLARE_METATYPE(StorageSlot);


/** Storage-slot struct extension with exact controller name. */
struct ExactStorageSlot : public StorageSlot
{
    ExactStorageSlot(const QString &strController,
                     KStorageBus enmBus, LONG iPort, LONG iDevice)
        : StorageSlot(enmBus, iPort, iDevice)
        , controller(strController)
    {}
    QString controller;
};


#endif /* !___UIDefs_h___ */

