/** @file
 * innotek Portable Runtime - Status Codes.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___iprt_err_h
#define ___iprt_err_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

__BEGIN_DECLS

/** @defgroup grp_rt_err            RTErr - Status Codes
 * @ingroup grp_rt
 * @{
 */

/** @defgroup grp_rt_err_hlp        Status Code Helpers
 * @ingroup grp_rt_err
 * @{
 */

/** @def RT_SUCCESS
 * Check for success. We expect success in normal cases, that is the code path depending on
 * this check is normally taken. To prevent any prediction use RT_SUCCESS_NP instead.
 *
 * @returns true if rc indicates success.
 * @returns false if rc indicates failure.
 *
 * @param   rc  The iprt status code to test.
 */
#define RT_SUCCESS(rc)      ( RT_LIKELY((int)(rc) >= VINF_SUCCESS) )

/** @def RT_SUCCESS_NP
 * Check for success. Don't predict the result.
 *
 * @returns true if rc indicates success.
 * @returns false if rc indicates failure.
 *
 * @param   rc  The iprt status code to test.
 */
#define RT_SUCCESS_NP(rc)   ( (int)(rc) >= VINF_SUCCESS )

/** @def RT_FAILURE
 * Check for failure. We don't expect in normal cases, that is the code path depending on
 * this check is normally NOT taken. To prevent any prediction use RT_FAILURE_NP instead.
 *
 * @returns true if rc indicates failure.
 * @returns false if rc indicates success.
 *
 * @param   rc  The iprt status code to test.
 */
#define RT_FAILURE(rc)      ( RT_UNLIKELY(!RT_SUCCESS_NP(rc)) )

/** @def RT_FAILURE_NP
 * Check for failure. Don't predict the result.
 *
 * @returns true if rc indicates failure.
 * @returns false if rc indicates success.
 *
 * @param   rc  The iprt status code to test.
 */
#define RT_FAILURE_NP(rc)   ( !RT_SUCCESS_NP(rc) )

/**
 * Converts a Darwin HRESULT error to an iprt status code.
 *
 * @returns iprt status code.
 * @param   iNativeCode    errno code.
 * @remark  Darwin only.
 */
RTDECL(int)  RTErrConvertFromDarwinCOM(int32_t iNativeCode);

/**
 * Converts a Darwin IOReturn error to an iprt status code.
 *
 * @returns iprt status code.
 * @param   iNativeCode    errno code.
 * @remark  Darwin only.
 */
RTDECL(int)  RTErrConvertFromDarwinIO(int iNativeCode);

/**
 * Converts a Darwin kern_return_t error to an iprt status code.
 *
 * @returns iprt status code.
 * @param   iNativeCode    errno code.
 * @remark  Darwin only.
 */
RTDECL(int)  RTErrConvertFromDarwinKern(int iNativeCode);

/**
 * Converts errno to iprt status code.
 *
 * @returns iprt status code.
 * @param   uNativeCode    errno code.
 */
RTDECL(int)  RTErrConvertFromErrno(unsigned uNativeCode);

/**
 * Converts a L4 errno to a iprt status code.
 *
 * @returns iprt status code.
 * @param   uNativeCode l4 errno.
 * @remark  L4 only.
 */
RTDECL(int)  RTErrConvertFromL4Errno(unsigned uNativeCode);

/**
 * Converts NT status code to iprt status code.
 *
 * Needless to say, this is only available on NT and winXX targets.
 *
 * @returns iprt status code.
 * @param   lNativeCode    NT status code.
 * @remark  Windows only.
 */
RTDECL(int)  RTErrConvertFromNtStatus(long lNativeCode);

/**
 * Converts OS/2 error code to iprt status code.
 *
 * @returns iprt status code.
 * @param   uNativeCode    OS/2 error code.
 * @remark  OS/2 only.
 */
RTDECL(int)  RTErrConvertFromOS2(unsigned uNativeCode);

/**
 * Converts Win32 error code to iprt status code.
 *
 * @returns iprt status code.
 * @param   uNativeCode    Win32 error code.
 * @remark  Windows only.
 */
RTDECL(int)  RTErrConvertFromWin32(unsigned uNativeCode);

/**
 * Converts an iprt status code to a errno status code.
 *
 * @returns errno status code.
 * @param   iErr    iprt status code.
 */
RTDECL(int)  RTErrConvertToErrno(int iErr);


#ifdef IN_RING3

/**
 * iprt status code message.
 */
