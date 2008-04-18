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
#include "VBoxOGL.h"
#include <VBox/version.h>
#include <VBox/VBoxGuest.h>
#include <stdarg.h>
#include <stdio.h>


       HINSTANCE    hDllVBoxOGL                 = 0;
static DWORD        dwOGLTlsIndex               = TLS_OUT_OF_INDEXES;
static VBOX_OGL_CTX vboxOGLCtx                  = {0};
       char         szOpenGLVersion[256]        = "";
       char         szOpenGLExtensions[8192]    = ""; /* this one can be rather long */


/**
 * Set the thread local OpenGL context
 *
 * @param   pCtx        thread local OpenGL context ptr
 */
void VBoxOGLSetThreadCtx(PVBOX_OGL_THREAD_CTX pCtx)
{
    BOOL ret;

    ret = TlsSetValue(dwOGLTlsIndex, pCtx);
    Assert(ret);
}


/**
 * Return the thread local OpenGL context
 *
 * @return thread local OpenGL context ptr or NULL if failure
 */
PVBOX_OGL_THREAD_CTX VBoxOGLGetThreadCtx()
{
    PVBOX_OGL_THREAD_CTX pCtx;

    pCtx = (PVBOX_OGL_THREAD_CTX)TlsGetValue(dwOGLTlsIndex);
    if (!pCtx)
    {
        /* lazy init */
        VBoxOGLThreadAttach();
        pCtx = (PVBOX_OGL_THREAD_CTX)TlsGetValue(dwOGLTlsIndex);
    }
    Assert(pCtx);
    return pCtx;
}

/**
 * Initialize the OpenGL guest-host communication channel
 *
 * @return success or failure (boolean)
 * @param   hDllInst        Dll instance handle
 */
BOOL VBoxOGLInit(HINSTANCE hDllInst)
{
    dwOGLTlsIndex = TlsAlloc();
    if (dwOGLTlsIndex == TLS_OUT_OF_INDEXES)
    {
        DbgPrintf(("TlsAlloc failed with %d\n", GetLastError()));
        return FALSE;
    }
    DbgPrintf(("VBoxOGLInit TLS index=%d\n", dwOGLTlsIndex));

    /* open VBox guest driver */
    vboxOGLCtx.hGuestDrv = CreateFile(VBOXGUEST_DEVICE_NAME,
                                      GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      NULL,
                                      OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL,
                                      NULL);
    if (vboxOGLCtx.hGuestDrv == INVALID_HANDLE_VALUE)
    {
        DbgPrintf(("VBoxService: could not open VBox Guest Additions driver! rc = %d\n", GetLastError()));
        return FALSE;
    }

    VBoxOGLThreadAttach();
    hDllVBoxOGL = hDllInst;
    return TRUE;
}

/**
 * Destroy the OpenGL guest-host communication channel
 *
 * @return success or failure (boolean)
 */
BOOL VBoxOGLExit()
{
    DbgPrintf(("VBoxOGLExit\n"));
    VBoxOGLThreadDetach();
    CloseHandle(vboxOGLCtx.hGuestDrv);

    hDllVBoxOGL = 0;
    return TRUE;
}


/**
 * Initialize new thread
 *
 * @return success or failure (boolean)
 */
