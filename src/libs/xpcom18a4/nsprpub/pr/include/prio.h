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
 * File:     prio.h
 *
 * Description:    PR i/o related stuff, such as file system access, file
 *         i/o, socket i/o, etc.
 */

#ifndef prio_h___
#define prio_h___

#include "prlong.h"
#include "prtime.h"
#include "prinrval.h"
#include "prinet.h"

#ifdef VBOX_WITH_XPCOM_NAMESPACE_CLEANUP
#define PR_GetInheritedFD VBoxNsprPR_GetInheritedFD
#define PR_SetFDInheritable VBoxNsprPR_SetFDInheritable
#define PR_Open VBoxNsprPR_Open
#define PR_Read VBoxNsprPR_Read
#define PR_Write VBoxNsprPR_Write
#define PR_Seek VBoxNsprPR_Seek
#define PR_Seek64 VBoxNsprPR_Seek64
#define PR_Poll VBoxNsprPR_Poll
#define PR_NewPollableEvent VBoxNsprPR_NewPollableEvent
#define PR_SetPollableEvent VBoxNsprPR_SetPollableEvent
#define PR_WaitForPollableEvent VBoxNsprPR_WaitForPollableEvent
#define PR_DestroyPollableEvent VBoxNsprPR_DestroyPollableEvent
#define PR_Close VBoxNsprPR_Close
#define PR_GetSpecialFD VBoxNsprPR_GetSpecialFD
#define PR_Connect VBoxNsprPR_Connect
#define PR_OpenTCPSocket VBoxNsprPR_OpenTCPSocket
#define PR_SetSocketOption VBoxNsprPR_SetSocketOption
#define PR_Bind VBoxNsprPR_Bind
#define PR_Listen VBoxNsprPR_Listen
#define PR_Accept VBoxNsprPR_Accept
#define PR_CreatePipe VBoxNsprPR_CreatePipe
#define PR_GetDescType VBoxNsprPR_GetDescType
#define PR_GetSpecialFD VBoxNsprPR_GetSpecialFD
#define PR_GetUniqueIdentity VBoxNsprPR_GetUniqueIdentity
#define PR_GetNameForIdentity VBoxNsprPR_GetNameForIdentity
#define PR_GetLayersIdentity VBoxNsprPR_GetLayersIdentity
#define PR_GetIdentitiesLayer VBoxNsprPR_GetIdentitiesLayer
#define PR_GetDefaultIOMethods VBoxNsprPR_GetDefaultIOMethods
#define PR_CreateIOLayerStub VBoxNsprPR_CreateIOLayerStub
#define PR_CreateIOLayer VBoxNsprPR_CreateIOLayer
#define PR_PushIOLayer VBoxNsprPR_PushIOLayer
#define PR_PopIOLayer VBoxNsprPR_PopIOLayer
#define PR_OpenFile VBoxNsprPR_OpenFile
#define PR_Writev VBoxNsprPR_Writev
#define PR_Delete VBoxNsprPR_Delete
#define PR_GetFileInfo VBoxNsprPR_GetFileInfo
#define PR_GetFileInfo64 VBoxNsprPR_GetFileInfo64
#define PR_GetOpenFileInfo VBoxNsprPR_GetOpenFileInfo
#define PR_GetOpenFileInfo64 VBoxNsprPR_GetOpenFileInfo64
#define PR_Available VBoxNsprPR_Available
#define PR_Sync VBoxNsprPR_Sync
#define PR_OpenTCPSocket VBoxNsprPR_OpenTCPSocket
#define PR_ConnectContinue VBoxNsprPR_ConnectContinue
#define PR_GetConnectStatus VBoxNsprPR_GetConnectStatus
#define PR_Shutdown VBoxNsprPR_Shutdown
#define PR_Recv VBoxNsprPR_Recv
#define PR_Send VBoxNsprPR_Send
#define PR_TransmitFile VBoxNsprPR_TransmitFile
#define PR_GetSockName VBoxNsprPR_GetSockName
#define PR_GetPeerName VBoxNsprPR_GetPeerName
#define PR_GetSocketOption VBoxNsprPR_GetSocketOption
#define PR_CreatePipe VBoxNsprPR_CreatePipe
#endif /* VBOX_WITH_XPCOM_NAMESPACE_CLEANUP */

