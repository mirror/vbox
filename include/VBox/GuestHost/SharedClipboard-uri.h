/* $Id$ */
/** @file
 * Shared Clipboard - Shared URI functions between host and guest.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
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

#ifndef VBOX_INCLUDED_GuestHost_SharedClipboard_uri_h
#define VBOX_INCLUDED_GuestHost_SharedClipboard_uri_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <map>

#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/fs.h>
#include <iprt/list.h>

#include <iprt/cpp/list.h>
#include <iprt/cpp/ministring.h>

#include <VBox/GuestHost/SharedClipboard.h>

/** @name Shared Clipboard handles.
 *  @{
 */

/** A Shared Clipboard list handle. */
typedef uint64_t SHAREDCLIPBOARDLISTHANDLE;
/** Pointer to a Shared Clipboard list handle. */
typedef SHAREDCLIPBOARDLISTHANDLE *PSHAREDCLIPBOARDLISTHANDLE;

/** Specifies an invalid Shared Clipboard list handle. */
#define SHAREDCLIPBOARDLISTHANDLE_INVALID        0

/** A Shared Clipboard object handle. */
typedef uint64_t SHAREDCLIPBOARDOBJHANDLE;
/** Pointer to a Shared Clipboard object handle. */
typedef SHAREDCLIPBOARDOBJHANDLE *PSHAREDCLIPBOARDOBJHANDLE;

/** Specifies an invalid Shared Clipboard object handle. */
#define SHAREDCLIPBOARDOBJHANDLE_INVALID         0

/** @} */

/** @name Shared Clipboard open/create flags.
 *  @{
 */

/** No flags. Initialization value. */
#define SHAREDCLIPBOARD_CF_NONE                  (0x00000000)

/** Lookup only the object, do not return a handle. All other flags are ignored. */
#define SHAREDCLIPBOARD_CF_LOOKUP                (0x00000001)

/** Create/open a directory. */
#define SHAREDCLIPBOARD_CF_DIRECTORY             (0x00000004)

/** Open/create action to do if object exists
 *  and if the object does not exists.
 *  REPLACE file means atomically DELETE and CREATE.
 *  OVERWRITE file means truncating the file to 0 and
 *  setting new size.
 *  When opening an existing directory REPLACE and OVERWRITE
 *  actions are considered invalid, and cause returning
 *  FILE_EXISTS with NIL handle.
 */
#define SHAREDCLIPBOARD_CF_ACT_MASK_IF_EXISTS      (0x000000F0)
#define SHAREDCLIPBOARD_CF_ACT_MASK_IF_NEW         (0x00000F00)

/** What to do if object exists. */
#define SHAREDCLIPBOARD_CF_ACT_OPEN_IF_EXISTS      (0x00000000)
#define SHAREDCLIPBOARD_CF_ACT_FAIL_IF_EXISTS      (0x00000010)
#define SHAREDCLIPBOARD_CF_ACT_REPLACE_IF_EXISTS   (0x00000020)
#define SHAREDCLIPBOARD_CF_ACT_OVERWRITE_IF_EXISTS (0x00000030)

/** What to do if object does not exist. */
#define SHAREDCLIPBOARD_CF_ACT_CREATE_IF_NEW       (0x00000000)
#define SHAREDCLIPBOARD_CF_ACT_FAIL_IF_NEW         (0x00000100)

/** Read/write requested access for the object. */
#define SHAREDCLIPBOARD_CF_ACCESS_MASK_RW          (0x00003000)

/** No access requested. */
#define SHAREDCLIPBOARD_CF_ACCESS_NONE             (0x00000000)
/** Read access requested. */
#define SHAREDCLIPBOARD_CF_ACCESS_READ             (0x00001000)
/** Write access requested. */
#define SHAREDCLIPBOARD_CF_ACCESS_WRITE            (0x00002000)
/** Read/Write access requested. */
#define SHAREDCLIPBOARD_CF_ACCESS_READWRITE        (SHAREDCLIPBOARD_CF_ACCESS_READ | SHAREDCLIPBOARD_CF_ACCESS_WRITE)

/** Requested share access for the object. */
#define SHAREDCLIPBOARD_CF_ACCESS_MASK_DENY        (0x0000C000)

/** Allow any access. */
#define SHAREDCLIPBOARD_CF_ACCESS_DENYNONE         (0x00000000)
/** Do not allow read. */
#define SHAREDCLIPBOARD_CF_ACCESS_DENYREAD         (0x00004000)
/** Do not allow write. */
#define SHAREDCLIPBOARD_CF_ACCESS_DENYWRITE        (0x00008000)
/** Do not allow access. */
#define SHAREDCLIPBOARD_CF_ACCESS_DENYALL          (SHAREDCLIPBOARD_CF_ACCESS_DENYREAD | SHAREDCLIPBOARD_CF_ACCESS_DENYWRITE)

/** Requested access to attributes of the object. */
#define SHAREDCLIPBOARD_CF_ACCESS_MASK_ATTR        (0x00030000)

/** No access requested. */
#define SHAREDCLIPBOARD_CF_ACCESS_ATTR_NONE        (0x00000000)
/** Read access requested. */
#define SHAREDCLIPBOARD_CF_ACCESS_ATTR_READ        (0x00010000)
/** Write access requested. */
#define SHAREDCLIPBOARD_CF_ACCESS_ATTR_WRITE       (0x00020000)
/** Read/Write access requested. */
#define SHAREDCLIPBOARD_CF_ACCESS_ATTR_READWRITE   (SHAREDCLIPBOARD_CF_ACCESS_ATTR_READ | SHAREDCLIPBOARD_CF_ACCESS_ATTR_WRITE)

/** The file is opened in append mode. Ignored if SHAREDCLIPBOARD_CF_ACCESS_WRITE is not set. */
#define SHAREDCLIPBOARD_CF_ACCESS_APPEND           (0x00040000)

/** @} */

/** Result of an open/create request.
 *  Along with handle value the result code
 *  identifies what has happened while
 *  trying to open the object.
 */
typedef enum _SHAREDCLIPBOARDCREATERESULT
{
    SHAREDCLIPBOARD_CREATERESULT_NONE,
    /** Specified path does not exist. */
    SHAREDCLIPBOARD_CREATERESULT_PATH_NOT_FOUND,
    /** Path to file exists, but the last component does not. */
    SHAREDCLIPBOARD_CREATERESULT_FILE_NOT_FOUND,
    /** File already exists and either has been opened or not. */
    SHAREDCLIPBOARD_CREATERESULT_FILE_EXISTS,
    /** New file was created. */
    SHAREDCLIPBOARD_CREATERESULT_FILE_CREATED,
    /** Existing file was replaced or overwritten. */
    SHAREDCLIPBOARD_CREATERESULT_FILE_REPLACED,
    /** Blow the type up to 32-bit. */
    SHAREDCLIPBOARD_CREATERESULT_32BIT_HACK = 0x7fffffff
} SHAREDCLIPBOARDCREATERESULT;
AssertCompile(SHAREDCLIPBOARD_CREATERESULT_NONE == 0);
AssertCompileSize(SHAREDCLIPBOARDCREATERESULT, 4);

/**
 * The available additional information in a SHAREDCLIPBOARDFSOBJATTR object.
 */
typedef enum _SHAREDCLIPBOARDFSOBJATTRADD
{
    /** No additional information is available / requested. */
    SHAREDCLIPBOARDFSOBJATTRADD_NOTHING = 1,
    /** The additional unix attributes (SHAREDCLIPBOARDFSOBJATTR::u::Unix) are
     *  available / requested. */
    SHAREDCLIPBOARDFSOBJATTRADD_UNIX,
    /** The additional extended attribute size (SHAREDCLIPBOARDFSOBJATTR::u::EASize) is
     *  available / requested. */
    SHAREDCLIPBOARDFSOBJATTRADD_EASIZE,
    /** The last valid item (inclusive).
     * The valid range is SHAREDCLIPBOARDFSOBJATTRADD_NOTHING thru
     * SHAREDCLIPBOARDFSOBJATTRADD_LAST. */
    SHAREDCLIPBOARDFSOBJATTRADD_LAST = SHAREDCLIPBOARDFSOBJATTRADD_EASIZE,

    /** The usual 32-bit hack. */
    SHAREDCLIPBOARDFSOBJATTRADD_32BIT_SIZE_HACK = 0x7fffffff
} SHAREDCLIPBOARDFSOBJATTRADD;