typedef struct RTSTATUSMSG
{
    /** Pointer to the short message string. */
    const char *pszMsgShort;
    /** Pointer to the full message string. */
    const char *pszMsgFull;
    /** Pointer to the define string. */
    const char *pszDefine;
    /** Status code number. */
    int         iCode;
} RTSTATUSMSG;
/** Pointer to iprt status code message. */
typedef RTSTATUSMSG *PRTSTATUSMSG;
/** Pointer to const iprt status code message. */
typedef const RTSTATUSMSG *PCRTSTATUSMSG;

/**
 * Get the message structure corresponding to a given iprt status code.
 *
 * @returns Pointer to read-only message description.
 * @param   rc      The status code.
 */
RTDECL(PCRTSTATUSMSG) RTErrGet(int rc);

/**
 * Get the define corresponding to a given iprt status code.
 *
 * @returns Pointer to read-only string with the \#define identifier.
 * @param   rc      The status code.
 */
#define RTErrGetDefine(rc)      (RTErrGet(rc)->pszDefine)

/**
 * Get the short description corresponding to a given iprt status code.
 *
 * @returns Pointer to read-only string with the description.
 * @param   rc      The status code.
 */
#define RTErrGetShort(rc)       (RTErrGet(rc)->pszMsgShort)

/**
 * Get the full description corresponding to a given iprt status code.
 *
 * @returns Pointer to read-only string with the description.
 * @param   rc      The status code.
 */
#define RTErrGetFull(rc)        (RTErrGet(rc)->pszMsgFull)

#ifdef RT_OS_WINDOWS
/**
 * Windows error code message.
 */
typedef struct RTWINERRMSG
{
    /** Pointer to the full message string. */
    const char *pszMsgFull;
    /** Pointer to the define string. */
    const char *pszDefine;
    /** Error code number. */
    long        iCode;
} RTWINERRMSG;
/** Pointer to Windows error code message. */
typedef RTWINERRMSG *PRTWINERRMSG;
/** Pointer to const Windows error code message. */
typedef const RTWINERRMSG *PCRTWINERRMSG;

/**
 * Get the message structure corresponding to a given Windows error code.
 *
 * @returns Pointer to read-only message description.
 * @param   rc      The status code.
 */
RTDECL(PCRTWINERRMSG) RTErrWinGet(long rc);
#endif /* RT_OS_WINDOWS */

#endif /* IN_RING3 */

/** @} */


/* SED-START */

/** @name Misc. Status Codes
 * @{
 */
/** Success. */
#define VINF_SUCCESS                        0

/** General failure - DON'T USE THIS!!!
 * (aka SUPDRV_ERR_GENERAL_FAILURE) */
#define VERR_GENERAL_FAILURE                (-1)
/** Invalid parameter.
 * (aka SUPDRV_ERR_INVALID_PARAM) */
#define VERR_INVALID_PARAMETER              (-2)
/** Invalid magic or cookie.
 * (aka SUPDRV_ERR_INVALID_MAGIC) */
#define VERR_INVALID_MAGIC                  (-3)
/** Invalid loader handle.
 * (aka SUPDRV_ERR_INVALID_HANDLE) */
#define VERR_INVALID_HANDLE                 (-4)
/** Failed to lock the address range.
 * (aka SUPDRV_ERR_INVALID_HANDLE) */
#define VERR_LOCK_FAILED                    (-5)
/** Invalid memory pointer.
 * (aka SUPDRV_ERR_INVALID_POINTER) */
#define VERR_INVALID_POINTER                (-6)
/** Failed to patch the IDT.
 * (aka SUPDRV_ERR_IDT_FAILED) */
#define VERR_IDT_FAILED                     (-7)
/** Memory allocation failed.
 * (aka SUPDRV_ERR_NO_MEMORY) */
#define VERR_NO_MEMORY                      (-8)
/** Already loaded.
 * (aka SUPDRV_ERR_ALREADY_LOADED) */
#define VERR_ALREADY_LOADED                 (-9)
/** Permission denied.
 * (aka SUPDRV_ERR_PERMISSION_DENIED) */
#define VERR_PERMISSION_DENIED              (-10)
/** Version mismatch.
 * (aka SUPDRV_ERR_VERSION_MISMATCH) */
#define VERR_VERSION_MISMATCH               (-11)
/** The request function is not implemented. */
#define VERR_NOT_IMPLEMENTED                (-12)

