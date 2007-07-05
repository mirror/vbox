/** @file
 *
 * VirtualBox Windows NT/2000/XP guest OpenGL ICD
 *
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 *
 */
#ifndef __VBOXOGL_H__
#define __VBOXOGL_H__

#include <windows.h>
/* get rid of the inconsistent dll linkage warnings */
#undef WINGDIAPI
#define WINGDIAPI
#include "GL/gl.h"

#include <iprt/cdefs.h>
#include <iprt/assert.h>

#include <VBox/HostServices/VBoxOpenGLSvc.h>
#include <VBox/HostServices/VBoxOGLOp.h>

typedef struct _icdTable 
{
    DWORD size;
    PROC  table[336];
} ICDTABLE, *PICDTABLE;


typedef struct
{
    HANDLE      hGuestDrv;

} VBOX_OGL_CTX, *PVBOX_OGL_CTX;

typedef struct
{
    uint32_t        u32ClientID;

    GLenum          glLastError;
    uint32_t        cCommands;
    uint8_t        *pCmdBuffer;
    uint8_t        *pCmdBufferEnd;
    uint8_t        *pCurrentCmd;
} VBOX_OGL_THREAD_CTX, *PVBOX_OGL_THREAD_CTX;


extern HINSTANCE hDllVBoxOGL;

void APIENTRY glSetError(GLenum glNewError);

#ifdef DEBUG
#define glLogError(a)               \
    {                               \
        /** @todo log error */      \
        glSetError(a);              \
    }
#define DbgPrintf(a)        VBoxDbgLog a

#ifdef VBOX_DEBUG_LVL2
#define DbgPrintf2(a)       VBoxDbgLog a
#else
#define DbgPrintf2(a)
#endif

#else
#define glLogError(a)       glSetError(a)
#define DbgPrintf(a)
#define DbgPrintf2(a)
#endif


/**
 * Initialize the OpenGL guest-host communication channel
 *
 * @return success or failure (boolean)
 * @param   hDllInst        Dll instance handle
 */
BOOL VBoxOGLInit(HINSTANCE hDllInst);

/**
 * Destroy the OpenGL guest-host communication channel
 *
 * @return success or failure (boolean)
 */
BOOL VBoxOGLExit();

/**
 * Initialize new thread
 *
 * @return success or failure (boolean)
 */
BOOL VBoxOGLThreadAttach();

/**
 * Clean up for terminating thread
 *
 * @return success or failure (boolean)
 */
BOOL VBoxOGLThreadDetach();

/**
 * Set the thread local OpenGL context
 *
 * @param   pCtx        thread local OpenGL context ptr
 */
void VBoxOGLSetThreadCtx(PVBOX_OGL_THREAD_CTX pCtx);

/**
 * Return the thread local OpenGL context
 *
 * @return thread local OpenGL context ptr or NULL if failure
 */
PVBOX_OGL_THREAD_CTX VBoxOGLGetThreadCtx();


/**
 * Queue a new OpenGL command
 *
 * @param   enmOp       OpenGL command op
 * @param   cParam      Number of parameters
 * @param   cbParams    Memory needed for parameters
 */
void VBoxCmdStart(uint32_t enmOp, uint32_t cParam, uint32_t cbParams);

/**
 * Add a parameter to the currently queued OpenGL command
 *
 * @param   pParam      Parameter ptr
 * @param   cbParam     Parameter value size
 */
void VBoxCmdSaveParameter(uint8_t *pParam, uint32_t cbParam);

/**
 * Add a parameter (variable size) to the currently queued OpenGL command
 *
 * @param   pParam      Parameter ptr
 * @param   cbParam     Parameter value size
 */
void VBoxCmdSaveMemParameter(uint8_t *pParam, uint32_t cbParam);

/**
 * Finish the queued command
 *
 * @param   enmOp       OpenGL command op
 */
void VBoxCmdStop(uint32_t enmOp);

                 
/**
 * Send an HGCM request
 *
 * @return VBox status code
 * @param   hDriver     Driver handle
 * @param   pvData      Data pointer
 * @param   cbData      Data size
 */
int vboxHGCMCall(HANDLE hDriver, void *pvData, unsigned cbData);

#ifdef DEBUG
/**
 * Log to the debug output device
 *
 * @param   pszFormat   Format string
 * @param   ...         Variable parameters
 */
void VBoxDbgLog(char *pszFormat, ...);
#endif

/**
 * Flush the OpenGL command queue and return the return val of the last command
 *
 * @returns return val of last command
 */
uint64_t VBoxOGLFlush();

/**
 * Flush the OpenGL command queue and return the return val of the last command
 * The last command's final parameter is a pointer where its result is stored
 *
 * @returns return val of last command
 * @param   pLastParam      Last parameter's address
 * @param   cbParam         Last parameter's size
 */
uint64_t VBoxOGLFlushPtr(void *pLastParam, uint32_t cbParam);

/**
 * Query the specified cached parameter
 * 
 * @returns requested cached value
 * @param   type        Parameter type (Note: minimal checks only!)
 */
GLint glInternalGetIntegerv(GLenum type);

/**
 * Query the specified cached texture level parameter
 * 
 * @returns requested cached value
 * @param   type        Parameter type (Note: minimal checks only!)
 */
GLint glInternalGetTexLevelParameteriv(GLenum type);

/**
 * Query the number of bytes required for a pixel in the specified format
 * 
 * @returns requested pixel size
 * @param   type        Parameter type
 */
GLint glInternalGetPixelFormatElements(GLenum format);

/**
 * Query the size of the specified data type
 *
 * @returns type size or 0 if unknown type
 * @param   type        data type
 */
GLint glVBoxGetDataTypeSize(GLenum type);

uint32_t glInternalLightvElem(GLenum pname);
uint32_t glInternalMaterialvElem(GLenum pname);
uint32_t glInternalTexEnvvElem(GLenum pname);
uint32_t glInternalTexGenvElem(GLenum pname);
uint32_t glInternalTexParametervElem(GLenum pname);


#endif /* __VBOXOGL_H__ */

