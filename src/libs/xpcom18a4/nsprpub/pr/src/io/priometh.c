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
#include "primpl.h"

#include <string.h>

/*****************************************************************************/
/************************** Invalid I/O method object ************************/
/*****************************************************************************/
PRIOMethods _pr_faulty_methods = {
    (PRDescType)0,
    (PRCloseFN)_PR_InvalidStatus,
    (PRReadFN)_PR_InvalidInt,
    (PRWriteFN)_PR_InvalidInt,
    (PRAvailableFN)_PR_InvalidInt,
    (PRAvailable64FN)_PR_InvalidInt64,
    (PRFsyncFN)_PR_InvalidStatus,
    (PRSeekFN)_PR_InvalidInt,
    (PRSeek64FN)_PR_InvalidInt64,
    (PRFileInfoFN)_PR_InvalidStatus,
    (PRFileInfo64FN)_PR_InvalidStatus,
    (PRWritevFN)_PR_InvalidInt,        
    (PRConnectFN)_PR_InvalidStatus,        
    (PRAcceptFN)_PR_InvalidDesc,        
    (PRBindFN)_PR_InvalidStatus,        
    (PRListenFN)_PR_InvalidStatus,        
    (PRShutdownFN)_PR_InvalidStatus,    
    (PRRecvFN)_PR_InvalidInt,        
    (PRSendFN)_PR_InvalidInt,        
    (PRRecvfromFN)_PR_InvalidInt,    
    (PRSendtoFN)_PR_InvalidInt,        
    (PRPollFN)_PR_InvalidInt16,
    (PRAcceptreadFN)_PR_InvalidInt,   
    (PRGetsocknameFN)_PR_InvalidStatus,    
    (PRGetpeernameFN)_PR_InvalidStatus,    
    (PRReservedFN)_PR_InvalidInt,    
    (PRReservedFN)_PR_InvalidInt,    
    (PRGetsocketoptionFN)_PR_InvalidStatus,
    (PRSetsocketoptionFN)_PR_InvalidStatus,
    (PRConnectcontinueFN)_PR_InvalidStatus,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt
};

PRIntn _PR_InvalidInt(void)
{
    PR_ASSERT(!"I/O method is invalid");
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}  /* _PR_InvalidInt */

PRInt16 _PR_InvalidInt16(void)
{
    PR_ASSERT(!"I/O method is invalid");
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}  /* _PR_InvalidInt */

PRInt64 _PR_InvalidInt64(void)
{
    PRInt64 rv;
    LL_I2L(rv, -1);
    PR_ASSERT(!"I/O method is invalid");
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return rv;
}  /* _PR_InvalidInt */

/*
 * An invalid method that returns PRStatus
 */

PRStatus _PR_InvalidStatus(void)
{
    PR_ASSERT(!"I/O method is invalid");
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}  /* _PR_InvalidDesc */

/*
 * An invalid method that returns a pointer
 */

PRFileDesc *_PR_InvalidDesc(void)
{
    PR_ASSERT(!"I/O method is invalid");
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return NULL;
}  /* _PR_InvalidDesc */

PR_IMPLEMENT(PRDescType) PR_GetDescType(PRFileDesc *file)
{
    return file->methods->file_type;
}

PR_IMPLEMENT(PRStatus) PR_Close(PRFileDesc *fd)
{
    return (fd->methods->close)(fd);
}

PR_IMPLEMENT(PRInt32) PR_Read(PRFileDesc *fd, void *buf, PRInt32 amount)
{
	return((fd->methods->read)(fd,buf,amount));
}

PR_IMPLEMENT(PRInt32) PR_Write(PRFileDesc *fd, const void *buf, PRInt32 amount)
{
	return((fd->methods->write)(fd,buf,amount));
}

PR_IMPLEMENT(PRInt32) PR_Seek(PRFileDesc *fd, PRInt32 offset, PRSeekWhence whence)
{
	return((fd->methods->seek)(fd, offset, whence));
}

PR_IMPLEMENT(PRInt64) PR_Seek64(PRFileDesc *fd, PRInt64 offset, PRSeekWhence whence)
{
	return((fd->methods->seek64)(fd, offset, whence));
}

PR_IMPLEMENT(PRInt32) PR_Available(PRFileDesc *fd)
{
	return((fd->methods->available)(fd));
}

PR_IMPLEMENT(PRInt64) PR_Available64(PRFileDesc *fd)
{
	return((fd->methods->available64)(fd));
}

PR_IMPLEMENT(PRStatus) PR_GetOpenFileInfo(PRFileDesc *fd, PRFileInfo *info)
{
	return((fd->methods->fileInfo)(fd, info));
}

PR_IMPLEMENT(PRStatus) PR_GetOpenFileInfo64(PRFileDesc *fd, PRFileInfo64 *info)
{
	return((fd->methods->fileInfo64)(fd, info));
}

PR_IMPLEMENT(PRStatus) PR_Sync(PRFileDesc *fd)
{
	return((fd->methods->fsync)(fd));
}