PR_BEGIN_EXTERN_C

/* Typedefs */
typedef struct PRDir            PRDir;
typedef struct PRDirEntry       PRDirEntry;
typedef struct PRFileDesc       PRFileDesc;
typedef struct PRFileInfo       PRFileInfo;
typedef struct PRFileInfo64     PRFileInfo64;
typedef union  PRNetAddr        PRNetAddr;
typedef struct PRIOMethods      PRIOMethods;
typedef struct PRPollDesc       PRPollDesc;
typedef struct PRFilePrivate    PRFilePrivate;

/*
***************************************************************************
** The file descriptor.
** This is the primary structure to represent any active open socket,
** whether it be a normal file or a network connection. Such objects
** are stackable (or layerable). Each layer may have its own set of
** method pointers and context private to that layer. All each layer
** knows about its neighbors is how to get to their method table.
***************************************************************************
*/

typedef PRIntn PRDescIdentity;          /* see: Layering file descriptors */

struct PRFileDesc {
    const PRIOMethods *methods;         /* the I/O methods table */
    PRFilePrivate *secret;              /* layer dependent data */
    PRFileDesc *lower, *higher;         /* pointers to adjacent layers */
    void (PR_CALLBACK *dtor)(PRFileDesc *fd);
                                        /* A destructor function for layer */
    PRDescIdentity identity;            /* Identity of this particular layer  */
};

/*
***************************************************************************
** PRTransmitFileFlags
**
** Flags for PR_TransmitFile.  Pass PR_TRANSMITFILE_CLOSE_SOCKET to
** PR_TransmitFile if the connection should be closed after the file
** is transmitted.
***************************************************************************
*/
typedef enum PRTransmitFileFlags {
    PR_TRANSMITFILE_KEEP_OPEN = 0,    /* socket is left open after file
                                       * is transmitted. */
    PR_TRANSMITFILE_CLOSE_SOCKET = 1  /* socket is closed after file
                                       * is transmitted. */
} PRTransmitFileFlags;

/*
**************************************************************************
** Macros for PRNetAddr
**
** Address families: PR_AF_INET, PR_AF_INET6, PR_AF_LOCAL
** IP addresses: PR_INADDR_ANY, PR_INADDR_LOOPBACK, PR_INADDR_BROADCAST
**************************************************************************
*/

#ifdef WIN32

#define PR_AF_INET 2
#define PR_AF_LOCAL 1
#define PR_INADDR_ANY (unsigned long)0x00000000
#define PR_INADDR_LOOPBACK 0x7f000001
#define PR_INADDR_BROADCAST (unsigned long)0xffffffff

#else /* WIN32 */

#define PR_AF_INET AF_INET
#define PR_AF_LOCAL AF_UNIX
#define PR_INADDR_ANY INADDR_ANY
#define PR_INADDR_LOOPBACK INADDR_LOOPBACK
#define PR_INADDR_BROADCAST INADDR_BROADCAST

#endif /* WIN32 */

/*
** Define PR_AF_INET6 in prcpucfg.h with the same
** value as AF_INET6 on platforms with IPv6 support.
** Otherwise define it here.
*/
#ifndef PR_AF_INET6
#define PR_AF_INET6 100
#endif

#ifndef PR_AF_UNSPEC
#define PR_AF_UNSPEC 0
#endif

/*
**************************************************************************
** A network address
**
** Only Internet Protocol (IPv4 and IPv6) addresses are supported.
** The address family must always represent IPv4 (AF_INET, probably == 2)
** or IPv6 (AF_INET6).
**************************************************************************
*************************************************************************/

struct PRIPv6Addr {
	union {
		PRUint8  _S6_u8[16];
		PRUint16 _S6_u16[8];
		PRUint32 _S6_u32[4];
		PRUint64 _S6_u64[2];
	} _S6_un;
};
#define pr_s6_addr		_S6_un._S6_u8
#define pr_s6_addr16	_S6_un._S6_u16
#define pr_s6_addr32	_S6_un._S6_u32
#define pr_s6_addr64 	_S6_un._S6_u64

