/** @file
 *
 * VirtualBox additions user session daemon.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___vboxclient_vboxclient_h
# define ___vboxclient_vboxclient_h

#include <iprt/cpp/utils.h>

/** Namespace for VBoxClient-specific things */
namespace VBoxClient {

/** A simple class describing a service.  VBoxClient will run exactly one
 * service per invocation. */
class Service : public RTCNonCopyable
{
public:
    /** Get the services default path to pidfile, relative to $HOME */
    virtual const char *getPidFilePath() = 0;
    /** Run the service main loop */
    virtual int run(bool fDaemonised = false) = 0;
    /** Clean up any global resources before we shut down hard */
    virtual void cleanup() = 0;
    /** Virtual destructor.  Not used */
    virtual ~Service() {}
};

extern Service *GetClipboardService();
extern Service *GetSeamlessService();
extern Service *GetDisplayService();
extern Service *GetHostVersionService();
#ifdef VBOX_WITH_DRAG_AND_DROP
extern Service *GetDragAndDropService();
#endif /* VBOX_WITH_DRAG_AND_DROP */

extern void CleanUp();

} /* namespace VBoxClient */

#endif /* !___vboxclient_vboxclient_h */
