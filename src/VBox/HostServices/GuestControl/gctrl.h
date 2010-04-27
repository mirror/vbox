/** @file
 * Guest Control Service: Internal function used by service, Main and testcase.
 */

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

#ifndef VBOX_GCTRL_H
#define VBOX_GCTRL_H

#include <VBox/err.h>
#include <VBox/hgcmsvc.h>

namespace guestControl {

/** @todo write docs! */
int gctrlPrepareExecArgv(char *pszArgs, void **ppvList, uint32_t *pcbList, uint32_t *pcArgs);
/** @todo write docs! */
int gctrlAddToExecEnvv(const char *pszEnv, void **ppvList, uint32_t *pcbList, uint32_t *pcEnv);
/** @todo write docs! */
#if 0
int gctrlAllocateExecBlock(PVBOXGUESTCTRLEXECBLOCK *ppBlock,
                           const char *pszCmd, uint32_t fFlags,
                           uint32_t cArgs,    const char * const *papszArgs,
                           uint32_t cEnvVars, const char * const *papszEnv,
                           const char *pszStdIn, const char *pszStdOut, const char *pszStdErr,
                           const char *pszUsername, const char *pszPassword, RTMSINTERVAL cMillies);
/** @todo write docs! */
int gctrlFreeExecBlock(PVBOXGUESTCTRLEXECBLOCK pBlock);
/** @todo write docs! */
int gctrlPrepareHostCmdExec(PVBOXHGCMSVCPARM *ppaParms, uint32_t *pcParms,
                            PVBOXGUESTCTRLEXECBLOCK pBlock);
/** @todo write docs! */
void gctrlFreeHostCmd(PVBOXHGCMSVCPARM paParms);
#endif
}

#endif /* !VBOX_GCTRL_H */

