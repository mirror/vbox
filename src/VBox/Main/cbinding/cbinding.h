/* $Revision$ */
/** @file cbinding.h
 * C binding for XPCOM.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

#ifndef ___VBoxXPCOMC_cbinding_h
#define ___VBoxXPCOMC_cbinding_h

#ifndef MOZ_UNICODE
# define MOZ_UNICODE
#endif

#ifdef __cplusplus
# include "VirtualBox_XPCOM.h"
#else
# include "VirtualBox_CXPCOM.h"
#endif

#ifdef IN_VBOXXPCOMC
# define VBOXXPCOMC_DECL(type)  PR_EXPORT(type)
#else
# define VBOXXPCOMC_DECL(type)  PR_IMPORT(type)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize/Uninitialize XPCOM. */
VBOXXPCOMC_DECL(void) VBoxComInitialize(IVirtualBox **virtualBox, ISession **session);
VBOXXPCOMC_DECL(void) VBoxComUninitialize(void);

/* Deallocation functions. */
VBOXXPCOMC_DECL(void) VBoxComUnallocMem(void *ptr);
VBOXXPCOMC_DECL(void) VBoxUtf16Free(PRUnichar *pwszString);
VBOXXPCOMC_DECL(void) VBoxUtf8Free(char *pszString);

/* Converting to and from UTF-8 and UTF-16. */
VBOXXPCOMC_DECL(int) VBoxUtf16ToUtf8(const PRUnichar *pwszString, char **ppszString);
VBOXXPCOMC_DECL(int) VBoxUtf8ToUtf16(const char *pszString, PRUnichar **ppwszString);

/* Getting and setting the environment variables. */
VBOXXPCOMC_DECL(const char *) VBoxGetEnv(const char *pszVar);
VBOXXPCOMC_DECL(int) VBoxSetEnv(const char *pszVar, const char *pszValue);


/**
 * Function table for dynamic linking.
 * Use VBoxGetFunctions() to obtain the pointer to it.
 */
typedef struct VBOXXPCOMC
{
    /** The size of the structure. */
    unsigned cb;
    /** The structure version. */
    unsigned uVersion;
    void  (*pfnComInitialize)(IVirtualBox **virtualBox, ISession **session);
    void  (*pfnComUninitialize)(void);

    void  (*pfnComUnallocMem)(void *pv);
    void  (*pfnUtf16Free)(PRUnichar *pwszString);
    void  (*pfnUtf8Free)(char *pszString);

    int   (*pfnUtf16ToUtf8)(const PRUnichar *pwszString, char **ppszString);
    int   (*pfnUtf8ToUtf16)(const char *pszString, PRUnichar **ppwszString);

    const char * (*pfnGetEnv)(const char *pszVar);
    int   (*pfnSetEnv)(const char *pszVar, const char *pszValue);
    /** Tail version, same as uVersion. */
    unsigned uEndVersion;
} VBOXXPCOMC;
/** Pointer to a const VBoxXPCOMC function table. */
typedef VBOXXPCOMC const *PCVBOXXPCOM;

/** The current interface version.
 * For use with VBoxGetXPCOMCFunctions and to be found in
 * VBOXXPCOMC::uVersion. */
#define VBOX_XPCOMC_VERSION     0x00010000U

VBOXXPCOMC_DECL(PCVBOXXPCOM) VBoxGetXPCOMCFunctions(unsigned uVersion);
/** Typedef for VBoxGetXPCOMCFunctions. */
typedef PCVBOXXPCOM (*PFNVBOXGETXPCOMCFUNCTIONS)(unsigned uVersion);

/** The symbol name of VBoxGetXPCOMCFunctions. */
#if defined(__APPLE__) || defined(__OS2__)
# define VBOX_GET_XPCOMC_FUNCTIONS_SYMBOL_NAME   "_VBoxGetXPCOMCFunctions"
#else
# define VBOX_GET_XPCOMC_FUNCTIONS_SYMBOL_NAME   "VBoxGetXPCOMCFunctions"
#endif


#ifdef __cplusplus
}
#endif

#endif /* !___VBoxXPCOMC_cbinding_h */

