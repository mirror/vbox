
VirtualBox for Mac OS X Limited Edition
----------------------------------------

This is an early development build of VirtualBox for the Mac OS X,
backup your data and don't expect everything to be highly polished 
and tuned just yet. If you find a *new* problem, meaning something
not listed below, please report it to the guy you got this build from.


Current issues / TODOs
-----------------------

FE/Qt:
- White spots instead of transparency for the status indicators (ugly).
- The button on the about screen is in a ugly square instead of showing the about screen image.
- Can't restart VirtualBox in 'view mode' when a VM is running.
- Host-F sends keycodes to guest and requires two tries to get back.
- virtuedesktops has some focues issues when switching back to a desktop with a fullscreen virtual box.
- Weird crash when reactivating (switching to it) the VM window.

Devices:
- Host DVD
- Audio In/Mic
- Host Interface Networking (see #1869).
- USB proxying (see #1870).
- Internal networking (depends on #1865).

Main / XPCOM:
- (test) read-only system wide install.

Misc:
- Installer (see #1812)
- VMX (see #1865)
- Check /dev/vboxdrv reference counting - seems it can be unloaded when it has processes attached.
- Figure out how to set the desirable permissions on /dev/vboxdrv.
- VBoxSDL is display updating is horribly broken (performance).
- VBoxSDL/BFE default hotkey needs to be changed.


How to "Install" and Run
------------------------

0. untar the archive somewhere

In the terminal:
1. enter 'dist/' and execute './load.sh' - it will invoke 'sudo'
   which in turn will ask you for your password.
2. enter 'VirtualBox.app/Contents/MacOS/' and start './VBoxSVC'

In finder:
3. open 'dist/' and double click on the 'VirtualBox' application.

