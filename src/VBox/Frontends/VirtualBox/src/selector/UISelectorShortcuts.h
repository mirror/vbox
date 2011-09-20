/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UISelectorShortcuts class declarations
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

#ifndef __UISelectorShortcuts_h__
#define __UISelectorShortcuts_h__

/* Local includes */
#include "UIShortcuts.h"

class UISelectorShortcuts: public UIShortcuts<UISelectorShortcuts>
{
public:
    enum SelectorShortcutType
    {
        VirtualMediaManagerShortcut,
        ImportApplianceShortcut,
        ExportApplianceShortcut,
        PreferencesShortcut,
        ExitShortcut,
        NewVMShortcut,
        AddVMShortcut,
        SettingsVMShortcut,
        CloneVMShortcut,
        RemoveVMShortcut,
        StartVMShortcut,
        DiscardVMShortcut,
        PauseVMShortcut,
        ResetVMShortcut,
        ACPIShutdownVMShortcut,
        PowerOffVMShortcut,
        RefreshVMShortcut,
        ShowVMLogShortcut,
        ShowVMInFileManagerShortcut,
        CreateVMAliasShortcut,
        ResortVMList,
        HelpShortcut,
        WebShortcut,
        ResetWarningsShortcut,
#ifdef VBOX_WITH_REGISTRATION
        RegisterShortcut,
#endif /* VBOX_WITH_REGISTRATION */
        UpdateShortcut,
        AboutShortcut,
        EndShortcutType
    };

private:
    /* Private member vars */
    UISelectorShortcuts();
    friend class UIShortcuts<UISelectorShortcuts>;
};

#define gSS UISelectorShortcuts::instance()

#endif /* !__UISelectorShortcuts_h__ */

