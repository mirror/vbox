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
import org.virtualbox_3_2.*;
import java.util.List;

class VBoxCallbacks extends VBoxObjectBase implements IVirtualBoxCallback
{
    public void onGuestPropertyChange(String machineId, String name, String value, String flags)
    {
        System.out.println("onGuestPropertyChange -- VM: " + machineId + ", " + name + "->" + value);
    }
    public void onSnapshotChange(String machineId, String snapshotId)
    {
    }
    public void onSnapshotDeleted(String machineId, String snapshotId)
    {
    }
    public void onSnapshotTaken(String machineId, String snapshotId) {}
    public void onSessionStateChange(String machineId, SessionState state)
    {
        System.out.println("onSessionStateChange -- VM: " + machineId + ", state: " + state);
    }
    public void onMachineRegistered(String machineId, Boolean registered) {}
    public void onMediumRegistered(String mediumId, DeviceType mediumType, Boolean registered) {}
    public void onExtraDataChange(String machineId, String key, String value)
    {
        System.out.println("onExtraDataChange -- VM: " + machineId + ": " + key+"->"+value);
    }
    public Boolean onExtraDataCanChange(String machineId, String key, String value, Holder<String> error) { return true; }
    public void onMachineDataChange(String machineId)
    {}
    public void onMachineStateChange(String machineId, MachineState state)
    {
        System.out.println("onMachineStateChange -- VM: " + machineId + ", state: " + state);
    }
}

public class TestVBox
{
    static void testCallbacks(VirtualBoxManager mgr, IVirtualBox vbox)
    {
        IVirtualBoxCallback cbs = mgr.createIVirtualBoxCallback(new VBoxCallbacks());
        vbox.registerCallback(cbs);
        for (int i=0; i<100; i++)
        {
            mgr.waitForEvents(500);
        }
        vbox.unregisterCallback(cbs);
    }

    static void testEnumeration(VirtualBoxManager mgr, IVirtualBox vbox)
    {
        List<IMachine> machs = vbox.getMachines();
        for (IMachine m : machs)
        {
            System.out.println("VM name: " + m.getName() + ", RAM size: " + m.getMemorySize() + "MB");
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

        System.out.println("\n--> initialized\n");

        try
        {
            IVirtualBox vbox = mgr.getVBox();
            System.out.println("VirtualBox version: " + vbox.getVersion() + "\n");
            testEnumeration(mgr, vbox);
            //testStart(mgr, vbox);
            testCallbacks(mgr, vbox);

            System.out.println("done, press Enter...");
            int ch = System.in.read();
        }
        catch (Throwable e)
        {
            e.printStackTrace();
        }

        mgr.cleanup();

        System.out.println("\n--< done\n");
    }

}
