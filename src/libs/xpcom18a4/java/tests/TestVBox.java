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

import org.mozilla.interfaces.*;
import org.virtualbox.*;

public class TestVBox
{
    public static void main(String[] args)
    {
        VirtualBoxManager mgr = VirtualBoxManager.getInstance(null);

        System.out.println("\n--> initialized\n");

        try
        {
            IVirtualBox vbox = mgr.getVBox();
            System.out.println("VirtualBox version: " + vbox.getVersion() + "\n");

            /* list all VMs and print some info for each */
            IMachine[] machs = vbox.getMachines(null);
            for (IMachine m : machs)
            {
                System.out.println("VM name: " + m.getName() + ", RAM size: " + m.getMemorySize() + "MB");
            }

            /* do something silly, start the first VM in the list */
            String m = machs[0].getName();
            System.out.println("\nAttempting to start VM '" + m + "'");
            if (mgr.startVm(m, 7000))
            {
                System.out.println("started, presss any key...");
                int ch = System.in.read();
            }
            else
            {
                System.out.println("cannot start machine "+m);
            }
        }
        catch (Throwable e)
        {
            e.printStackTrace();
        }

        mgr.cleanup();

        System.out.println("\n--< done\n");
    }

}
