/** @file
 *
 * VBox HDD container test utility - scripting engine.
 */

/*
 * Copyright (C) 2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#ifndef _VDScript_h__
#define _VDScript_h__

/** Handle to the scripting context. */
typedef struct VDSCRIPTCTXINT *VDSCRIPTCTX;
/** Pointer to a scripting context handle. */
typedef VDSCRIPTCTX *PVDSCRIPTCTX;

/**
 * Supprted primitive types in the scripting engine.
 */
typedef enum VDSCRIPTTYPE
{
    /** Invalid type, do not use. */
    VDSCRIPTTYPE_INVALID = 0,
    /** void type, used for no return value of methods. */
    VDSCRIPTTYPE_VOID,
    /** unsigned 8bit integer. */
    VDSCRIPTTYPE_UINT8,
    VDSCRIPTTYPE_INT8,
    VDSCRIPTTYPE_UINT16,
    VDSCRIPTTYPE_INT16,
    VDSCRIPTTYPE_UINT32,
    VDSCRIPTTYPE_INT32,
    VDSCRIPTTYPE_UINT64,
    VDSCRIPTTYPE_INT64,
    VDSCRIPTTYPE_STRING,
    VDSCRIPTTYPE_BOOL,
    /** As usual, the 32bit blowup hack. */
    VDSCRIPTTYPE_32BIT_HACK = 0x7fffffff
} VDSCRIPTTYPE;
/** Pointer to a type. */
typedef VDSCRIPTTYPE *PVDSCRIPTTYPE;
/** Pointer to a const type. */
typedef const VDSCRIPTTYPE *PCVDSCRIPTTYPE;

/**
 * Script argument.
 */
typedef struct VDSCRIPTARG
{
    /** Type of the argument. */
    VDSCRIPTTYPE    enmType;
    /** Value */
    union
    {
        uint8_t     u8;
        int8_t      i8;
        uint16_t    u16;
        int16_t     i16;
        uint32_t    u32;
        int32_t     i32;
        uint64_t    u64;
        int64_t     i64;
        const char *psz;
        bool        f;
    };
} VDSCRIPTARG;
/** Pointer to an argument. */
typedef VDSCRIPTARG *PVDSCRIPTARG;

/** Script callback. */
typedef DECLCALLBACK(int) FNVDSCRIPTCALLBACK(PVDSCRIPTARG paScriptArgs, void *pvUser);
/** Pointer to a script callback. */
typedef FNVDSCRIPTCALLBACK *PFNVDSCRIPTCALLBACK;

/**
 * Callback registration structure.
 */
typedef struct VDSCRIPTCALLBACK
{
    /** The function name. */
    const char            *pszFnName;
    /** The return type of the function. */
    VDSCRIPTTYPE           enmTypeReturn;
    /** Pointer to the array of argument types. */
    PCVDSCRIPTTYPE         paArgs;
    /** Number of arguments this method takes. */
    unsigned               cArgs;
    /** The callback handler. */
    PFNVDSCRIPTCALLBACK    pfnCallback;
} VDSCRIPTCALLBACK;
/** Pointer to a callback register entry. */
typedef VDSCRIPTCALLBACK *PVDSCRIPTCALLBACK;
/** Pointer to a const callback register entry. */
typedef const VDSCRIPTCALLBACK *PCVDSCRIPTCALLBACK;

/**
 * Create a new scripting context.
 *
 * @returns VBox status code.
 * @param   phScriptCtx    Where to store the scripting context on success.
 */
DECLHIDDEN(int) VDScriptCtxCreate(PVDSCRIPTCTX phScriptCtx);

/**
 * Destroys the given scripting context.
 *
 * @returns nothing.
 * @param   hScriptCtx     The script context to destroy.
 */
DECLHIDDEN(void) VDScriptCtxDestroy(VDSCRIPTCTX hScriptCtx);

/**
 * Register callbacks for the scripting context.
 *
 * @returns VBox status code.
 * @param   hScriptCtx     The script context handle.
 * @param   paCallbacks    Pointer to the callbacks to register.
 * @param   cCallbacks     Number of callbacks in the array.
 * @param   pvUser         Opaque user data to pass on the callback invocation.
 */
DECLHIDDEN(int) VDScriptCtxCallbacksRegister(VDSCRIPTCTX hScriptCtx, PCVDSCRIPTCALLBACK paCallbacks,
                                             unsigned cCallbacks, void *pvUser);

/**
 * Load a given script into the context.
 *
 * @returns VBox status code.
 * @param   hScriptCtx     The script context handle.
 * @param   pszScript      Pointer to the char buffer containing the script.
 */
DECLHIDDEN(int) VDScriptCtxLoadScript(VDSCRIPTCTX hScriptCtx, const char *pszScript);

/**
 * Execute a given method in the script context.
 *
 * @returns VBox status code.
 * @param   hScriptCtx     The script context handle.
 * @param   pszFnCall      The method to call.
 * @param   paArgs         Pointer to arguments to pass.
 * @param   cArgs          Number of arguments.
 */
DECLHIDDEN(int) VDScriptCtxCallFn(VDSCRIPTCTX hScriptCtx, const char *pszFnCall,
                                  PVDSCRIPTARG paArgs, unsigned cArgs);

#endif /* _VDScript_h__ */
