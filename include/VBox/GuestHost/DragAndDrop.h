/* $Id$ */
/** @file
 * DnD: Share functions between host and guest.
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
#include <iprt/types.h>

#include <iprt/cpp/list.h>
#include <iprt/cpp/ministring.h>

int DnDDirCreateDroppedFilesEx(const char *pszPath, char *pszDropDir, size_t cbDropDir);
int DnDDirCreateDroppedFiles(char *pszDropDir, size_t cbDropDir);

bool DnDMIMEHasFileURLs(const char *pcszFormat, size_t cchFormatMax);
bool DnDMIMENeedsDropDir(const char *pcszFormat, size_t cchFormatMax);

int DnDPathSanitizeFilename(char *pszPath, size_t cbPath);
int DnDPathSanitize(char *pszPath, size_t cbPath);

typedef struct DnDURIPath
{
    DnDURIPath(const RTCString &strSrcPath,
               const RTCString &strDstPath,
               uint32_t fMode, uint64_t cbSize)
        : m_strSrcPath(strSrcPath)
        , m_strDstPath(strDstPath)
        , m_fMode(fMode)
        , m_cbSize(cbSize) {}

    RTCString m_strSrcPath;
    RTCString m_strDstPath;
    uint32_t  m_fMode;
    uint64_t  m_cbSize;

} DnDURIPath;

class DnDURIList
{
public:

    DnDURIList(void);
    virtual ~DnDURIList(void);

public:

    int AppendPath(const char *pszPath, uint32_t fFlags);
    int AppendPathsFromList(const RTCList<RTCString> &lstURI, uint32_t fFlags);
    void Clear(void);
    const DnDURIPath &First(void) const { return m_lstTree.first(); }
    bool IsEmpty(void) { return m_lstTree.isEmpty(); }
    void RemoveFirst(void);
    RTCString RootToString(void) { return RTCString::join(m_lstRoot, "\r\n") + "\r\n"; }
    size_t TotalBytes(void) { return m_cbTotal; }

protected:

    int appendPathRecursive(const char *pcszPath, size_t cbBaseLen, uint32_t fFlags);

protected:

    /** List of top-level (root) URI entries. */
    RTCList<RTCString>     m_lstRoot;
    /** List of all URI objects added. */
    RTCList<DnDURIPath>    m_lstTree;
    /** Total size of all URI objects, that is, the file
     *  size of all objects (in bytes). */
    size_t                 m_cbTotal;
};
#endif /* ___VBox_GuestHost_DragAndDrop_h */

