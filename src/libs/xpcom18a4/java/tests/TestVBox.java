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
