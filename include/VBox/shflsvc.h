/** @file
 * Shared Folders:
 * Common header for host service and guest clients.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_shflsvc_h
#define ___VBox_shflsvc_h

#include <VBox/types.h>
#include <VBox/VBoxGuest.h>
#include <VBox/hgcmsvc.h>
#include <iprt/fs.h>


/** Some bit flag manipulation macros. to be moved to VBox/cdefs.h? */
#ifndef BIT_FLAG
#define BIT_FLAG(__Field,__Flag)       ((__Field) & (__Flag))
#endif

#ifndef BIT_FLAG_SET
#define BIT_FLAG_SET(__Field,__Flag)   ((__Field) |= (__Flag))
#endif

#ifndef BIT_FLAG_CLEAR
#define BIT_FLAG_CLEAR(__Field,__Flag) ((__Field) &= ~(__Flag))
#endif


/**
 * Structures shared between guest and the service
 * can be relocated and use offsets to point to variable
 * length parts.
 */

/**
 * Shared folders protocol works with handles.
 * Before doing any action on a file system object,
 * one have to obtain the object handle via a SHFL_FN_CREATE
 * request. A handle must be closed with SHFL_FN_CLOSE.
 */

/** Shared Folders service functions. (guest)
 *  @{
 */

/** Query mappings changes. */
#define SHFL_FN_QUERY_MAPPINGS      (1)
/** Query mappings changes. */
#define SHFL_FN_QUERY_MAP_NAME      (2)
/** Open/create object. */
#define SHFL_FN_CREATE              (3)
/** Close object handle. */
#define SHFL_FN_CLOSE               (4)
/** Read object content. */
#define SHFL_FN_READ                (5)
/** Write new object content. */
#define SHFL_FN_WRITE               (6)
/** Lock/unlock a range in the object. */
#define SHFL_FN_LOCK                (7)
/** List object content. */
#define SHFL_FN_LIST                (8)
/** Query/set object information. */
#define SHFL_FN_INFORMATION         (9)
/** Remove object */
#define SHFL_FN_REMOVE              (11)
/** Map folder (legacy) */
#define SHFL_FN_MAP_FOLDER_OLD      (12)
/** Unmap folder */
#define SHFL_FN_UNMAP_FOLDER        (13)
/** Rename object (possibly moving it to another directory) */
#define SHFL_FN_RENAME              (14)
/** Flush file */
#define SHFL_FN_FLUSH               (15)
/** @todo macl, a description, please. */
#define SHFL_FN_SET_UTF8            (16)
/** Map folder */
#define SHFL_FN_MAP_FOLDER          (17)

/** @} */

/** Shared Folders service functions. (host)
 *  @{
 */

/** Add shared folder mapping. */
#define SHFL_FN_ADD_MAPPING         (1)
/** Remove shared folder mapping. */
#define SHFL_FN_REMOVE_MAPPING      (2)
/** Set the led status light address */
#define SHFL_FN_SET_STATUS_LED      (3)

/** @} */

/** Root handle for a mapping. Root handles are unique.
 *  @note
 *  Function parameters structures consider
 *  the root handle as 32 bit value. If the typedef
 *  will be changed, then function parameters must be
 *  changed accordingly. All those parameters are marked
 *  with SHFLROOT in comments.
 */
typedef uint32_t SHFLROOT;


/** A shared folders handle for an opened object. */
typedef uint64_t SHFLHANDLE;

#define SHFL_HANDLE_NIL  ((SHFLHANDLE)~0LL)
#define SHFL_HANDLE_ROOT ((SHFLHANDLE)0LL)

/** Hardcoded maximum number of shared folder mapping available to the guest. */
#define SHFL_MAX_MAPPINGS    (64)

/** Shared Folders strings. They can be either UTF8 or Unicode.
 *  @{
 */

typedef struct _SHFLSTRING
{
    /** Size of string String buffer in bytes. */
    uint16_t u16Size;

    /** Length of string without trailing nul in bytes. */
    uint16_t u16Length;

    /** UTF8 or Unicode16 string. Nul terminated. */
    union
    {
        uint8_t  utf8[1];
        uint16_t ucs2[1];
    } String;
} SHFLSTRING;

typedef SHFLSTRING *PSHFLSTRING;

/** Calculate size of the string. */
DECLINLINE(uint32_t) ShflStringSizeOfBuffer (PSHFLSTRING pString)
{
    return pString? sizeof (SHFLSTRING) - sizeof (pString->String) + pString->u16Size: 0;
}