/* Assert sizes of the IRPT types we're using below. */
AssertCompileSize(RTFMODE,      4);
AssertCompileSize(RTFOFF,       8);
AssertCompileSize(RTINODE,      8);
AssertCompileSize(RTTIMESPEC,   8);
AssertCompileSize(RTDEV,        4);
AssertCompileSize(RTUID,        4);

/**
 * Shared Clipboard filesystem object attributes.
 */
#pragma pack(1)
typedef struct _SHAREDCLIPBOARDFSOBJATTR
{
    /** Mode flags (st_mode). RTFS_UNIX_*, RTFS_TYPE_*, and RTFS_DOS_*.
     * @remarks We depend on a number of RTFS_ defines to remain unchanged.
     *          Fortuntately, these are depending on windows, dos and unix
     *          standard values, so this shouldn't be much of a pain. */
    RTFMODE         fMode;

    /** The additional attributes available. */
    SHAREDCLIPBOARDFSOBJATTRADD  enmAdditional;

    /**
     * Additional attributes.
     *
     * Unless explicitly specified to an API, the API can provide additional
     * data as it is provided by the underlying OS.
     */
    union SHAREDCLIPBOARDFSOBJATTRUNION
    {
        /** Additional Unix Attributes
         * These are available when SHAREDCLIPBOARDFSOBJATTRADD is set in fUnix.
         */
         struct SHAREDCLIPBOARDFSOBJATTRUNIX
         {
            /** The user owning the filesystem object (st_uid).
             * This field is ~0U if not supported. */
            RTUID           uid;

            /** The group the filesystem object is assigned (st_gid).
             * This field is ~0U if not supported. */
            RTGID           gid;

            /** Number of hard links to this filesystem object (st_nlink).
             * This field is 1 if the filesystem doesn't support hardlinking or
             * the information isn't available.
             */
            uint32_t        cHardlinks;

            /** The device number of the device which this filesystem object resides on (st_dev).
             * This field is 0 if this information is not available. */
            RTDEV           INodeIdDevice;

            /** The unique identifier (within the filesystem) of this filesystem object (st_ino).
             * Together with INodeIdDevice, this field can be used as a OS wide unique id
             * when both their values are not 0.
             * This field is 0 if the information is not available. */
            RTINODE         INodeId;

            /** User flags (st_flags).
             * This field is 0 if this information is not available. */
            uint32_t        fFlags;

            /** The current generation number (st_gen).
             * This field is 0 if this information is not available. */
            uint32_t        GenerationId;

            /** The device number of a character or block device type object (st_rdev).
             * This field is 0 if the file isn't of a character or block device type and
             * when the OS doesn't subscribe to the major+minor device idenfication scheme. */
            RTDEV           Device;
        } Unix;

        /**
         * Extended attribute size.
         */
        struct SHAREDCLIPBOARDFSOBJATTREASIZE
        {
            /** Size of EAs. */
            RTFOFF          cb;
        } EASize;
    } u;
} SHAREDCLIPBOARDFSOBJATTR;
#pragma pack()
AssertCompileSize(SHAREDCLIPBOARDFSOBJATTR, 44);
/** Pointer to a shared folder filesystem object attributes structure. */
typedef SHAREDCLIPBOARDFSOBJATTR *PSHAREDCLIPBOARDFSOBJATTR;
/** Pointer to a const shared folder filesystem object attributes structure. */
typedef const SHAREDCLIPBOARDFSOBJATTR *PCSHAREDCLIPBOARDFSOBJATTR;

/**
 * Shared Clipboard file system object information structure.
 */
#pragma pack(1)
typedef struct _SHAREDCLIPBOARDFSOBJINFO
{
   /** Logical size (st_size).
    * For normal files this is the size of the file.
    * For symbolic links, this is the length of the path name contained
    * in the symbolic link.
    * For other objects this fields needs to be specified.
    */
   RTFOFF       cbObject;

   /** Disk allocation size (st_blocks * DEV_BSIZE). */
   RTFOFF       cbAllocated;

   /** Time of last access (st_atime).
    * @remarks  Here (and other places) we depend on the IPRT timespec to
    *           remain unchanged. */
   RTTIMESPEC   AccessTime;

   /** Time of last data modification (st_mtime). */
   RTTIMESPEC   ModificationTime;

   /** Time of last status change (st_ctime).
    * If not available this is set to ModificationTime.
    */
   RTTIMESPEC   ChangeTime;

   /** Time of file birth (st_birthtime).
    * If not available this is set to ChangeTime.
    */
   RTTIMESPEC   BirthTime;

   /** Attributes. */
   SHAREDCLIPBOARDFSOBJATTR Attr;

} SHAREDCLIPBOARDFSOBJINFO;
#pragma pack()
AssertCompileSize(SHAREDCLIPBOARDFSOBJINFO, 92);
/** Pointer to a shared folder filesystem object information structure. */
typedef SHAREDCLIPBOARDFSOBJINFO *PSHAREDCLIPBOARDFSOBJINFO;
/** Pointer to a const shared folder filesystem object information
 *  structure. */
typedef const SHAREDCLIPBOARDFSOBJINFO *PCSHAREDCLIPBOARDFSOBJINFO;

#pragma pack(1)
typedef struct _VBOXCLIPBOARDCREATEPARMS
{
    /** Returned SHAREDCLIPBOARDOBJHANDLE of opened object. */
    SHAREDCLIPBOARDOBJHANDLE    uHandle;
    /** Returned result of the operation. */
    SHAREDCLIPBOARDCREATERESULT enmResult;
    /** SHAREDCLIPBOARD_CF_* */
    uint32_t                    fCreate;
    /**
     * Attributes of object to create and
     * returned actual attributes of opened/created object.
     */
    SHAREDCLIPBOARDFSOBJINFO    ObjInfo;
} VBOXCLIPBOARDCREATEPARMS, *PVBOXCLIPBOARDCREATEPARMS;
#pragma pack()

/** Clipboard area ID. A valid area is >= 1.
 *  If 0 is specified, the last (most recent) area is meant.
 *  Set to UINT32_MAX if not initialized. */
typedef uint32_t SHAREDCLIPBOARDAREAID;

/** Defines a non-initialized (nil) clipboard area. */
#define NIL_SHAREDCLIPBOARDAREAID       UINT32_MAX

/** SharedClipboardArea open flags. */
typedef uint32_t SHAREDCLIPBOARDAREAOPENFLAGS;

/** No clipboard area open flags specified. */
#define SHAREDCLIPBOARDAREA_OPEN_FLAGS_NONE               0
/** The clipboard area must not exist yet. */
#define SHAREDCLIPBOARDAREA_OPEN_FLAGS_MUST_NOT_EXIST     RT_BIT(0)
/** Mask of all valid clipboard area open flags.  */
#define SHAREDCLIPBOARDAREA_OPEN_FLAGS_VALID_MASK         0x1

/** SharedClipboardURIObject flags. */
typedef uint32_t SHAREDCLIPBOARDURIOBJECTFLAGS;

/** No flags specified. */
#define SHAREDCLIPBOARDURIOBJECT_FLAGS_NONE                   0

/** Mask of all valid Shared Clipboard URI object flags. */
#define SHAREDCLIPBOARDURIOBJECT_FLAGS_VALID_MASK             UINT32_C(0x0)

/**
 * Class for handling Shared Clipboard URI objects.
 * This class abstracts the access and handling objects when performing Shared Clipboard actions.
 */
class SharedClipboardURIObject
{
public:

    /**
     * Enumeration for specifying an URI object type.
     */
    enum Type
    {
        /** Unknown type, do not use. */
        Type_Unknown = 0,
        /** Object is a file. */
        Type_File,
        /** Object is a directory. */
        Type_Directory,
        /** The usual 32-bit hack. */
        Type_32Bit_Hack = 0x7fffffff
    };

    enum Storage
    {
        Storage_Unknown = 0,
        Storage_Local,
        Storage_Temporary,
        /** The usual 32-bit hack. */
        Storage_32Bit_Hack = 0x7fffffff
    };

    /**
     * Enumeration for specifying an URI object view
     * for representing its data accordingly.
     */
    enum View
    {
        /** Unknown view, do not use. */
        View_Unknown = 0,
        /** Handle data from the source point of view. */
        View_Source,
        /** Handle data from the destination point of view. */
        View_Target,
        /** The usual 32-bit hack. */
        View_Dest_32Bit_Hack = 0x7fffffff
    };

