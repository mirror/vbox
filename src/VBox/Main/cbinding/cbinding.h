/* $Revision$ */
/** @file cbinding.h
 * C binding for XPCOM.
 */

#ifndef ___cbinding_h
#define ___cbinding_h

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

/* Converting to and from ASCII. */
VBOXXPCOMC_DECL(const char *) VBoxConvertPRUnichartoAscii(PRUnichar *src);
VBOXXPCOMC_DECL(const PRUnichar *) VBoxConvertAsciitoPRUnichar(char *src);

/* Converting to and from UTF-8 and UTF-16. */
VBOXXPCOMC_DECL(int) VBoxUtf16ToUtf8(const PRUnichar *pwszString, char **ppszString);
VBOXXPCOMC_DECL(int) VBoxUtf8ToUtf16(const char *pszString, PRUnichar **ppwszString);

#ifdef __cplusplus
}
#endif

#endif /* !___cbinding_h */

