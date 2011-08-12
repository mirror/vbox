/** @file
 *
 * VirtualBox Guest Control - Private data definitions / classes.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_GUESTIMPLPRIVATE
#define ____H_GUESTIMPLPRIVATE

#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/VirtualBox.h>

#include <map>
#include <vector>

using namespace com;

#ifdef VBOX_WITH_GUEST_CONTROL
# include <VBox/HostServices/GuestControlSvc.h>
using namespace guestControl;
#endif

class Guest;
class Progress;

/** Structure representing the "value" side of a "key=value" pair. */
class VBOXGUESTCTRL_STREAMVALUE
{
public:

    VBOXGUESTCTRL_STREAMVALUE() { }
    VBOXGUESTCTRL_STREAMVALUE(const char *pszValue)
        : mValue(pszValue) {}

    VBOXGUESTCTRL_STREAMVALUE(const VBOXGUESTCTRL_STREAMVALUE& aThat)
           : mValue(aThat.mValue) {}

    Utf8Str mValue;
};

/** Map containing "key=value" pairs of a guest process stream. */
typedef std::pair< Utf8Str, VBOXGUESTCTRL_STREAMVALUE > GuestCtrlStreamPair;
typedef std::map < Utf8Str, VBOXGUESTCTRL_STREAMVALUE > GuestCtrlStreamPairMap;
typedef std::map < Utf8Str, VBOXGUESTCTRL_STREAMVALUE >::iterator GuestCtrlStreamPairMapIter;
typedef std::map < Utf8Str, VBOXGUESTCTRL_STREAMVALUE >::const_iterator GuestCtrlStreamPairMapIterConst;

/**
 * Class representing a block of stream pairs (key=value). Each block in a raw guest
 * output stream is separated by "\0\0", each pair is separated by "\0". The overall
 * end of a guest stream is marked by "\0\0\0\0".
 */
class GuestProcessStreamBlock
{
public:

    GuestProcessStreamBlock();

    //GuestProcessStreamBlock(GuestProcessStreamBlock &);

    virtual ~GuestProcessStreamBlock();

public:

    void Clear();

    int GetInt64Ex(const char *pszKey, int64_t *piVal);

    int64_t GetInt64(const char *pszKey);

    size_t GetCount();

    const char* GetString(const char *pszKey);

    int GetUInt32Ex(const char *pszKey, uint32_t *puVal);

    uint32_t GetUInt32(const char *pszKey);

    int SetValue(const char *pszKey, const char *pszValue);

protected:

    GuestCtrlStreamPairMap m_mapPairs;
};

/** Vector containing multiple allocated stream pair objects. */
typedef std::vector< GuestProcessStreamBlock > GuestCtrlStreamObjects;
typedef std::vector< GuestProcessStreamBlock >::iterator GuestCtrlStreamObjectsIter;
typedef std::vector< GuestProcessStreamBlock >::const_iterator GuestCtrlStreamObjectsIterConst;

/**
 * Class for parsing machine-readable guest process output by VBoxService'
 * toolbox commands ("vbox_ls", "vbox_stat" etc), aka "guest stream".
 */
class GuestProcessStream
{

public:

    GuestProcessStream();

    virtual ~GuestProcessStream();

public:

    int AddData(const BYTE *pbData, size_t cbData);

    void Destroy();

    uint32_t GetOffset();

    int ParseBlock(GuestProcessStreamBlock &streamBlock);

protected:

    /** Currently allocated size of internal stream buffer. */
    uint32_t m_cbAllocated;
    /** Currently used size of allocated internal stream buffer. */
    uint32_t m_cbSize;
    /** Current offset within the internal stream buffer. */
    uint32_t m_cbOffset;
    /** Internal stream buffer. */
    BYTE *m_pbBuffer;
};

class GuestTask
{

public:

    enum TaskType
    {
        /** Copies a file from host to the guest. */
        TaskType_CopyFileToGuest   = 50,
        /** Copies a file from guest to the host. */
        TaskType_CopyFileFromGuest = 55,
        /** Update Guest Additions by directly copying the required installer
         *  off the .ISO file, transfer it to the guest and execute the installer
         *  with system privileges. */
        TaskType_UpdateGuestAdditions = 100
    };

    GuestTask(TaskType aTaskType, Guest *aThat, Progress *aProgress);

    virtual ~GuestTask();

    int startThread();

    static int taskThread(RTTHREAD aThread, void *pvUser);
    static int uploadProgress(unsigned uPercent, void *pvUser);
    static HRESULT setProgressErrorInfo(HRESULT hr,
                                        ComObjPtr<Progress> pProgress, const char * pszText, ...);
    static HRESULT setProgressErrorInfo(HRESULT hr,
                                        ComObjPtr<Progress> pProgress, ComObjPtr<Guest> pGuest);

    TaskType taskType;
    Guest *pGuest;
    ComObjPtr<Progress> progress;
    HRESULT rc;

    /* Task data. */
    Utf8Str strSource;
    Utf8Str strDest;
    Utf8Str strUserName;
    Utf8Str strPassword;
    ULONG   uFlags;
};
#endif // ____H_GUESTIMPLPRIVATE