typedef struct PRIPv6Addr PRIPv6Addr;

union PRNetAddr {
    struct {
        PRUint16 family;                /* address family (0x00ff maskable) */
        char data[14];                  /* raw address data */
    } raw;
    struct {
        PRUint16 family;                /* address family (AF_INET) */
        PRUint16 port;                  /* port number */
        PRUint32 ip;                    /* The actual 32 bits of address */
        char pad[8];
    } inet;
    struct {
        PRUint16 family;                /* address family (AF_INET6) */
        PRUint16 port;                  /* port number */
        PRUint32 flowinfo;              /* routing information */
        PRIPv6Addr ip;                  /* the actual 128 bits of address */
        PRUint32 scope_id;              /* set of interfaces for a scope */
    } ipv6;
#if defined(XP_UNIX)
    struct {                            /* Unix domain socket address */
        PRUint16 family;                /* address family (AF_UNIX) */
        char path[104];                 /* null-terminated pathname */
    } local;
#endif
};

/*
***************************************************************************
** PRSockOption
**
** The file descriptors can have predefined options set after they file
** descriptor is created to change their behavior. Only the options in
** the following enumeration are supported.
***************************************************************************
*/
typedef enum PRSockOption
{
    PR_SockOpt_Nonblocking,     /* nonblocking io */
    PR_SockOpt_Linger,          /* linger on close if data present */
    PR_SockOpt_Reuseaddr,       /* allow local address reuse */
    PR_SockOpt_Keepalive,       /* keep connections alive */
    PR_SockOpt_RecvBufferSize,  /* send buffer size */
    PR_SockOpt_SendBufferSize,  /* receive buffer size */

    PR_SockOpt_IpTimeToLive,    /* time to live */
    PR_SockOpt_IpTypeOfService, /* type of service and precedence */

    PR_SockOpt_AddMember,       /* add an IP group membership */
    PR_SockOpt_DropMember,      /* drop an IP group membership */
    PR_SockOpt_McastInterface,  /* multicast interface address */
    PR_SockOpt_McastTimeToLive, /* multicast timetolive */
    PR_SockOpt_McastLoopback,   /* multicast loopback */

    PR_SockOpt_NoDelay,         /* don't delay send to coalesce packets */
    PR_SockOpt_MaxSegment,      /* maximum segment size */
    PR_SockOpt_Broadcast,       /* enable broadcast */
    PR_SockOpt_Last
} PRSockOption;

typedef struct PRLinger {
	PRBool polarity;		    /* Polarity of the option's setting */
	PRIntervalTime linger;	    /* Time to linger before closing */
} PRLinger;

typedef struct PRMcastRequest {
	PRNetAddr mcaddr;			/* IP multicast address of group */
	PRNetAddr ifaddr;			/* local IP address of interface */
} PRMcastRequest;

typedef struct PRSocketOptionData
{
    PRSockOption option;
    union
    {
        PRUintn ip_ttl;             /* IP time to live */
        PRUintn mcast_ttl;          /* IP multicast time to live */
        PRUintn tos;                /* IP type of service and precedence */
        PRBool non_blocking;        /* Non-blocking (network) I/O */
        PRBool reuse_addr;          /* Allow local address reuse */
        PRBool keep_alive;          /* Keep connections alive */
        PRBool mcast_loopback;      /* IP multicast loopback */
        PRBool no_delay;            /* Don't delay send to coalesce packets */
        PRBool broadcast;           /* Enable broadcast */
        PRSize max_segment;         /* Maximum segment size */
        PRSize recv_buffer_size;    /* Receive buffer size */
        PRSize send_buffer_size;    /* Send buffer size */
        PRLinger linger;            /* Time to linger on close if data present */
        PRMcastRequest add_member;  /* add an IP group membership */
        PRMcastRequest drop_member; /* Drop an IP group membership */
        PRNetAddr mcast_if;         /* multicast interface address */
    } value;
} PRSocketOptionData;

/*
***************************************************************************
** PRIOVec
**
** The I/O vector is used by the write vector method to describe the areas
** that are affected by the ouput operation.
***************************************************************************
*/
typedef struct PRIOVec {
    char *iov_base;
    int iov_len;
} PRIOVec;

