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
import com.jacob.activeX.*;
import com.jacob.com.ComThread;
import com.jacob.com.Dispatch;
import com.jacob.com.Variant;
import com.jacob.com.SafeArray;
import com.jacob.com.DispatchEvents;


public class VBoxTest {

	private ActiveXComponent vbox = null;

	public VBoxTest() {
		setUp();
	}

	public void setUp() {
		if (vbox == null) {
		   ComThread.InitMTA();
		   vbox = new ActiveXComponent("VirtualBox.VirtualBox");
		   String v = Dispatch.get(vbox, "Version").getString();
	           System.out.println("vbox is "+v);
		}
	}

        public void startVm(String name)
        {
	   Dispatch m = Dispatch.call(vbox, "findMachine", name).toDispatch();
           ActiveXComponent session = new ActiveXComponent("VirtualBox.Session");
           String id = Dispatch.get(m, "Id").getString();
           String type= "gui";
	   Dispatch p = Dispatch.call(vbox, "openRemoteSession", session, id, type, "").toDispatch();
           Dispatch.call(p, "waitForCompletion", 10000);
        }

	public String list()
        {
           // returns first machine
           SafeArray sa = Dispatch.get(vbox, "Machines").toSafeArray();
           String rv = null;

	   for (int i = sa.getLBound(); i <= sa.getUBound(); i++)
           {
              Dispatch m = sa.getVariant(i).getDispatch();
              String name = Dispatch.get(m, "Name").getString();
              if (rv == null)
                 rv = name;
              String id = Dispatch.get(m, "Id").getString();
              System.out.println("name="+name+" id="+id);
           }
           return rv;
        }

	public void tearDown() {
            ComThread.Release();
	}


        public void testCallbacks()
        {
           VBoxCallbacks cb = new VBoxCallbacks();
           ActiveXDispatchEvents de = new ActiveXDispatchEvents(vbox, cb);
           Dispatch.call(vbox, "registerCallback", de);
        }

        public void testCallbacks1()
        {
           ActiveXComponent cbw = new ActiveXComponent("VirtualBox.CallbackWrapper");
           VBoxCallbacks cb = new VBoxCallbacks();
           //Dispatch.call(cbw,  "setLocalObject", cb);
           Dispatch.call(vbox, "registerCallback", cbw);
        }

        public void testCallbacks2()
        {
           VBoxCallbacks cb = new VBoxCallbacks();
           DispatchEvents de = new DispatchEvents(vbox, cb, "VirtualBox.VirtualBox");
           //Dispatch.call(vbox, "registerCallback", );

        }

	public static void main(String[] args) {
		VBoxTest vbox = new VBoxTest();
                String first = vbox.list();
                //vbox.startVm(first);
                vbox.testCallbacks2();
                try {
                   Thread.sleep(20000);
                } catch (InterruptedException ie) {}
		vbox.tearDown();
	}
}
