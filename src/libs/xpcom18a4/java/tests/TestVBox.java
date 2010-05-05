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
    public static void main(String[] args) {
        VirtualBoxManager mgr = VirtualBoxManager.getInstance(null);

        System.out.println("\n--> initialized\n");

        try {

            IVirtualBox vbox = mgr.getVBox();

            System.out.println("vers="+vbox.getVersion());
            IMachine[] machs = vbox.getMachines(null);
            //IMachine m = vbox.findMachine("SL");
            for (IMachine m : machs)
            {
                System.out.println("mach="+m.getName()+" RAM="+m.getMemorySize()+"M");
            }

            String m = machs[0].getName();
            if (mgr.startVm(m, 7000))
            {
                System.out.println("started, presss any key...");
                int ch = System.in.read();
            }
            else
            {
                System.out.println("cannot start machine "+m);
            }
        } catch (Throwable e) {
            e.printStackTrace();
        }

        mgr.cleanup();

        System.out.println("\n--< done\n");
    }

}