DECLINLINE(uint32_t) ShflStringLength (PSHFLSTRING pString)
{
    return pString? pString->u16Length: 0;
}

DECLINLINE(PSHFLSTRING) ShflStringInitBuffer(void *pvBuffer, uint32_t u32Size)
{
    PSHFLSTRING pString = NULL;

    uint32_t u32HeaderSize = sizeof (SHFLSTRING) - sizeof (pString->String);

    /* Check that the buffer size is big enough to hold a zero sized string
     * and is not too big to fit into 16 bit variables.
     */
    if (u32Size >= u32HeaderSize && u32Size - u32HeaderSize <= 0xFFFF)
    {
        pString = (PSHFLSTRING)pvBuffer;
        pString->u16Size = u32Size - u32HeaderSize;
        pString->u16Length = 0;
    }

    return pString;
}

/** @} */


/** Result of an open/create request.
 *  Along with handle value the result code
 *  identifies what has happened while
 *  trying to open the object.
 */
typedef enum _SHFLCREATERESULT
{
    SHFL_NO_RESULT,
    /** Specified path does not exist. */
    SHFL_PATH_NOT_FOUND,
    /** Path to file exists, but the last component does not. */
    SHFL_FILE_NOT_FOUND,
    /** File already exists and either has been opened or not. */
    SHFL_FILE_EXISTS,
    /** New file was created. */
    SHFL_FILE_CREATED,
    /** Existing file was replaced or overwritten. */
    SHFL_FILE_REPLACED
} SHFLCREATERESULT;


/** Open/create flags.
 *  @{
 */

/** No flags. Initialization value. */
#define SHFL_CF_NONE                  (0x00000000)

/** Lookup only the object, do not return a handle. All other flags are ignored. */
#define SHFL_CF_LOOKUP                (0x00000001)

/** Open parent directory of specified object.
 *  Useful for the corresponding Windows FSD flag
 *  and for opening paths like \\dir\\*.* to search the 'dir'.
 *  @todo possibly not needed???
 */
#define SHFL_CF_OPEN_TARGET_DIRECTORY (0x00000002)

/** Create/open a directory. */
#define SHFL_CF_DIRECTORY             (0x00000004)

/** Append data to a file. */
#define SHFL_CF_APPEND                (0x00000008)

/** Open/create action to do if object exists
 *  and if the object does not exists.
 *  REPLACE file means atomically DELETE and CREATE.
 *  OVERWRITE file means truncating the file to 0 and
 *  setting new size.
 *  When opening an existing directory REPLACE and OVERWRITE
 *  actions are considered invalid, and cause returning
 *  FILE_EXISTS with NIL handle.
 */
#define SHFL_CF_ACT_MASK_IF_EXISTS      (0x000000F0)
#define SHFL_CF_ACT_MASK_IF_NEW         (0x00000F00)

/** What to do if object exists. */
#define SHFL_CF_ACT_OPEN_IF_EXISTS      (0x00000000)
#define SHFL_CF_ACT_FAIL_IF_EXISTS      (0x00000010)
#define SHFL_CF_ACT_REPLACE_IF_EXISTS   (0x00000020)
#define SHFL_CF_ACT_OVERWRITE_IF_EXISTS (0x00000030)

/** What to do if object does not exist. */
#define SHFL_CF_ACT_CREATE_IF_NEW       (0x00000000)
#define SHFL_CF_ACT_FAIL_IF_NEW         (0x00000100)

/** Read/write requested access for the object. */
#define SHFL_CF_ACCESS_MASK_RW          (0x00003000)

/** No access requested. */
#define SHFL_CF_ACCESS_NONE             (0x00000000)
/** Read access requested. */
#define SHFL_CF_ACCESS_READ             (0x00001000)
/** Write access requested. */
#define SHFL_CF_ACCESS_WRITE            (0x00002000)
/** Read/Write access requested. */
#define SHFL_CF_ACCESS_READWRITE        (SHFL_CF_ACCESS_READ | SHFL_CF_ACCESS_WRITE)

/** Requested share access for the object. */
#define SHFL_CF_ACCESS_MASK_DENY        (0x0000C000)

/** Allow any access. */
#define SHFL_CF_ACCESS_DENYNONE         (0x00000000)
/** Do not allow read. */
#define SHFL_CF_ACCESS_DENYREAD         (0x00004000)
/** Do not allow write. */
#define SHFL_CF_ACCESS_DENYWRITE        (0x00008000)
/** Do not allow access. */
#define SHFL_CF_ACCESS_DENYALL          (SHFL_CF_ACCESS_DENYREAD | SHFL_CF_ACCESS_DENYWRITE)


