/** @file
 *
 * VirtualBox additions user session daemon.
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___vboxclient_vboxclient_h
# define ___vboxclient_vboxclient_h

#include <iprt/cpputils.h>

/** Namespace for VBoxClient-specific things */
namespace VBoxClient {

/** A simple class describing a service.  VBoxClient will run exactly one
 * service per invocation. */
class Service : public stdx::non_copyable
{
public:
    /** Get the services default path to pidfile, relative to $HOME */
    virtual const char *getPidFilePath() = 0;
    /** Run the service main loop */
    virtual int run() = 0;
    /** Clean up any global resources before we shut down hard */
    virtual void cleanup() = 0;
    /** Virtual destructor.  Not used */
    virtual ~Service() {}
};

extern Service *GetClipboardService();
extern Service *GetSeamlessService();
extern Service *GetAutoResizeService();

extern void CleanUp();

} /* namespace VBoxClient */

#endif /* !___vboxclient_vboxclient_h */
