/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UISelectorShortcuts class definitions
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
#include "UISelectorShortcuts.h"

template <> UISelectorShortcuts* UIShortcuts<UISelectorShortcuts>::m_pInstance = 0;

UISelectorShortcuts::UISelectorShortcuts()
{
    /* Defaults */
    m_Shortcuts[VirtualMediaManagerShortcut] = UIKeySequence("VirtualMediaManager", "Ctrl+D");
    m_Shortcuts[ImportApplianceShortcut]     = UIKeySequence("ImportAppliance",     "Ctrl+I");
    m_Shortcuts[ExportApplianceShortcut]     = UIKeySequence("ExportAppliance",     "Ctrl+E");
    m_Shortcuts[PreferencesShortcut]         = UIKeySequence("Preferences",         "Ctrl+G");
    m_Shortcuts[ExitShortcut]                = UIKeySequence("Exit",                "Ctrl+Q");
    m_Shortcuts[NewVMShortcut]               = UIKeySequence("NewVM",               "Ctrl+N");
    m_Shortcuts[AddVMShortcut]               = UIKeySequence("AddVM",               "Ctrl+A");
    m_Shortcuts[AddVMGroupShortcut]          = UIKeySequence("AddVMGroup",          "Ctrl+U");
    m_Shortcuts[SettingsVMShortcut]          = UIKeySequence("SettingsVM",          "Ctrl+S");
    m_Shortcuts[CloneVMShortcut]             = UIKeySequence("CloneVM",             "Ctrl+O");
    m_Shortcuts[BreakVMGroupShortcut]        = UIKeySequence("BreakVMGroup",        "Ctrl+B");
    m_Shortcuts[RemoveVMShortcut]            = UIKeySequence("RemoveVM",            "Ctrl+R");
    m_Shortcuts[RenameVMGroupShortcut]       = UIKeySequence("RenameVMGroup",       "Ctrl+M");
    m_Shortcuts[StartVMShortcut]             = UIKeySequence("StartVM");
    m_Shortcuts[DiscardVMShortcut]           = UIKeySequence("DiscardVM",           "Ctrl+J");
    m_Shortcuts[PauseVMShortcut]             = UIKeySequence("PauseVM",             "Ctrl+P");
    m_Shortcuts[ResetVMShortcut]             = UIKeySequence("ResetVM",             "Ctrl+T");
    m_Shortcuts[ACPIShutdownVMShortcut]      = UIKeySequence("ACPIShutdownVM",      "Ctrl+H");
    m_Shortcuts[PowerOffVMShortcut]          = UIKeySequence("PowerOffVM",          "Ctrl+F");
    m_Shortcuts[RefreshVMShortcut]           = UIKeySequence("RefreshVM");
    m_Shortcuts[ShowVMLogShortcut]           = UIKeySequence("ShowVMLog",           "Ctrl+L");
    m_Shortcuts[ShowVMInFileManagerShortcut] = UIKeySequence("ShowVMInFileManager");
    m_Shortcuts[CreateVMAliasShortcut]       = UIKeySequence("CreateVMAlias");
    m_Shortcuts[SortParentGroup]             = UIKeySequence("SortParentGroup");
    m_Shortcuts[SortGroup]                   = UIKeySequence("SortGroup");
    m_Shortcuts[HelpShortcut]                = UIKeySequence("Help",                QKeySequence::HelpContents);
    m_Shortcuts[WebShortcut]                 = UIKeySequence("Web");
    m_Shortcuts[ResetWarningsShortcut]       = UIKeySequence("ResetWarnings");
    m_Shortcuts[NetworkAccessManager]        = UIKeySequence("NetworkAccessManager");
#ifdef VBOX_WITH_REGISTRATION
    m_Shortcuts[RegisterShortcut]            = UIKeySequence("Register");
#endif /* VBOX_WITH_REGISTRATION */
    m_Shortcuts[UpdateShortcut]              = UIKeySequence("Update");
    m_Shortcuts[AboutShortcut]               = UIKeySequence("About");
    /* Get a list of overwritten keys */
    loadExtraData(GUI_Input_SelectorShortcuts, EndShortcutType);
}