/** Failed to allocate temporary memory. */
#define VERR_NO_TMP_MEMORY                  (-20)
/** Invalid file mode mask (RTFMODE). */
#define VERR_INVALID_FMODE                  (-21)
/** Incorrect call order. */
#define VERR_WRONG_ORDER                    (-22)
/** There is no TLS (thread local storage) available for storing the current thread. */
#define VERR_NO_TLS_FOR_SELF                (-23)
/** Failed to set the TLS (thread local storage) entry which points to our thread structure. */
#define VERR_FAILED_TO_SET_SELF_TLS         (-24)
/** Not able to allocate contiguous memory. */
#define VERR_NO_CONT_MEMORY                 (-26)
/** No memory available for page table or page directory. */
#define VERR_NO_PAGE_MEMORY                 (-27)
/** Already initialized. */
#define VINF_ALREADY_INITIALIZED            28
/** The specified thread is dead. */
#define VERR_THREAD_IS_DEAD                 (-29)
/** The specified thread is not waitable. */
#define VERR_THREAD_NOT_WAITABLE            (-30)
/** Pagetable not present. */
#define VERR_PAGE_TABLE_NOT_PRESENT         (-31)
/** Internal error - we're screwed if this happens. */
#define VERR_INTERNAL_ERROR                 (-32)
/** The per process timer is busy. */
#define VERR_TIMER_BUSY                     (-33)
/** Address conflict. */
#define VERR_ADDRESS_CONFLICT               (-34)
/** Unresolved (unknown) host platform error. */
#define VERR_UNRESOLVED_ERROR               (-35)
/** Invalid function. */
#define VERR_INVALID_FUNCTION               (-36)
/** Not supported. */
#define VERR_NOT_SUPPORTED                  (-37)
/** Access denied. */
#define VERR_ACCESS_DENIED                  (-38)
/** Call interrupted. */
#define VERR_INTERRUPTED                    (-39)
/** Timeout. */
#define VERR_TIMEOUT                        (-40)
/** Buffer too small to save result. */
#define VERR_BUFFER_OVERFLOW                (-41)
/** Buffer too small to save result. */
#define VINF_BUFFER_OVERFLOW                41
/** Data size overflow. */
#define VERR_TOO_MUCH_DATA                  (-42)
/** Max threads number reached. */
#define VERR_MAX_THRDS_REACHED              (-43)
/** Max process number reached. */
#define VERR_MAX_PROCS_REACHED              (-44)
/** The recipient process has refused the signal. */
#define VERR_SIGNAL_REFUSED                 (-45)
/** A signal is already pending. */
#define VERR_SIGNAL_PENDING                 (-46)
/** The signal being posted is not correct. */
#define VERR_SIGNAL_INVALID                 (-47)
/** The state changed.
 * This is a generic error message and needs a context to make sense. */
#define VERR_STATE_CHANGED                  (-48)
/** Warning, the state changed.
 * This is a generic error message and needs a context to make sense. */
#define VWRN_STATE_CHANGED                  48
/** Error while parsing UUID string */
#define VERR_INVALID_UUID_FORMAT            (-49)
/** The specified process was not found. */
#define VERR_PROCESS_NOT_FOUND              (-50)
/** The process specified to a non-block wait had not exitted. */
#define VERR_PROCESS_RUNNING                (-51)
/** Retry the operation. */
#define VERR_TRY_AGAIN                      (-52)
/** Generic parse error. */
#define VERR_PARSE_ERROR                    (-53)
/** Value out of range. */
#define VERR_OUT_OF_RANGE                   (-54)
/** A numeric convertion encountered a value which was too big for the target. */
#define VERR_NUMBER_TOO_BIG                 (-55)
/** A numeric convertion encountered a value which was too big for the target. */
#define VWRN_NUMBER_TOO_BIG                 55
/** The number begin converted (string) contained no digits. */
#define VERR_NO_DIGITS                      (-56)
/** The number begin converted (string) contained no digits. */
#define VWRN_NO_DIGITS                      56
/** Encountered a '-' during convertion to an unsigned value. */
#define VERR_NEGATIVE_UNSIGNED              (-57)
/** Encountered a '-' during convertion to an unsigned value. */
#define VWRN_NEGATIVE_UNSIGNED              57
/** Error while characters translation (unicode and so). */
#define VERR_NO_TRANSLATION                 (-58)
/** Encountered unicode code point which is reserved for use as endian indicator (0xffff or 0xfffe). */
#define VERR_CODE_POINT_ENDIAN_INDICATOR    (-59)
/** Encountered unicode code point in the surrogate range (0xd800 to 0xdfff). */
#define VERR_CODE_POINT_SURROGATE           (-60)
/** A string claiming to be UTF-8 is incorrectly encoded. */
#define VERR_INVALID_UTF8_ENCODING          (-61)
/** Ad string claiming to be in UTF-16 is incorrectly encoded. */
#define VERR_INVALID_UTF16_ENCODING         (-62)
/** Encountered a unicode code point which cannot be represented as UTF-16. */
#define VERR_CANT_RECODE_AS_UTF16           (-63)
/** Got an out of memory condition trying to allocate a string. */
#define VERR_NO_STR_MEMORY                  (-64)
/** Got an out of memory condition trying to allocate a UTF-16 (/UCS-2) string. */
#define VERR_NO_UTF16_MEMORY                (-65)
/** Get an out of memory condition trying to allocate a code point array. */
#define VERR_NO_CODE_POINT_MEMORY           (-66)
/** Can't free the memory because it's used in mapping. */
#define VERR_MEMORY_BUSY                    (-67)
/** The timer can't be started because it's already active. */
#define VERR_TIMER_ACTIVE                   (-68)
/** The timer can't be stopped because i's already suspended. */
#define VERR_TIMER_SUSPENDED                (-69)
/** The operation was cancelled by the user. */
#define VERR_CANCELLED                      (-70)
/** Failed to initialize a memory object.
 * Exactly what this means is OS specific. */
