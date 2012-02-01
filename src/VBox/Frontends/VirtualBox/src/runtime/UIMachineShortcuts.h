/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineShortcuts class declarations
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

#ifndef __UIMachineShortcuts_h__
#define __UIMachineShortcuts_h__

/* Local includes */
#include "UIShortcuts.h"

class UIMachineShortcuts: public UIShortcuts<UIMachineShortcuts>
{
public:
    enum MachineShortcutType
    {
        SettingsDialogShortcut,
        TakeSnapshotShortcut,
        TakeScreenshotShortcut,
        InformationDialogShortcut,
        MouseIntegrationShortcut,
        TypeCADShortcut,
        TypeCABSShortcut,
        PauseShortcut,
        ResetShortcut,
        ShutdownShortcut,
        CloseShortcut,
        FullscreenModeShortcut,
        SeamlessModeShortcut,
        ScaleModeShortcut,
        GuestAutoresizeShortcut,
        WindowAdjustShortcut,
        NetworkAdaptersDialogShortcut,
        SharedFoldersDialogShortcut,
        VRDPServerShortcut,
        InstallGuestAdditionsShortcut,
#ifdef VBOX_WITH_DEBUGGER_GUI
        StatisticWindowShortcut,
        CommandLineWindowShortcut,
        LoggingShortcut,
#endif /* VBOX_WITH_DEBUGGER_GUI */
        HelpShortcut,
        WebShortcut,
        ResetWarningsShortcut,
        NetworkAccessManager,
#ifdef VBOX_WITH_REGISTRATION
        RegisterShortcut,
#endif /* VBOX_WITH_REGISTRATION */
        UpdateShortcut,
        AboutShortcut,
        PopupMenuShortcut,
        EndShortcutType
    };

private:
    /* Private member vars */
    UIMachineShortcuts();

    friend class UIShortcuts<UIMachineShortcuts>;
};

#define gMS UIMachineShortcuts::instance()

#endif /* !__UIMachineShortcuts_h__ */