    SharedClipboardURIObject(void);
    SharedClipboardURIObject(Type type, const RTCString &strSrcPathAbs = "", const RTCString &strDstPathAbs = "");
    virtual ~SharedClipboardURIObject(void);

public:

    /**
     * Returns the given absolute source path of the object.
     *
     * @return  Absolute source path of the object.
     */
    const RTCString &GetSourcePathAbs(void) const { return m_strSrcPathAbs; }

    /**
     * Returns the given, absolute destination path of the object.
     *
     * @return  Absolute destination path of the object.
     */
    const RTCString &GetDestPathAbs(void) const { return m_strTgtPathAbs; }

    RTFMODE GetMode(void) const;

    uint64_t GetProcessed(void) const;

    uint64_t GetSize(void) const;

    /**
     * Returns the object's type.
     *
     * @return  The object's type.
     */
    Type GetType(void) const { return m_enmType; }

    /**
     * Returns the object's view.
     *
     * @return  The object's view.
     */
    View GetView(void) const { return m_enmView; }

public:

    int SetSize(uint64_t cbSize);

public:

    void Close(void);
    bool IsComplete(void) const;
    bool IsOpen(void) const;
    int OpenDirectory(View enmView, uint32_t fCreate = 0, RTFMODE fMode = 0);
    int OpenDirectoryEx(const RTCString &strPathAbs, View enmView,
                        uint32_t fCreate = 0, RTFMODE fMode = 0,
                        SHAREDCLIPBOARDURIOBJECTFLAGS fFlags = SHAREDCLIPBOARDURIOBJECT_FLAGS_NONE);
    int OpenFile(View enmView, uint64_t fOpen = 0, RTFMODE fMode = 0);
    int OpenFileEx(const RTCString &strPathAbs, View enmView,
                   uint64_t fOpen = 0, RTFMODE fMode = 0,
                   SHAREDCLIPBOARDURIOBJECTFLAGS fFlags  = SHAREDCLIPBOARDURIOBJECT_FLAGS_NONE);
    int QueryInfo(View enmView);
    int Read(void *pvBuf, size_t cbBuf, uint32_t *pcbRead);
    void Reset(void);
    int SetDirectoryData(const RTCString &strPathAbs, View enmView, uint32_t fOpen = 0, RTFMODE fMode = 0,
                         SHAREDCLIPBOARDURIOBJECTFLAGS fFlags = SHAREDCLIPBOARDURIOBJECT_FLAGS_NONE);
    int SetFileData(const RTCString &strPathAbs, View enmView, uint64_t fOpen = 0, RTFMODE fMode = 0,
                    SHAREDCLIPBOARDURIOBJECTFLAGS fFlags = SHAREDCLIPBOARDURIOBJECT_FLAGS_NONE);
    int Write(const void *pvBuf, size_t cbBuf, uint32_t *pcbWritten);

public:

    static int RebaseURIPath(RTCString &strPath, const RTCString &strBaseOld = "", const RTCString &strBaseNew = "");

protected:

    void closeInternal(void);
    int setDirectoryDataInternal(const RTCString &strPathAbs, View enmView, uint32_t fCreate = 0, RTFMODE fMode = 0,
                                 SHAREDCLIPBOARDURIOBJECTFLAGS fFlags = SHAREDCLIPBOARDURIOBJECT_FLAGS_NONE);
    int setFileDataInternal(const RTCString &strPathAbs, View enmView, uint64_t fOpen = 0, RTFMODE fMode = 0,
                            SHAREDCLIPBOARDURIOBJECTFLAGS fFlags = SHAREDCLIPBOARDURIOBJECT_FLAGS_NONE);
    int queryInfoInternal(View enmView);

protected:

    /** The object's type. */
    Type      m_enmType;
    /** The object's view. */
    View      m_enmView;
    /** Where the object is being stored to. */
    Storage   m_enmStorage;
    /** Absolute path (base) for the source. */
    RTCString m_strSrcPathAbs;
    /** Absolute path (base) for the target. */
    RTCString m_strTgtPathAbs;
    /** Saved SHAREDCLIPBOARDURIOBJECT_FLAGS. */
    uint32_t  m_fFlags;
    /** Requested file mode.
     *  Note: The actual file mode of an opened file will be in objInfo. */
    RTFMODE   m_fModeRequested;

    /** Union containing data depending on the object's type. */
    union
    {
        /** Structure containing members for objects that
         *  are files. */
        struct
        {
            /** File handle. */
            RTFILE      hFile;
            /** File system object information of this file. */
            RTFSOBJINFO objInfo;
            /** Requested file open flags. */
            uint32_t    fOpenRequested;
            /** Bytes to proces for reading/writing. */
            uint64_t    cbToProcess;
            /** Bytes processed reading/writing. */
            uint64_t    cbProcessed;
        } File;
        struct
        {
            /** Directory handle. */
            RTDIR       hDir;
            /** File system object information of this directory. */
            RTFSOBJINFO objInfo;
            /** Requested directory creation flags. */
            uint32_t    fCreateRequested;
        } Dir;
    } u;
};

/**
 * Structure for keeping a reply message.
 */
typedef struct _VBOXCLIPBOARDREPLY
{
    /** Message type of type VBOX_SHAREDCLIPBOARD_REPLYMSGTYPE_XXX. */
    uint32_t uType;
    /** IPRT result of overall operation. Note: int vs. uint32! */
    uint32_t rc;
    union
    {
        struct
        {
            SHAREDCLIPBOARDLISTHANDLE uHandle;
        } ListOpen;
        struct
        {
            SHAREDCLIPBOARDOBJHANDLE uHandle;
        } ObjOpen;
    } u;
    /** Pointer to optional payload. */
    void    *pvPayload;
    /** Payload size (in bytes). */
    uint32_t cbPayload;
} VBOXCLIPBOARDREPLY, *PVBOXCLIPBOARDREPLY;

/**
 * Structure for maintaining Shared Clipboard list open paramters.
 */
typedef struct _VBOXCLIPBOARDLISTOPENPARMS
{
    /** Listing flags (see VBOX_SHAREDCLIPBOARD_LIST_FLAG_XXX). */
    uint32_t fList;
    /** Size (in bytes) of the filter string. */
    uint32_t cbFilter;
    /** Filter string. DOS wilcard-style. */
    char    *pszFilter;
    /** Size (in bytes) of the listing path. */
    uint32_t cbPath;
    /** Listing path (absolute). If empty or NULL the listing's root path will be opened. */
    char    *pszPath;
} VBOXCLIPBOARDLISTOPENPARMS, *PVBOXCLIPBOARDLISTOPENPARMS;

/**
 * Structure for keeping a Shared Clipboard list header.
 */
typedef struct _VBOXCLIPBOARDLISTHDR
{
    /** Feature flag(s). Not being used atm. */
    uint32_t fFeatures;
    /** Total objects returned. */
    uint64_t cTotalObjects;
    /** Total size (in bytes) returned. */
    uint64_t cbTotalSize;
    /** Compression being method used. Not implemented yet. */
    uint32_t enmCompression;
    /** Checksum type being used. Not implemented yet. */
    uint32_t enmChecksumType;
} VBOXCLIPBOARDLISTHDR, *PVBOXCLIPBOARDLISTHDR;

/**
 * Structure for a Shared Clipboard list entry.
 */
typedef struct _VBOXCLIPBOARDLISTENTRY
{
    /** Information flag(s). */
    uint32_t fInfo;
    /** Size (in bytes) of the actual list entry. */
    uint32_t cbInfo;
    /** Data of the actual list entry. */
    void    *pvInfo;
} VBOXCLIPBOARDLISTENTRY, *PVBOXCLIPBOARDLISTENTRY;

/**
 * Structure for a Shared Clipboard object header.
 */
typedef struct _VBOXCLIPBOARDOBJHDR
{
    /** Header type. Currently not being used. */
    uint32_t enmType;
} VBOXCLIPBOARDOBJHDR, *PVBOXCLIPBOARDOBJHDR;

/**
 * Enumeration for specifying a clipboard area object type.
 */
