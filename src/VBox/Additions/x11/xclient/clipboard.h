/** @file
 *
 * Shared Clipboard:
 * Linux guest.
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
 */

#ifndef __Additions_linux_clipboard_h
# define __Additions_linux_clipboard_h

#include <VBox/log.h>
#include "thread.h"                 /* for VBoxGuestThread */

extern void vboxClipboardDisconnect (void);
extern int vboxClipboardConnect (void);
extern int vboxClipboardMain (void);

/**
 * Display change request monitor thread function
 */
class VBoxGuestClipboardThread : public VBoxGuestThreadFunction
{
private:
    // Copying or assigning a thread object is not sensible
    VBoxGuestClipboardThread(const VBoxGuestClipboardThread&);
    VBoxGuestClipboardThread& operator=(const VBoxGuestClipboardThread&);

    // Private member variables
    /** Have we been initialised yet? */
    bool mInit;
    /** The thread object running us. */
    VBoxGuestThread *mThread;
public:
    VBoxGuestClipboardThread() { mInit = false; }
    /**
     * Initialise the class and check that the guest supports dynamic resizing.
     * @returns iprt status value
     */
    int init(void)
    {
        if (mInit)
            return true;
        return vboxClipboardConnect();
    }
    /**
     * The actual thread function.
     *
     * @returns iprt status code as thread return value
     * @param pParent the VBoxGuestThread running this thread function
     */
    virtual int threadFunction(VBoxGuestThread *pThread)
    {
        vboxClipboardMain();
        return VINF_SUCCESS;
    }
    /**
     * Send a signal to the thread function that it should exit
     */
    virtual void stop(void) { vboxClipboardDisconnect(); }
};

class VBoxGuestClipboard
{
private:
    /** No copying or assignment. */
    VBoxGuestClipboard(const VBoxGuestClipboard &);
    VBoxGuestClipboard& operator=(const VBoxGuestClipboard &);

    /** Our monitor thread function */
    VBoxGuestClipboardThread mThreadFunction;
    /** And the thread for the thread function */
    VBoxGuestThread mThread;
    /** Are we initialised? */
    bool mInit;
public:
    /**
     * Initialise the class.
     * @returns iprt status value
     */
    int init(void)
    {
        LogFlowThisFunc(("\n"));
        int rc = mThreadFunction.init();
        if (RT_SUCCESS(rc))
            rc = mThread.start();
        if (RT_SUCCESS(rc))
            mInit = true;
        LogFlowThisFunc(("returning %Rrc\n", rc));
        return rc;
    }
    /**
     * Uninitialise the class.
     * @param cMillies how long to wait for the thread to stop
     */
    void uninit(unsigned cMillies = RT_INDEFINITE_WAIT)
    {
        LogFlowThisFunc(("\n"));
        if (mInit)
            mThread.stop(cMillies, NULL);
        LogFlowThisFunc(("returning\n"));
    }

    VBoxGuestClipboard() : mThread(&mThreadFunction, 0, RTTHREADTYPE_MSG_PUMP,
                                   RTTHREADFLAGS_WAITABLE, "SHCLIP MAIN")
    { mInit = false; }
    ~VBoxGuestClipboard()
    {
        LogFlowThisFunc(("\n"));
        if (mInit)
            try {
                uninit(2000);
            } catch (...) { }
        LogFlowThisFunc(("returning\n"));
    }
};

#endif /* __Additions_linux_clipboard_h not defined */