/*
***************************************************************************
** Discover what type of socket is being described by the file descriptor.
***************************************************************************
*/
typedef enum PRDescType
{
    PR_DESC_FILE = 1,
    PR_DESC_SOCKET_TCP = 2,
    PR_DESC_SOCKET_UDP = 3,
    PR_DESC_LAYERED = 4,
    PR_DESC_PIPE = 5
} PRDescType;

typedef enum PRSeekWhence {
    PR_SEEK_SET = 0,
    PR_SEEK_CUR = 1,
    PR_SEEK_END = 2
} PRSeekWhence;

NSPR_API(PRDescType) PR_GetDescType(PRFileDesc *file);

/*
***************************************************************************
** PRIOMethods
**
** The I/O methods table provides procedural access to the functions of
** the file descriptor. It is the responsibility of a layer implementor
** to provide suitable functions at every entry point. If a layer provides
** no functionality, it should call the next lower(higher) function of the
** same name (e.g., return fd->lower->method->close(fd->lower));
**
** Not all functions are implemented for all types of files. In cases where
** that is true, the function will return a error indication with an error
** code of PR_INVALID_METHOD_ERROR.
***************************************************************************
*/

typedef PRStatus (PR_CALLBACK *PRCloseFN)(PRFileDesc *fd);
typedef PRInt32 (PR_CALLBACK *PRReadFN)(PRFileDesc *fd, void *buf, PRInt32 amount);
typedef PRInt32 (PR_CALLBACK *PRWriteFN)(PRFileDesc *fd, const void *buf, PRInt32 amount);
typedef PRInt32 (PR_CALLBACK *PRAvailableFN)(PRFileDesc *fd);
typedef PRStatus (PR_CALLBACK *PRFsyncFN)(PRFileDesc *fd);
typedef PROffset32 (PR_CALLBACK *PRSeekFN)(PRFileDesc *fd, PROffset32 offset, PRSeekWhence how);
typedef PROffset64 (PR_CALLBACK *PRSeek64FN)(PRFileDesc *fd, PROffset64 offset, PRSeekWhence how);
typedef PRStatus (PR_CALLBACK *PRFileInfoFN)(PRFileDesc *fd, PRFileInfo *info);
typedef PRStatus (PR_CALLBACK *PRFileInfo64FN)(PRFileDesc *fd, PRFileInfo64 *info);
typedef PRInt32 (PR_CALLBACK *PRWritevFN)(
    PRFileDesc *fd, const PRIOVec *iov, PRInt32 iov_size,
    PRIntervalTime timeout);
typedef PRStatus (PR_CALLBACK *PRConnectFN)(
    PRFileDesc *fd, const PRNetAddr *addr, PRIntervalTime timeout);
typedef PRFileDesc* (PR_CALLBACK *PRAcceptFN) (
    PRFileDesc *fd, PRNetAddr *addr, PRIntervalTime timeout);
typedef PRStatus (PR_CALLBACK *PRBindFN)(PRFileDesc *fd, const PRNetAddr *addr);
typedef PRStatus (PR_CALLBACK *PRListenFN)(PRFileDesc *fd, PRIntn backlog);
typedef PRStatus (PR_CALLBACK *PRShutdownFN)(PRFileDesc *fd, PRIntn how);
typedef PRInt32 (PR_CALLBACK *PRRecvFN)(
    PRFileDesc *fd, void *buf, PRInt32 amount,
    PRIntn flags, PRIntervalTime timeout);
typedef PRInt32 (PR_CALLBACK *PRSendFN) (
    PRFileDesc *fd, const void *buf, PRInt32 amount,
    PRIntn flags, PRIntervalTime timeout);
typedef PRInt16 (PR_CALLBACK *PRPollFN)(
    PRFileDesc *fd, PRInt16 in_flags, PRInt16 *out_flags);
typedef PRStatus (PR_CALLBACK *PRGetsocknameFN)(PRFileDesc *fd, PRNetAddr *addr);
typedef PRStatus (PR_CALLBACK *PRGetpeernameFN)(PRFileDesc *fd, PRNetAddr *addr);
typedef PRStatus (PR_CALLBACK *PRGetsocketoptionFN)(
    PRFileDesc *fd, PRSocketOptionData *data);