#define VERR_MEMOBJ_INIT_FAILED             (-71)
/** Out of memory condition when allocating memory with low physical backing. */
#define VERR_NO_LOW_MEMORY                  (-72)
/** Out of memory condition when allocating physical memory (without mapping). */
#define VERR_NO_PHYS_MEMORY                 (-73)
/** The address (virtual or physical) is too big. */
#define VERR_ADDRESS_TOO_BIG                (-74)
/** Failed to map a memory object. */
#define VERR_MAP_FAILED                     (-75)
/** Trailing characters. */
#define VERR_TRAILING_CHARS                 (-76)
/** Trailing characters. */
#define VWRN_TRAILING_CHARS                 76
/** Trailing spaces. */
#define VERR_TRAILING_SPACES                (-77)
/** Trailing spaces. */
#define VWRN_TRAILING_SPACES                77
/** RTGetOpt: command line option not recognized. */
#define VERR_GETOPT_UNKNOWN_OPTION          (-78)
/** RTGetOpt: command line option needs argument. */
#define VERR_GETOPT_REQUIRED_ARGUMENT_MISSING  (-79)
/** RTGetOpt: command line option has argument with bad format. */
#define VERR_GETOPT_INVALID_ARGUMENT_FORMAT (-80)

/** @} */


/** @name Common File/Disk/Pipe/etc Status Codes
 * @{
 */
/** Unresolved (unknown) file i/o error. */
#define VERR_FILE_IO_ERROR                  (-100)
/** File/Device open failed. */
#define VERR_OPEN_FAILED                    (-101)
/** File not found. */
#define VERR_FILE_NOT_FOUND                 (-102)
/** Path not found. */
#define VERR_PATH_NOT_FOUND                 (-103)
/** Invalid (malformed) file/path name. */
#define VERR_INVALID_NAME                   (-104)
/** File/Device already exists. */
#define VERR_ALREADY_EXISTS                 (-105)
/** Too many open files. */
#define VERR_TOO_MANY_OPEN_FILES            (-106)
/** Seek error. */
#define VERR_SEEK                           (-107)
/** Seek below file start. */
#define VERR_NEGATIVE_SEEK                  (-108)
/** Trying to seek on device. */
#define VERR_SEEK_ON_DEVICE                 (-109)
/** Reached the end of the file. */
#define VERR_EOF                            (-110)
/** Reached the end of the file. */
#define VINF_EOF                            110
/** Generic file read error. */
#define VERR_READ_ERROR                     (-111)
/** Generic file write error. */
#define VERR_WRITE_ERROR                    (-112)
/** Write protect error. */
#define VERR_WRITE_PROTECT                  (-113)
/** Sharing violetion, file is being used by another process. */
#define VERR_SHARING_VIOLATION              (-114)
/** Unable to lock a region of a file. */
#define VERR_FILE_LOCK_FAILED               (-115)
/** File access error, another process has locked a portion of the file. */
#define VERR_FILE_LOCK_VIOLATION            (-116)
/** File or directory can't be created. */
#define VERR_CANT_CREATE                    (-117)
/** Directory can't be deleted. */
#define VERR_CANT_DELETE_DIRECTORY          (-118)
/** Can't move file to another disk. */
#define VERR_NOT_SAME_DEVICE                (-119)
/** The filename or extension is too long. */
#define VERR_FILENAME_TOO_LONG              (-120)
/** Media not present in drive. */
#define VERR_MEDIA_NOT_PRESENT              (-121)
/** The type of media was not recognized. Not formatted? */
#define VERR_MEDIA_NOT_RECOGNIZED           (-122)
/** Can't unlock - region was not locked. */
#define VERR_FILE_NOT_LOCKED                (-123)
/** Unrecoverable error: lock was lost. */
#define VERR_FILE_LOCK_LOST                 (-124)
/** Can't delete directory with files. */
#define VERR_DIR_NOT_EMPTY                  (-125)
/** A directory operation was attempted on a non-directory object. */
#define VERR_NOT_A_DIRECTORY                (-126)
/** A non-directory operation was attempted on a directory object. */
#define VERR_IS_A_DIRECTORY                 (-127)
/** Tried to grow a file beyond the limit imposed by the process or the filesystem. */
#define VERR_FILE_TOO_BIG                   (-128)
/** @} */


