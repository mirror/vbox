/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMedium related declarations
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

#ifndef __UIMediumDefs_h__
#define __UIMediumDefs_h__

/* COM includes: */
#include "COMEnums.h"

/* UIMediumDefs namespace: */
namespace UIMediumDefs
{
    /* UIMedium types: */
    enum UIMediumType
    {
        UIMediumType_Invalid,
        UIMediumType_HardDisk,
        UIMediumType_DVD,
        UIMediumType_Floppy,
        UIMediumType_All
    };

    /* Convert global medium type (KDeviceType) to local (UIMediumType): */
    UIMediumType mediumTypeToLocal(KDeviceType globalType);

    /* Convert local medium type (UIMediumType) to global (KDeviceType): */
    KDeviceType mediumTypeToGlobal(UIMediumType localType);
}

/* Using this namespace globally: */
using namespace UIMediumDefs;

/* Let QMetaType know about UIMediumType: */
Q_DECLARE_METATYPE(UIMediumType);

#endif /* __UIMediumDefs_h__ */

