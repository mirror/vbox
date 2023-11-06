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

/*
** File:	pprio.h
**
** Description:	Private definitions for I/O related structures
*/

#ifndef pprio_h___
#define pprio_h___

#include "prtypes.h"
#include "prio.h"

#ifdef VBOX_WITH_XPCOM_NAMESPACE_CLEANUP
#define PR_GetFileMethods VBoxNsprPR_GetFileMethods
#define PR_GetTCPMethods VBoxNsprPR_GetTCPMethods
#define PR_GetUDPMethods VBoxNsprPR_GetUDPMethods
#define PR_GetPipeMethods VBoxNsprPR_GetPipeMethods
#define PR_FileDesc2NativeHandle VBoxNsprPR_FileDesc2NativeHandle
#define PR_ChangeFileDescNativeHandle VBoxNsprPR_ChangeFileDescNativeHandle
#define PR_AllocFileDesc VBoxNsprPR_AllocFileDesc
#define PR_FreeFileDesc VBoxNsprPR_FreeFileDesc
#define PR_ImportFile VBoxNsprPR_ImportFile
#define PR_ImportPipe VBoxNsprPR_ImportPipe
#define PR_ImportTCPSocket VBoxNsprPR_ImportTCPSocket
#define PR_ImportUDPSocket VBoxNsprPR_ImportUDPSocket
#define PR_CreateSocketPollFd VBoxNsprPR_CreateSocketPollFd
#define PR_DestroySocketPollFd VBoxNsprPR_DestroySocketPollFd
#define PR_Socket VBoxNsprPR_Socket
#define PR_LockFile VBoxNsprPR_LockFile
#define PR_TLockFile VBoxNsprPR_TLockFile
#define PR_UnlockFile VBoxNsprPR_UnlockFile
#define PR_EmulateAcceptRead VBoxNsprPR_EmulateAcceptRead
#define PR_EmulateSendFile VBoxNsprPR_EmulateSendFile
#define PR_NTFast_AcceptRead VBoxNsprPR_NTFast_AcceptRead
#define PR_NTFast_AcceptRead_WithTimeoutCallback VBoxNsprPR_NTFast_AcceptRead_WithTimeoutCallback
#define PR_NTFast_Accept VBoxNsprPR_NTFast_Accept
#define PR_NTFast_UpdateAcceptContext VBoxNsprPR_NTFast_UpdateAcceptContext
#define PR_NT_CancelIo VBoxNsprPR_NT_CancelIo
#define PR_Init_Log VBoxNsprPR_Init_Log
#endif /* VBOX_WITH_XPCOM_NAMESPACE_CLEANUP */

PR_BEGIN_EXTERN_C

/************************************************************************/
/************************************************************************/

/* Return the method tables for files, tcp sockets and udp sockets */
NSPR_API(const PRIOMethods*)    PR_GetFileMethods(void);
NSPR_API(const PRIOMethods*)    PR_GetTCPMethods(void);
NSPR_API(const PRIOMethods*)    PR_GetUDPMethods(void);
NSPR_API(const PRIOMethods*)    PR_GetPipeMethods(void);

/*
** Convert a NSPR Socket Handle to a Native Socket handle.
** This function will be obsoleted with the next release; avoid using it.
*/
NSPR_API(PRInt32)      PR_FileDesc2NativeHandle(PRFileDesc *);
NSPR_API(void)         PR_ChangeFileDescNativeHandle(PRFileDesc *, PRInt32);
NSPR_API(PRFileDesc*)  PR_AllocFileDesc(PRInt32 osfd,
                                         const PRIOMethods *methods);
NSPR_API(void)         PR_FreeFileDesc(PRFileDesc *fd);
/*
** Import an existing OS file to NSPR. 
*/
NSPR_API(PRFileDesc*)  PR_ImportFile(PRInt32 osfd);
NSPR_API(PRFileDesc*)  PR_ImportPipe(PRInt32 osfd);
NSPR_API(PRFileDesc*)  PR_ImportTCPSocket(PRInt32 osfd);
NSPR_API(PRFileDesc*)  PR_ImportUDPSocket(PRInt32 osfd);


/*
 *************************************************************************
 * FUNCTION: PR_CreateSocketPollFd
 * DESCRIPTION:
 *     Create a PRFileDesc wrapper for a native socket handle, for use with
 *	   PR_Poll only
 * INPUTS:
 *     None
 * OUTPUTS:
 *     None
 * RETURN: PRFileDesc*
 *     Upon successful completion, PR_CreateSocketPollFd returns a pointer
 *     to the PRFileDesc created for the native socket handle
 *     Returns a NULL pointer if the create of a new PRFileDesc failed
 *
 **************************************************************************
 */

NSPR_API(PRFileDesc*)	PR_CreateSocketPollFd(PRInt32 osfd);

/*
 *************************************************************************
 * FUNCTION: PR_DestroySocketPollFd
 * DESCRIPTION:
 *     Destroy the PRFileDesc wrapper created by PR_CreateSocketPollFd
 * INPUTS:
 *     None
 * OUTPUTS:
 *     None
 * RETURN: PRFileDesc*
 *     Upon successful completion, PR_DestroySocketPollFd returns
 *	   PR_SUCCESS, else PR_FAILURE
 *
 **************************************************************************
 */

NSPR_API(PRStatus) PR_DestroySocketPollFd(PRFileDesc *fd);


/*
** Macros for PR_Socket
**
** Socket types: PR_SOCK_STREAM, PR_SOCK_DGRAM
*/

#define PR_SOCK_STREAM SOCK_STREAM
#define PR_SOCK_DGRAM SOCK_DGRAM

/*
** Create a new Socket; this function is obsolete.
*/
NSPR_API(PRFileDesc*)	PR_Socket(PRInt32 domain, PRInt32 type, PRInt32 proto);

/*
** Emulate acceptread by accept and recv.
*/
NSPR_API(PRInt32) PR_EmulateAcceptRead(PRFileDesc *sd, PRFileDesc **nd,
    PRNetAddr **raddr, void *buf, PRInt32 amount, PRIntervalTime timeout);

PR_END_EXTERN_C

#endif /* pprio_h___ */