typedef PRStatus (PR_CALLBACK *PRSetsocketoptionFN)(
    PRFileDesc *fd, const PRSocketOptionData *data);
typedef PRStatus (PR_CALLBACK *PRConnectcontinueFN)(
    PRFileDesc *fd, PRInt16 out_flags);
typedef PRIntn (PR_CALLBACK *PRReservedFN)(PRFileDesc *fd);

struct PRIOMethods {
    PRDescType file_type;           /* Type of file represented (tos)           */
    PRCloseFN close;                /* close file and destroy descriptor        */
    PRReadFN read;                  /* read up to specified bytes into buffer   */
    PRWriteFN write;                /* write specified bytes from buffer        */
    PRAvailableFN available;        /* determine number of bytes available      */
    PRFsyncFN fsync;                /* flush all buffers to permanent store     */
    PRSeekFN seek;                  /* position the file to the desired place   */
    PRSeek64FN seek64;              /*           ditto, 64 bit                  */
    PRFileInfoFN fileInfo;          /* Get information about an open file       */
    PRFileInfo64FN fileInfo64;      /*           ditto, 64 bit                  */
    PRWritevFN writev;              /* Write segments as described by iovector  */
    PRConnectFN connect;            /* Connect to the specified (net) address   */
    PRAcceptFN accept;              /* Accept a connection for a (net) peer     */
    PRBindFN bind;                  /* Associate a (net) address with the fd    */
    PRListenFN listen;              /* Prepare to listen for (net) connections  */
    PRShutdownFN shutdown;          /* Shutdown a (net) connection              */
    PRRecvFN recv;                  /* Solicit up the the specified bytes       */
    PRSendFN send;                  /* Send all the bytes specified             */
    PRPollFN poll;                  /* Test the fd to see if it is ready        */
    PRGetsocknameFN getsockname;    /* Get (net) address associated with fd     */
    PRGetpeernameFN getpeername;    /* Get peer's (net) address                 */
    PRReservedFN reserved_fn_6;     /* reserved for future use */
    PRReservedFN reserved_fn_5;     /* reserved for future use */
    PRGetsocketoptionFN getsocketoption;
                                    /* Get current setting of specified option  */
    PRSetsocketoptionFN setsocketoption;
                                    /* Set value of specified option            */
    PRConnectcontinueFN connectcontinue;
                                    /* Continue a nonblocking connect */
    PRReservedFN reserved_fn_3;		/* reserved for future use */
    PRReservedFN reserved_fn_2;		/* reserved for future use */
    PRReservedFN reserved_fn_1;		/* reserved for future use */
    PRReservedFN reserved_fn_0;		/* reserved for future use */
};

/*
 **************************************************************************
 * FUNCTION:    PR_Open
 * DESCRIPTION:    Open a file for reading, writing, or both.
 * INPUTS:
 *     const char *name
 *         The path name of the file to be opened
 *     PRIntn flags
 *         The file status flags.
 *         It is a bitwise OR of the following bit flags (only one of
 *         the first three flags below may be used):
 *		PR_RDONLY        Open for reading only.
 *		PR_WRONLY        Open for writing only.
 *		PR_RDWR          Open for reading and writing.
 *		PR_CREATE_FILE   If the file does not exist, the file is created
 *                              If the file exists, this flag has no effect.
 *      PR_SYNC          If set, each write will wait for both the file data
 *                              and file status to be physically updated.
 *		PR_APPEND        The file pointer is set to the end of
 *                              the file prior to each write.
 *		PR_TRUNCATE      If the file exists, its length is truncated to 0.
 *      PR_EXCL          With PR_CREATE_FILE, if the file does not exist,
 *                              the file is created. If the file already 
 *                              exists, no action and NULL is returned
 *
 *     PRIntn mode
 *         The access permission bits of the file mode, if the file is
 *         created when PR_CREATE_FILE is on.
 * OUTPUTS:    None
 * RETURNS:    PRFileDesc *
 *     If the file is successfully opened,
 *     returns a pointer to the PRFileDesc
 *     created for the newly opened file.
 *     Returns a NULL pointer if the open
 *     failed.
 * SIDE EFFECTS:
 * RESTRICTIONS:
 * MEMORY:
 *     The return value, if not NULL, points to a dynamically allocated
 *     PRFileDesc object.
 * ALGORITHM:
 **************************************************************************
 */

