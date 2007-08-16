/** @file
 * OpenGL:
 * Common header for host service and guest clients.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBox_HostService_VBoxOpenGLSvc_h
#define ___VBox_HostService_VBoxOpenGLSvc_h

#include <VBox/types.h>
#include <VBox/VBoxGuest.h>
#include <VBox/hgcmsvc.h>

/* OpenGL command buffer size */
#define VBOX_OGL_MAX_CMD_BUFFER                     (128*1024)
#define VBOX_OGL_CMD_ALIGN                          4
#define VBOX_OGL_CMD_ALIGN_MASK                     (VBOX_OGL_CMD_ALIGN-1)
#define VBOX_OGL_CMD_MAGIC                          0x1234ABCD

/* for debugging */
#define VBOX_OGL_CMD_STRICT

/* OpenGL command block */
typedef struct
{
#ifdef VBOX_OGL_CMD_STRICT
    uint32_t    Magic;
#endif
    uint32_t    enmOp;
    uint32_t    cbCmd;
    uint32_t    cParams;
    /* start of variable size parameter array */
} VBOX_OGL_CMD, *PVBOX_OGL_CMD;

typedef struct
{
#ifdef VBOX_OGL_CMD_STRICT
    uint32_t    Magic;
#endif
    uint32_t    cbParam;
    /* start of variable size parameter */
} VBOX_OGL_VAR_PARAM, *PVBOX_OGL_VAR_PARAM;

/** OpenGL Folders service functions. (guest)
 *  @{
 */

/** Query mappings changes. */
#define VBOXOGL_FN_GLGETSTRING          (1)
#define VBOXOGL_FN_GLFLUSH              (2)
#define VBOXOGL_FN_GLFLUSHPTR           (3)
#define VBOXOGL_FN_GLCHECKEXT           (4)

/** @} */

/** Function parameter structures.
 *  @{
 */

/**
 * VBOXOGL_FN_GLGETSTRING
 */

/** Parameters structure. */
typedef struct
{
    VBoxGuestHGCMCallInfo   hdr;

    /** 32bit, in: name
     * GLenum name parameter
     */
    HGCMFunctionParameter   name;

    /** pointer, in/out
     * Buffer for requested string
     */
    HGCMFunctionParameter   pString;
} VBoxOGLglGetString;

/** Number of parameters */
#define VBOXOGL_CPARMS_GLGETSTRING (2)



/**
 * VBOXOGL_FN_GLFLUSH
 */

/** Parameters structure. */
typedef struct
{
    VBoxGuestHGCMCallInfo   hdr;

    /** pointer, in
     * Command buffer
     */
    HGCMFunctionParameter   pCmdBuffer;

    /** 32bit, out: cCommands
     * Number of commands in the buffer
     */
    HGCMFunctionParameter   cCommands;
    
    /** 64bit, out: retval
     * uint64_t return code of last command
     */
    HGCMFunctionParameter   retval;

    /** 32bit, out: lasterror
     * GLenum current last error
     */
    HGCMFunctionParameter   lasterror;

} VBoxOGLglFlush;

/** Number of parameters */
#define VBOXOGL_CPARMS_GLFLUSH (4)

/**
 * VBOXOGL_FN_GLFLUSHPTR
 */

/** Parameters structure. */
typedef struct
{
    VBoxGuestHGCMCallInfo   hdr;

    /** pointer, in
     * Command buffer
     */
    HGCMFunctionParameter   pCmdBuffer;

    /** 32bit, out: cCommands
     * Number of commands in the buffer
     */
    HGCMFunctionParameter   cCommands;

    /** pointer, in
     * Last command's final parameter memory block
     */
    HGCMFunctionParameter   pLastParam;
    
    /** 64bit, out: retval
     * uint64_t return code of last command
     */
    HGCMFunctionParameter   retval;

    /** 32bit, out: lasterror
     * GLenum current last error
     */
    HGCMFunctionParameter   lasterror;

} VBoxOGLglFlushPtr;

/** Number of parameters */
#define VBOXOGL_CPARMS_GLFLUSHPTR (5)


/**
 * VBOXOGL_FN_GLCHECKEXT
 */

/** Parameters structure. */
typedef struct
{
    VBoxGuestHGCMCallInfo   hdr;

    /** pointer, in
     * Extension function name
     */
    HGCMFunctionParameter   pszExtFnName;

} VBoxOGLglCheckExt;

/** Number of parameters */
#define VBOXOGL_CPARMS_GLCHECKEXT (1)

/** @} */


#endif