/** @name Generic Filesystem I/O Status Codes
 * @{
 */
/** Unresolved (unknown) disk i/o error.  */
#define VERR_DISK_IO_ERROR                  (-150)
/** Invalid drive number. */
#define VERR_INVALID_DRIVE                  (-151)
/** Disk is full. */
#define VERR_DISK_FULL                      (-152)
/** Disk was changed. */
#define VERR_DISK_CHANGE                    (-153)
/** Drive is locked. */
#define VERR_DRIVE_LOCKED                   (-154)
/** The specified disk or diskette cannot be accessed. */
#define VERR_DISK_INVALID_FORMAT            (-155)
/** Too many symbolic links. */
#define VERR_TOO_MANY_SYMLINKS              (-156)
/** @} */


/** @name Generic Directory Enumeration Status Codes
 * @{
 */
/** Unresolved (unknown) search error. */
#define VERR_SEARCH_ERROR                   (-200)
/** No more files found. */
#define VERR_NO_MORE_FILES                  (-201)
/** No more search handles available. */
#define VERR_NO_MORE_SEARCH_HANDLES         (-202)
/** RTDirReadEx() failed to retrieve the extra data which was requested. */
#define VWRN_NO_DIRENT_INFO                 203
/** @} */


/** @name Generic Device I/O Status Codes
 * @{
 */
/** Unresolved (unknown) device i/o error. */
#define VERR_DEV_IO_ERROR                   (-250)
/** Device i/o: Bad unit. */
#define VERR_IO_BAD_UNIT                    (-251)
/** Device i/o: Not ready. */
#define VERR_IO_NOT_READY                   (-252)
/** Device i/o: Bad command. */
#define VERR_IO_BAD_COMMAND                 (-253)
/** Device i/o: CRC error. */
#define VERR_IO_CRC                         (-254)
/** Device i/o: Bad length. */
#define VERR_IO_BAD_LENGTH                  (-255)
/** Device i/o: Sector not found. */
#define VERR_IO_SECTOR_NOT_FOUND            (-256)
/** Device i/o: General failure. */
#define VERR_IO_GEN_FAILURE                 (-257)
/** @} */


/** @name Generic Pipe I/O Status Codes
 * @{
 */
/** Unresolved (unknown) pipe i/o error. */
#define VERR_PIPE_IO_ERROR                  (-300)
/** Broken pipe. */
#define VERR_BROKEN_PIPE                    (-301)
/** Bad pipe. */
#define VERR_BAD_PIPE                       (-302)
/** Pipe is busy. */
#define VERR_PIPE_BUSY                      (-303)
/** No data in pipe. */
#define VERR_NO_DATA                        (-304)
/** Pipe is not connected. */
#define VERR_PIPE_NOT_CONNECTED             (-305)
/** More data available in pipe. */
#define VERR_MORE_DATA                      (-306)
/** @} */


/** @name Generic Semaphores Status Codes
 * @{
 */
