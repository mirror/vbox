/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef prproces_h___
#define prproces_h___

#include "prtypes.h"
#include "prio.h"

#ifdef VBOX_WITH_XPCOM_NAMESPACE_CLEANUP
#define PR_CreateProcessDetached VBoxNsprPR_CreateProcessDetached
#define PR_ProcessAttrSetInheritableFD VBoxNsprPR_ProcessAttrSetInheritableFD
#define PR_DestroyProcessAttr VBoxNsprPR_DestroyProcessAttr
#define PR_NewProcessAttr VBoxNsprPR_NewProcessAttr
#define PR_ResetProcessAttr VBoxNsprPR_ResetProcessAttr
#define PR_ProcessAttrSetStdioRedirect VBoxNsprPR_ProcessAttrSetStdioRedirect
#define PR_ProcessAttrSetCurrentDirectory VBoxNsprPR_ProcessAttrSetCurrentDirectory
#endif /* VBOX_WITH_XPCOM_NAMESPACE_CLEANUP */

PR_BEGIN_EXTERN_C

/************************************************************************/
/*****************************PROCESS OPERATIONS*************************/
/************************************************************************/

typedef struct PRProcess PRProcess;
typedef struct PRProcessAttr PRProcessAttr;

NSPR_API(PRProcessAttr *) PR_NewProcessAttr(void);

NSPR_API(void) PR_ResetProcessAttr(PRProcessAttr *attr);

NSPR_API(void) PR_DestroyProcessAttr(PRProcessAttr *attr);

NSPR_API(void) PR_ProcessAttrSetStdioRedirect(
    PRProcessAttr *attr,
    PRSpecialFD stdioFd,
    PRFileDesc *redirectFd
);

NSPR_API(PRStatus) PR_ProcessAttrSetCurrentDirectory(
    PRProcessAttr *attr,
    const char *dir
);

NSPR_API(PRStatus) PR_ProcessAttrSetInheritableFD(
    PRProcessAttr *attr,
    PRFileDesc *fd,
    const char *name
);

NSPR_API(PRStatus) PR_CreateProcessDetached(
    const char *path,
    char *const *argv,
    char *const *envp,
    const PRProcessAttr *attr);

PR_END_EXTERN_C

#endif /* prproces_h___ */