typedef enum _SHAREDCLIPBOARDAREAOBJTYPE
{
    /** Unknown object type; do not use. */
    SHAREDCLIPBOARDAREAOBJTYPE_UNKNOWN = 0,
    /** Object is a directory. */
    SHAREDCLIPBOARDAREAOBJTYPE_DIR,
    /** Object is a file. */
    SHAREDCLIPBOARDAREAOBJTYPE_FILE,
    /** Object is a symbolic link. */
    SHAREDCLIPBOARDAREAOBJTYPE_SYMLINK,
    /** The usual 32-bit hack. */
    SHAREDCLIPBOARDAREAOBJTYPE_32Bit_Hack = 0x7fffffff
} SHAREDCLIPBOARDAREAOBJTYPE;

/** Defines a clipboard area object state. */
typedef uint32_t SHAREDCLIPBOARDAREAOBJSTATE;

/** No object state set. */
#define SHAREDCLIPBOARDAREAOBJSTATE_NONE                0
/** The object is considered as being complete (e.g. serialized). */
#define SHAREDCLIPBOARDAREAOBJSTATE_COMPLETE            RT_BIT(0)

/**
 * Lightweight structure to keep a clipboard area object's state.
 *
 * Note: We don't want to use the ClipboardURIObject class here, as this
 *       is too heavy for this purpose.
 */
typedef struct _SHAREDCLIPBOARDAREAOBJ
{
    SHAREDCLIPBOARDAREAOBJTYPE  enmType;
    SHAREDCLIPBOARDAREAOBJSTATE fState;
} SHAREDCLIPBOARDAREAOBJ, *PSHAREDCLIPBOARDAREAOBJ;

/**
 * Class for maintaining a Shared Clipboard area
 * on the host or guest. This will contain all received files & directories
 * for a single Shared Clipboard operation.
 *
 * In case of a failed Shared Clipboard operation this class can also
 * perform a gentle rollback if required.
 */
class SharedClipboardArea
{
public:

    SharedClipboardArea(void);
    SharedClipboardArea(const char *pszPath, SHAREDCLIPBOARDAREAID uID = NIL_SHAREDCLIPBOARDAREAID,
                        SHAREDCLIPBOARDAREAOPENFLAGS fFlags = SHAREDCLIPBOARDAREA_OPEN_FLAGS_NONE);
    virtual ~SharedClipboardArea(void);

public:

    uint32_t AddRef(void);
    uint32_t Release(void);

    int Lock(void);
    int Unlock(void);

    int AddObject(const char *pszPath, const SHAREDCLIPBOARDAREAOBJ &Obj);
    int GetObject(const char *pszPath, PSHAREDCLIPBOARDAREAOBJ pObj);

    int Close(void);
    bool IsOpen(void) const;
    int OpenEx(const char *pszPath, SHAREDCLIPBOARDAREAID uID = NIL_SHAREDCLIPBOARDAREAID,
               SHAREDCLIPBOARDAREAOPENFLAGS fFlags = SHAREDCLIPBOARDAREA_OPEN_FLAGS_NONE);
    int OpenTemp(SHAREDCLIPBOARDAREAID uID = NIL_SHAREDCLIPBOARDAREAID,
                 SHAREDCLIPBOARDAREAOPENFLAGS fFlags = SHAREDCLIPBOARDAREA_OPEN_FLAGS_NONE);
    SHAREDCLIPBOARDAREAID GetID(void) const;
    const char *GetDirAbs(void) const;
    uint32_t GetRefCount(void);
    int Reopen(void);
    int Reset(bool fDeleteContent);
    int Rollback(void);

public:

    static int PathConstruct(const char *pszBase, SHAREDCLIPBOARDAREAID uID, char *pszPath, size_t cbPath);

protected:

    int initInternal(void);
    int destroyInternal(void);
    int closeInternal(void);

protected:

    typedef std::map<RTCString, SHAREDCLIPBOARDAREAOBJ> SharedClipboardAreaFsObjMap;

    /** Creation timestamp (in ms). */
    uint64_t                     m_tsCreatedMs;
    /** Number of references to this instance. */
    volatile uint32_t            m_cRefs;
    /** Critical section for serializing access. */
    RTCRITSECT                   m_CritSect;
    /** Open flags. */
    uint32_t                     m_fOpen;
    /** Directory handle for root clipboard directory. */
    RTDIR                        m_hDir;
    /** Absolute path to root clipboard directory. */
    RTCString                    m_strPathAbs;
    /** List for holding created directories in the case of a rollback. */
    SharedClipboardAreaFsObjMap  m_mapObj;
    /** Associated clipboard area ID. */
    SHAREDCLIPBOARDAREAID        m_uID;
};

int SharedClipboardPathSanitizeFilename(char *pszPath, size_t cbPath);
int SharedClipboardPathSanitize(char *pszPath, size_t cbPath);

/** SharedClipboardURIList flags. */
typedef uint32_t SHAREDCLIPBOARDURILISTFLAGS;

/** No flags specified. */
#define SHAREDCLIPBOARDURILIST_FLAGS_NONE                   0
/** Keep the original paths, don't convert paths to relative ones. */
#define SHAREDCLIPBOARDURILIST_FLAGS_ABSOLUTE_PATHS         RT_BIT(0)
/** Resolve all symlinks. */
#define SHAREDCLIPBOARDURILIST_FLAGS_RESOLVE_SYMLINKS       RT_BIT(1)
/** Keep the files + directory entries open while
 *  being in this list. */
#define SHAREDCLIPBOARDURILIST_FLAGS_KEEP_OPEN              RT_BIT(2)
/** Lazy loading: Only enumerate sub directories when needed.
 ** @todo Implement lazy loading.  */
#define SHAREDCLIPBOARDURILIST_FLAGS_LAZY                   RT_BIT(3)

/** Mask of all valid Shared Clipboard URI list flags. */
#define SHAREDCLIPBOARDURILIST_FLAGS_VALID_MASK             UINT32_C(0xF)

class SharedClipboardURIList
{
public:

    SharedClipboardURIList(void);
    virtual ~SharedClipboardURIList(void);

public:

    int AppendNativePath(const char *pszPath, SHAREDCLIPBOARDURILISTFLAGS fFlags);
    int AppendNativePathsFromList(const char *pszNativePaths, size_t cbNativePaths, SHAREDCLIPBOARDURILISTFLAGS fFlags);
    int AppendNativePathsFromList(const RTCList<RTCString> &lstNativePaths, SHAREDCLIPBOARDURILISTFLAGS fFlags);
    int AppendURIObject(SharedClipboardURIObject *pObject);
    int AppendURIPath(const char *pszURI, SHAREDCLIPBOARDURILISTFLAGS fFlags);
    int AppendURIPathsFromList(const char *pszURIPaths, size_t cbURIPaths, SHAREDCLIPBOARDURILISTFLAGS fFlags);
    int AppendURIPathsFromList(const RTCList<RTCString> &lstURI, SHAREDCLIPBOARDURILISTFLAGS fFlags);

    void Clear(void);
    SharedClipboardURIObject *At(size_t i) const { return m_lstTree.at(i); }
    SharedClipboardURIObject *First(void) const { return m_lstTree.first(); }
    bool IsEmpty(void) const { return m_lstTree.isEmpty(); }
    void RemoveFirst(void);
    int SetFromURIData(const void *pvData, size_t cbData, SHAREDCLIPBOARDURILISTFLAGS fFlags);

    RTCString GetRootEntries(const RTCString &strPathBase = "", const RTCString &strSeparator = "\r\n") const;
    uint64_t GetRootCount(void) const { return m_lstRoot.size(); }
    uint64_t GetTotalCount(void) const { return m_cTotal; }
    uint64_t GetTotalBytes(void) const { return m_cbTotal; }

protected:

    int appendEntry(const char *pcszSource, const char *pcszTarget, SHAREDCLIPBOARDURILISTFLAGS fFlags);
    int appendObject(SharedClipboardURIObject *pObject);
    int appendPathRecursive(const char *pcszSrcPath, const char *pcszDstPath, const char *pcszDstBase, size_t cchDstBase, SHAREDCLIPBOARDURILISTFLAGS fFlags);

protected:

    /** List of all top-level file/directory entries.
     *  Note: All paths are kept internally as UNIX paths for
     *        easier conversion/handling!  */
    RTCList<RTCString>                  m_lstRoot;
    /** List of all URI objects added. The list's content
     *  might vary depending on how the objects are being
     *  added (lazy or not). */
    RTCList<SharedClipboardURIObject *> m_lstTree;
    /** Total number of all URI objects. */
    uint64_t                            m_cTotal;
    /** Total size of all URI objects, that is, the file
     *  size of all objects (in bytes).
     *  Note: Do *not* size_t here, as we also want to support large files
     *        on 32-bit guests. */
    uint64_t                            m_cbTotal;
};

