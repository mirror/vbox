/* $Revision$ */
/** @file VBoxXPCOMCGlue.h
 * Glue for dynamically linking with VBoxXPCOMC.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef ___VBoxXPCOMC_cglue_h
#define ___VBoxXPCOMC_cglue_h

#include "VirtualBox_CXPCOM.h"

#ifdef __cplusplus
extern "C" {
#endif

/** The dlopen handle for VBoxXPCOMC. */
extern void *g_hVBoxXPCOMC;
/** The last load error. */
extern char g_szVBoxErrMsg[256];
/** Pointer to the VBoxXPCOMC function table.  */
extern PCVBOXXPCOM g_pVBoxFuncs;
/** Pointer to VBoxGetXPCOMCFunctions for the loaded VBoxXPCOMC so/dylib/dll. */
extern PFNVBOXGETXPCOMCFUNCTIONS g_pfnGetFunctions;


int VBoxCGlueInit(const char *pszMsgPrefix);
void VBoxCGlueTerm(void);


#ifdef __cplusplus
}
#endif

#endif

