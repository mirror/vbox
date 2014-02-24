/* $Id$ */
/** @file
 * DnD: Shared functions between host and guest.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBox_GuestHost_DragAndDrop_h
#define ___VBox_GuestHost_DragAndDrop_h

#include <iprt/assert.h>
#include <iprt/cdefs.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/types.h>

#include <iprt/cpp/list.h>
#include <iprt/cpp/ministring.h>

int DnDDirCreateDroppedFilesEx(const char *pszPath, char *pszDropDir, size_t cbDropDir);
int DnDDirCreateDroppedFiles(char *pszDropDir, size_t cbDropDir);

bool DnDMIMEHasFileURLs(const char *pcszFormat, size_t cchFormatMax);
bool DnDMIMENeedsDropDir(const char *pcszFormat, size_t cchFormatMax);

int DnDPathSanitizeFilename(char *pszPath, size_t cbPath);
int DnDPathSanitize(char *pszPath, size_t cbPath);

class DnDURIObject
{
public:

    enum Type
    {
        Unknown = 0,
        File,
        Directory
    };

    DnDURIObject(Type type,
                 const RTCString &strSrcPath,
                 const RTCString &strDstPath,
                 uint32_t fMode, uint64_t cbSize);
    virtual ~DnDURIObject(void);

public:

    const RTCString &GetSourcePath(void) const { return m_strSrcPath; }
    const RTCString &GetDestPath(void) const { return m_strDstPath; }
    uint32_t GetMode(void) const { return m_fMode; }
    uint64_t GetSize(void) const { return m_cbSize; }
    Type GetType(void) const { return m_Type; }

public:

    bool IsComplete(void) const;
    static int RebaseURIPath(RTCString &strPath, const RTCString &strBaseOld, const RTCString &strBaseNew);
    int Read(void *pvBuf, uint32_t cbToRead, uint32_t *pcbRead);

protected:

    void closeInternal(void);

protected:

    Type      m_Type;
    RTCString m_strSrcPath;
    RTCString m_strDstPath;
    uint32_t  m_fMode;
    /** Size (in bytes) to read/write. */
    uint64_t  m_cbSize;
    /** Bytes processed reading/writing. */
    uint64_t  m_cbProcessed;

    union
    {
        RTFILE m_hFile;
    } u;
};

class DnDURIList
{
public:

    DnDURIList(void);
    virtual ~DnDURIList(void);

public:

    int AppendNativePath(const char *pszPath, uint32_t fFlags);
    int AppendNativePathsFromList(const char *pszNativePaths, size_t cbNativePaths, uint32_t fFlags);
    int AppendNativePathsFromList(const RTCList<RTCString> &lstNativePaths, uint32_t fFlags);
    int AppendURIPath(const char *pszURI, uint32_t fFlags);
    int AppendURIPathsFromList(const char *pszURIPaths, size_t cbURIPaths, uint32_t fFlags);
    int AppendURIPathsFromList(const RTCList<RTCString> &lstURI, uint32_t fFlags);

    void Clear(void);
    DnDURIObject &First(void) { return m_lstTree.first(); }
    bool IsEmpty(void) { return m_lstTree.isEmpty(); }
    void RemoveFirst(void);
    int RootFromURIData(const void *pvData, size_t cbData, uint32_t fFlags);
    RTCString RootToString(const RTCString &strBasePath = "", const RTCString &strSeparator = "\r\n");
    size_t RootCount(void) { return m_lstRoot.size(); }
    size_t TotalBytes(void) { return m_cbTotal; }

protected:

    int appendPathRecursive(const char *pcszPath, size_t cbBaseLen, uint32_t fFlags);

protected:

    /** List of all top-level file/directory entries. */
    RTCList<RTCString>     m_lstRoot;
    /** List of all URI objects added. */
    RTCList<DnDURIObject>  m_lstTree;
    /** Total size of all URI objects, that is, the file
     *  size of all objects (in bytes). */
    size_t                 m_cbTotal;
};
#endif /* ___VBox_GuestHost_DragAndDrop_h */

