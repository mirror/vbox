/** @file
 * Common GUI Library - Darwin Cursor routines.
 *
 * @todo Move this up somewhere so that the two SDL GUIs can use this code too.
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


#ifndef __DarwinCursor_h__
#define __DarwinCursor_h__

#include <iprt/cdefs.h>

__BEGIN_DECLS

/**
 * A Darwin cursor.
 */
typedef struct DARWINCURSOR
{
    /** The QuickTime cursor name. */
    char    szName[128];
} DARWINCURSOR;
/** Pointer to a darwin cursor. */
typedef DARWINCURSOR *PDARWINCURSOR;


int DarwinCursorCreate(unsigned cx, unsigned cy, unsigned xHot, unsigned yHot, bool fHasAlpha,
                       const void *pvAndMask, const void *pvShape, PDARWINCURSOR pCursor);
int DarwinCursorDestroy(PDARWINCURSOR pCursor);
int DarwinCursorSet(PDARWINCURSOR pCursor);
int DarwinCursorHide(void);
int DarwinCursorShow(void);
void DarwinCursorClearHandle(PDARWINCURSOR pCursor);
bool DarwinCursorIsNull(PDARWINCURSOR pCursor);
//void DarwinCursorDumpFormatInfo(uint32_t Format);

__END_DECLS

#endif