/* Open flags */
#define PR_RDONLY       0x01
#define PR_WRONLY       0x02
#define PR_RDWR         0x04
#define PR_CREATE_FILE  0x08
#define PR_APPEND       0x10
#define PR_TRUNCATE     0x20
#define PR_SYNC         0x40
#define PR_EXCL         0x80

/*
** File modes ....
**
** CAVEAT: 'mode' is currently only applicable on UNIX platforms.
** The 'mode' argument may be ignored by PR_Open on other platforms.
**
**   00400   Read by owner.
**   00200   Write by owner.
**   00100   Execute (search if a directory) by owner.
**   00040   Read by group.
**   00020   Write by group.
**   00010   Execute by group.
**   00004   Read by others.
**   00002   Write by others
**   00001   Execute by others.
**
*/

NSPR_API(PRFileDesc*) PR_Open(const char *name, PRIntn flags, PRIntn mode);

/*
 **************************************************************************
 * FUNCTION: PR_OpenFile
 * DESCRIPTION:
 *     Open a file for reading, writing, or both.
 *     PR_OpenFile has the same prototype as PR_Open but implements
 *     the specified file mode where possible.
 **************************************************************************
 */

/* File mode bits */
#define PR_IRWXU 00700  /* read, write, execute/search by owner */
#define PR_IRUSR 00400  /* read permission, owner */
#define PR_IWUSR 00200  /* write permission, owner */
#define PR_IXUSR 00100  /* execute/search permission, owner */
#define PR_IRWXG 00070  /* read, write, execute/search by group */
#define PR_IRGRP 00040  /* read permission, group */
#define PR_IWGRP 00020  /* write permission, group */
#define PR_IXGRP 00010  /* execute/search permission, group */
#define PR_IRWXO 00007  /* read, write, execute/search by others */
#define PR_IROTH 00004  /* read permission, others */
#define PR_IWOTH 00002  /* write permission, others */
#define PR_IXOTH 00001  /* execute/search permission, others */

NSPR_API(PRFileDesc*) PR_OpenFile(
    const char *name, PRIntn flags, PRIntn mode);

/*
 **************************************************************************
 * FUNCTION: PR_Close
 * DESCRIPTION:
 *     Close a file or socket.
 * INPUTS:
 *     PRFileDesc *fd
 *         a pointer to a PRFileDesc.
 * OUTPUTS:
 *     None.
 * RETURN:
 *     PRStatus
 * SIDE EFFECTS:
 * RESTRICTIONS:
 *     None.
 * MEMORY:
 *     The dynamic memory pointed to by the argument fd is freed.
 **************************************************************************
 */

NSPR_API(PRStatus)    PR_Close(PRFileDesc *fd);

/*
 **************************************************************************
 * FUNCTION: PR_Read
 * DESCRIPTION:
 *     Read bytes from a file or socket.
 *     The operation will block until either an end of stream indication is
 *     encountered, some positive number of bytes are transferred, or there
 *     is an error. No more than 'amount' bytes will be transferred.
 * INPUTS:
 *     PRFileDesc *fd
 *         pointer to the PRFileDesc object for the file or socket
 *     void *buf
 *         pointer to a buffer to hold the data read in.
 *     PRInt32 amount
 *         the size of 'buf' (in bytes)
 * OUTPUTS:
 * RETURN:
 *     PRInt32
 *         a positive number indicates the number of bytes actually read in.
 *         0 means end of file is reached or the network connection is closed.
 *         -1 indicates a failure. The reason for the failure is obtained
 *         by calling PR_GetError().
 * SIDE EFFECTS:
 *     data is written into the buffer pointed to by 'buf'.
 * RESTRICTIONS:
 *     None.
 * MEMORY:
 *     N/A
 * ALGORITHM:
 *     N/A
 **************************************************************************
 */

NSPR_API(PRInt32) PR_Read(PRFileDesc *fd, void *buf, PRInt32 amount);