/** @} */

#pragma pack(1)
typedef struct _SHFLCREATEPARMS
{
    /* Returned handle of opened object. */
    SHFLHANDLE Handle;

    /* Returned result of the operation */
    SHFLCREATERESULT Result;

    /* SHFL_CF_* */
    uint32_t CreateFlags;

    /* Attributes of object to create and
     * returned actual attributes of opened/created object.
     */
    RTFSOBJINFO Info;

} SHFLCREATEPARMS;
#pragma pack()

typedef SHFLCREATEPARMS *PSHFLCREATEPARMS;


/** Shared Folders mappings.
 *  @{
 */

/** The mapping has been added since last query. */
#define SHFL_MS_NEW        (1)
/** The mapping has been deleted since last query. */
#define SHFL_MS_DELETED    (2)

typedef struct _SHFLMAPPING
{
    /** Mapping status. */
    uint32_t u32Status;
    /** Root handle. */
    SHFLROOT root;
} SHFLMAPPING;

typedef SHFLMAPPING *PSHFLMAPPING;

/** @} */

/** Shared Folder directory information
 *  @{
 */

typedef struct _SHFLDIRINFO
{
    /** Full information about the object. */
    RTFSOBJINFO     Info;
    /** The length of the short field (number of RTUTF16 chars).
     * It is 16-bit for reasons of alignment. */
    uint16_t        cucShortName;
    /** The short name for 8.3 compatability.
     * Empty string if not available.
     */
    RTUTF16         uszShortName[14];
    /** @todo malc, a description, please. */
    SHFLSTRING      name;
} SHFLDIRINFO, *PSHFLDIRINFO;

typedef struct _SHFLVOLINFO
{
    RTFOFF         ullTotalAllocationBytes;
    RTFOFF         ullAvailableAllocationBytes;
    uint32_t       ulBytesPerAllocationUnit;
    uint32_t       ulBytesPerSector;
    uint32_t       ulSerial;
    RTFSPROPERTIES fsProperties;
} SHFLVOLINFO, *PSHFLVOLINFO;

/** @} */

/** Function parameter structures.
 *  @{
 */

/**
 * SHFL_FN_QUERY_MAPPINGS
 */

#define SHFL_MF_UCS2 (0x00000000)
/** Guest uses UTF8 strings, if not set then the strings are unicode (UCS2). */
#define SHFL_MF_UTF8 (0x00000001)

/** Type of guest system. For future system dependent features. */
#define SHFL_MF_SYSTEM_MASK    (0x0000FF00)
#define SHFL_MF_SYSTEM_NONE    (0x00000000)
#define SHFL_MF_SYSTEM_WINDOWS (0x00000100)
#define SHFL_MF_SYSTEM_LINUX   (0x00000200)

/** Parameters structure. */
typedef struct _VBoxSFQueryMappings
{
    VBoxGuestHGCMCallInfo callInfo;

    /** 32bit, in:
     * Flags describing various client needs.
     */
    HGCMFunctionParameter flags;

    /** 32bit, in/out:
     * Number of mappings the client expects.
     * This is the number of elements in the
     * mappings array.
     */
    HGCMFunctionParameter numberOfMappings;

    /** pointer, in/out:
     * Points to array of SHFLMAPPING structures.
     */
    HGCMFunctionParameter mappings;

} VBoxSFQueryMappings;

/** Number of parameters */
#define SHFL_CPARMS_QUERY_MAPPINGS (3)



/**
 * SHFL_FN_QUERY_MAP_NAME
 */

/** Parameters structure. */
typedef struct _VBoxSFQueryMapName
{
    VBoxGuestHGCMCallInfo callInfo;

    /** 32bit, in: SHFLROOT
     * Root handle of the mapping which name is queried.
     */
    HGCMFunctionParameter root;

    /** pointer, in/out:
     * Points to SHFLSTRING buffer.
     */
    HGCMFunctionParameter name;

} VBoxSFQueryMapName;

/** Number of parameters */
#define SHFL_CPARMS_QUERY_MAP_NAME (2)

/**
 * SHFL_FN_MAP_FOLDER_OLD
 */

