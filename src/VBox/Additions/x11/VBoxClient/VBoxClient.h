/** @file
 *
 * VirtualBox additions user session daemon.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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
    /** @todo Should this also have a component relative to the X server number?
     */
    virtual const char *getPidFilePath() = 0;
    /** Special initialisation, if needed.  @a pause and @a resume are
     * guaranteed not to be called until after this returns. */
    virtual int init() { return VINF_SUCCESS; }
    /** Run the service main loop */
    virtual int run(bool fDaemonised = false) = 0;
    /** Pause the service loop.  This must be safe to call on a different thread
     * and potentially before @a run is or after it exits.
     * This is called by the VT monitoring thread to allow the service to disable
     * itself when the X server is switched out.  If the monitoring functionality
     * is available then @a pause or @a resume will be called as soon as it starts
     * up. */
    virtual int pause() { return VINF_SUCCESS; }
    /** Resume after pausing.  The same applies here as for @a pause. */
    virtual int resume() { return VINF_SUCCESS; }
    /** Clean up any global resources before we shut down hard.  The last calls
     * to @a pause and @a resume are guaranteed to finish before this is called.
     */
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
