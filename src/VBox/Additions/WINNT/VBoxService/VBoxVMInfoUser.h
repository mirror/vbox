/* $Id$ */
/** @file
 * VBoxVMInfoUser - User information for the host.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */

#ifndef ___VBOXSERVICEVMINFOUSER_H
#define ___VBOXSERVICEVMINFOUSER_H

typedef struct _VBOXUSERINFO
{
    TCHAR szUser[_MAX_PATH];
} VBOXUSERINFO;

int vboxVMInfoUser (VBOXINFORMATIONCONTEXT* a_pCtx);

#endif /* !___VBOXSERVICEVMINFOUSER_H */


