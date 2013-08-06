/* $Id$ */

/** @file
 *
 * VirtualBox API client watcher
 */

/*
 * Copyright (C) 2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_CLIENTWATCHER
#define ____H_CLIENTWATCHER

#include <list>
#include <VBox/com/ptr.h>
#include <VBox/com/AutoLock.h>

#include "VirtualBoxImpl.h"

#if defined(RT_OS_WINDOWS)
# define CWUPDATEREQARG NULL
# define CWUPDATEREQTYPE HANDLE
#elif defined(RT_OS_OS2)
# define CWUPDATEREQARG NIL_RTSEMEVENT
# define CWUPDATEREQTYPE RTSEMEVENT
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
# define CWUPDATEREQARG
# define CWUPDATEREQTYPE RTSEMEVENT
#else
# error "Port me!"
#endif

/**
 * Class which checks for API clients which have crashed/exited, and takes
 * the necessary cleanup actions. Singleton.
 */
class VirtualBox::ClientWatcher
{
public:
    /**
     * Constructor which creates a usable instance
     *
     * @param pVirtualBox   Reference to VirtualBox object
     */
    ClientWatcher(const ComObjPtr<VirtualBox> &pVirtualBox);

    /**
     * Default destructor. Cleans everything up.
     */
    ~ClientWatcher();

    bool isReady();

    void update();
    void addProcess(RTPROCESS pid);

private:
    /**
     * Default constructor. Don't use, will not create a sensible instance.
     */
    ClientWatcher();

    static DECLCALLBACK(int) worker(RTTHREAD /* thread */, void *pvUser);

    VirtualBox *mVirtualBox;
    RTTHREAD mThread;
    CWUPDATEREQTYPE mUpdateReq;
    util::RWLockHandle mLock;

    typedef std::list<RTPROCESS> ProcessList;
    ProcessList mProcesses;

#ifdef VBOX_WITH_SYS_V_IPC_SESSION_WATCHER
    uint8_t mUpdateAdaptCtr;
#endif
};

#endif /* !____H_CLIENTWATCHER */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
