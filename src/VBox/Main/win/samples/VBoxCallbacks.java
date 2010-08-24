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

import com.jacob.com.*;

public class VBoxCallbacks
{
    public void OnGuestPropertyChange(Variant[] args)
    {
        String machineId = args[0].getString();
        String name = args[1].getString();
	String value = args[2].getString();
        String flags = args[3].getString();;
        System.out.println("onGuestPropertyChange -- VM: " + machineId + ", " + name + "->" + value);
    }
    public void OnSnapshotChange(String machineId, String snapshotId)
    {
        System.out.println("onGuestSnapshotChange -- VM: " + machineId + ", " + snapshotId);
    }
    public void onSnapshotDeleted(String machineId, String snapshotId)
    {
        System.out.println("onSnapshotDeleted -- VM: " + machineId + ", " + snapshotId);
    }
    public void onSnapshotTaken(String machineId, String snapshotId)
    {
    }
    public void OnSessionStateChange(Variant[] args)
    {
        String machineId = args[0].getString();
	int state = args[1].getInt();
        System.out.println("onSessionStateChange -- VM: " + machineId + ", state: " + state);
    }
    public void onMachineRegistered(String machineId, boolean registered) {}
    public void onMediumRegistered(String mediumId, long mediumType, boolean registered) {}
    public void onExtraDataChange(String machineId, String key, String value)
    {
        System.out.println("onExtraDataChange -- VM: " + machineId + ": " + key+"->"+value);
    }
    public Variant OnExtraDataCanChange(String machineId, String key, String value, String[] error)
    {
      return new Variant(true);
    }
    public void onMachineDataChange(String machineId)
    {}
    public void OnMachineStateChange(Variant[] args)
    {
        String machineId = args[0].getString();
        int state = args[1].getInt();
        System.out.println("onMachineStateChange -- VM: " + machineId + ", state: " + state);
    }
}