/** Unresolved (unknown) semaphore error. */
#define VERR_SEM_ERROR                      (-350)
/** Too many semaphores. */
#define VERR_TOO_MANY_SEMAPHORES            (-351)
/** Exclusive semaphore is owned by another process. */
#define VERR_EXCL_SEM_ALREADY_OWNED         (-352)
/** The semaphore is set and cannot be closed. */
#define VERR_SEM_IS_SET                     (-353)
/** The semaphore cannot be set again. */
#define VERR_TOO_MANY_SEM_REQUESTS          (-354)
/** Attempt to release mutex not owned by caller. */
#define VERR_NOT_OWNER                      (-355)
/** The semaphore has been opened too many times. */
#define VERR_TOO_MANY_OPENS                 (-356)
/** The maximum posts for the event semaphore has been reached. */
#define VERR_TOO_MANY_POSTS                 (-357)
/** The event semaphore has already been posted. */
#define VERR_ALREADY_POSTED                 (-358)
/** The event semaphore has already been reset. */
#define VERR_ALREADY_RESET                  (-359)
/** The semaphore is in use. */
#define VERR_SEM_BUSY                       (-360)
/** The previous ownership of this semaphore has ended. */
#define VERR_SEM_OWNER_DIED                 (-361)
/** Failed to open semaphore by name - not found. */
#define VERR_SEM_NOT_FOUND                  (-362)
/** Semaphore destroyed while waiting. */
#define VERR_SEM_DESTROYED                  (-363)
/** Nested ownership requests are not permitted for this semaphore type. */
#define VERR_SEM_NESTED                     (-364)
/** Deadlock detected. */
#define VERR_DEADLOCK                       (-365)
/** Ping-Pong listen or speak out of turn error. */
#define VERR_SEM_OUT_OF_TURN                (-366)
/** @} */


/** @name Generic Network I/O Status Codes
 * @{
 */
/** Unresolved (unknown) network error. */
#define VERR_NET_IO_ERROR                       (-400)
/** The network is busy or is out of resources. */
#define VERR_NET_OUT_OF_RESOURCES               (-401)
/** Net host name not found. */
#define VERR_NET_HOST_NOT_FOUND                 (-402)
/** Network path not found. */
#define VERR_NET_PATH_NOT_FOUND                 (-403)
/** General network printing error. */
#define VERR_NET_PRINT_ERROR                    (-404)
/** The machine is not on the network. */
#define VERR_NET_NO_NETWORK                     (-405)
/** Name is not unique on the network. */
#define VERR_NET_NOT_UNIQUE_NAME                (-406)

/* These are BSD networking error codes - numbers correspond, don't mess! */
/** Operation in progress. */
#define VERR_NET_IN_PROGRESS                    (-436)
/** Operation already in progress. */
#define VERR_NET_ALREADY_IN_PROGRESS            (-437)
/** Attempted socket operation with a non-socket handle.
 * (This includes closed handles.) */
#define VERR_NET_NOT_SOCKET                     (-438)
/** Destination address required. */
#define VERR_NET_DEST_ADDRESS_REQUIRED          (-439)
/** Message too long. */
#define VERR_NET_MSG_SIZE                       (-440)
/** Protocol wrong type for socket. */
#define VERR_NET_PROTOCOL_TYPE                  (-441)
/** Protocol not available. */
#define VERR_NET_PROTOCOL_NOT_AVAILABLE         (-442)
/** Protocol not supported. */
#define VERR_NET_PROTOCOL_NOT_SUPPORTED         (-443)
/** Socket type not supported. */
#define VERR_NET_SOCKET_TYPE_NOT_SUPPORTED      (-444)
/** Operation not supported. */
#define VERR_NET_OPERATION_NOT_SUPPORTED        (-445)
/** Protocol family not supported. */
#define VERR_NET_PROTOCOL_FAMILY_NOT_SUPPORTED  (-446)
/** Address family not supported by protocol family. */
#define VERR_NET_ADDRESS_FAMILY_NOT_SUPPORTED   (-447)
/** Address already in use. */
#define VERR_NET_ADDRESS_IN_USE                 (-448)
/** Can't assign requested address. */
#define VERR_NET_ADDRESS_NOT_AVAILABLE          (-449)
/** Network is down. */
#define VERR_NET_DOWN                           (-450)
/** Network is unreachable. */
#define VERR_NET_UNREACHABLE                    (-451)
/** Network dropped connection on reset. */
#define VERR_NET_CONNECTION_RESET               (-452)
/** Software caused connection abort. */
#define VERR_NET_CONNECTION_ABORTED             (-453)
/** Connection reset by peer. */
#define VERR_NET_CONNECTION_RESET_BY_PEER       (-454)
/** No buffer space available. */
#define VERR_NET_NO_BUFFER_SPACE                (-455)
/** Socket is already connected. */
#define VERR_NET_ALREADY_CONNECTED              (-456)
/** Socket is not connected. */
#define VERR_NET_NOT_CONNECTED                  (-457)
/** Can't send after socket shutdown. */
#define VERR_NET_SHUTDOWN                       (-458)
/** Too many references: can't splice. */
#define VERR_NET_TOO_MANY_REFERENCES            (-459)
/** Too many references: can't splice. */
#define VERR_NET_CONNECTION_TIMED_OUT           (-460)
/** Connection refused. */
#define VERR_NET_CONNECTION_REFUSED             (-461)
/* ELOOP is not net. */
/* ENAMETOOLONG is not net. */
/** Host is down. */
#define VERR_NET_HOST_DOWN                      (-464)
/** No route to host. */
#define VERR_NET_HOST_UNREACHABLE               (-465)
/** @} */


