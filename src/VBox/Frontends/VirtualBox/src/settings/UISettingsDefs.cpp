/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UISettingsDefs implementation
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

/* VBox includes: */
#include "UISettingsDefs.h"

/* Using declarations: */
using namespace UISettingsDefs;

/* Machine state => Settings dialog type converter: */
SettingsDialogType UISettingsDefs::determineSettingsDialogType(KSessionState sessionState, KMachineState machineState)
{
    SettingsDialogType result = SettingsDialogType_Wrong;
    switch (machineState)
    {
        case KMachineState_PoweredOff:
        case KMachineState_Teleported:
        case KMachineState_Aborted:
            result = sessionState == KSessionState_Unlocked ? SettingsDialogType_Offline :
                                                              SettingsDialogType_Online;
            break;
        case KMachineState_Saved:
            result = SettingsDialogType_Saved;
            break;
        case KMachineState_Running:
        case KMachineState_Paused:
            result = SettingsDialogType_Online;
            break;
        default:
            break;
    }
    return result;
}

