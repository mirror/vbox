/* $Id$ */
/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
package org.virtualbox;

import java.io.File;

import org.mozilla.xpcom.*;
import org.mozilla.interfaces.*;

public class VirtualBoxManager
{
    private Mozilla             mozilla;
    private IVirtualBox         vbox;
    private nsIComponentManager componentManager;
    private nsIServiceManager   servMgr;

    private VirtualBoxManager(Mozilla mozilla,nsIServiceManager servMgr)
    {
        this.mozilla = mozilla;
        this.servMgr = servMgr;
        this.componentManager = mozilla.getComponentManager();
        this.vbox = (IVirtualBox) this.componentManager
                    .createInstanceByContractID("@virtualbox.org/VirtualBox;1",
                                                null,
						IVirtualBox.IVIRTUALBOX_IID);
        if (this.vbox == null) {
            throw new RuntimeException("Failed to create IVirtualBox");
        }
    }

    public IVirtualBox getVBox()
    {
        return this.vbox;
    }

    public ISession makeSession()
    {
        return (ISession) componentManager
                .createInstanceByContractID("@virtualbox.org/Session;1", null,
                                            ISession.ISESSION_IID);
    }


    private static VirtualBoxManager mgr;

    public static synchronized VirtualBoxManager getInstance(String home)
    {
        if (mgr != null)
            return mgr;

        if (home == null)
            home = System.getProperty("vbox.home");

        File grePath = new File(home);
        Mozilla mozilla = Mozilla.getInstance();
        mozilla.initialize(grePath);
        nsIServiceManager servMgr = null;
        try {
            servMgr = mozilla.initXPCOM(grePath, null);
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }

        mgr = new VirtualBoxManager(mozilla, servMgr);
        return mgr;
    }

    public void cleanup()
    {
        // cleanup
        mozilla.shutdownXPCOM(servMgr);
        mozilla = null;
    }

    public boolean progressBar(IProgress p, int wait)
    {
        long end = System.currentTimeMillis() + wait;
        while (!p.getCompleted())
        {
            mozilla.waitForEvents(0);
            p.waitForCompletion(wait);
            if (System.currentTimeMillis() >= end)
                return false;
        }

        return true;
    }

    public boolean startVm(String name, int timeout)
    {
        IMachine m = vbox.findMachine(name);
        if (m == null)
            return false;
        ISession session = makeSession();


        String mid = m.getId();
        String type = "gui";
        IProgress p = vbox.openRemoteSession(session, mid, type, "");
        progressBar(p, timeout);
        session.close();
        return true;
    }

    public Mozilla getMozilla()
    {
        return mozilla;
    }

    public void waitForEvents(long tmo)
    {
        mozilla.waitForEvents(tmo);
    }

    public ILocalOwner makeWrapper(nsISupports obj)
    {

       ILocalOwner lo = (ILocalOwner) this.componentManager
               .createInstanceByContractID("@virtualbox.org/CallbackWrapper;1",
                                           null,
                                           ILocalOwner.ILOCALOWNER_IID);
       lo.setLocalObject(obj);
       return lo;
    }

    public IVirtualBoxCallback makeVirtualBoxCallback(IVirtualBoxCallback obj)
    {
       ILocalOwner lo = makeWrapper(obj);
       return (IVirtualBoxCallback)lo.queryInterface(IVirtualBoxCallback.IVIRTUALBOXCALLBACK_IID);
    }
}