/** Parameters structure. */
typedef struct _VBoxSFMapFolder_Old
{
    VBoxGuestHGCMCallInfo callInfo;

    /** pointer, in:
     * Points to SHFLSTRING buffer.
     */
    HGCMFunctionParameter path;

    /** pointer, out: SHFLROOT
     * Root handle of the mapping which name is queried.
     */
    HGCMFunctionParameter root;

    /** pointer, in: RTUTF16
     * Path delimiter
     */
    HGCMFunctionParameter delimiter;

} VBoxSFMapFolder_Old;

/** Number of parameters */
#define SHFL_CPARMS_MAP_FOLDER_OLD (3)

/**
 * SHFL_FN_MAP_FOLDER
 */

/** Parameters structure. */
typedef struct _VBoxSFMapFolder
{
    VBoxGuestHGCMCallInfo callInfo;

    /** pointer, in:
     * Points to SHFLSTRING buffer.
     */
    HGCMFunctionParameter path;

    /** pointer, out: SHFLROOT
     * Root handle of the mapping which name is queried.
     */
    HGCMFunctionParameter root;

    /** pointer, in: RTUTF16
     * Path delimiter
     */
    HGCMFunctionParameter delimiter;

    /** pointer, in: SHFLROOT
     * Case senstive flag
     */
    HGCMFunctionParameter fCaseSensitive;

} VBoxSFMapFolder;

/** Number of parameters */
#define SHFL_CPARMS_MAP_FOLDER (4)

/**
 * SHFL_FN_UNMAP_FOLDER
 */

/** Parameters structure. */
typedef struct _VBoxSFUnmapFolder
{
    VBoxGuestHGCMCallInfo callInfo;

    /** pointer, in: SHFLROOT
     * Root handle of the mapping which name is queried.
     */
    HGCMFunctionParameter root;

} VBoxSFUnmapFolder;

/** Number of parameters */
#define SHFL_CPARMS_UNMAP_FOLDER (1)


/**
 * SHFL_FN_CREATE
 */

/** Parameters structure. */
typedef struct _VBoxSFCreate
{
    VBoxGuestHGCMCallInfo callInfo;

    /** pointer, in: SHFLROOT
     * Root handle of the mapping which name is queried.
     */
    HGCMFunctionParameter root;

    /** pointer, in:
     * Points to SHFLSTRING buffer.
     */
    HGCMFunctionParameter path;

    /** pointer, in/out:
     * Points to SHFLCREATEPARMS buffer.
     */
    HGCMFunctionParameter parms;

} VBoxSFCreate;

/** Number of parameters */
#define SHFL_CPARMS_CREATE (3)


/**
 * SHFL_FN_CLOSE
 */

/** Parameters structure. */
typedef struct _VBoxSFClose
{
    VBoxGuestHGCMCallInfo callInfo;

    /** pointer, in: SHFLROOT
     * Root handle of the mapping which name is queried.
     */
    HGCMFunctionParameter root;


    /** value64, in:
     * SHFLHANDLE of object to close.
     */
    HGCMFunctionParameter handle;

} VBoxSFClose;

/** Number of parameters */
#define SHFL_CPARMS_CLOSE (2)


/**
 * SHFL_FN_READ
 */

/** Parameters structure. */
typedef struct _VBoxSFRead
{
    VBoxGuestHGCMCallInfo callInfo;

    /** pointer, in: SHFLROOT
     * Root handle of the mapping which name is queried.
     */
    HGCMFunctionParameter root;

    /** value64, in:
     * SHFLHANDLE of object to read from.
     */
    HGCMFunctionParameter handle;

    /** value64, in:
     * Offset to read from.
     */
    HGCMFunctionParameter offset;

    /** value64, in/out:
     * Bytes to read/How many were read.
     */
    HGCMFunctionParameter cb;

    /** pointer, out:
     * Buffer to place data to.
     */
    HGCMFunctionParameter buffer;

} VBoxSFRead;

/** Number of parameters */
#define SHFL_CPARMS_READ (5)



/**
 * SHFL_FN_WRITE
 */

/** Parameters structure. */
typedef struct _VBoxSFWrite
{
    VBoxGuestHGCMCallInfo callInfo;

    /** pointer, in: SHFLROOT
     * Root handle of the mapping which name is queried.
     */
    HGCMFunctionParameter root;

    /** value64, in:
     * SHFLHANDLE of object to write to.
     */
    HGCMFunctionParameter handle;

    /** value64, in:
     * Offset to write to.
     */
    HGCMFunctionParameter offset;

    /** value64, in/out:
     * Bytes to write/How many were written.
     */
    HGCMFunctionParameter cb;

    /** pointer, in:
     * Data to write.
     */
    HGCMFunctionParameter buffer;

} VBoxSFWrite;