/** @name TCP Status Codes
 * @{
 */
/** Stop the TCP server. */
#define VERR_TCP_SERVER_STOP                    (-500)
/** The server was stopped. */
#define VINF_TCP_SERVER_STOP                    500
/** @} */


/** @name L4 Specific Status Codes
 * @{
 */
/** Invalid offset in an L4 dataspace */
#define VERR_L4_INVALID_DS_OFFSET               (-550)
/** IPC error */
#define VERR_IPC                                (-551)
/** Item already used */
#define VERR_RESOURCE_IN_USE                    (-552)
/** Source/destination not found */
#define VERR_IPC_PROCESS_NOT_FOUND              (-553)
/** Receive timeout */
#define VERR_IPC_RECEIVE_TIMEOUT                (-554)
/** Send timeout */
#define VERR_IPC_SEND_TIMEOUT                   (-555)
/** Receive cancelled */
#define VERR_IPC_RECEIVE_CANCELLED              (-556)
/** Send cancelled */
#define VERR_IPC_SEND_CANCELLED                 (-557)
/** Receive aborted */
#define VERR_IPC_RECEIVE_ABORTED                (-558)
/** Send aborted */
#define VERR_IPC_SEND_ABORTED                   (-559)
/** Couldn't map pages during receive */
#define VERR_IPC_RECEIVE_MAP_FAILED             (-560)
/** Couldn't map pages during send */
#define VERR_IPC_SEND_MAP_FAILED                (-561)
/** Send pagefault timeout in receive */
#define VERR_IPC_RECEIVE_SEND_PF_TIMEOUT        (-562)
/** Send pagefault timeout in send */
#define VERR_IPC_SEND_SEND_PF_TIMEOUT           (-563)
/** (One) receive buffer was too small, or too few buffers */
#define VINF_IPC_RECEIVE_MSG_CUT                564
/** (One) send buffer was too small, or too few buffers */
#define VINF_IPC_SEND_MSG_CUT                   565
/** Dataspace manager server not found */
#define VERR_L4_DS_MANAGER_NOT_FOUND            (-566)
/** @} */


/** @name Loader Status Codes.
 * @{
 */
