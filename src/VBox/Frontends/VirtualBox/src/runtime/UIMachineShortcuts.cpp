/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineShortcuts class definitions
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

/* Local includes */
#include "UIMachineShortcuts.h"

template <> UIMachineShortcuts* UIShortcuts<UIMachineShortcuts>::m_pInstance = 0;

UIMachineShortcuts::UIMachineShortcuts()
{
    /* Defaults */
    m_Shortcuts[SettingsDialogShortcut]        = UIKeySequence("SettingsDialog",        "S");
    m_Shortcuts[TakeSnapshotShortcut]          = UIKeySequence("TakeSnapshot",          "T");
    m_Shortcuts[TakeScreenshotShortcut]        = UIKeySequence("TakeScreenshot",        "E");
    m_Shortcuts[InformationDialogShortcut]     = UIKeySequence("InformationDialog",     "N");
    m_Shortcuts[MouseIntegrationShortcut]      = UIKeySequence("MouseIntegration" ,     "I");
    m_Shortcuts[TypeCADShortcut]               = UIKeySequence("TypeCAD",               "Del");
    m_Shortcuts[TypeCABSShortcut]              = UIKeySequence("TypeCABS",              "Backspace");
    m_Shortcuts[PauseShortcut]                 = UIKeySequence("Pause",                 "P");
    m_Shortcuts[ResetShortcut]                 = UIKeySequence("Reset",                 "R");
#ifdef Q_WS_MAC
    m_Shortcuts[ShutdownShortcut]              = UIKeySequence("Shutdown",              "U");
#else /* Q_WS_MAC */
    m_Shortcuts[ShutdownShortcut]              = UIKeySequence("Shutdown",              "H");
#endif /* Q_WS_MAC */
    m_Shortcuts[CloseShortcut]                 = UIKeySequence("Close",                 "Q");
    m_Shortcuts[FullscreenModeShortcut]        = UIKeySequence("FullscreenMode",        "F");
    m_Shortcuts[SeamlessModeShortcut]          = UIKeySequence("SeamlessMode",          "L");
    m_Shortcuts[ScaleModeShortcut]             = UIKeySequence("ScaleMode",             "C");
    m_Shortcuts[GuestAutoresizeShortcut]       = UIKeySequence("GuestAutoresize",       "G");
    m_Shortcuts[WindowAdjustShortcut]          = UIKeySequence("WindowAdjust",          "A");
    m_Shortcuts[NetworkAdaptersDialogShortcut] = UIKeySequence("NetworkAdaptersDialog");
    m_Shortcuts[SharedFoldersDialogShortcut]   = UIKeySequence("SharedFoldersDialog");
    m_Shortcuts[VRDPServerShortcut]            = UIKeySequence("VRDPServer");
    m_Shortcuts[InstallGuestAdditionsShortcut] = UIKeySequence("InstallGuestAdditions", "D");
#ifdef VBOX_WITH_DEBUGGER_GUI
    m_Shortcuts[StatisticWindowShortcut]       = UIKeySequence("StatisticWindow");
    m_Shortcuts[CommandLineWindowShortcut]     = UIKeySequence("CommandLineWindow");
    m_Shortcuts[LoggingShortcut]               = UIKeySequence("Logging");
#endif /* VBOX_WITH_DEBUGGER_GUI */
    m_Shortcuts[HelpShortcut]                  = UIKeySequence("Help");
    m_Shortcuts[WebShortcut]                   = UIKeySequence("Web");
    m_Shortcuts[ResetWarningsShortcut]         = UIKeySequence("ResetWarnings");
#ifdef VBOX_WITH_REGISTRATION
    m_Shortcuts[RegisterShortcut]              = UIKeySequence("Register");
#endif /* VBOX_WITH_REGISTRATION */
    m_Shortcuts[UpdateShortcut]                = UIKeySequence("Update");
    m_Shortcuts[AboutShortcut]                 = UIKeySequence("About");
    m_Shortcuts[PopupMenuShortcut]             = UIKeySequence("PopupMenu",             "Home");
    /* Overwrite the key sequences with the one in extra data. */
    loadExtraData(VBoxDefs::GUI_Input_MachineShortcuts, EndShortcutType);
}

