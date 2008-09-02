/* $Id$ */
/** @file
 * VBoxVMInfoAdditions - Guest Additions information for the host.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */

#ifndef ___VBOXSERVICEVMINFOADDITIONS_H
#define ___VBOXSERVICEVMINFOADDITIONS_H

typedef struct _VBOXFILEINFO
{
    TCHAR* pszFilePath;
    TCHAR* pszFileName;
} VBOXFILEINFO;

int vboxVMInfoAdditions (VBOXINFORMATIONCONTEXT* a_pCtx);

#endif /* !___VBOXSERVICEVMINFOADDITIONS_H */