/** Number of parameters */
#define SHFL_CPARMS_WRITE (5)



/**
 * SHFL_FN_LOCK
 */

/** Lock owner is the HGCM client. */

/** Lock mode bit mask. */
#define SHFL_LOCK_MODE_MASK  (0x3)
/** Cancel lock on the given range. */
#define SHFL_LOCK_CANCEL     (0x0)
/** Aquire read only lock. Prevent write to the range. */
#define SHFL_LOCK_SHARED     (0x1)
/** Aquire write lock. Prevent both write and read to the range. */
#define SHFL_LOCK_EXCLUSIVE  (0x2)

/** Do not wait for lock if it can not be acquired at the time. */
#define SHFL_LOCK_NOWAIT     (0x0)
/** Wait and acquire lock. */
#define SHFL_LOCK_WAIT       (0x4)

/** Lock the specified range. */
#define SHFL_LOCK_PARTIAL    (0x0)
/** Lock entire object. */
#define SHFL_LOCK_ENTIRE     (0x8)

/** Parameters structure. */
typedef struct _VBoxSFLock
{
    VBoxGuestHGCMCallInfo callInfo;

    /** pointer, in: SHFLROOT
     * Root handle of the mapping which name is queried.
     */
    HGCMFunctionParameter root;

    /** value64, in:
     * SHFLHANDLE of object to be locked.
     */
    HGCMFunctionParameter handle;

    /** value64, in:
     * Starting offset of lock range.
     */
    HGCMFunctionParameter offset;

    /** value64, in:
     * Length of range.
     */
    HGCMFunctionParameter length;

    /** value32, in:
     * Lock flags SHFL_LOCK_*.
     */
    HGCMFunctionParameter flags;

} VBoxSFLock;

/** Number of parameters */
#define SHFL_CPARMS_LOCK (5)



/**
 * SHFL_FN_FLUSH
 */

/** Parameters structure. */
typedef struct _VBoxSFFlush
{
    VBoxGuestHGCMCallInfo callInfo;

    /** pointer, in: SHFLROOT
     * Root handle of the mapping which name is queried.
     */
    HGCMFunctionParameter root;

    /** value64, in:
     * SHFLHANDLE of object to be locked.
     */
    HGCMFunctionParameter handle;

} VBoxSFFlush;

/** Number of parameters */
#define SHFL_CPARMS_FLUSH (2)

/**
 * SHFL_FN_LIST
 */

/** Listing information includes variable length RTDIRENTRY[EX] structures. */

/** @todo might be necessary for future. */
#define SHFL_LIST_NONE          0
#define SHFL_LIST_RETURN_ONE        1

/** Parameters structure. */
typedef struct _VBoxSFList
{
    VBoxGuestHGCMCallInfo callInfo;

    /** pointer, in: SHFLROOT
     * Root handle of the mapping which name is queried.
     */
    HGCMFunctionParameter root;

    /** value64, in:
     * SHFLHANDLE of object to be listed.
     */
    HGCMFunctionParameter handle;

    /** value32, in:
     * List flags SHFL_LIST_*.
     */
    HGCMFunctionParameter flags;

    /** value32, in/out:
     * Bytes to be used for listing information/How many bytes were used.
     */
    HGCMFunctionParameter cb;

    /** pointer, in/optional
     * Points to SHFLSTRING buffer that specifies a search path.
     */
    HGCMFunctionParameter path;

    /** pointer, out:
     * Buffer to place listing information to. (SHFLDIRINFO)
     */
    HGCMFunctionParameter buffer;

    /** value32, in/out:
     * Indicates a key where the listing must be resumed.
     * in: 0 means start from begin of object.
     * out: 0 means listing completed.
     */
    HGCMFunctionParameter resumePoint;

    /** pointer, out:
     * Number of files returned
     */
    HGCMFunctionParameter cFiles;

} VBoxSFList;

/** Number of parameters */
#define SHFL_CPARMS_LIST (8)



/**
 * SHFL_FN_INFORMATION
 */

/** Mask of Set/Get bit. */
#define SHFL_INFO_MODE_MASK    (0x1)
/** Get information */
#define SHFL_INFO_GET          (0x0)
/** Set information */
#define SHFL_INFO_SET          (0x1)