int SharedClipboardURIListHdrAlloc(PVBOXCLIPBOARDLISTHDR *ppListHdr);
void SharedClipboardURIListHdrFree(PVBOXCLIPBOARDLISTHDR pListHdr);
PVBOXCLIPBOARDLISTHDR SharedClipboardURIListHdrDup(PVBOXCLIPBOARDLISTHDR pListHdr);
int SharedClipboardURIListHdrInit(PVBOXCLIPBOARDLISTHDR pListHdr);
void SharedClipboardURIListHdrDestroy(PVBOXCLIPBOARDLISTHDR pListHdr);
void SharedClipboardURIListHdrFree(PVBOXCLIPBOARDLISTHDR pListHdr);
void SharedClipboardURIListHdrReset(PVBOXCLIPBOARDLISTHDR pListHdr);
bool SharedClipboardURIListHdrIsValid(PVBOXCLIPBOARDLISTHDR pListHdr);

int SharedClipboardURIListOpenParmsCopy(PVBOXCLIPBOARDLISTOPENPARMS pDst, PVBOXCLIPBOARDLISTOPENPARMS pSrc);
PVBOXCLIPBOARDLISTOPENPARMS SharedClipboardURIListOpenParmsDup(PVBOXCLIPBOARDLISTOPENPARMS pParms);
int SharedClipboardURIListOpenParmsInit(PVBOXCLIPBOARDLISTOPENPARMS pParms);
void SharedClipboardURIListOpenParmsDestroy(PVBOXCLIPBOARDLISTOPENPARMS pParms);

int SharedClipboardURIListEntryAlloc(PVBOXCLIPBOARDLISTENTRY *ppListEntry);
void SharedClipboardURIListEntryFree(PVBOXCLIPBOARDLISTENTRY pListEntry);
int SharedClipboardURIListEntryCopy(PVBOXCLIPBOARDLISTENTRY pDst, PVBOXCLIPBOARDLISTENTRY pSrc);
PVBOXCLIPBOARDLISTENTRY SharedClipboardURIListEntryDup(PVBOXCLIPBOARDLISTENTRY pListEntry);
int SharedClipboardURIListEntryInit(PVBOXCLIPBOARDLISTENTRY pListEntry);
void SharedClipboardURIListEntryDestroy(PVBOXCLIPBOARDLISTENTRY pListEntry);
bool SharedClipboardURIListEntryIsValid(PVBOXCLIPBOARDLISTENTRY pListEntry);

/**
 * Enumeration specifying an URI transfer direction.
 */
typedef enum _SHAREDCLIPBOARDURITRANSFERDIR
{
    /** Unknown transfer directory. */
    SHAREDCLIPBOARDURITRANSFERDIR_UNKNOWN = 0,
    /** Read transfer (from source). */
    SHAREDCLIPBOARDURITRANSFERDIR_READ,
    /** Write transfer (to target). */
    SHAREDCLIPBOARDURITRANSFERDIR_WRITE,
    /** The usual 32-bit hack. */
    SHAREDCLIPBOARDURITRANSFERDIR_32BIT_HACK = 0x7fffffff
} SHAREDCLIPBOARDURITRANSFERDIR;

struct _SHAREDCLIPBOARDURITRANSFER;
typedef struct _SHAREDCLIPBOARDURITRANSFER SHAREDCLIPBOARDURITRANSFER;

/**
 * Structure for handling a single URI object context.
 */
typedef struct _SHAREDCLIPBOARDCLIENTURIOBJCTX
{
    SHAREDCLIPBOARDURITRANSFER *pTransfer;
    SHAREDCLIPBOARDOBJHANDLE    uHandle;
} SHAREDCLIPBOARDCLIENTURIOBJCTX, *PSHAREDCLIPBOARDCLIENTURIOBJCTX;

typedef struct _SHAREDCLIPBOARDURITRANSFEROBJSTATE
{
    uint64_t                    cbProcessed;
} SHAREDCLIPBOARDURITRANSFEROBJSTATE, *PSHAREDCLIPBOARDURITRANSFEROBJSTATE;

typedef struct _SHAREDCLIPBOARDURITRANSFEROBJ
{
    SHAREDCLIPBOARDOBJHANDLE           uHandle;
    char                              *pszPathAbs;
    SHAREDCLIPBOARDFSOBJINFO           objInfo;
    SHAREDCLIPBOARDSOURCE              enmSource;
    SHAREDCLIPBOARDURITRANSFEROBJSTATE State;
} SHAREDCLIPBOARDURITRANSFEROBJ, *PSHAREDCLIPBOARDURITRANSFEROBJ;

/** No status set. */
#define SHAREDCLIPBOARDURITRANSFERSTATUS_NONE           0
/** The transfer has been announced but is not running yet. */
#define SHAREDCLIPBOARDURITRANSFERSTATUS_READY          1
/** The transfer is active and running. */
#define SHAREDCLIPBOARDURITRANSFERSTATUS_RUNNING        2
/** The transfer has been completed. */
#define SHAREDCLIPBOARDURITRANSFERSTATUS_COMPLETED      3
/** The transfer has been canceled. */
#define SHAREDCLIPBOARDURITRANSFERSTATUS_CANCELED       4
/** The transfer ran into an unrecoverable error. */
#define SHAREDCLIPBOARDURITRANSFERSTATUS_ERROR          5

/** Defines a transfer status. */
typedef uint32_t SHAREDCLIPBOARDURITRANSFERSTATUS;

/**
 * Structure for an (optional) URI transfer event payload.
 */
typedef struct _SHAREDCLIPBOARDURITRANSFERPAYLOAD
{
    /** Payload ID; currently unused. */
    uint32_t uID;
    /** Pointer to actual payload data. */
    void    *pvData;
    /** Size (in bytes) of actual payload data. */
    uint32_t cbData;
} SHAREDCLIPBOARDURITRANSFERPAYLOAD, *PSHAREDCLIPBOARDURITRANSFERPAYLOAD;

/**
 * Structure for maintaining an URI transfer event.
 */
typedef struct _SHAREDCLIPBOARDURITRANSFEREVENT
{
    /** Event semaphore for signalling the event. */
    RTSEMEVENT                         hEventSem;
    /** Payload to this event. Optional and can be NULL. */
    PSHAREDCLIPBOARDURITRANSFERPAYLOAD pPayload;
} SHAREDCLIPBOARDURITRANSFEREVENT, *PSHAREDCLIPBOARDURITRANSFEREVENT;

/**
 * Enumeration for an URI transfer event type.
 */
typedef enum _SHAREDCLIPBOARDURITRANSFEREVENTTYPE
{
    SHAREDCLIPBOARDURITRANSFEREVENTTYPE_UNKNOWN,
    SHAREDCLIPBOARDURITRANSFEREVENTTYPE_LIST_OPEN,
    SHAREDCLIPBOARDURITRANSFEREVENTTYPE_LIST_CLOSE,
    SHAREDCLIPBOARDURITRANSFEREVENTTYPE_LIST_HDR_READ,
    SHAREDCLIPBOARDURITRANSFEREVENTTYPE_LIST_HDR_WRITE,
    SHAREDCLIPBOARDURITRANSFEREVENTTYPE_LIST_ENTRY_READ,
    SHAREDCLIPBOARDURITRANSFEREVENTTYPE_LIST_ENTRY_WRITE,
    SHAREDCLIPBOARDURITRANSFEREVENTTYPE_FILE_OPEN,
    SHAREDCLIPBOARDURITRANSFEREVENTTYPE_FILE_CLOSE,
    SHAREDCLIPBOARDURITRANSFEREVENTTYPE_FILE_READ,
    SHAREDCLIPBOARDURITRANSFEREVENTTYPE_FILE_WRITE,
    SHAREDCLIPBOARDURITRANSFEREVENTTYPE_ERROR,
    /** Marks the end of the event list. */
    SHAREDCLIPBOARDURITRANSFEREVENTTYPE_LAST,
    /** The usual 32-bit hack. */
    SHAREDCLIPBOARDURITRANSFEREVENTTYPE_32BIT_HACK = 0x7fffffff
} SHAREDCLIPBOARDURITRANSFEREVENTTYPE;

