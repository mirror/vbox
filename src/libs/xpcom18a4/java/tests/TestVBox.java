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

    public static class VBoxCallbacks implements IVirtualBoxCallback
    {
        public void onGuestPropertyChange(String machineId, String name, String value, String flags) {}
        public void onSnapshotChange(String machineId, String snapshotId) {}
        public void onSnapshotDeleted(String machineId, String snapshotId) {}
        public void onSnapshotTaken(String machineId, String snapshotId) {}
        /** @todo is there a type SessionState instead of long? */
        public void onSessionStateChange(String machineId, long state) {}
        public void onMachineRegistered(String machineId, boolean registered) {}
        /** @todo long -> MediumType */
        public void onMediumRegistered(String mediumId, long mediumType, boolean registered) {}
        public void onExtraDataChange(String machineId, String key, String value) {}
        public boolean onExtraDataCanChange(String machineId, String key, String value, String[] error) { return true; }
        public void onMachineDataChange(String machineId) {}
        /** @todo long -> MachineState */
        public void onMachineStateChange(String machineId, long state) { System.out.println("onMachineStateChange -- VM: " + machineId + ", state: " + state); };

        /** @todo ugly reimplementation of queryInterface, should have base class to derive from */
        public nsISupports queryInterface(String iid) { return org.mozilla.xpcom.Mozilla.queryInterface(this, iid); }
    };


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
                try
                {
                    System.out.println("VM name: " + m.getName() + ", RAM size: " + m.getMemorySize() + "MB");
                    System.out.println(" HWVirt: " + m.getHWVirtExProperty(HWVirtExPropertyType.Enabled)
                                       + ", Nested Paging: " + m.getHWVirtExProperty(HWVirtExPropertyType.NestedPaging)
                                       + ", PAE: " + m.getCPUProperty(CPUPropertyType.PAE) );
                }
                catch (Throwable e)
                {
                    e.printStackTrace();
                }
            }

            VBoxCallbacks vboxCallbacks = new VBoxCallbacks();
            vbox.registerCallback(mgr.makeVirtualBoxCallback(vboxCallbacks));

            /* do something silly, start the first VM in the list */
            String m = machs[0].getName();
            System.out.println("\nAttempting to start VM '" + m + "'");
            if (mgr.startVm(m, 7000))
            {
                if (false)
                {
                    System.out.println("started, presss any key...");
                    int ch = System.in.read();
                } else {
                    while (true)
                    {
                        mgr.waitForEvents(500);
                    }
                }
            }
            else
            {
                System.out.println("cannot start machine "+m);
            }

            vbox.unregisterCallback(vboxCallbacks);
        }
        catch (Throwable e)
        {
            e.printStackTrace();
        }

        mgr.cleanup();

        System.out.println("\n--< done\n");
    }

}