BOOL VBoxOGLThreadAttach()
{
    PVBOX_OGL_THREAD_CTX pCtx;

    DbgPrintf(("VBoxOGLThreadAttach id=%x\n", GetCurrentThreadId()));

    pCtx = (PVBOX_OGL_THREAD_CTX)malloc(sizeof(*pCtx));
    AssertReturn(pCtx, FALSE);
    if (!pCtx)
        return FALSE;

    pCtx->pCmdBuffer    = (uint8_t *)VirtualAlloc(NULL, VBOX_OGL_MAX_CMD_BUFFER, MEM_COMMIT, PAGE_READWRITE);
    pCtx->pCmdBufferEnd = pCtx->pCmdBuffer + VBOX_OGL_MAX_CMD_BUFFER;
    pCtx->pCurrentCmd   = pCtx->pCmdBuffer;
    pCtx->cCommands     = 0;
    Assert(pCtx->pCmdBuffer);
    if (!pCtx->pCmdBuffer)
        return FALSE;

    VBoxOGLSetThreadCtx(pCtx);

    VBoxGuestHGCMConnectInfo info;
    memset (&info, 0, sizeof (info));

    info.Loc.type = VMMDevHGCMLoc_LocalHost_Existing;

    strcpy (info.Loc.u.host.achName, "VBoxSharedOpenGL");

    DWORD cbReturned;

    if (DeviceIoControl(vboxOGLCtx.hGuestDrv,
                        IOCTL_VBOXGUEST_HGCM_CONNECT,
                        &info, sizeof (info),
                        &info, sizeof (info),
                        &cbReturned,
                        NULL))
    {
        if (info.result == VINF_SUCCESS)
        {
            pCtx->u32ClientID = info.u32ClientID;
            DbgPrintf(("HGCM connect was successful: client id =%x\n", pCtx->u32ClientID));
        }
    }
    else
    {
        DbgPrintf(("HGCM connect failed with rc=%x\n", GetLastError()));
        return FALSE;
    }

    VBoxOGLglGetString parms;
    memset(&parms, 0, sizeof(parms));

    VBOX_INIT_CALL(&parms.hdr, GLGETSTRING, pCtx);

    parms.name.type                      = VMMDevHGCMParmType_32bit;
    parms.name.u.value32                 = GL_VERSION;
    parms.pString.type                   = VMMDevHGCMParmType_LinAddr;
    parms.pString.u.Pointer.size         = sizeof(szOpenGLVersion);
    parms.pString.u.Pointer.u.linearAddr = (vmmDevHypPtr)szOpenGLVersion;

    int rc = vboxHGCMCall(&parms, sizeof (parms));

    if (    VBOX_FAILURE(rc)
        ||  VBOX_FAILURE(parms.hdr.result))
    {
        DbgPrintf(("GL_VERSION failed with %x %x\n", rc, parms.hdr.result));
        return FALSE;
    }
    DbgPrintf(("GL_VERSION=%s\n", szOpenGLVersion));

    /* Initialize OpenGL extensions */
    vboxInitOpenGLExtensions(pCtx);

    return TRUE;
}

/**
 * Clean up for terminating thread
 *
 * @return success or failure (boolean)
 */
BOOL VBoxOGLThreadDetach()
{
    PVBOX_OGL_THREAD_CTX pCtx;

    DbgPrintf(("VBoxOGLThreadDetach id=%x\n", GetCurrentThreadId()));

    pCtx = VBoxOGLGetThreadCtx();
    if (pCtx && pCtx->u32ClientID)
    {
        VBoxGuestHGCMDisconnectInfo info;

        memset (&info, 0, sizeof (info));

        info.u32ClientID = pCtx->u32ClientID;

        DWORD cbReturned;

        BOOL bRet = DeviceIoControl(vboxOGLCtx.hGuestDrv,
                                    IOCTL_VBOXGUEST_HGCM_DISCONNECT,
                                    &info, sizeof (info),
                                    &info, sizeof (info),
                                    &cbReturned,
                                    NULL);

        if (!bRet)
        {
            DbgPrintf(("Disconnect failed with %x\n", GetLastError()));
        }
    }
    if (pCtx)
    {
        if (pCtx->pCmdBuffer)
            VirtualFree(pCtx->pCmdBuffer, 0, MEM_RELEASE);

        free(pCtx);
        VBoxOGLSetThreadCtx(NULL);
    }

    return TRUE;
}



/**
 * Send an HGCM request
 *
 * @return VBox status code
 * @param   pvData      Data pointer
 * @param   cbData      Data size
 */
