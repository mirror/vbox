/* $Id$ */
/** @file
 * VBoxBugReport - VirtualBox command-line diagnostics tool, internal header file.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___H_VBOXBUGREPORT
#define ___H_VBOXBUGREPORT

/*
 * Introduction.
 *
 * In the most general sense a bug report is a collection of data obtained from
 * the user's host system. It may include files common for all VMs, like the
 * VBoxSVC.log file, as well as files related to particular machines. It may
 * also contain the output of commands executed on the host, as well as data
 * collected via OS APIs.
 */

/* @todo not sure if using a separate namespace would be beneficial */

#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/tar.h>
#include <iprt/vfs.h>

#ifdef RT_OS_WINDOWS
#define VBOXMANAGE "VBoxManage.exe"
#else /* !RT_OS_WINDOWS */
#define VBOXMANAGE "VBoxManage"
#endif /* !RT_OS_WINDOWS */

/* Base */

inline void handleRtError(int rc, const char *pszMsgFmt, ...)
{
    if (RT_FAILURE(rc))
    {
        va_list va;
        va_start(va, pszMsgFmt);
        RTCString msgArgs(pszMsgFmt, va);
        va_end(va);
        RTCStringFmt msg("%s (rc=%d)\n", msgArgs.c_str(), rc);
        throw RTCError(msg.c_str());
    }
}

inline void handleComError(HRESULT hr, const char *pszMsgFmt, ...)
{
    if (FAILED(hr))
    {
        va_list va;
        va_start(va, pszMsgFmt);
        RTCString msgArgs(pszMsgFmt, va);
        va_end(va);
        RTCStringFmt msg("%s (hr=0x%x)\n", msgArgs.c_str(), hr);
        throw RTCError(msg.c_str());
    }
}

/*
 * An abstract class serving as the root of the bug report item tree.
 */
class BugReportItem
{
public:
    BugReportItem(const char *pszTitle);
    virtual ~BugReportItem();
    virtual const char *getTitle(void);
    virtual PRTSTREAM getStream(void) = 0;
private:
    char *m_pszTitle;
};

/*
 * An abstract class to serve as a base class for all report types.
 */
class BugReport
{
public:
    BugReport(const char *pszFileName);
    virtual ~BugReport();
    virtual int addItem(BugReportItem* item) = 0;
    virtual void complete(void) = 0;
protected:
    char *m_pszFileName;
};

/*
 * An auxiliary class providing formatted output into a temporary file for item
 * classes that obtain data via host OS APIs.
 */
class BugReportStream : public BugReportItem
{
public:
    BugReportStream(const char *pszTitle);
    virtual ~BugReportStream();
    virtual PRTSTREAM getStream(void);
protected:
    int printf(const char *pszFmt, ...);
    int putStr(const char *pszString);
private:
    PRTSTREAM m_Strm;
    char m_szFileName[RTPATH_MAX];
};


/*
 * This class provides a platform-agnostic way to create platform-specific item
 * objects.
 *
 * @todo At the moment it is capable of creating a single object, the one
 * intended to collect network adapter data. There will be more later.
 *
 * @todo Make an abstract class if enough platform-specific factories implemented.
 */
class BugReportItemFactory
{
public:
    virtual BugReportItem *createNetworkAdapterReport(void) { return NULL; };
};



/* Generic */

/*
 * This class reports everything into a single text file.
 */
class BugReportText : public BugReport
{
public:
    BugReportText(const char *pszFileName);
    virtual ~BugReportText();
    virtual int addItem(BugReportItem* item);
    virtual void complete(void) {};
private:
    PRTSTREAM m_StrmTxt;
};

/*
 * This class reports items as individual files archived into a single compressed TAR file.
 */
class BugReportTarGzip : public BugReport
{
public:
    BugReportTarGzip(const char *pszFileName);
    virtual ~BugReportTarGzip();
    virtual int addItem(BugReportItem* item);
    virtual void complete(void);
private:
    /*
     * Helper class to release handles going out of scope.
     */
    class VfsIoStreamHandle
    {
    public:
        VfsIoStreamHandle() : m_hVfsStream(NIL_RTVFSIOSTREAM) {};
        ~VfsIoStreamHandle() { release(); }
        PRTVFSIOSTREAM getPtr(void) { return &m_hVfsStream; };
        RTVFSIOSTREAM get(void) { return m_hVfsStream; };
        void release(void)
        {
            if (m_hVfsStream != NIL_RTVFSIOSTREAM)
                RTVfsIoStrmRelease(m_hVfsStream);
            m_hVfsStream = NIL_RTVFSIOSTREAM;
        };
    private:
        RTVFSIOSTREAM m_hVfsStream;
    };

    VfsIoStreamHandle m_hVfsGzip;

    RTTAR m_hTar;
    RTTARFILE m_hTarFile;
    char m_szTarName[RTPATH_MAX];
};

/*
 * BugReportFile adds a file as an item to a report.
 */
class BugReportFile : public BugReportItem
{
public:
    BugReportFile(const char *pszPath, const char *pcszName);
    virtual ~BugReportFile();
    virtual PRTSTREAM getStream(void);

private:
    char *m_pszPath;
    PRTSTREAM m_Strm;
};

/*
 * A base class for item classes that collect CLI output.
 */
class BugReportCommand : public BugReportItem
{
public:
    BugReportCommand(const char *pszTitle, const char *pszExec, ...);
    virtual ~BugReportCommand();
    virtual PRTSTREAM getStream(void);
private:
    PRTSTREAM m_Strm;
    char m_szFileName[RTPATH_MAX];
    char *m_papszArgs[32];
};


/* Platform-specific */

BugReportItemFactory *createBugReportItemFactory(void);

#endif /* !___H_VBOXBUGREPORT */
