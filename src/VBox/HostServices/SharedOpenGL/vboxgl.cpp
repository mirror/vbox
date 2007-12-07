/** @file
 * VBox OpenGL
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


#include <iprt/alloc.h>
#include <iprt/string.h>
#include <iprt/assert.h>
#include <VBox/err.h>
#define VBOX_OGL_WITH_CMD_STRINGS
#define VBOX_OGL_WITH_FUNCTION_WRAPPERS
#include "vboxgl.h"

#define LOG_GROUP LOG_GROUP_SHARED_OPENGL
#include <VBox/log.h>

/**
 * glGetString implementation
 *
 * @returns VBox error code
 * @param   pClient     Client context
 * @param   name        glGetString name parameter
 * @param   pString     String pointer
 * @param   pcbString   String length (in/out)
 */
int vboxglGetString(VBOXOGLCTX *pClient, GLenum name, char *pString, uint32_t *pcbString)
{
    const GLubyte *pName;
    uint32_t cbLen;
    int      rc = VINF_SUCCESS;

    vboxglEnableOpenGL(pClient);

    pName = glGetString(name);
    if (pName == NULL)
    {
        Log(("glGetString failed for name %x\n", name));
        rc = VERR_INVALID_PARAMETER;
        goto end;
    }
    cbLen = strlen((char *)pName) + 1;

    if (cbLen > *pcbString)
        cbLen = *pcbString - 1;

    memcpy(pString, pName, cbLen);
    /* force termination */
    pString[cbLen] = 0;
    *pcbString = cbLen + 1;

end:

    vboxglDisableOpenGL(pClient);

    return rc;
}

/**
 * Flush all queued OpenGL commands
 *
 * @returns VBox error code
 * @param   pClient         Client context
 * @param   pCmdBuffer      Command buffer
 * @param   cbCmdBuffer     Command buffer size
 * @param   cCommands       Number of commands in the buffer
 * @param   pLastError      Pointer to last error (out)
 * @param   pLastRetVal     Pointer to return val of last executed command (out)
 */
int vboxglFlushBuffer(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer, uint32_t cbCmdBuffer, uint32_t cCommands, GLenum *pLastError, uint64_t *pLastRetVal)
{
    uint32_t i;
    uint8_t *pOrgBuffer = pCmdBuffer;

    Log(("vboxglFlushBuffer cCommands=%d cbCmdBuffer=%x\n", cCommands, cbCmdBuffer));

    pClient->fHasLastError = false;

    for (i=0;i<cCommands;i++)
    {
        PVBOX_OGL_CMD pCmd = (PVBOX_OGL_CMD)pCmdBuffer;

        Assert(((RTHCUINTPTR)pCmdBuffer & VBOX_OGL_CMD_ALIGN_MASK) == 0);
#ifdef VBOX_OGL_CMD_STRICT
        AssertMsgReturn(pCmd->Magic == VBOX_OGL_CMD_MAGIC, ("Invalid magic dword %x\n", pCmd->Magic), VERR_INVALID_PARAMETER);
#endif
        AssertMsgReturn(pCmd->enmOp < VBOX_OGL_OP_Last, ("Invalid OpenGL cmd %x\n", pCmd->enmOp), VERR_INVALID_PARAMETER);

if (   pCmd->enmOp != VBOX_OGL_OP_Vertex3f
    && pCmd->enmOp != VBOX_OGL_OP_Normal3f)
        Log(("Flush cmd %s cParams=%d cbCmd=%x\n", pszVBoxOGLCmd[pCmd->enmOp], pCmd->cParams, pCmd->cbCmd));

        /* call wrapper */
        AssertMsgReturn(pfnOGLWrapper[pCmd->enmOp], ("No wrapper for opcode %x\n", pCmd->enmOp), VERR_INVALID_PARAMETER);
        pfnOGLWrapper[pCmd->enmOp](pClient, pCmdBuffer);

        pCmdBuffer += pCmd->cbCmd;
    }
    AssertReturn(pCmdBuffer == pOrgBuffer + cbCmdBuffer, VERR_INVALID_PARAMETER);

    *pLastRetVal = pClient->lastretval;
    if (pClient->fHasLastError)
        *pLastError = pClient->ulLastError;
    else
        *pLastError = glGetError();

#ifdef DEBUG
    Log(("Flush: last return value=%VX64\n", *pLastRetVal));
    switch(*pLastError)
    {
    case GL_NO_ERROR:
        Log(("Flush: last error GL_NO_ERROR (%x)\n", GL_NO_ERROR));
        break;
    case GL_INVALID_ENUM:
        Log(("Flush: last error GL_INVALID_ENUM (%x)\n", GL_INVALID_ENUM));
        break;
    case GL_INVALID_VALUE:
        Log(("Flush: last error GL_INVALID_VALUE (%x)\n", GL_INVALID_VALUE));
        break;
    case GL_INVALID_OPERATION:
        Log(("Flush: last error GL_INVALID_OPERATION (%x)\n", GL_INVALID_OPERATION));
        break;
    case GL_STACK_OVERFLOW:
        Log(("Flush: last error GL_STACK_OVERFLOW (%x)\n", GL_STACK_OVERFLOW));
        break;
    case GL_STACK_UNDERFLOW:
        Log(("Flush: last error GL_STACK_UNDERFLOW (%x)\n", GL_STACK_UNDERFLOW));
        break;
    case GL_OUT_OF_MEMORY:
        Log(("Flush: last error GL_OUT_OF_MEMORY (%x)\n", GL_OUT_OF_MEMORY));
        break;
    }
#endif
    return VINF_SUCCESS;
}
