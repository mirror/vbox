/** @file
 *
 * Shared Clipboard:
 * Mac OS X host implementation.
 */

/*
 * Copyright (C) 2008 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __DARWIN_PASTEBOARD_H
#define __DARWIN_PASTEBOARD_H

typedef struct OpaquePasteboardRef;
typedef struct OpaquePasteboardRef *PasteboardRef;

int initPasteboard (PasteboardRef &pPasteboard);
void destroyPasteboard (PasteboardRef &pPasteboard);

int queryPasteboardFormats (PasteboardRef pPasteboard, uint32_t &u32Formats);
int readFromPasteboard (PasteboardRef pPasteboard, uint32_t u32Format, void *pv, uint32_t cb, uint32_t *pcbActual);
int writeToPasteboard (PasteboardRef pPasteboard, void *pv, uint32_t cb, uint32_t u32Format);

#endif /* __DARWIN-PASTEBOARD_H */

