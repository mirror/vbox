/* $Id$ */

/** @file
 *
 * VirtualBox API client token holder (in the client process)
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

#ifndef ____H_CLIENTTOKENHOLDER
#define ____H_CLIENTTOKENHOLDER

#include "SessionImpl.h"

#if defined(RT_OS_WINDOWS)
# define CTHSEMARG NULL
# define CTHSEMTYPE HANDLE
/* this second semaphore is only used on Windows */
# define CTHTHREADSEMARG NULL
# define CTHTHREADSEMTYPE HANDLE
#elif defined(RT_OS_OS2)
# define CTHSEMARG NIL_RTSEMEVENT
# define CTHSEMTYPE RTSEMEVENT
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
# define CTHSEMARG -1
# define CTHSEMTYPE int
#else
# error "Port me!"
#endif


/**
 * Class which holds a client token.
 */
class Session::ClientTokenHolder
{
public:
    /**
     * Constructor which creates a usable instance
     *
     * @param strTokenId    String with identifier of the token
     */
    ClientTokenHolder(const Utf8Str &strTokenId);

    /**
     * Default destructor. Cleans everything up.
     */
    ~ClientTokenHolder();

    /**
     * Check if object contains a usable token.
     */
    bool isReady();

private:
    /**
     * Default constructor. Don't use, will not create a sensible instance.
     */
    ClientTokenHolder();

    Utf8Str mClientTokenId;
    CTHSEMTYPE mSem;
    RTTHREAD mThread;
#ifdef RT_OS_WINDOWS
    CTHTHREADSEMTYPE mThreadSem;
#endif
};

#endif /* !____H_CLIENTTOKENHOLDER */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