int vboxHGCMCall(void *pvData, unsigned cbData)
{
    DWORD cbReturned;

    if (DeviceIoControl (vboxOGLCtx.hGuestDrv,
                         IOCTL_VBOXGUEST_HGCM_CALL,
                         pvData, cbData,
                         pvData, cbData,
                         &cbReturned,
                         NULL))
    {
        return VINF_SUCCESS;
    }
    DbgPrintf(("vboxCall failed with %x\n", GetLastError()));
    return VERR_NOT_SUPPORTED;
}

#ifdef DEBUG
/**
 * Log to the debug output device
 *
 * @param   pszFormat   Format string
 * @param   ...         Variable parameters
 */
void VBoxDbgLog(char *pszFormat, ...)
{
   va_list va;

   va_start(va, pszFormat);
   CHAR  Buffer[10240];
   if (strlen(pszFormat) < 512)
   {
      vsprintf (Buffer, pszFormat, va);

      printf(Buffer);
//      OutputDebugStringA(Buffer);
   }

   va_end (va);
}
#endif


GLenum APIENTRY glGetError (void)
{
    PVBOX_OGL_THREAD_CTX pCtx = VBoxOGLGetThreadCtx();

    /** @todo if fetch error flag set -> flush buffer */
    return pCtx->glLastError;
}

void APIENTRY glSetError(GLenum glNewError)
{
    PVBOX_OGL_THREAD_CTX pCtx = VBoxOGLGetThreadCtx();

    pCtx->glLastError = glNewError;
}

/**
 * Query OpenGL strings
 *
 * @returns OpenGL string pointer
 * @param   name    string type
 */
const GLubyte * APIENTRY glGetString (GLenum name)
{
    DbgPrintf(("glGetString %x\n", name));
    switch (name)
    {
    /* Note: We hide the host vendor and renderer to avoid exposing potential critical information (exploits) */
    case GL_VENDOR:
        return (const GLubyte *)VBOX_VENDOR;

    case GL_RENDERER:
        return (const GLubyte *)"VirtualBox OpenGL Renderer";

    case GL_VERSION:
        return (const GLubyte *)szOpenGLVersion;

    case GL_EXTENSIONS:
        return (const GLubyte *)szOpenGLExtensions;

    default:
        glLogError(GL_INVALID_ENUM);
        return NULL;
    }
}

/**
 * Flush the OpenGL command queue and return the return val of the last command
 *
 * @returns return val of last command
 */
uint64_t VBoxOGLFlush()
{
    PVBOX_OGL_THREAD_CTX pCtx = VBoxOGLGetThreadCtx();
    VBoxOGLglFlush       parms;

    AssertReturn(pCtx->pCurrentCmd > pCtx->pCmdBuffer, 0);

    VBOX_INIT_CALL(&parms.hdr, GLFLUSH, pCtx);

    parms.pCmdBuffer.type                   = VMMDevHGCMParmType_LinAddr_In;
    parms.pCmdBuffer.u.Pointer.size         = pCtx->pCurrentCmd - pCtx->pCmdBuffer;
    parms.pCmdBuffer.u.Pointer.u.linearAddr = (vmmDevHypPtr)pCtx->pCmdBuffer;
    parms.cCommands.type                    = VMMDevHGCMParmType_32bit;
    parms.cCommands.u.value32               = pCtx->cCommands;
    parms.retval.type                       = VMMDevHGCMParmType_64bit;
    parms.retval.u.value64                  = 0;
    parms.lasterror.type                    = VMMDevHGCMParmType_32bit;
    parms.lasterror.u.value32               = 0;

    int rc = vboxHGCMCall(&parms, sizeof (parms));

    /* reset command buffer */
    pCtx->pCurrentCmd = pCtx->pCmdBuffer;
    pCtx->cCommands   = 0;

    if (    VBOX_FAILURE(rc)
        ||  VBOX_FAILURE(parms.hdr.result))
    {
        DbgPrintf(("GL_FLUSH failed with %x %x\n", rc, parms.hdr.result));
        return 0;
    }

    glSetError(parms.lasterror.u.value32);
#ifdef DEBUG
    if (parms.lasterror.u.value32)
        DbgPrintf(("Last command returned error %x\n", parms.lasterror.u.value32));
#endif
    return parms.retval.u.value64;
}

