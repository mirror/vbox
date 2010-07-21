/* $Id:$ */
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
import org.virtualbox_3_3.*;
import java.util.List;
import java.util.Arrays;
import java.math.BigInteger;

public class TestVBox
{
    static void processEvent(IEvent ev)
    {
        System.out.println("got event: " + ev);

        VBoxEventType type = ev.getType();
        System.out.println("type = "+type);

        switch (type)
        {
            case OnMachineStateChanged:
                IMachineStateChangedEvent mcse = IMachineStateChangedEvent.queryInterface(ev);
                if (mcse == null)
                    System.out.println("Cannot query an interface");
                else
                    System.out.println("mid="+mcse.getMachineId());
                break;
        }
    }

    static class EventHandler
    {
        EventHandler() {}
        public void handleEvent(IEvent ev)
        {
            try {
                processEvent(ev);
            } catch (Throwable t) {
                t.printStackTrace();
            }
        }
    }

    static void testEvents(VirtualBoxManager mgr, IEventSource es, boolean active)
    {
        // active mode for Java doesn't fully work yet, and using passive
        // is more portable (the only mode for MSCOM and WS) and thus generally
        // recommended
        IEventListener listener = active ? mgr.createListener(new EventHandler()) : es.createListener();

        es.registerListener(listener, Arrays.asList(VBoxEventType.Any), false);

        try {
            for (int i=0; i<100; i++)
            {
                System.out.print(".");
                if (active)
                {
                    mgr.waitForEvents(500);
                }
                else
                {
                    IEvent ev = es.getEvent(listener, 1000);
                    if (ev != null)
                    {
                        processEvent(ev);
                        es.eventProcessed(listener, ev);
                    }
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }

        es.unregisterListener(listener);
    }

    static void testEnumeration(VirtualBoxManager mgr, IVirtualBox vbox)
    {
        List<IMachine> machs = vbox.getMachines();
        for (IMachine m : machs)
        {
            System.out.println("VM name: " + m.getName());// + ", RAM size: " + m.getMemorySize() + "MB");
            System.out.println(" HWVirt: " + m.getHWVirtExProperty(HWVirtExPropertyType.Enabled)
                               + ", Nested Paging: " + m.getHWVirtExProperty(HWVirtExPropertyType.NestedPaging)
                               + ", PAE: " + m.getCPUProperty(CPUPropertyType.PAE) );
        }
    }

    static void testStart(VirtualBoxManager mgr, IVirtualBox vbox)
    {
        String m =  vbox.getMachines().get(0).getName();
        System.out.println("\nAttempting to start VM '" + m + "'");
        mgr.startVm(m, null, 7000);
    }

    public static void main(String[] args)
    {
        VirtualBoxManager mgr = VirtualBoxManager.getInstance(null);

        boolean ws = false;
        String  url = null;
        String  user = null;
        String  passwd = null;

        for (int i = 0; i<args.length; i++)
        {
            if ("-w".equals(args[i]))
                ws = true;
            else if ("-url".equals(args[i]))
                url = args[++i];
            else if ("-user".equals(args[i]))
                user = args[++i];
            else if ("-passwd".equals(args[i]))
                passwd = args[++i];
        }

        if (ws)
        {
            try {
                mgr.connect(url, user, passwd);
            } catch (VBoxException e) {
                e.printStackTrace();
                System.out.println("Cannot connect, start webserver first!");
            }
        }

        try
        {
            IVirtualBox vbox = mgr.getVBox();
            if (vbox != null)
            {
                System.out.println("VirtualBox version: " + vbox.getVersion() + "\n");
                testEnumeration(mgr, vbox);
                testStart(mgr, vbox);
                testEvents(mgr, vbox.getEventSource(), false);

                System.out.println("done, press Enter...");
                int ch = System.in.read();
            }
        }
        catch (VBoxException e)
        {
            System.out.println("VBox error: "+e.getMessage()+" original="+e.getWrapped());
            e.printStackTrace();
        }
        catch (java.io.IOException e)
        {
            e.printStackTrace();
        }

        if (ws)
        {
            try {
                mgr.disconnect();
            } catch (VBoxException e) {
                e.printStackTrace();
            }
        }

        mgr.cleanup();

    }

}
