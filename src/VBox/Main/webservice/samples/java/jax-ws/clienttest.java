/*
 * Sample client for the VirtualBox webservice, written in Java.
 *
 * Don't forget to run VBOX webserver
 * with 'vboxwebsrv -t 1000' command, to calm down watchdog thread.
 *
 * Copyright (C) 2008 Sun Microsystems, Inc.
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
import com.sun.xml.ws.commons.virtualbox_2_1.*;
import java.util.*;
import javax.xml.ws.Holder;
import org.virtualbox_2_1.*;

public class clienttest
{
    IWebsessionManager mgr;
    IVirtualBox vbox;

    public clienttest()
    {
        mgr = new IWebsessionManager("http://localhost:18083/");
        vbox = mgr.logon("test", "test");
        System.out.println("Initialized connection to VirtualBox version " + vbox.getVersion());
    }

    public void disconnect()
    {
        mgr.disconnect(vbox);
    }

    class Desktop
    {
        String name;
        UUID   uuid;

        Desktop(int n)
        {
            name = "Mach"+n;
            uuid = UUID.randomUUID();
        }
        String getName()
        {
            return name;
        }
        UUID getId()
        {
            return uuid;
        }
    }

    public void test()
    {
        for (int i=0; i<100; i++)
        {
            String baseFolder =
                    vbox.getSystemProperties().getDefaultMachineFolder();
            Desktop desktop = new Desktop(i);
            IMachine machine =  vbox.createMachine(baseFolder,
                                                   "linux",
                                                   desktop.getName(),
                                                   desktop.getId());
            machine.saveSettings();
            mgr.cleanupUnused();
        }
    }

    public void test2()
    {
        ISession session = mgr.getSessionObject(vbox);
        UUID id = UUID.fromString("bc8b6219-2775-42c4-f1b2-b48b3c177294");
        vbox.openSession(session, id);
        IMachine mach = session.getMachine();
        IBIOSSettings bios = mach.getBIOSSettings();
        bios.setIOAPICEnabled(true);
        mach.saveSettings();
        session.close();
    }


    public void test3()
    {
        
        IWebsessionManager mgr1 = new IWebsessionManager("http://localhost:18082/");
        IWebsessionManager mgr2 = new IWebsessionManager("http://localhost:18083/");
        IVirtualBox vbox1 = mgr1.logon("test", "test");
        IVirtualBox vbox2 = mgr2.logon("test", "test");
        

        System.out.println("connection 1 to VirtualBox version " + vbox1.getVersion());
        System.out.println("connection 2 to VirtualBox version " + vbox2.getVersion());
        mgr1.disconnect(vbox1);
        mgr2.disconnect(vbox2);

        mgr1 = new IWebsessionManager("http://localhost:18082/");
        mgr2 = new IWebsessionManager("http://localhost:18083/");
        vbox1 = mgr1.logon("test", "test");
        vbox2 = mgr2.logon("test", "test");
        
        System.out.println("second connection 1 to VirtualBox version " + vbox1.getVersion());
        System.out.println("second connection 2 to VirtualBox version " + vbox2.getVersion());

        mgr1.disconnect(vbox1);
        mgr2.disconnect(vbox2);
    }

    public void showVMs()
    {
        try
        {
            int i = 0;
            for (IMachine m : vbox.getMachines())
            {
                System.out.println("Machine " + (i++) + ": " + " [" + m.getId() + "]" + " - " + m.getName());
            }
        }
        catch (Exception e)
        {
            e.printStackTrace();
        }
    }

    public void listHostInfo()
    {
        try
        {
            IHost host = vbox.getHost();
            long uProcCount = host.getProcessorCount();
            System.out.println("Processor count: " + uProcCount);

            for (long i=0; i<uProcCount; i++)
            {
                System.out.println("Processor #" + i + " speed: " + host.getProcessorSpeed(i) + "MHz");
            }

            IPerformanceCollector  oCollector = vbox.getPerformanceCollector();

            List<IPerformanceMetric> aMetrics =
                oCollector.getMetrics(Arrays.asList(new String[]{"*"}),
                                      Arrays.asList(new IUnknown[]{host}));

            for (IPerformanceMetric m : aMetrics)
            {
                System.out.println("known metric = "+m.getMetricName());
            }

            Holder<List<String>> names = new Holder<List<String>> ();
            Holder<List<IUnknown>> objects = new Holder<List<IUnknown>>() ;
            Holder<List<String>> units = new Holder<List<String>>();
            Holder<List<Long>> scales =  new Holder<List<Long>>();
            Holder<List<Long>> sequenceNumbers =  new Holder<List<Long>>();
            Holder<List<Long>> indices =  new Holder<List<Long>>();
            Holder<List<Long>> lengths =  new Holder<List<Long>>();

            List<Integer> vals =
                oCollector.queryMetricsData(Arrays.asList(new String[]{"*"}),
                                            Arrays.asList(new IUnknown[]{host}),
                                            names, objects, units, scales,
                                            sequenceNumbers, indices, lengths);

            for (int i=0; i < names.value.size(); i++)
                System.out.println("name: "+names.value.get(i));
        }
        catch (Exception e)
        {
            e.printStackTrace();
        }
    }

    public void startVM(String strVM)
    {
        ISession oSession = null;
        IMachine oMachine = null;

        try
        {
            oSession = mgr.getSessionObject(vbox);

            // first assume we were given a UUID
            try
            {
                oMachine = vbox.getMachine(UUID.fromString(strVM));
            }
            catch (Exception e)
            {
                try
                {
                    oMachine = vbox.findMachine(strVM);
                }
                catch (Exception e1)
                {
                }
            }

            if (oMachine == null)
            {
                System.out.println("Error: can't find VM \"" + strVM + "\"");
            }
            else
            {
                UUID uuid = oMachine.getId();
                String sessionType = "gui";
                String env = "DISPLAY=:0.0";
                IProgress oProgress =
                    vbox.openRemoteSession(oSession,
                                            uuid,
                                            sessionType,
                                            env);
                System.out.println("Session for VM " + uuid + " is opening...");
                oProgress.waitForCompletion(10000);

                long rc = oProgress.getResultCode();
                if (rc != 0)
                    System.out.println("Session failed!");
            }
        }
        catch (Exception e)
        {
            e.printStackTrace();
        }
        finally
        {
            if (oSession != null)
            {
                oSession.close();
            }
        }
    }

    public void cleanup()
    {
        try
        {
            if (vbox != null)
            {
                disconnect();
                vbox = null;
                System.out.println("Logged off.");
            }
            mgr.cleanupUnused();
            mgr = null;
        }
        catch (Exception e)
        {
            e.printStackTrace();
        }
    }

    public static void printArgs()
    {
        System.out.println(  "Usage: java clienttest <mode> ..." +
                             "\nwith <mode> being:" +
                             "\n   show vms            list installed virtual machines" +
                             "\n   list hostinfo       list host info" +
                             "\n   startvm <vmname|uuid> start the given virtual machine");
    }

    public static void main(String[] args)
    {
        if (args.length < 1)
        {
            System.out.println("Error: Must specify at least one argument.");
            printArgs();
        }
        else
        {
            clienttest c = new clienttest();
            if (args[0].equals("show"))
            {
                if (args.length == 2)
                {
                    if (args[1].equals("vms"))
                        c.showVMs();
                    else
                        System.out.println("Error: Unknown argument to \"show\": \"" + args[1] + "\".");
                }
                else
                    System.out.println("Error: Missing argument to \"show\" command");
            }
            else if (args[0].equals("list"))
            {
                if (args.length == 2)
                {
                    if (args[1].equals("hostinfo"))
                        c.listHostInfo();
                    else
                        System.out.println("Error: Unknown argument to \"show\": \"" + args[1] + "\".");
                }
                else
                    System.out.println("Error: Missing argument to \"list\" command");
            }
            else if (args[0].equals("startvm"))
            {
                if (args.length == 2)
                {
                    c.startVM(args[1]);
                }
                else
                    System.out.println("Error: Missing argument to \"startvm\" command");
            }
            else if (args[0].equals("test"))
            {
                c.test3();
            }
            else
                System.out.println("Error: Unknown command: \"" + args[0] + "\".");

            c.cleanup();
        }
    }
}