PR_IMPLEMENT(PRStatus) PR_Connect(
    PRFileDesc *fd, const PRNetAddr *addr, PRIntervalTime timeout)
{
	return((fd->methods->connect)(fd,addr,timeout));
}

PR_IMPLEMENT(PRStatus) PR_ConnectContinue(
    PRFileDesc *fd, PRInt16 out_flags)
{
	return((fd->methods->connectcontinue)(fd,out_flags));
}

PR_IMPLEMENT(PRFileDesc*) PR_Accept(PRFileDesc *fd, PRNetAddr *addr,
PRIntervalTime timeout)
{
	return((fd->methods->accept)(fd,addr,timeout));
}

PR_IMPLEMENT(PRStatus) PR_Bind(PRFileDesc *fd, const PRNetAddr *addr)
{
	return((fd->methods->bind)(fd,addr));
}

PR_IMPLEMENT(PRStatus) PR_Shutdown(PRFileDesc *fd, PRShutdownHow how)
{
	return((fd->methods->shutdown)(fd,how));
}

PR_IMPLEMENT(PRStatus) PR_Listen(PRFileDesc *fd, PRIntn backlog)
{
	return((fd->methods->listen)(fd,backlog));
}

PR_IMPLEMENT(PRInt32) PR_Recv(PRFileDesc *fd, void *buf, PRInt32 amount,
PRIntn flags, PRIntervalTime timeout)
{
	return((fd->methods->recv)(fd,buf,amount,flags,timeout));
}

PR_IMPLEMENT(PRInt32) PR_Send(PRFileDesc *fd, const void *buf, PRInt32 amount,
PRIntn flags, PRIntervalTime timeout)
{
	return((fd->methods->send)(fd,buf,amount,flags,timeout));
}

PR_IMPLEMENT(PRInt32) PR_Writev(PRFileDesc *fd, const PRIOVec *iov,
PRInt32 iov_size, PRIntervalTime timeout)
{
    if (iov_size > PR_MAX_IOVECTOR_SIZE)
    {
        PR_SetError(PR_BUFFER_OVERFLOW_ERROR, 0);
        return -1;
    }
	return((fd->methods->writev)(fd,iov,iov_size,timeout));
}

PR_IMPLEMENT(PRInt32) PR_RecvFrom(PRFileDesc *fd, void *buf, PRInt32 amount,
PRIntn flags, PRNetAddr *addr, PRIntervalTime timeout)
{
	return((fd->methods->recvfrom)(fd,buf,amount,flags,addr,timeout));
}

PR_IMPLEMENT(PRInt32) PR_SendTo(
    PRFileDesc *fd, const void *buf, PRInt32 amount,
    PRIntn flags, const PRNetAddr *addr, PRIntervalTime timeout)
{
	return((fd->methods->sendto)(fd,buf,amount,flags,addr,timeout));
}

PR_IMPLEMENT(PRInt32) PR_AcceptRead(
    PRFileDesc *sd, PRFileDesc **nd, PRNetAddr **raddr,
    void *buf, PRInt32 amount, PRIntervalTime timeout)
{
	return((sd->methods->acceptread)(sd, nd, raddr, buf, amount,timeout));
}

PR_IMPLEMENT(PRStatus) PR_GetSockName(PRFileDesc *fd, PRNetAddr *addr)
{
	return((fd->methods->getsockname)(fd,addr));
}

PR_IMPLEMENT(PRStatus) PR_GetPeerName(PRFileDesc *fd, PRNetAddr *addr)
{
	return((fd->methods->getpeername)(fd,addr));
}

PR_IMPLEMENT(PRStatus) PR_GetSocketOption(
    PRFileDesc *fd, PRSocketOptionData *data)
{
	return((fd->methods->getsocketoption)(fd, data));
}

PR_IMPLEMENT(PRStatus) PR_SetSocketOption(
    PRFileDesc *fd, const PRSocketOptionData *data)
{
	return((fd->methods->setsocketoption)(fd, data));
}

PR_IMPLEMENT(PRInt32) PR_EmulateAcceptRead(
    PRFileDesc *sd, PRFileDesc **nd, PRNetAddr **raddr,
    void *buf, PRInt32 amount, PRIntervalTime timeout)
{
    PRInt32 rv = -1;
    PRNetAddr remote;
    PRFileDesc *accepted = NULL;

    /*
    ** The timeout does not apply to the accept portion of the
    ** operation - it waits indefinitely.
    */
    accepted = PR_Accept(sd, &remote, PR_INTERVAL_NO_TIMEOUT);
    if (NULL == accepted) return rv;

    rv = PR_Recv(accepted, buf, amount, 0, timeout);
    if (rv >= 0)
    {
        /* copy the new info out where caller can see it */
#define AMASK ((PRPtrdiff)7)  /* mask for alignment of PRNetAddr */
        PRPtrdiff aligned = (PRPtrdiff)buf + amount + AMASK;
        *raddr = (PRNetAddr*)(aligned & ~AMASK);
        memcpy(*raddr, &remote, PR_NETADDR_SIZE(&remote));
        *nd = accepted;
        return rv;
    }

    PR_Close(accepted);
    return rv;
}

/* priometh.c */
