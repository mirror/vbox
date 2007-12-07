/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * VBoxBFE main header
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __H_VBOXBFE
#define __H_VBOXBFE

#include <VBox/types.h>

/** Enables the rawr[0|3], patm, and casm options. */
#define VBOXSDL_ADVANCED_OPTIONS

enum
{
 NetworkAdapterCount = 4,
 MaxSharedFolders = 16,
};


/** The user.code field of the SDL_USER_EVENT_TERMINATE event.
 * @{
 */
/** Normal termination. */
#define VBOXSDL_TERM_NORMAL             0
/** Abnormal termination. */
#define VBOXSDL_TERM_ABEND              1
/** @} */

extern VMSTATE     machineState;
extern PVM         pVM;
extern int         gHostKey;
extern int         gHostKeySym;
extern bool        gfAllowFullscreenToggle;
extern const char *g_pszStateFile;
extern const char *g_pszProgressString;
extern unsigned    g_uProgressPercent;

void   startProgressInfo(const char *pszStr);
void   callProgressInfo(PVM pVM, unsigned uPercent, void *pvUser);
void   endProgressInfo();
bool   fActivateHGCM();

#endif // __H_VBOXBFE
