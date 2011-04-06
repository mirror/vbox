/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Header with definitions and functions related to settings dialog
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UISettingsDefs_h__
#define __UISettingsDefs_h__

/* VBox includes: */
#include "COMDefs.h"

/* Settings dialog namespace: */
namespace UISettingsDefs
{
    /* Settings dialog types: */
    enum SettingsDialogType
    {
        SettingsDialogType_Wrong,
        SettingsDialogType_Offline,
        SettingsDialogType_Saved,
        SettingsDialogType_Online
    };

    /* Machine state => Settings dialog type converter: */
    SettingsDialogType machineStateToSettingsDialogType(KMachineState machineState);
}

#endif /* __UISettingsDefs_h__ */

