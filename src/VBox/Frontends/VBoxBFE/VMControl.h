/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * VBoxBFE VM control routines header
 *
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * innotek GmbH confidential
 * All rights reserved
 */

#ifndef ____H_VMCONTROL
#define ____H_VMCONTROL

int  VMCtrlToggleFullscreen(void);
int  VMCtrlPause(void);
int  VMCtrlResume(void);
int  VMCtrlReset(void);
int  VMCtrlACPIButton(void);
int  VMCtrlSave(void (*pfnQuit)(void));

#endif
