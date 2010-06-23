/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#ifndef ___VBoxVideoCm_h___
#define ___VBoxVideoCm_h___

typedef struct VBOXVIDEOCM_MGR
{
    KEVENT SynchEvent;
    /* session list */
    LIST_ENTRY SessionList;
} VBOXVIDEOCM_MGR, *PVBOXVIDEOCM_MGR;

typedef struct VBOXVIDEOCM_CTX
{
    LIST_ENTRY SessionEntry;
    struct VBOXVIDEOCM_SESSION *pSession;
    uint64_t u64UmData;
} VBOXVIDEOCM_CTX, *PVBOXVIDEOCM_CTX;

void vboxVideoCmCtxInitEmpty(PVBOXVIDEOCM_CTX pContext);

NTSTATUS vboxVideoCmCtxAdd(PVBOXVIDEOCM_MGR pMgr, PVBOXVIDEOCM_CTX pContext, HANDLE hUmEvent, uint64_t u64UmData);
NTSTATUS vboxVideoCmCtxRemove(PVBOXVIDEOCM_MGR pMgr, PVBOXVIDEOCM_CTX pContext);
NTSTATUS vboxVideoCmInit(PVBOXVIDEOCM_MGR pMgr);
NTSTATUS vboxVideoCmTerm(PVBOXVIDEOCM_MGR pMgr);

void* vboxVideoCmCmdCreate(PVBOXVIDEOCM_CTX pContext, uint32_t cbSize);
void* vboxVideoCmCmdReinitForContext(void *pvCmd, PVBOXVIDEOCM_CTX pContext);
void vboxVideoCmCmdRetain(void *pvCmd);
void vboxVideoCmCmdRelease(void *pvCmd);
#define VBOXVIDEOCM_SUBMITSIZE_DEFAULT (~0UL)
void vboxVideoCmCmdSubmit(void *pvCmd, uint32_t cbSize);

NTSTATUS vboxVideoCmEscape(PVBOXVIDEOCM_CTX pContext, PVBOXWDDM_GETVBOXVIDEOCMCMD_HDR pvCmd, uint32_t cbCmd);

#endif /* #ifndef ___VBoxVideoCm_h___ */
