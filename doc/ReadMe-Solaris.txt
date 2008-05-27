
Sun xVM VirtualBox for OpenSolaris
----------------------------------

Installing:
-----------

* Important Note:
You must boot OpenSolaris in the same instruction set as the VirtualBox install package.
This means if you are installing the 64-bit version of VirtualBox you MUST boot in
64-bit mode. This is because the installer will setup the VirtualBox driver. Use the command
"isainfo -k" to check what instruction set is currently being used. If the output is "amd64"
you have booted in 64-bit mode, otherwise it is 32-bit.

After extracting the contents of the tar.gz file perform the following steps:

1. Login as root using the "su" command.

2. Install the packages (in this order):
     If you have not installed vbi module (check for the file /platform/i86pc/kernel/misc/vbi) run:

        pkgadd -G -d VirtualBoxKern-@VBOX_VERSION_STRING@-SunOS-r@VBOX_SVN_REV@.pkg

     Once the vbi module is installed run:

        pkgadd -d VirtualBox-@VBOX_VERSION_STRING@-SunOS-@BUILD_TARGET_ARCH@-r@VBOX_SVN_REV@.pkg

3. For each package the installer would ask you to "Select package(s) you wish to process"
        For this type "1" or "all".

4. Then type "y" when asked about continuing with the installation.

Now all the necessary files would be installed on your system. Start VirtualBox by typing
VirtualBox from the terminal.


Un-Installating:
----------------

To remove VirtualBox from your system perform the following steps:

1. Login as root using the "su" command.

2. Run the command:
        pkgrm SUNWvbox

To remove the VirtualBox kernel interface module run the command:
        pkgrm SUNWvboxkern

