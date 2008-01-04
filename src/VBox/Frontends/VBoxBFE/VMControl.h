/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * VBoxBFE VM control routines header
 *
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

#ifndef ____H_VMCONTROL
#define ____H_VMCONTROL

int  VMCtrlToggleFullscreen(void);
int  VMCtrlPause(void);
int  VMCtrlResume(void);
int  VMCtrlReset(void);
int  VMCtrlACPIPowerButton(void);
int  VMCtrlACPISleepButton(void);
int  VMCtrlSave(void (*pfnQuit)(void));

#endif
