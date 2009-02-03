/** @file cbinding.h
 *
 * C binding for XPCOM.
 *
 * $Id$
 */

#ifndef __cbinding_h__
#define __cbinding_h__

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

VBOXXPCOMC_DECL(void) VBoxComInitialize(IVirtualBox **virtualBox, ISession **session);
VBOXXPCOMC_DECL(void) VBoxComUninitialize(void);
VBOXXPCOMC_DECL(void) VBoxComUnallocStr(PRUnichar *str_dealloc);
VBOXXPCOMC_DECL(void) VBoxComUnallocIID(nsIID *iid);
VBOXXPCOMC_DECL(const char *) VBoxConvertPRUnichartoAscii(PRUnichar *src);
VBOXXPCOMC_DECL(const PRUnichar *) VBoxConvertAsciitoPRUnichar(char *src);
VBOXXPCOMC_DECL(const PRUnichar *) VBoxConvertUTF8toPRUnichar(char *src);
VBOXXPCOMC_DECL(const char *) VBoxConvertPRUnichartoUTF8(PRUnichar *src);
VBOXXPCOMC_DECL(int)  VBoxStrToUtf16(const char *pszString, PRUnichar **ppwszString);
VBOXXPCOMC_DECL(int)  VBoxUtf16ToUtf8(const PRUnichar *pszString, char **ppwszString);
VBOXXPCOMC_DECL(void) VBoxUtf16Free(PRUnichar *pwszString);
VBOXXPCOMC_DECL(void) VBoxStrFree(char *pszString);

#ifdef __cplusplus
}
#endif

#endif /* __cbinding_h__ */