/** Map of URI transfer events.
 *  The key specifies the event type of SHAREDCLIPBOARDURITRANSFEREVENTTYPE. */
typedef std::map<uint32_t, SHAREDCLIPBOARDURITRANSFEREVENT *> SharedClipboardURITransferEventMap;

typedef struct _SHAREDCLIPBOARDURILISTHANDLEINFO
{
    VBOXCLIPBOARDLISTOPENPARMS OpenParms;
    RTFMODE                     fMode;
    union
    {
        struct
        {
            union
            {
                RTDIR  hDirRoot;
                RTFILE hFile;
            };
        } Local;
    } u;
} SHAREDCLIPBOARDURILISTHANDLEINFO, *PSHAREDCLIPBOARDURILISTHANDLEINFO;

typedef struct _SHAREDCLIPBOARDOBJHANDLEINFO
{
    union
    {
        struct
        {
            RTDIR hDirRoot;
        } Local;
    } u;
} SHAREDCLIPBOARDURIOBJHANDLEINFO, *PSHAREDCLIPBOARDURIOBJHANDLEINFO;

/** Map of URI list handles.
 *  The key specifies the list handle. */
typedef std::map<SHAREDCLIPBOARDLISTHANDLE, SHAREDCLIPBOARDURILISTHANDLEINFO *> SharedClipboardURIListMap;

typedef std::map<SHAREDCLIPBOARDOBJHANDLE, SHAREDCLIPBOARDURILISTHANDLEINFO *> SharedClipboardURIObjMap;

/**
 * Structure for maintaining an URI transfer state.
 * Everything in here will be part of a saved state (later).
 */
typedef struct _SHAREDCLIPBOARDURITRANSFERSTATE
{
    /** The transfer's current status. */
    SHAREDCLIPBOARDURITRANSFERSTATUS    enmStatus;
    /** The transfer's direction. */
    SHAREDCLIPBOARDURITRANSFERDIR       enmDir;
    /** The transfer's source. */
    SHAREDCLIPBOARDSOURCE               enmSource;
} SHAREDCLIPBOARDURITRANSFERSTATE, *PSHAREDCLIPBOARDURITRANSFERSTATE;

struct _SHAREDCLIPBOARDURITRANSFER;
typedef struct _SHAREDCLIPBOARDURITRANSFER *PSHAREDCLIPBOARDURITRANSFER;

/**
 * Structure maintaining URI clipboard provider context data.
 * This is handed in to the provider implementation callbacks.
 */
typedef struct _SHAREDCLIPBOARDPROVIDERCTX
{
    /** Pointer to the related URI transfer. */
    PSHAREDCLIPBOARDURITRANSFER pTransfer;
    /** User-defined data pointer. Can be NULL if not needed. */
    void                       *pvUser;
} SHAREDCLIPBOARDPROVIDERCTX, *PSHAREDCLIPBOARDPROVIDERCTX;

/** Defines an URI clipboard provider function declaration with additional parameters. */
#define SHAREDCLIPBOARDPROVIDERFUNCDECL(a_Name, ...) \
    typedef DECLCALLBACK(int) RT_CONCAT(FNSHAREDCLIPBOARDPROVIDER, a_Name)(PSHAREDCLIPBOARDPROVIDERCTX, __VA_ARGS__); \
    typedef RT_CONCAT(FNSHAREDCLIPBOARDPROVIDER, a_Name) RT_CONCAT(*PFNSHAREDCLIPBOARDPROVIDER, a_Name);

/** Defines an URI clipboard provider function declaration with additional parameters. */
#define SHAREDCLIPBOARDPROVIDERFUNCDECLRET(a_Ret, a_Name, ...) \
    typedef DECLCALLBACK(a_Ret) RT_CONCAT(FNSHAREDCLIPBOARDPROVIDER, a_Name)(PSHAREDCLIPBOARDPROVIDERCTX, __VA_ARGS__); \
    typedef RT_CONCAT(FNSHAREDCLIPBOARDPROVIDER, a_Name) RT_CONCAT(*PFNSHAREDCLIPBOARDPROVIDER, a_Name);

/** Defines an URI clipboard provider function declaration (no additional parameters). */
#define SHAREDCLIPBOARDPROVIDERFUNCDECLVOID(a_Name) \
    typedef DECLCALLBACK(int) RT_CONCAT(FNSHAREDCLIPBOARDPROVIDER, a_Name)(PSHAREDCLIPBOARDPROVIDERCTX); \
    typedef RT_CONCAT(FNSHAREDCLIPBOARDPROVIDER, a_Name) RT_CONCAT(*PFNSHAREDCLIPBOARDPROVIDER, a_Name);

/** Declares a URI clipboard provider function member. */
#define SHAREDCLIPBOARDPROVIDERFUNCMEMBER(a_Name, a_Member) \
    RT_CONCAT(PFNSHAREDCLIPBOARDPROVIDER, a_Name) a_Member;

SHAREDCLIPBOARDPROVIDERFUNCDECLVOID(TRANSFEROPEN)
SHAREDCLIPBOARDPROVIDERFUNCDECLVOID(TRANSFERCLOSE)
SHAREDCLIPBOARDPROVIDERFUNCDECL(LISTOPEN, PVBOXCLIPBOARDLISTOPENPARMS pOpenParms, PSHAREDCLIPBOARDLISTHANDLE phList)
SHAREDCLIPBOARDPROVIDERFUNCDECL(LISTCLOSE, SHAREDCLIPBOARDLISTHANDLE hList);
SHAREDCLIPBOARDPROVIDERFUNCDECL(LISTHDRREAD, SHAREDCLIPBOARDLISTHANDLE hList, PVBOXCLIPBOARDLISTHDR pListHdr)
SHAREDCLIPBOARDPROVIDERFUNCDECL(LISTHDRWRITE, SHAREDCLIPBOARDLISTHANDLE hList, PVBOXCLIPBOARDLISTHDR pListHdr)
SHAREDCLIPBOARDPROVIDERFUNCDECL(LISTENTRYREAD, SHAREDCLIPBOARDLISTHANDLE hList, PVBOXCLIPBOARDLISTENTRY pEntry)
SHAREDCLIPBOARDPROVIDERFUNCDECL(LISTENTRYWRITE, SHAREDCLIPBOARDLISTHANDLE hList, PVBOXCLIPBOARDLISTENTRY pEntry)
SHAREDCLIPBOARDPROVIDERFUNCDECL(OBJOPEN, const char *pszPath, PVBOXCLIPBOARDCREATEPARMS pCreateParms, PSHAREDCLIPBOARDOBJHANDLE phObj)
SHAREDCLIPBOARDPROVIDERFUNCDECL(OBJCLOSE, SHAREDCLIPBOARDOBJHANDLE hObj)
SHAREDCLIPBOARDPROVIDERFUNCDECL(OBJREAD, SHAREDCLIPBOARDOBJHANDLE hObj, void *pvData, uint32_t cbData, uint32_t fFlags, uint32_t *pcbRead)
SHAREDCLIPBOARDPROVIDERFUNCDECL(OBJWRITE, SHAREDCLIPBOARDOBJHANDLE hObj, void *pvData, uint32_t cbData, uint32_t fFlags, uint32_t *pcbWritten)
SHAREDCLIPBOARDPROVIDERFUNCDECLRET(uint64_t, OBJGETSIZE, SHAREDCLIPBOARDOBJHANDLE hObj)
SHAREDCLIPBOARDPROVIDERFUNCDECLRET(uint64_t, OBJGETPROCESSED, SHAREDCLIPBOARDOBJHANDLE hObj)
SHAREDCLIPBOARDPROVIDERFUNCDECLRET(const char *, OBJGETPATH, SHAREDCLIPBOARDOBJHANDLE hObj)

/**
 * Shared Clipboard URI provider interface table.
 */
