
Sun VirtualBox for OpenSolaris (TM) Operating System
----------------------------------------------------

Upgrading:
----------

If you have an existing VirtualBox installation and you are upgrading to
a newer version of VirtualBox, please uninstall the previous version
completely before installing a newer one.

You should uninstall BOTH the VirtualBox base package as well as the
VirtualBox kernel interface package before installing the new ones.

Please refer to the "Uninstalling" section at the end of this document
for details.

 +--------+
 |  NOTE  |
 +--------+

 VirtualBox 3.1 includes experimental USB support and requires OpenSolaris
 build 124 or higher. VirtualBox USB support on Solaris 10 is not supported
 due to limitations in the kernel.


Installing:
-----------

After extracting the contents of the tar.gz file perform the following steps:

1. Login as root using the "su" command.

2. Install the VirtualBox package:

        pkgadd -d VirtualBox-@VBOX_VERSION_STRING@-SunOS-r@VBOX_SVN_REV@.pkg

    To perform an unattended (non-interactive) installation of this
	package, add "-n -a autoresponse SUNWvbox" (without quotes)
	to the end of the above pkgadd command.

3. For each package, the installer will ask you to "Select package(s) you
   wish to process". In response, type "1".

4. Type "y" when asked about continuing the installation.

At this point, all the required files should be installed on your system.
You can launch VirtualBox by running 'VirtualBox' from the terminal.


Uninstalling:
-------------

To remove VirtualBox from your system, perform the following steps:

1. Login as root using the "su" command.

2. To remove VirtualBox, run the command:
        pkgrm SUNWvbox

3. To remove the VirtualBox kernel interface module, run the command:
        pkgrm SUNWvboxkern
    * Only required if you're uninstall VirtualBox versions 3.0.x or lower.

