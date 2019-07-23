/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineAttributeSetter namespace declaration.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_globals_UIMachineAttributeSetter_h
#define FEQT_INCLUDED_SRC_globals_UIMachineAttributeSetter_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"

/** Known machine attributes. */
enum MachineAttribute
{
    MachineAttribute_Invalid,
    MachineAttribute_Name,
    MachineAttribute_OSType,
    MachineAttribute_Location,
};

/** Namespace used to assign CMachine attributes on more convenient basis. */
namespace UIMachineAttributeSetter
{
    /** Assigns @a comMachine @a guiAttribute of specified @a enmType. */
    SHARED_LIBRARY_STUFF void setMachineAttribute(const CMachine &comMachine,
                                                  const MachineAttribute &enmType,
                                                  const QVariant &guiAttribute);
}
using namespace UIMachineAttributeSetter /* if header included */;

#endif /* !FEQT_INCLUDED_SRC_globals_UIMachineAttributeSetter_h */
