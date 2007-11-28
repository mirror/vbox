/** @file
 *
 * VirtualBox additions client application: thread class.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __Additions_client_thread_h
# define __Additions_client_thread_h

#include <iprt/thread.h>
#include <iprt/err.h>

#include <VBox/log.h>

class VBoxGuestThread;

/** Virtual parent class for thread functions for the VBoxGuestThread class. */
class VBoxGuestThreadFunction
{
public:
    VBoxGuestThreadFunction(void);
    virtual ~VBoxGuestThreadFunction(void) {}
    /**
      * The actual thread function.
      *
      * @returns iprt status code as thread return value
      * @param pParent the VBoxGuestThread running this thread function
      */
    virtual int threadFunction(VBoxGuestThread *pPThread) = 0;
    /**
     * Send a signal to the thread function that it should exit.  This should not block.
     */
    virtual void stop(void) = 0;
};

/** C++ wrapper for VBox runtime threads. */
class VBoxGuestThread
{
private:
    // Private member variables
    /** The thread function for this thread */
    VBoxGuestThreadFunction *mFunction;
    /** The size of the stack for the new thread.  Use 0 for the default stack size. */
    size_t mStack;
    /** The thread type. Used for deciding scheduling attributes of the thread. */
    RTTHREADTYPE mType;
    /** Flags of the RTTHREADFLAGS type (ORed together). */
    unsigned mFlags;
    /** Thread name */
    const char *mName;
    /** The VBox runtime thread handle. */
    RTTHREAD mSelf;
    /** Is the thread currently running? */
    volatile bool mRunning;
    /** Should the thread be stopped? */
    volatile bool mExit;

    // Typedefs
    /** Ourselves, for use in the thread function. */
    typedef VBoxGuestThread *PSELF;
public:
    /**
     * Initialise the class.
     * @param   pFunction   the thread function for this thread
     * @param   cbStack     The size of the stack for the new thread.
     *                      Use 0 for the default stack size.
     * @param   enmType     The thread type. Used for deciding scheduling attributes
     *                      of the thread.
     * @param   fFlags      Flags of the RTTHREADFLAGS type (ORed together).
     * @param   pszName     Thread name.
     */
    VBoxGuestThread(VBoxGuestThreadFunction *pFunction, size_t cbStack, RTTHREADTYPE enmType,
                    unsigned fFlags, const char *pszName)
    {
        mFunction = pFunction;
        mStack = cbStack;
        mType = enmType;
        mFlags = fFlags;
        mName = pszName;
        mSelf = NIL_RTTHREAD;
        mRunning = false;
        mExit = false;
    }
    /** Stop the thread using its stop method and get the exit value.
     * @returns iprt status code
     * @param   cMillies        The number of milliseconds to wait. Use RT_INDEFINITE_WAIT for
     *                              an indefinite wait.  Only relevant if the thread is
     *                              waitable.
     * @param   prc             Where to store the return code of the thread. Optional.
     */
    int stop(unsigned cMillies, int *prc);

    /** Destroy the class, stopping the thread if necessary. */
    ~VBoxGuestThread(void);

    /** Return the VBox runtime thread handle. */
    RTTHREAD getSelf(void) { return mSelf; }

    /** Start the thread. */
    int start(void);

    /** Yield the CPU */
    bool yield(void);

    /** Is the thread running? */
    bool isRunning(void) { return mRunning; }

    /** Should the thread function exit? */
    bool isStopping(void) { return mExit; }
private:
    // Copying or assigning a thread object is not sensible
    VBoxGuestThread(const VBoxGuestThread&);
    VBoxGuestThread& operator=(const VBoxGuestThread&);

    // Member functions
    /** The "real" thread function for the VBox runtime. */
    static DECLCALLBACK(int) threadFunction(RTTHREAD self, void *pvUser);
};

#endif /* __Additions_client_thread_h not defined */