/*
 ***************************************************************************
 * FUNCTION: PR_Write
 * DESCRIPTION:
 *     Write a specified number of bytes to a file or socket.  The thread
 *     invoking this function blocks until all the data is written.
 * INPUTS:
 *     PRFileDesc *fd
 *         pointer to a PRFileDesc object that refers to a file or socket
 *     const void *buf
 *         pointer to the buffer holding the data
 *     PRInt32 amount
 *         amount of data in bytes to be written from the buffer
 * OUTPUTS:
 *     None.
 * RETURN: PRInt32
 *     A positive number indicates the number of bytes successfully written.
 *     A -1 is an indication that the operation failed. The reason
 *     for the failure is obtained by calling PR_GetError().
 ***************************************************************************
 */

NSPR_API(PRInt32) PR_Write(PRFileDesc *fd,const void *buf,PRInt32 amount);

/**************************************************************************/

typedef enum PRFileType
{
    PR_FILE_FILE = 1,
    PR_FILE_DIRECTORY = 2,
    PR_FILE_OTHER = 3
} PRFileType;

struct PRFileInfo {
    PRFileType type;        /* Type of file */
    PROffset32 size;        /* Size, in bytes, of file's contents */
    PRTime creationTime;    /* Creation time per definition of PRTime */
    PRTime modifyTime;      /* Last modification time per definition of PRTime */
};

struct PRFileInfo64 {
    PRFileType type;        /* Type of file */
    PROffset64 size;        /* Size, in bytes, of file's contents */
    PRTime creationTime;    /* Creation time per definition of PRTime */
    PRTime modifyTime;      /* Last modification time per definition of PRTime */
};

/****************************************************************************
 * FUNCTION: PR_GetFileInfo, PR_GetFileInfo64
 * DESCRIPTION:
 *     Get the information about the file with the given path name. This is
 *     applicable only to NSFileDesc describing 'file' types (see
 * INPUTS:
 *     const char *fn
 *         path name of the file
 * OUTPUTS:
 *     PRFileInfo *info
 *         Information about the given file is written into the file
 *         information object pointer to by 'info'.
 * RETURN: PRStatus
 *     PR_GetFileInfo returns PR_SUCCESS if file information is successfully
 *     obtained, otherwise it returns PR_FAILURE.
 ***************************************************************************
 */

NSPR_API(PRStatus) PR_GetFileInfo(const char *fn, PRFileInfo *info);
NSPR_API(PRStatus) PR_GetFileInfo64(const char *fn, PRFileInfo64 *info);

/*
 *************************************************************************
 * FUNCTION: PR_Seek, PR_Seek64
 * DESCRIPTION:
 *     Moves read-write file offset
 * INPUTS:
 *     PRFileDesc *fd
 *         Pointer to a PRFileDesc object.
 *     PROffset32, PROffset64 offset
 *         Specifies a value, in bytes, that is used in conjunction
 *         with the 'whence' parameter to set the file pointer.  A
 *         negative value causes seeking in the reverse direction.
 *     PRSeekWhence whence
 *         Specifies how to interpret the 'offset' parameter in setting
 *         the file pointer associated with the 'fd' parameter.
 *         Values for the 'whence' parameter are:
 *             PR_SEEK_SET  Sets the file pointer to the value of the
 *                          'offset' parameter
 *             PR_SEEK_CUR  Sets the file pointer to its current location
 *                          plus the value of the offset parameter.
 *             PR_SEEK_END  Sets the file pointer to the size of the
 *                          file plus the value of the offset parameter.
 * OUTPUTS:
 *     None.
 * RETURN: PROffset32, PROffset64
 *     Upon successful completion, the resulting pointer location,
 *     measured in bytes from the beginning of the file, is returned.
 *     If the PR_Seek() function fails, the file offset remains
 *     unchanged, and the returned value is -1. The error code can
 *     then be retrieved via PR_GetError().
 *************************************************************************
 */

NSPR_API(PROffset32) PR_Seek(PRFileDesc *fd, PROffset32 offset, PRSeekWhence whence);
NSPR_API(PROffset64) PR_Seek64(PRFileDesc *fd, PROffset64 offset, PRSeekWhence whence);

PR_END_EXTERN_C

#endif /* prio_h___ */
