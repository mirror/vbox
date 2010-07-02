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

class VBoxCallbacks extends VBoxObjectBase implements IVirtualBoxCallback
{
    public void onGuestPropertyChange(String machineId, String name, String value, String flags)
    {
        System.out.println("onGuestPropertyChange -- VM: " + machineId + ", " + name + "->" + value);
    }
    public void onSnapshotChange(String machineId, String snapshotId)
    {
        System.out.println("onSnapshotChange -- VM: " + machineId + ", snap: " + snapshotId);

    }
    public void onSnapshotDeleted(String machineId, String snapshotId)
    {
        System.out.println("onSnapshotDeleted -- VM: " + machineId + ", snap: " + snapshotId);
    }
    public void onSnapshotTaken(String machineId, String snapshotId)
    {
        System.out.println("onSnapshotTaken -- VM: " + machineId + ", snap: " + snapshotId);
    }
    public void onSessionStateChange(String machineId, SessionState state)
    {
        System.out.println("onSessionStateChange -- VM: " + machineId + ", state: " + state);
    }
    public void onMachineRegistered(String machineId, Boolean registered)
    {
        System.out.println("onMachineRegistered -- VM: " + machineId + ", registered: " + registered);
    }
    public void onMediumRegistered(String mediumId, DeviceType mediumType, Boolean registered)
    {
        System.out.println("onMediumRegistered -- ID: " + mediumId + ", type=" + mediumType + ", registered: " + registered);
    }
    public void onExtraDataChange(String machineId, String key, String value)
    {
        System.out.println("onExtraDataChange -- VM: " + machineId + ": " + key+"->"+value);
    }
    public Boolean onExtraDataCanChange(String machineId, String key, String value, Holder<String> error)
    {
        return true;
    }
    public void onMachineDataChange(String machineId)
    {
        System.out.println("onMachineDataChange -- VM: " + machineId);
    }
    public void onMachineStateChange(String machineId, MachineState state)
    {
        System.out.println("onMachineStateChange -- VM: " + machineId + ", state: " + state);
    }
}

class ConsoleCallbacks extends VBoxObjectBase implements IConsoleCallback
{
    String mach;
    ConsoleCallbacks(String mach)
    {
       this.mach = mach;
    }
    public void onMousePointerShapeChange(Boolean visible, Boolean alpha, Long xHot, Long yHot, Long width, Long height, List<Short> shape)
    {
       System.out.println("onMousePointerShapeChange -- VM: " + mach);
    }
    public void onMouseCapabilityChange(Boolean supportsAbsolute, Boolean supportsRelative, Boolean needsHostCursor)
    {
       System.out.println("onMouseCapabilityChange -- VM: " + mach+" abs="+supportsAbsolute+ " rel="+supportsRelative+" need host="+needsHostCursor);
    }
    public void onKeyboardLedsChange(Boolean numLock, Boolean capsLock, Boolean scrollLock)
    {
       System.out.println("onKeyboardLedsChange -- VM: " + mach);
    }
    public void onStateChange(org.virtualbox_3_3.MachineState state)
    {
       System.out.println("onStateChange -- VM: " + mach);
    }
    public void onAdditionsStateChange()
    {
       System.out.println("onAdditionsStateChange -- VM: " + mach);
    }
    public void onNetworkAdapterChange(org.virtualbox_3_3.INetworkAdapter networkAdapter)
    {
       System.out.println("onNetworkAdapterChange -- VM: " + mach);
    }
    public void onSerialPortChange(org.virtualbox_3_3.ISerialPort serialPort)
    {
       System.out.println("onSerialPortChange -- VM: " + mach);
    }
    public void onParallelPortChange(org.virtualbox_3_3.IParallelPort parallelPort)
    {
       System.out.println("onParallelPortChange -- VM: " + mach);
    }
    public void onStorageControllerChange()
    {
       System.out.println("onStorageControllerChange -- VM: " + mach);
    }
    public void onMediumChange(org.virtualbox_3_3.IMediumAttachment mediumAttachment)
    {
       System.out.println("onMediumChange -- VM: " + mach);
    }
    public void onCPUChange(Long cpu, Boolean add)
    {
       System.out.println("onCPUChange -- VM: " + mach);
    }
    public void onVRDPServerChange()
    {
       System.out.println("onVRDPServerChange -- VM: " + mach);
    }
    public void onRemoteDisplayInfoChange()
    {
       System.out.println("onRemoteDisplayInfoChange -- VM: " + mach);
    }
    public void onUSBControllerChange()
    {
       System.out.println("onUSBControllerChange -- VM: " + mach);
    }
    public void onUSBDeviceStateChange(org.virtualbox_3_3.IUSBDevice device, Boolean attached, org.virtualbox_3_3.IVirtualBoxErrorInfo error)
    {
       System.out.println("onUSBDeviceStateChange -- VM: " + mach);
    }
    public void onSharedFolderChange(org.virtualbox_3_3.Scope scope)
    {
       System.out.println("onSharedFolderChange -- VM: " + mach);
    }

    public void onRuntimeError(Boolean fatal, String id, String message)
    {
       System.out.println("onRuntimeError -- VM: " + mach);
    }

    public Boolean onCanShowWindow()
    {
       System.out.println("onCanShowWindow -- VM: " + mach);
       return true;
    }

    public BigInteger onShowWindow()
    {
       System.out.println("onShowWindow -- VM: " + mach);
       return BigInteger.ZERO;
    }
}

public class TestVBox
{
    static void testCallbacks(VirtualBoxManager mgr, IVirtualBox vbox)
    {

        IVirtualBoxCallback cbs = new VBoxCallbacks();
        mgr.registerGlobalCallback(vbox, cbs);

        IMachine mach = vbox.getMachines().get(0);
        IConsoleCallback mcbs = new ConsoleCallbacks(mach.getName());

        ISession session = null;
        try {
          session = mgr.openMachineSession(mach);
          mgr.registerMachineCallback(session, mcbs);

          for (int i=0; i<100; i++)
          {
            mgr.waitForEvents(500);
          }

          System.out.println("done waiting");

          mgr.unregisterMachineCallback(session, mcbs);
        } catch (Exception e) {
          e.printStackTrace();
        } finally {
          mgr.closeMachineSession(session);
        }
        mgr.unregisterGlobalCallback(vbox, cbs);
    }


    static void processEvent(IEvent ev)
    {
        System.out.println("got event: " + ev);

        VBoxEventType type = ev.getType();
        System.out.println("type = "+type);

        switch (type)
        {
            case OnMachineStateChange:
                IMachineStateChangeEvent mcse = IMachineStateChangeEvent.queryInterface(ev);
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

        System.out.println("\n--> initialized\n");

        try
        {
            IVirtualBox vbox = mgr.getVBox();
            System.out.println("VirtualBox version: " + vbox.getVersion() + "\n");
            testEnumeration(mgr, vbox);
            testStart(mgr, vbox);
            //testCallbacks(mgr, vbox);
            testEvents(mgr, vbox.getEventSource(), false);

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
