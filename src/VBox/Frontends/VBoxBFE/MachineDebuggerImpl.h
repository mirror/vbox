/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * Declaration of MachineDebugger class
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

#ifndef ____H_MACHINEDEBUGGER
#define ____H_MACHINEDEBUGGER

class MachineDebugger
{
public:


    MachineDebugger ();
    virtual ~MachineDebugger () {};

    // IMachineDebugger properties
    STDMETHOD(COMGETTER(Singlestep))(BOOL *enabled);
    STDMETHOD(COMSETTER(Singlestep))(BOOL enable);
    STDMETHOD(COMGETTER(RecompileUser))(BOOL *enabled);
    STDMETHOD(COMSETTER(RecompileUser))(BOOL enable);
    STDMETHOD(COMGETTER(RecompileSupervisor))(BOOL *enabled);
    STDMETHOD(COMSETTER(RecompileSupervisor))(BOOL enable);
    STDMETHOD(COMGETTER(PATMEnabled))(BOOL *enabled);
    STDMETHOD(COMSETTER(PATMEnabled))(BOOL enable);
    STDMETHOD(COMGETTER(CSAMEnabled))(BOOL *enabled);
    STDMETHOD(COMSETTER(CSAMEnabled))(BOOL enable);

    // IMachineDebugger methods

    // "public-private methods"
    void flushQueuedSettings();

private:
    // flags whether settings have been queued because
    // they could not be sent to the VM (not up yet, etc.)
    int singlestepQueued;
    int recompileUserQueued;
    int recompileSupervisorQueued;
    int patmEnabledQueued;
    int csamEnabledQueued;
    bool fFlushMode;
};


extern MachineDebugger *gMachineDebugger;

#endif // ____H_MACHINEDEBUGGER