/**
 * Flush the OpenGL command queue and return the return val of the last command
 * The last command's final parameter is a pointer where its result is stored
 *
 * @returns return val of last command
 * @param   pLastParam      Last parameter's address
 * @param   cbParam         Last parameter's size
 */
uint64_t VBoxOGLFlushPtr(void *pLastParam, uint32_t cbParam)
{
    PVBOX_OGL_THREAD_CTX pCtx = VBoxOGLGetThreadCtx();
    VBoxOGLglFlushPtr    parms;

    AssertReturn(pCtx->pCurrentCmd > pCtx->pCmdBuffer, 0);

    VBOX_INIT_CALL(&parms.hdr, GLFLUSHPTR, pCtx);

    parms.pCmdBuffer.type                   = VMMDevHGCMParmType_LinAddr_In;
    parms.pCmdBuffer.u.Pointer.size         = pCtx->pCurrentCmd - pCtx->pCmdBuffer;
    parms.pCmdBuffer.u.Pointer.u.linearAddr = (vmmDevHypPtr)pCtx->pCmdBuffer;
    parms.cCommands.type                    = VMMDevHGCMParmType_32bit;
    parms.cCommands.u.value32               = pCtx->cCommands;
    parms.retval.type                       = VMMDevHGCMParmType_64bit;
    parms.retval.u.value64                  = 0;
    parms.lasterror.type                    = VMMDevHGCMParmType_32bit;
    parms.lasterror.u.value32               = 0;
    if (cbParam)
    {
        Assert(pLastParam);
        parms.pLastParam.type                   = VMMDevHGCMParmType_LinAddr;
        parms.pLastParam.u.Pointer.size         = cbParam;
        parms.pLastParam.u.Pointer.u.linearAddr = (vmmDevHypPtr)pLastParam;
    }
    else
    {
        /* Placeholder as HGCM doesn't like NULL pointers */
        Assert(!cbParam && !pLastParam);
        parms.pLastParam.type                   = VMMDevHGCMParmType_32bit;
        parms.pLastParam.u.value32              = 0;
    }

    int rc = vboxHGCMCall(&parms, sizeof (parms));

    /* reset command buffer */
    pCtx->pCurrentCmd = pCtx->pCmdBuffer;
    pCtx->cCommands   = 0;

    if (    VBOX_FAILURE(rc)
        ||  VBOX_FAILURE(parms.hdr.result))
    {
        DbgPrintf(("GL_FLUSH failed with %x %x\n", rc, parms.hdr.result));
        return 0;
    }

    glSetError(parms.lasterror.u.value32);
#ifdef DEBUG
    if (parms.lasterror.u.value32)
        DbgPrintf(("Last command returned error %x\n", parms.lasterror.u.value32));
#endif
    return parms.retval.u.value64;
}

/**
 * Check if an OpenGL extension is available on the host
 *
 * @returns available or not
 * @param   pszExtFunctionName  
 */
bool VBoxIsExtensionAvailable(const char *pszExtFunctionName)
{
    PVBOX_OGL_THREAD_CTX pCtx = VBoxOGLGetThreadCtx();
    VBoxOGLglCheckExt parms;

    VBOX_INIT_CALL(&parms.hdr, GLCHECKEXT, pCtx);

    parms.pszExtFnName.type                   = VMMDevHGCMParmType_LinAddr_In;
    parms.pszExtFnName.u.Pointer.size         = strlen(pszExtFunctionName)+1;
    parms.pszExtFnName.u.Pointer.u.linearAddr = (vmmDevHypPtr)pszExtFunctionName;

    int rc = vboxHGCMCall(&parms, sizeof (parms));

    if (    VBOX_FAILURE(rc)
        ||  VBOX_FAILURE(parms.hdr.result))
    {
        DbgPrintf(("GLCHECKEXT failed with %x %x\n", rc, parms.hdr.result));
        return false;
    }

    return true;
}
