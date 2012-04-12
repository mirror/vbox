$Id$

Setting up the source trees
===========================

Check out the EDK2 trunk/edk2 to some directory of your choice (the command
creates an edk2 subdirectory):

  svn checkout \
    --username guest --password guest \
    -r9572 https://edk2.svn.sourceforge.net/svnroot/edk2/trunk/edk2 edk2

Note for giters: r9572 = 1dba456e1a72a3c2d896f25f94ddeaa64d10a3be

Enter into the edk2 directory and check out EFI/Firmware2/VBoxPkg into a
VBoxPkg subdirectory:

  svn checkout \
    http://www.virtualbox.org/svn/vbox/trunk/src/VBox/Devices/EFI/Firmware2/VBoxPkg VBoxPkg

Enter into the VBoxPkg/Include and check out include/iprt and include/VBox:

  svn checkout http://www.virtualbox.org/svn/vbox/trunk/include/iprt iprt
  svn checkout http://www.virtualbox.org/svn/vbox/trunk/include/VBox VBox

Then copy version-generated.h from
<VBox-trunk>/out/win.*/*/version-generated.h (into VBoxPkg/include/).
<VBox-trunk>/out/win.*/*/product-generated.h (into VBoxPkg/include/).


Symlink alternative for Vista
-----------------------------

Say you've got VBox checked out as e:\vbox\trunk and you're on 32-bit Windows
and having done a debug build. Check out EDK2 somewhere (see above). Then do:

  kmk_ln -s %VBOXSVN%\src\VBox\Devices\EFI\Firmware2\VBoxPkg\ edk2\VBoxPkg
  kmk_ln -s %VBOXSVN%\include\iprt\ edk2\VBoxPkg\Include\iprt
  kmk_ln -s %VBOXSVN%\include\VBox\ edk2\VBoxPkg\Include\VBox
  kmk_ln -s %VBOXSVN%\out\win.x86\debug\version-generated.h edk2\VBoxPkg\Include\version-generated.h
  kmk_ln -s %VBOXSVN%\out\win.x86\debug\product-generated.h edk2\VBoxPkg\Include\product-generated.h

MinGW for Linux
================

To install MinGW on Ubuntu systems, just perform 

  apt-get install mingw32-binutils mingw32 mingw32-runtime

After that, you can even avoid setting up symlinks, as build script will do
that automagically.

MinGW-w64 for Linux
===================
To build the X64 firmware on Linux, the wimgw-w64 port of mingw is required.
The binaries are available at:

  http://sourceforge.net/projects/mingw-w64/files/

on recent Ubuntu systems mingw-w64 is available in repository:

   apt-get install mingw-w64

Some non-fatal warnings might appears while compiling on Linux machine so it
is recommended to disable -Werror at Conf/tools_def.txt:*_UNIXGCC_X64_CC_FLAGS.

While building some versions of wingw-w64/linker might complain that __ModuleEntryPoint wasn't found (and fills entry point field with some default value) 
to fix that, just split the the definition (IA32 and X64),with removing leading underscore '_' for X64 at Conf/tools_def.txt:
  *_UNIXGCC_*_DLINK_FLAGS=... -entry _$(IMAGE_ENTRY_POINT) ...
  to 
  *_UNIXGCC_IA32_DLINK_FLAGS=... -entry _$(IMAGE_ENTRY_POINT) ...
  *_UNIXGCC_X64_DLINK_FLAGS=... -entry $(IMAGE_ENTRY_POINT) ...

Setting up the environment
==========================

First, enter the VirtualBox environment using tools/env.cmd (and whatever
local additions you normally use).

Go to the EDK2 source tree you set up in the previous section and run
VBoxPkg/env.cmd (Windows) and VBoxPkg/env.sh (Unix).

That's it. You can now run build.


Patching
========

VBox guests and hardware required some modifications in EDK2 do before
building some patches are required:

  cat VBoxPkg/edk2.patch-pmtimer | patch -p0 
  cat VBoxPkg/edk2.patch-no_blocking_partition | patch -p0 
  cat VBoxPkg/edk2.patch-ovmf_pei | patch -p0 
  cat VBoxPkg/edk2.patch-no_blocking_partition | patch -p0
  cat VBoxPkg/edk2.patch-apple | patch -p0
  cat VBoxPkg/edk2.patch-rtc | patch -p0
  cat VBoxPkg/edk2.patch-mem_acpi | patch -p0
  cat VBoxPkg/edk2.patch-idtgdt | patch -p0


Building
========
Edit Cont/target.txt:

  $ cat Conf/target.txt 
  ACTIVE_PLATFORM = VBoxPkg/VBoxPkgOSE.dsc
  TARGET =  DEBUG
  TARGET_ARCH = IA32
  TOOL_CHAIN_CONF = Conf/tools_def.txt
  TOOL_CHAIN_TAG = UNIXGCC
  MAX_CONCURRENT_THREAD_NUMBER = 1
  MULTIPLE_THREAD = Disable
  BUILD_RULE_CONF = Conf/build_rule.txt
 
The make program is called 'build' (edk2\BaseTools\Bin\Win32\build.exe). To
start building just execute 'build'. If you have a multicore machine and run
into bad build errors, try 'build -n 1' to avoid mixing up errors. For more
options try 'build --help'.


Running
=======

Copy (or symlink) Build\VBoxPkg\DEBUG_MYTOOLS\FV\VBOX.fd to the
VirtualBox bin directory as vboxefi.fv.
    copy      e:\edk2\Build\VBoxPkg\DEBUG_MYTOOLS\FV\VBOX.fd e:\vbox\trunk\out\win.x86\debug\bin\VBoxEFI32.fd
  or
    kmk_ln -s e:\edk2\Build\VBoxPkg\DEBUG_MYTOOLS\FV\VBOX.fd e:\vbox\trunk\out\win.x86\debug\bin\VBoxEFI32.fd

You need to build have a VirtualBox debug build with the following in your
Note that these options will not change the VirtualBox behavior only enable
the EFI feature.

Create a new VM with enabled EFI support.

Currently all there is to see is in the log output and debugger. Suggested
log setup (debug builds only):

    set VBOX_LOG=dev_efi.e.l2
    set VBOX_LOG_DEST=stderr
    set VBOX_LOG_FLAGS=unbuffered msprog thread

And suggested way of starting the VM:

  VirtualBox.exe --startvm efi