/** Invalid executable signature. */
#define VERR_INVALID_EXE_SIGNATURE              (-600)
/** The iprt loader recognized a ELF image, but doesn't support loading it. */
#define VERR_ELF_EXE_NOT_SUPPORTED              (-601)
/** The iprt loader recognized a PE image, but doesn't support loading it. */
#define VERR_PE_EXE_NOT_SUPPORTED               (-602)
/** The iprt loader recognized a LX image, but doesn't support loading it. */
#define VERR_LX_EXE_NOT_SUPPORTED               (-603)
/** The iprt loader recognized a LE image, but doesn't support loading it. */
#define VERR_LE_EXE_NOT_SUPPORTED               (-604)
/** The iprt loader recognized a NE image, but doesn't support loading it. */
#define VERR_NE_EXE_NOT_SUPPORTED               (-605)
/** The iprt loader recognized a MZ image, but doesn't support loading it. */
#define VERR_MZ_EXE_NOT_SUPPORTED               (-606)
/** The iprt loader recognized an a.out image, but doesn't support loading it. */
#define VERR_AOUT_EXE_NOT_SUPPORTED             (-607)
/** Bad executable. */
#define VERR_BAD_EXE_FORMAT                     (-608)
/** Symbol (export) not found. */
#define VERR_SYMBOL_NOT_FOUND                   (-609)
/** Module not found. */
#define VERR_MODULE_NOT_FOUND                   (-610)
/** The loader resolved an external symbol to an address to big for the image format. */
#define VERR_SYMBOL_VALUE_TOO_BIG               (-611)
/** The image is too big. */
#define VERR_IMAGE_TOO_BIG                      (-612)
/** The image base address is to high for this image type. */
#define VERR_IMAGE_BASE_TOO_HIGH                (-614)
/** The PE loader encountered delayed imports, a feature which hasn't been implemented yet. */
#define VERR_LDRPE_DELAY_IMPORT                 (-620)
/** The PE loader doesn't have a clue what the security data directory entry is all about. */
#define VERR_LDRPE_SECURITY                     (-621)
/** The PE loader doesn't know how to deal with the global pointer data directory entry yet. */
#define VERR_LDRPE_GLOBALPTR                    (-622)
/** The PE loader doesn't support the TLS data directory yet. */
#define VERR_LDRPE_TLS                          (-623)
/** The PE loader doesn't grok the COM descriptor data directory entry. */
#define VERR_LDRPE_COM_DESCRIPTOR               (-624)
/** The PE loader encountered an unknown load config directory/header size. */
#define VERR_LDRPE_LOAD_CONFIG_SIZE             (-625)
/** The PE loader encountered a lock prefix table, a feature which hasn't been implemented yet. */
#define VERR_LDRPE_LOCK_PREFIX_TABLE            (-626)
/** The ELF loader doesn't handle foreign endianness. */
#define VERR_LDRELF_ODD_ENDIAN                  (-630)
/** The ELF image is 'dynamic', the ELF loader can only deal with 'relocatable' images at present. */
#define VERR_LDRELF_DYN                         (-631)
/** The ELF image is 'executable', the ELF loader can only deal with 'relocatable' images at present. */
#define VERR_LDRELF_EXEC                        (-632)
/** The ELF image was created for an unsupported target machine type. */
#define VERR_LDRELF_MACHINE                     (-633)
/** The ELF version is not supported. */
#define VERR_LDRELF_VERSION                     (-634)
/** The ELF loader cannot handle multiple SYMTAB sections. */
#define VERR_LDRELF_MULTIPLE_SYMTABS            (-635)
/** The ELF loader encountered a relocation type which is not implemented. */
#define VERR_LDRELF_RELOCATION_NOT_SUPPORTED    (-636)
/** The ELF loader encountered a bad symbol index. */
#define VERR_LDRELF_INVALID_SYMBOL_INDEX        (-637)
/** The ELF loader encountered an invalid symbol name offset. */
#define VERR_LDRELF_INVALID_SYMBOL_NAME_OFFSET  (-638)
/** The ELF loader encountered an invalid relocation offset. */
#define VERR_LDRELF_INVALID_RELOCATION_OFFSET   (-639)
/** The ELF loader didn't find the symbol/string table for the image. */
#define VERR_LDRELF_NO_SYMBOL_OR_NO_STRING_TABS (-640)
/** @}*/

/** @name Debug Info Reader Status Codes.
 * @{
 */
/** The specified segment:offset address was invalid. Typically an attempt at
 * addressing outside the segment boundrary. */
#define VERR_DBGMOD_INVALID_ADDRESS             (-650)
/** @} */

/** @name Request Packet Status Codes.
 * @{
 */
/** Invalid RT request type.
 * For the RTReqAlloc() case, the caller just specified an illegal enmType. For
 * all the other occurences it means indicates corruption, broken logic, or stupid
 * interface user. */
#define VERR_RT_REQUEST_INVALID_TYPE            (-700)
/** Invalid RT request state.
 * The state of the request packet was not the expected and accepted one(s). Either
 * the interface user screwed up, or we've got corruption/broken logic. */
#define VERR_RT_REQUEST_STATE                   (-701)
/** Invalid RT request packet.
 * One or more of the RT controlled packet members didn't contain the correct
 * values. Some thing's broken. */
#define VERR_RT_REQUEST_INVALID_PACKAGE         (-702)
/** The status field has not been updated yet as the request is still
 * pending completion. Someone queried the iStatus field before the request
 * has been fully processed. */
#define VERR_RT_REQUEST_STATUS_STILL_PENDING    (-703)
/** The request has been freed, don't read the status now.
 * Someone is reading the iStatus field of a freed request packet. */
#define VERR_RT_REQUEST_STATUS_FREED            (-704)
/** @} */

/** @name Environment Status Code
 * @{
 */
/** The specified environment variable was not found. (RTEnvGetEx) */
#define VERR_ENV_VAR_NOT_FOUND                  (-750)
/** The specified environment variable was not found. (RTEnvUnsetEx) */
#define VINF_ENV_VAR_NOT_FOUND                  (750)
/** @} */

/** @name Multiprocessor Status Code
 * @{
 */
/** The specified cpu is offline. */
#define VERR_CPU_OFFLINE                        (-800)
/** The specified cpu was not found. */
#define VERR_CPU_NOT_FOUND                      (-801)
/** @} */

/* SED-END */

/** @} */

__END_DECLS

#endif

