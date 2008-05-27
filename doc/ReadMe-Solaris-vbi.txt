
Sun xVM VirtualBox Kernel Interface Module (VBI) for OpenSolaris
----------------------------------------------------------------

This package installs the VirtualBox Kernel Interface Module (VBI). This kernel module
interfaces between VirtualBox and the ever evolving SunOS kernel sheilding VirtualBox
from changes in the SunOS kernel.

This package installs both the 32-bit (x86) and 64-bit (amd64) binaries of VBI, and
VirtualBox would be dynamically linked to whichever one is appropriate.

* Important Note:
This is NOT the VirtualBox kernel driver! It is an kernel interface layer that is
used by the VirtualBox kernel driver.


Installing:
-----------

After extracting the contents of the tar.gz file perform the following steps:

1. Login as root using the "su" command.

2. Run the command:
        pkgadd -G -d VirtualBoxKern-@VBOX_VERSION_STRING@-SunOS-r@VBOX_SVN_REV@.pkg
    The -G switch ensures you install only into the global zone.

3. The installer would then ask you to "Select package(s) you wish to process"
        For this type "1" or "all".

4. Then type "y" when asked about continuing with the installation.

Now all the necessary files would be installed on your system.


Un-Installating:
----------------

To remove the VirtualBox kernel interface from your system perform the following steps:

1. Login as root using the "su" command.

2. Run the command:
        pkgrm SUNWvboxkern

