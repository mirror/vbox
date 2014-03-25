/** @file
 * VBox Qt GUI - UIMedium related declarations.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIMediumDefs_h___
#define ___UIMediumDefs_h___

/* COM includes: */
#include "COMEnums.h"

/** UIMediumDefs namespace. */
namespace UIMediumDefs
{
    /** UIMedium types. */
    enum UIMediumType
    {
        UIMediumType_Invalid,
        UIMediumType_HardDisk,
        UIMediumType_DVD,
        UIMediumType_Floppy,
        UIMediumType_All
    };

    /** Converts global medium type (KDeviceType) to local (UIMediumType). */
    UIMediumType mediumTypeToLocal(KDeviceType globalType);

    /** Convert local medium type (UIMediumType) to global (KDeviceType). */
    KDeviceType mediumTypeToGlobal(UIMediumType localType);
}
/* Using this namespace globally: */
using namespace UIMediumDefs;

/** Medium-target. */
struct UIMediumTarget
{
    /** Medium-target types. */
    enum UIMediumTargetType { UIMediumTargetType_WithID, UIMediumTargetType_WithLocation };

    /** Default medium-target constructor. */
    UIMediumTarget()
        : type(UIMediumTargetType_WithID)
        , name(QString()), port(0), device(0), mediumType(UIMediumType_Invalid)
        , data(QString())
    {}

    /** Unmount medium-target constructor. */
    UIMediumTarget(const QString &strName, LONG iPort, LONG iDevice)
        : type(UIMediumTargetType_WithID)
        , name(strName), port(iPort), device(iDevice), mediumType(UIMediumType_Invalid)
        , data(QString())
    {}

    /** Open medium-target constructor. */
    UIMediumTarget(const QString &strName, LONG iPort, LONG iDevice, UIMediumType otherMediumType)
        : type(UIMediumTargetType_WithID)
        , name(strName), port(iPort), device(iDevice), mediumType(otherMediumType)
        , data(QString())
    {}

    /** Predefined medium-target constructor. */
    UIMediumTarget(const QString &strName, LONG iPort, LONG iDevice, const QString &strID)
        : type(UIMediumTargetType_WithID)
        , name(strName), port(iPort), device(iDevice), mediumType(UIMediumType_Invalid)
        , data(strID)
    {}

    /** Recent medium-target constructor. */
    UIMediumTarget(const QString &strName, LONG iPort, LONG iDevice, UIMediumType otherMediumType, const QString &strLocation)
        : type(UIMediumTargetType_WithLocation)
        , name(strName), port(iPort), device(iDevice), mediumType(otherMediumType)
        , data(strLocation)
    {}

    /** Determines medium-target type. */
    UIMediumTargetType type;

    /** Determines controller name. */
    QString name;
    /** Determines controller port. */
    LONG port;
    /** Determines controller device. */
    LONG device;

    /** Determines medium-target medium-type. */
    UIMediumType mediumType;

    /** Depending on medium-target type holds <i>ID</i> or <i>location</i>. */
    QString data;
};

/* Let QMetaType know about our types: */
Q_DECLARE_METATYPE(UIMediumType);
Q_DECLARE_METATYPE(UIMediumTarget);

#endif /* !___UIMediumDefs_h___ */