typedef struct _SHAREDCLIPBOARDPROVIDERINTERFACE
{
    SHAREDCLIPBOARDPROVIDERFUNCMEMBER(TRANSFEROPEN, pfnTransferOpen);
    SHAREDCLIPBOARDPROVIDERFUNCMEMBER(TRANSFERCLOSE, pfnTransferClose);
    SHAREDCLIPBOARDPROVIDERFUNCMEMBER(LISTOPEN, pfnListOpen);
    SHAREDCLIPBOARDPROVIDERFUNCMEMBER(LISTCLOSE, pfnListClose);
    SHAREDCLIPBOARDPROVIDERFUNCMEMBER(LISTHDRREAD, pfnListHdrRead);
    SHAREDCLIPBOARDPROVIDERFUNCMEMBER(LISTHDRWRITE, pfnListHdrWrite);
    SHAREDCLIPBOARDPROVIDERFUNCMEMBER(LISTENTRYREAD, pfnListEntryRead);
    SHAREDCLIPBOARDPROVIDERFUNCMEMBER(LISTENTRYWRITE, pfnListEntryWrite);
    SHAREDCLIPBOARDPROVIDERFUNCMEMBER(OBJOPEN, pfnObjOpen);
    SHAREDCLIPBOARDPROVIDERFUNCMEMBER(OBJCLOSE, pfnObjClose);
    SHAREDCLIPBOARDPROVIDERFUNCMEMBER(OBJREAD, pfnObjRead);
    SHAREDCLIPBOARDPROVIDERFUNCMEMBER(OBJWRITE, pfnObjWrite);
    SHAREDCLIPBOARDPROVIDERFUNCMEMBER(OBJGETSIZE, pfnObjGetSize);
    SHAREDCLIPBOARDPROVIDERFUNCMEMBER(OBJGETPROCESSED, pfnObjGetProcessed);
    SHAREDCLIPBOARDPROVIDERFUNCMEMBER(OBJGETPATH, pfnObjGetPath);
} SHAREDCLIPBOARDPROVIDERINTERFACE, *PSHAREDCLIPBOARDPROVIDERINTERFACE;

/**
 * Structure for the Shared Clipboard provider creation context.
 */
typedef struct _SHAREDCLIPBOARDPROVIDERCREATIONCTX
{
    /** Specifies what the source of the provider is. */
    SHAREDCLIPBOARDSOURCE             enmSource;
    /** The provider interface table. */
    SHAREDCLIPBOARDPROVIDERINTERFACE  Interface;
    /** Provider callback data. */
    void                             *pvUser;
} SHAREDCLIPBOARDPROVIDERCREATIONCTX, *PSHAREDCLIPBOARDPROVIDERCREATIONCTX;

struct _SHAREDCLIPBOARDURITRANSFER;
typedef _SHAREDCLIPBOARDURITRANSFER *PSHAREDCLIPBOARDURITRANSFER;

/**
 * Structure for storing URI transfer callback data.
 */
typedef struct _SHAREDCLIPBOARDURITRANSFERCALLBACKDATA
{
    /** Pointer to related URI transfer. */
    PSHAREDCLIPBOARDURITRANSFER pTransfer;
    /** Saved user pointer. */
    void                       *pvUser;
} SHAREDCLIPBOARDURITRANSFERCALLBACKDATA, *PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA;

#define SHAREDCLIPBOARDTRANSFERCALLBACKDECLVOID(a_Name) \
    typedef DECLCALLBACK(void) RT_CONCAT(FNSHAREDCLIPBOARDCALLBACK, a_Name)(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData); \
    typedef RT_CONCAT(FNSHAREDCLIPBOARDCALLBACK, a_Name) RT_CONCAT(*PFNSHAREDCLIPBOARDCALLBACK, a_Name);

#define SHAREDCLIPBOARDTRANSFERCALLBACKDECL(a_Name, ...) \
    typedef DECLCALLBACK(void) RT_CONCAT(FNSHAREDCLIPBOARDCALLBACK, a_Name)(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData, __VA_ARGS__); \
    typedef RT_CONCAT(FNSHAREDCLIPBOARDCALLBACK, a_Name) RT_CONCAT(*PFNSHAREDCLIPBOARDCALLBACK, a_Name);

#define SHAREDCLIPBOARDTRANSFERCALLBACKMEMBER(a_Name, a_Member) \
    RT_CONCAT(PFNSHAREDCLIPBOARDCALLBACK, a_Name) a_Member;

SHAREDCLIPBOARDTRANSFERCALLBACKDECLVOID(TRANSFERPREPARE)
SHAREDCLIPBOARDTRANSFERCALLBACKDECLVOID(TRANSFERSTARTED)
SHAREDCLIPBOARDTRANSFERCALLBACKDECLVOID(LISTHEADERCOMPLETE)
SHAREDCLIPBOARDTRANSFERCALLBACKDECLVOID(LISTENTRYCOMPLETE)
SHAREDCLIPBOARDTRANSFERCALLBACKDECL    (TRANSFERCOMPLETE, int rc)
SHAREDCLIPBOARDTRANSFERCALLBACKDECLVOID(TRANSFERCANCELED)
SHAREDCLIPBOARDTRANSFERCALLBACKDECL    (TRANSFERERROR, int rc)

/**
 * Structure acting as a function callback table for URI transfers.
 * All callbacks are optional and therefore can be NULL.
 */
typedef struct _SHAREDCLIPBOARDURITRANSFERCALLBACKS
{
    /** Saved user pointer. */
    void                                  *pvUser;
    /** Function pointer, called when the transfer is going to be prepared. */
    SHAREDCLIPBOARDTRANSFERCALLBACKMEMBER(TRANSFERPREPARE, pfnTransferPrepare);
    /** Function pointer, called when the transfer has been started. */
    SHAREDCLIPBOARDTRANSFERCALLBACKMEMBER(TRANSFERSTARTED, pfnTransferStarted);
    /** Function pointer, called when reading / writing the list header is complete. */
    SHAREDCLIPBOARDTRANSFERCALLBACKMEMBER(LISTHEADERCOMPLETE, pfnListHeaderComplete);
    /** Function pointer, called when reading / writing a list entry is complete. */
    SHAREDCLIPBOARDTRANSFERCALLBACKMEMBER(LISTENTRYCOMPLETE, pfnListEntryComplete);
    /** Function pointer, called when the transfer is complete. */
    SHAREDCLIPBOARDTRANSFERCALLBACKMEMBER(TRANSFERCOMPLETE, pfnTransferComplete);
    /** Function pointer, called when the transfer has been canceled. */
    SHAREDCLIPBOARDTRANSFERCALLBACKMEMBER(TRANSFERCANCELED, pfnTransferCanceled);
    /** Function pointer, called when transfer resulted in an unrecoverable error. */
    SHAREDCLIPBOARDTRANSFERCALLBACKMEMBER(TRANSFERERROR, pfnTransferError);
} SHAREDCLIPBOARDURITRANSFERCALLBACKS, *PSHAREDCLIPBOARDURITRANSFERCALLBACKS;

/**
 * Structure for thread-related members for a single URI transfer.
 */
typedef struct _SHAREDCLIPBOARDURITRANSFERTHREAD
{
    /** Thread handle for the reading / writing thread.
     *  Can be NIL_RTTHREAD if not being used. */
    RTTHREAD                    hThread;
    /** Thread started indicator. */
    volatile bool               fStarted;
    /** Thread stop flag. */
    volatile bool               fStop;
    /** Thread cancelled flag / indicator. */
    volatile bool               fCancelled;
} SHAREDCLIPBOARDURITRANSFERTHREAD, *PSHAREDCLIPBOARDURITRANSFERTHREAD;

/**
 * Structure for maintaining a single URI transfer.
 *
 ** @todo Not yet thread safe.
 */
typedef struct _SHAREDCLIPBOARDURITRANSFER
{
    /** The node member for using this struct in a RTList. */
    RTLISTNODE                          Node;
    /** Critical section for serializing access. */
    RTCRITSECT                          CritSect;
    /** The transfer's state (for SSM, later). */
    SHAREDCLIPBOARDURITRANSFERSTATE     State;
    /** Events related to this transfer. */
    SharedClipboardURITransferEventMap *pMapEvents;
    SharedClipboardURIListMap          *pMapLists;
    SharedClipboardURIObjMap           *pMapObj;
    /** The transfer's own (local) area, if any (can be NULL if not needed).
     *  The area itself has a clipboard area ID assigned.
     *  On the host this area ID gets shared (maintained / locked) across all VMs via VBoxSVC. */
    SharedClipboardArea                *pArea;
    SHAREDCLIPBOARDPROVIDERCTX          ProviderCtx;
    /** The transfer's provider interface. */
    SHAREDCLIPBOARDPROVIDERINTERFACE    ProviderIface;
    /** The transfer's (optional) callback table. */
    SHAREDCLIPBOARDURITRANSFERCALLBACKS Callbacks;
    /** Opaque pointer to implementation-specific parameters. */
    void                               *pvUser;
    /** Size (in bytes) of implementation-specific parameters. */
    size_t                              cbUser;
    /** Contains thread-related attributes. */
    SHAREDCLIPBOARDURITRANSFERTHREAD    Thread;
} SHAREDCLIPBOARDURITRANSFER, *PSHAREDCLIPBOARDURITRANSFER;

