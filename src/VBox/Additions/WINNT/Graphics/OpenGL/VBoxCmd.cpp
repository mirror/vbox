/** @file
 *
 * VBoxDisp -- Windows Guest OpenGL ICD
 *
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */
#define VBOX_OGL_WITH_CMD_STRINGS
#include "VBoxOGL.h"
#include <VBox/version.h>
#include <stdarg.h>
#include <stdio.h>


/**
 * Queue a new OpenGL command
 *
 * @param   enmOp       OpenGL command op
 * @param   cParams     Number of parameters
 * @param   cbParams    Memory needed for parameters
 */
void VBoxCmdStart(uint32_t enmOp, uint32_t cParams, uint32_t cbParams)
{
    PVBOX_OGL_THREAD_CTX pCtx = VBoxOGLGetThreadCtx();
    PVBOX_OGL_CMD        pCmd = (PVBOX_OGL_CMD)pCtx->pCurrentCmd;

    DbgPrintf2(("Start %s cParams=%d cbParams=%d\n", pszVBoxOGLCmd[enmOp], cParams, cbParams));

    if (pCtx->pCurrentCmd + cParams*VBOX_OGL_CMD_ALIGN + cbParams + sizeof(VBOX_OGL_CMD) >= pCtx->pCmdBufferEnd)
    {
        DbgPrintf(("VBoxCmdStart -> cmd queue full -> flush!\n"));
        VBoxOGLFlush();
        pCmd = (PVBOX_OGL_CMD)pCtx->pCurrentCmd;
    }
    Assert(pCtx->pCurrentCmd + cbParams + sizeof(VBOX_OGL_CMD) < pCtx->pCmdBufferEnd);

#ifdef VBOX_OGL_CMD_STRICT
    pCmd->Magic       = VBOX_OGL_CMD_MAGIC;
#endif
    pCmd->enmOp       = enmOp;
    pCmd->cbCmd       = sizeof(VBOX_OGL_CMD);
    pCmd->cParams     = cParams;
    return;
}

/**
 * Add a parameter (fixed size) to the currently queued OpenGL command
 *
 * @param   pParam      Parameter ptr
 * @param   cbParam     Parameter value size
 */
void VBoxCmdSaveParameter(uint8_t *pParam, uint32_t cbParam)
{
    PVBOX_OGL_THREAD_CTX pCtx = VBoxOGLGetThreadCtx();
    PVBOX_OGL_CMD        pCmd = (PVBOX_OGL_CMD)pCtx->pCurrentCmd;
    uint8_t             *pCurrentCmdBlock = (uint8_t *)pCmd + pCmd->cbCmd;

    Assert(pCurrentCmdBlock + cbParam < pCtx->pCmdBufferEnd);

#ifdef DEBUG
    switch(cbParam)
    {
    case 1:
        DbgPrintf2(("Param %s val=%x cbParam=%d\n", pszVBoxOGLCmd[pCmd->enmOp], *(uint8_t *)pParam, cbParam));
        break;
    case 2:
        DbgPrintf2(("Param %s val=%x cbParam=%d\n", pszVBoxOGLCmd[pCmd->enmOp], *(uint16_t *)pParam, cbParam));
        break;
    case 4:
    default:
        DbgPrintf2(("Param %s val=%x cbParam=%d\n", pszVBoxOGLCmd[pCmd->enmOp], *(uint32_t *)pParam, cbParam));
        break;
    }
#endif

    memcpy(pCurrentCmdBlock, pParam, cbParam);
    pCmd->cbCmd += cbParam;
    pCmd->cbCmd  = RT_ALIGN(pCmd->cbCmd, VBOX_OGL_CMD_ALIGN);
}

/**
 * Add a parameter (variable size) to the currently queued OpenGL command
 *
 * @param   pParam      Parameter ptr
 * @param   cbParam     Parameter value size
 */
void VBoxCmdSaveMemParameter(uint8_t *pParam, uint32_t cbParam)
{
    PVBOX_OGL_THREAD_CTX pCtx = VBoxOGLGetThreadCtx();
    PVBOX_OGL_CMD        pCmd = (PVBOX_OGL_CMD)pCtx->pCurrentCmd;
    uint8_t             *pCurrentCmdBlock = (uint8_t *)pCmd + pCmd->cbCmd;
    PVBOX_OGL_VAR_PARAM  pCurrentParam = (PVBOX_OGL_VAR_PARAM)pCurrentCmdBlock;

    Assert(pCurrentCmdBlock + cbParam < pCtx->pCmdBufferEnd);

    DbgPrintf2(("Mem Param %s cbParam=%d\n", pszVBoxOGLCmd[pCmd->enmOp], cbParam));

#ifdef VBOX_OGL_CMD_STRICT
    pCurrentParam->Magic   = VBOX_OGL_CMD_MAGIC;
#endif

    if (pParam && cbParam)
    {
        pCurrentParam->cbParam = cbParam;
        memcpy(pCurrentParam+1, pParam, cbParam);
    }
    else
    {
        pCurrentParam->cbParam = 0;
        cbParam = 0;
    }
    pCmd->cbCmd += sizeof(*pCurrentParam) + cbParam;
    pCmd->cbCmd  = RT_ALIGN(pCmd->cbCmd, VBOX_OGL_CMD_ALIGN);
}

/**
 * Finish the queued command
 *
 * @param   enmOp       OpenGL command op
 */
void VBoxCmdStop(uint32_t enmOp)
{
    PVBOX_OGL_THREAD_CTX pCtx = VBoxOGLGetThreadCtx();
    PVBOX_OGL_CMD        pCmd = (PVBOX_OGL_CMD)pCtx->pCurrentCmd;
    uint8_t             *pCurrentCmdBlock = (uint8_t *)pCmd + pCmd->cbCmd;

    DbgPrintf2(("End %s cbCmd=%d\n", pszVBoxOGLCmd[pCmd->enmOp], pCmd->cbCmd));

    pCtx->pCurrentCmd = pCurrentCmdBlock;
    pCtx->cCommands++;
}

