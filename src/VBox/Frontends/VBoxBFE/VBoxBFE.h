/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * VBoxBFE main header
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __H_VBOXBFE
#define __H_VBOXBFE

#include <VBox/types.h>

/** Enables the rawr[0|3], patm, and casm options. */
#define VBOXSDL_ADVANCED_OPTIONS

/** @todo fix this */
#define NetworkAdapterCount    4


/** The user.code field of the SDL_USER_EVENT_TERMINATE event.
 * @{
 */
/** Normal termination. */
#define VBOXSDL_TERM_NORMAL             0
/** Abnormal termination. */
#define VBOXSDL_TERM_ABEND              1
/** @} */

extern VMSTATE machineState;
extern PVM pVM;
extern int gHostKey;
extern int gHostKeySym;
extern bool gfAllowFullscreenToggle;

#endif // __H_VBOXBFE