/**
 * Structure for keeping URI clipboard information around.
 */
typedef struct _SHAREDCLIPBOARDURICTX
{
    /** Critical section for serializing access. */
    RTCRITSECT                  CritSect;
    /** List of transfers. */
    RTLISTANCHOR                List;
    /** Number of running (concurrent) transfers.
     *  At the moment we only support only one transfer per client at a time. */
    uint32_t                    cRunning;
    /** Maximum Number of running (concurrent) transfers.
     *  At the moment we only support only one transfer per client at a time. */
    uint32_t                    cMaxRunning;
    /** Number of total transfers (in list). */
    uint32_t                    cTransfers;
} SHAREDCLIPBOARDURICTX, *PSHAREDCLIPBOARDURICTX;

int SharedClipboardURIObjCtxInit(PSHAREDCLIPBOARDCLIENTURIOBJCTX pObjCtx);
void SharedClipboardURIObjCtxDestroy(PSHAREDCLIPBOARDCLIENTURIOBJCTX pObjCtx);
bool SharedClipboardURIObjCtxIsValid(PSHAREDCLIPBOARDCLIENTURIOBJCTX pObjCtx);

int SharedClipboardURITransferCreate(SHAREDCLIPBOARDURITRANSFERDIR enmDir, SHAREDCLIPBOARDSOURCE enmSource,
                                     PSHAREDCLIPBOARDURITRANSFER *ppTransfer);
int SharedClipboardURITransferDestroy(PSHAREDCLIPBOARDURITRANSFER pTransfer);

int SharedClipboardURITransferOpen(PSHAREDCLIPBOARDURITRANSFER pTransfer);
int SharedClipboardURITransferClose(PSHAREDCLIPBOARDURITRANSFER pTransfer);

int SharedClipboardURITransferListOpen(PSHAREDCLIPBOARDURITRANSFER pTransfer, PVBOXCLIPBOARDLISTOPENPARMS pOpenParms,
                                       PSHAREDCLIPBOARDLISTHANDLE phList);
int SharedClipboardURITransferListClose(PSHAREDCLIPBOARDURITRANSFER pTransfer, SHAREDCLIPBOARDLISTHANDLE hList);
int SharedClipboardURITransferListGetHeader(PSHAREDCLIPBOARDURITRANSFER pTransfer, SHAREDCLIPBOARDLISTHANDLE hList,
                                            PVBOXCLIPBOARDLISTHDR pHdr);
PSHAREDCLIPBOARDURITRANSFEROBJ SharedClipboardURITransferListGetObj(PSHAREDCLIPBOARDURITRANSFER pTransfer,
                                                                    SHAREDCLIPBOARDLISTHANDLE hList, uint64_t uIdx);
int SharedClipboardURITransferListRead(PSHAREDCLIPBOARDURITRANSFER pTransfer, SHAREDCLIPBOARDLISTHANDLE hList,
                                       PVBOXCLIPBOARDLISTENTRY pEntry);
int SharedClipboardURITransferListWrite(PSHAREDCLIPBOARDURITRANSFER pTransfer, SHAREDCLIPBOARDLISTHANDLE hList,
                                        PVBOXCLIPBOARDLISTENTRY pEntry);
bool SharedClipboardURITransferListHandleIsValid(PSHAREDCLIPBOARDURITRANSFER pTransfer, SHAREDCLIPBOARDLISTHANDLE hList);

int SharedClipboardURITransferPrepare(PSHAREDCLIPBOARDURITRANSFER pTransfer);
int SharedClipboardURITransferSetInterface(PSHAREDCLIPBOARDURITRANSFER pTransfer,
                                           PSHAREDCLIPBOARDPROVIDERCREATIONCTX pCreationCtx);
void SharedClipboardURITransferReset(PSHAREDCLIPBOARDURITRANSFER pTransfer);
SharedClipboardArea *SharedClipboardURITransferGetArea(PSHAREDCLIPBOARDURITRANSFER pTransfer);
SHAREDCLIPBOARDSOURCE SharedClipboardURITransferGetSource(PSHAREDCLIPBOARDURITRANSFER pTransfer);
SHAREDCLIPBOARDURITRANSFERSTATUS SharedClipboardURITransferGetStatus(PSHAREDCLIPBOARDURITRANSFER pTransfer);
int SharedClipboardURITransferHandleReply(PSHAREDCLIPBOARDURITRANSFER pTransfer, PVBOXCLIPBOARDREPLY pReply);
int SharedClipboardURITransferRun(PSHAREDCLIPBOARDURITRANSFER pTransfer, PFNRTTHREAD pfnThreadFunc, void *pvUser);
void SharedClipboardURITransferSetCallbacks(PSHAREDCLIPBOARDURITRANSFER pTransfer,
                                            PSHAREDCLIPBOARDURITRANSFERCALLBACKS pCallbacks);

int SharedClipboardURITransferPayloadAlloc(uint32_t uID, const void *pvData, uint32_t cbData,
                                           PSHAREDCLIPBOARDURITRANSFERPAYLOAD *ppPayload);
void SharedClipboardURITransferPayloadFree(PSHAREDCLIPBOARDURITRANSFERPAYLOAD pPayload);

int SharedClipboardURITransferEventRegister(PSHAREDCLIPBOARDURITRANSFER pTransfer, uint32_t uID);
int SharedClipboardURITransferEventUnregister(PSHAREDCLIPBOARDURITRANSFER pTransfer, uint32_t uID);
int SharedClipboardURITransferEventWait(PSHAREDCLIPBOARDURITRANSFER pTransfer, uint32_t uID, RTMSINTERVAL uTimeoutMs,
                                        PSHAREDCLIPBOARDURITRANSFERPAYLOAD *ppPayload);
int SharedClipboardURITransferEventSignal(PSHAREDCLIPBOARDURITRANSFER pTransfer, uint32_t uID, PSHAREDCLIPBOARDURITRANSFERPAYLOAD pPayload);

int SharedClipboardURITransferRead(PSHAREDCLIPBOARDURITRANSFER pTransfer);
int SharedClipboardURITransferReadObjects(PSHAREDCLIPBOARDURITRANSFER pTransfer);

int SharedClipboardURITransferWrite(PSHAREDCLIPBOARDURITRANSFER pTransfer);
int SharedClipboardURITransferWriteObjects(PSHAREDCLIPBOARDURITRANSFER pTransfer);

int SharedClipboardURICtxInit(PSHAREDCLIPBOARDURICTX pURI);
void SharedClipboardURICtxDestroy(PSHAREDCLIPBOARDURICTX pURI);
void SharedClipboardURICtxReset(PSHAREDCLIPBOARDURICTX pURI);
PSHAREDCLIPBOARDURITRANSFER SharedClipboardURICtxGetTransfer(PSHAREDCLIPBOARDURICTX pURI, uint32_t uIdx);
uint32_t SharedClipboardURICtxGetRunningTransfers(PSHAREDCLIPBOARDURICTX pURI);
uint32_t SharedClipboardURICtxGetTotalTransfers(PSHAREDCLIPBOARDURICTX pURI);
void SharedClipboardURICtxTransfersCleanup(PSHAREDCLIPBOARDURICTX pURI);
bool SharedClipboardURICtxTransfersMaximumReached(PSHAREDCLIPBOARDURICTX pURI);
int SharedClipboardURICtxTransferAdd(PSHAREDCLIPBOARDURICTX pURI, PSHAREDCLIPBOARDURITRANSFER pTransfer);
int SharedClipboardURICtxTransferRemove(PSHAREDCLIPBOARDURICTX pURI, PSHAREDCLIPBOARDURITRANSFER pTransfer);

void SharedClipboardFsObjFromIPRT(PSHAREDCLIPBOARDFSOBJINFO pDst, PCRTFSOBJINFO pSrc);

bool SharedClipboardMIMEHasFileURLs(const char *pcszFormat, size_t cchFormatMax);
bool SharedClipboardMIMENeedsCache(const char *pcszFormat, size_t cchFormatMax);

#endif /* !VBOX_INCLUDED_GuestHost_SharedClipboard_uri_h */