/** Get name of the object. */
#define SHFL_INFO_NAME         (0x2)
/** Set size of object (extend/trucate); only applies to file objects */
#define SHFL_INFO_SIZE         (0x4)
/** Get/Set file object info. */
#define SHFL_INFO_FILE         (0x8)
/** Get volume information. */
#define SHFL_INFO_VOLUME       (0x10)

/** @todo different file info structures */


/** Parameters structure. */
typedef struct _VBoxSFInformation
{
    VBoxGuestHGCMCallInfo callInfo;

    /** pointer, in: SHFLROOT
     * Root handle of the mapping which name is queried.
     */
    HGCMFunctionParameter root;

    /** value64, in:
     * SHFLHANDLE of object to be listed.
     */
    HGCMFunctionParameter handle;

    /** value32, in:
     * SHFL_INFO_*
     */
    HGCMFunctionParameter flags;

    /** value32, in/out:
     * Bytes to be used for information/How many bytes were used.
     */
    HGCMFunctionParameter cb;

    /** pointer, in/out:
     * Information to be set/get (RTFSOBJINFO or SHFLSTRING).
     * Do not forget to set the RTFSOBJINFO::Attr::enmAdditional for Get operation as well.
     */
    HGCMFunctionParameter info;

} VBoxSFInformation;

/** Number of parameters */
#define SHFL_CPARMS_INFORMATION (5)


/**
 * SHFL_FN_REMOVE
 */

#define SHFL_REMOVE_FILE        (0x1)
#define SHFL_REMOVE_DIR         (0x2)

/** Parameters structure. */
typedef struct _VBoxSFRemove
{
    VBoxGuestHGCMCallInfo callInfo;

    /** pointer, in: SHFLROOT
     * Root handle of the mapping which name is queried.
     */
    HGCMFunctionParameter root;

    /** pointer, in:
     * Points to SHFLSTRING buffer.
     */
    HGCMFunctionParameter path;

    /** value32, in:
     * remove flags (file/directory)
     */
    HGCMFunctionParameter flags;

} VBoxSFRemove;

#define SHFL_CPARMS_REMOVE  (3)


/**
 * SHFL_FN_RENAME
 */

#define SHFL_RENAME_FILE                (0x1)
#define SHFL_RENAME_DIR                 (0x2)
#define SHFL_RENAME_REPLACE_IF_EXISTS   (0x4)

/** Parameters structure. */
typedef struct _VBoxSFRename
{
    VBoxGuestHGCMCallInfo callInfo;

    /** pointer, in: SHFLROOT
     * Root handle of the mapping which name is queried.
     */
    HGCMFunctionParameter root;

    /** pointer, in:
     * Points to SHFLSTRING src.
     */
    HGCMFunctionParameter src;

    /** pointer, in:
     * Points to SHFLSTRING dest.
     */
    HGCMFunctionParameter dest;

    /** value32, in:
     * rename flags (file/directory)
     */
    HGCMFunctionParameter flags;

} VBoxSFRename;

#define SHFL_CPARMS_RENAME  (4)

/**
 * SHFL_FN_ADD_MAPPING
 */

/** Parameters structure. */
typedef struct _VBoxSFAddMapping
{
    VBoxGuestHGCMCallInfo callInfo;

    /** pointer, in: Folder name
     * Points to SHFLSTRING buffer.
     */
    HGCMFunctionParameter folder;

    /** pointer, in: Mapping name
     * Points to SHFLSTRING buffer.
     */
    HGCMFunctionParameter mapping;

    /** bool, in: Writable
     * True (default) if the folder is writable.
     */
    HGCMFunctionParameter writable;

} VBoxSFAddMapping;

#define SHFL_CPARMS_ADD_MAPPING  (3)


/**
 * SHFL_FN_REMOVE_MAPPING
 */

/** Parameters structure. */
typedef struct _VBoxSFRemoveMapping
{
    VBoxGuestHGCMCallInfo callInfo;

    /** pointer, in: Guest name
     * Points to SHFLSTRING buffer.
     */
    HGCMFunctionParameter path;

} VBoxSFRemoveMapping;

#define SHFL_CPARMS_REMOVE_MAPPING (1)


/**
 * SHFL_FN_SET_STATUS_LED
 */

/** Parameters structure. */
typedef struct _VBoxSFSetStatusLed
{
    VBoxGuestHGCMCallInfo callInfo;

    /** pointer, in: LED address
     * Points to PPDMLED buffer.
     */
    HGCMFunctionParameter led;

} VBoxSFSetStatusLed;

#define SHFL_CPARMS_SET_STATUS_LED (1)

/** @} */

#endif
