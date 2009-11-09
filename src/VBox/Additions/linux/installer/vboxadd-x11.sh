#! /bin/sh
# Sun VirtualBox
# Linux Additions X11 setup init script ($Revision$)
#

#
# Copyright (C) 2006-2009 Sun Microsystems, Inc.
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#
# Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
# Clara, CA 95054 USA or visit http://www.sun.com if you need
# additional information or have any questions.
#


# chkconfig: 35 30 70
# description: VirtualBox Linux Additions kernel modules
#
### BEGIN INIT INFO
# Provides:       vboxadd-x11
# Required-Start:
# Required-Stop:
# Default-Start:
# Default-Stop:
# Description:    VirtualBox Linux Additions X11 setup
### END INIT INFO

PATH=$PATH:/bin:/sbin:/usr/sbin
LOG="/var/log/vboxadd-install-x11.log"

# Find the version of X installed
# The last of the three is for the X.org 6.7 included in Fedora Core 2
xver=`X -version 2>&1`
x_version=`echo "$xver" | sed -n 's/^X Window System Version \([0-9.]\+\)/\1/p'``echo "$xver" | sed -n 's/^XFree86 Version \([0-9.]\+\)/\1/p'``echo "$xver" | sed -n 's/^X Protocol Version 11, Revision 0, Release \([0-9.]\+\)/\1/p'``echo "$xver" | sed -n 's/^X.Org X Server \([0-9.]\+\)/\1/p'`
# Version of Redhat or Fedora installed.  Needed for setting up selinux policy.
redhat_release=`cat /etc/redhat-release 2> /dev/null`
# All the different possible locations for XFree86/X.Org configuration files
# - how many of these have ever been used?
x11conf_files="/etc/X11/xorg.conf /etc/X11/xorg.conf-4 /etc/X11/.xorg.conf \
    /etc/xorg.conf /usr/etc/X11/xorg.conf-4 /usr/etc/X11/xorg.conf \
    /usr/lib/X11/xorg.conf-4 /usr/lib/X11/xorg.conf /etc/X11/XF86Config-4 \
    /etc/X11/XF86Config /etc/XF86Config /usr/X11R6/etc/X11/XF86Config-4 \
    /usr/X11R6/etc/X11/XF86Config /usr/X11R6/lib/X11/XF86Config-4 \
    /usr/X11R6/lib/X11/XF86Config"

if [ -f /etc/arch-release ]; then
    system=arch
elif [ -f /etc/redhat-release ]; then
    system=redhat
elif [ -f /etc/SuSE-release ]; then
    system=suse
elif [ -f /etc/gentoo-release ]; then
    system=gentoo
elif [ -f /etc/lfs-release -a -d /etc/rc.d/init.d ]; then
    system=lfs
else
    system=other
fi

if [ "$system" = "arch" ]; then
    USECOLOR=yes
    . /etc/rc.d/functions
    fail_msg() {
        stat_fail
    }

    succ_msg() {
        stat_done
    }

    begin() {
        stat_busy "$1"
    }
fi

if [ "$system" = "redhat" ]; then
    . /etc/init.d/functions
    fail_msg() {
        echo_failure
        echo
    }
    succ_msg() {
        echo_success
        echo
    }
    begin() {
        echo -n "$1"
    }
fi

if [ "$system" = "suse" ]; then
    . /etc/rc.status
    fail_msg() {
        rc_failed 1
        rc_status -v
    }
    succ_msg() {
        rc_reset
        rc_status -v
    }
    begin() {
        echo -n "$1"
    }
fi

if [ "$system" = "gentoo" ]; then
    if [ -f /sbin/functions.sh ]; then
        . /sbin/functions.sh
    elif [ -f /etc/init.d/functions.sh ]; then
        . /etc/init.d/functions.sh
    fi
    fail_msg() {
        eend 1
    }
    succ_msg() {
        eend $?
    }
    begin() {
        ebegin $1
    }
    if [ "`which $0`" = "/sbin/rc" ]; then
        shift
    fi
fi

if [ "$system" = "lfs" ]; then
    . /etc/rc.d/init.d/functions
    fail_msg() {
        echo_failure
    }
    succ_msg() {
        echo_ok
    }
    begin() {
        echo $1
    }
fi

if [ "$system" = "other" ]; then
    fail_msg() {
        echo " ...fail!"
    }
    succ_msg() {
        echo " ...done."
    }
    begin() {
        echo -n $1
    }
fi

dev=/dev/vboxguest
userdev=/dev/vboxuser
owner=vboxadd
group=1

fail()
{
    if [ "$system" = "gentoo" ]; then
        eerror $1
        exit 1
    fi
    fail_msg
    echo "($1)"
    exit 1
}

# Install an X11 desktop startup application.  The application should be a shell script.
# Its name should be purely alphanumeric and should start with a two digit number (preferably
# 98 or thereabouts) to indicate its place in the Debian Xsession startup order.
#
# syntax: install_x11_startup_app app desktop service_name
install_x11_startup_app() {
    app_src=$1
    desktop_src=$2
    service_name=$3
    alt_command=$4
    test -r "$app_src" ||
        { echo "install_x11_startup_app: no script given"; return 1; }
    test -r "$desktop_src" ||
        { echo "install_x11_startup_app: no desktop file given"; return 1; }
    test -n "$service_name" ||
        { echo "install_x11_startup_app: no service given"; return 1; }
    test -n "$alt_command" ||
        { echo "install_x11_startup_app: no service given"; return 1; }
    app_dest=`basename $app_src sh`
    app_dest_sh=`basename $app_src sh`.sh
    desktop_dest=`basename $desktop_src`
    found=0
    x11_autostart="/etc/xdg/autostart"
    kde_autostart="/usr/share/autostart"
    redhat_dir=/etc/X11/Xsession.d
    mandriva_dir=/etc/X11/xinit.d
    debian_dir=/etc/X11/xinit/xinitrc.d
    if [ -d "$mandriva_dir" -a -w "$mandriva_dir" -a -x "$mandriva_dir" ]
    then
        install -m 0644 $app_src "$mandriva_dir/$app_dest"
        found=1
    fi
    if [ -d "$x11_autostart" -a -w "$x11_autostart" -a -x "$x11_autostart" ]
    then
        install -m 0644 $desktop_src "$x11_autostart/$desktop_dest"
        found=1
    fi
    if [ -d "$kde_autostart" -a -w "$kde_autostart" -a -x "$kde_autostart" ]
    then
        install -m 0644 $desktop_src "$kde_autostart/$desktop_dest"
        found=1
    fi
    if [ -d "$redhat_dir" -a -w "$redhat_dir" -a -x "$redhat_dir" ]
    then
        install -m 0644 $app_src "$redhat_dir/$app_dest"
        found=1
    fi
    if [ -d "$debian_dir" -a -w "$debian_dir" -a -x "$debian_dir" ]
    then
        install -m 0755 $app_src "$debian_dir/$app_dest_sh"
        found=1
    fi
    if [ $found -eq 1 ]; then
        return 0
    fi
    cat << EOF
Could not set up the X Window $service_name service.
To start the $service_name service at log-in for a given user,
add the command $alt_command to the file .xinitrc in their home
directory.
EOF
    return 1
}


start()
{
    # Todo: check configuration and correct it if necessary
    return 0
}

stop()
{
    return 0
}

restart()
{
    stop && start
    return 0
}

# setup_script
setup()
{
    begin "Setting up the X Window System drivers"
    lib_dir="/usr/lib/VBoxGuestAdditions"
    share_dir="/usr/share/VBoxGuestAdditions"
    test -x "$lib_dir" -a -x "$share_dir" ||
        fail "Invalid Guest Additions configuration found"
    # By default, we want to run our X Window System configuration script
    dox11config=1
    # But not the special version for X.Org 1.5+
    dox11config15=0
    # And we don't install our SUSE/X.Org 1.5 configuration file by default
    dox11config15suse=0
    # We want to use hal for auto-loading the mouse driver
    useHalForMouse=1
    # And on newer servers, we want to test whether dynamic resizing will work
    testRandR=1
    # The video driver to install for X.Org 6.9+
    vboxvideo_src=
    # The mouse driver to install for X.Org 6.9+
    vboxmouse_src=

    modules_dir=`X -showDefaultModulePath 2>&1` || modules_dir=
    if [ -z "$modules_dir" ]; then
        for dir in /usr/lib64/xorg/modules /usr/lib/xorg/modules /usr/X11R6/lib64/modules /usr/X11R6/lib/modules /usr/X11R6/lib/X11/modules; do
            if [ -d $dir ]; then
                modules_dir=$dir
                break
            fi
        done
    fi

    test -z "$x_version" -o -z "$modules_dir" &&
        fail "Could not find X.org or XFree86 on the guest system.  The X Window drivers \
will not be installed."

    echo
    case $x_version in
        1.7.99.* )
            echo "Warning: unsupported pre-release version of X.Org Server installed.  Not"
            echo "installing the X.Org drivers."
            dox11config=0
            ;;
        1.6.99.* | 1.7.* )
            begin "Installing experimental Xorg Server 1.7 modules"
            vboxvideo_src=vboxvideo_drv_17.so
            vboxmouse_src=vboxmouse_drv_17.so
            dox11config=0
            ;;
        1.5.99.* | 1.6.* )
            begin "Installing Xorg Server 1.6 modules"
            vboxvideo_src=vboxvideo_drv_16.so
            vboxmouse_src=vboxmouse_drv_16.so
            dox11config15=1
            ;;
        1.4.99.* | 1.5.* )
            # Fedora 9 shipped X.Org Server version 1.4.99.9x (1.5.0 RC)
            # in its released version
            begin "Installing Xorg Server 1.5 modules"
            vboxvideo_src=vboxvideo_drv_15.so
            vboxmouse_src=vboxmouse_drv_15.so
            # SUSE with X.Org 1.5 is a special case, and is handled specially
            if [ -f /etc/SuSE-release ]
            then
                dox11config15suse=1
            else
                # This means do a limited configuration for systems with
                # autodetection support
                dox11config15=1
            fi
            ;;
        1.4.* )
            begin "Installing Xorg Server 1.4 modules"
            vboxvideo_src=vboxvideo_drv_14.so
            vboxmouse_src=vboxmouse_drv_14.so
            useHalForMouse=0
            ;;
        1.3.* )
            # This was the first release which gave the server version number
            # rather than the X11 release version when you did 'X -version'.
            begin "Installing Xorg Server 1.3 modules"
            vboxvideo_src=vboxvideo_drv_13.so
            vboxmouse_src=vboxmouse_drv_71.so
            useHalForMouse=0
            ;;
        7.1.* | 7.2.* )
            begin "Installing Xorg 7.1 modules"
            vboxvideo_src=vboxvideo_drv_71.so
            vboxmouse_src=vboxmouse_drv_71.so
            useHalForMouse=0
            testRandR=0
            ;;
        6.9.* | 7.0.* )
            begin "Installing Xorg 6.9/7.0 modules"
            vboxvideo_src=vboxvideo_drv_70.so
            vboxmouse_src=vboxmouse_drv_70.so
            useHalForMouse=0
            testRandR=0
            ;;
        6.7* | 6.8.* | 4.2.* | 4.3.* )
            # Assume X.Org post-fork or XFree86
            begin "Installing XFree86 4.3/Xorg 6.8 modules"
            ln -s "$lib_dir/vboxvideo_drv.o" "$modules_dir/drivers/vboxvideo_drv.o"
            ln -s "$lib_dir/vboxmouse_drv.o" "$modules_dir/input/vboxmouse_drv.o"
            useHalForMouse=0
            testRandR=0
            ;;
        * )
            echo "Warning: unknown version of the X Window System installed.  Not installing"
            echo "X Window System drivers."
            dox11config=0
            useHalForMouse=0
            ;;
    esac
    if [ -n "$vboxvideo_src" -a -n "$vboxmouse_src" ]; then
        rm "$modules_dir/drivers/vboxvideo_drv.so" 2>/dev/null
        rm "$modules_dir/input/vboxmouse_drv.so" 2>/dev/null
        ln -s "$lib_dir/$vboxvideo_src" "$modules_dir/drivers/vboxvideo_drv.so"
        ln -s "$lib_dir/$vboxmouse_src" "$modules_dir/input/vboxmouse_drv.so" &&
        succ_msg
    fi
    if [ $testRandR -eq 1 ]; then
        # Run VBoxRandR in test mode as it prints out useful information if
        # dynamic resizing can't be used.  Don't fail here though.
        /usr/bin/VBoxRandR --test 1>&2
    else
        echo "You appear to be have an old version of the X Window system installed on"
        echo "your guest system.  Seamless mode and dynamic resizing will not work in"
        echo "this guest."
    fi
    # Install selinux policy for Fedora 7 and 8 to allow the X server to
    # open device files
    case "$redhat_release" in
        Fedora\ release\ 7* | Fedora\ release\ 8* )
            semodule -i vbox_x11.pp > /dev/null 2>&1
            ;;
    esac

    # Our logging code generates some glue code on 32-bit systems.  At least F10
    # needs a rule to allow this.  Send all output to /dev/null in case this is
    # completely irrelevant on the target system.
    chcon -t unconfined_execmem_exec_t '/usr/bin/VBoxClient' > /dev/null 2>&1
    semanage fcontext -a -t unconfined_execmem_exec_t '/usr/bin/VBoxClient' > /dev/null 2>&1

    # Certain Ubuntu/Debian versions use a special PCI-id file to identify
    # video drivers.  Some versions have the directory and don't use it.
    # Those versions can autoload vboxvideo though, so we don't need to
    # hack the configuration file for them.
    test -f /etc/debian_version -a -d /usr/share/xserver-xorg/pci &&
    {
        test -h -a ! -e "$lib_dir/vboxvideo.ids" &&
            rm -f "$lib_dir/vboxvideo.ids"
        ln -s "$lib_dir/vboxvideo.ids" /usr/share/xserver-xorg/pci 2>/dev/null
        test "$useHalForMouse" -eq 1 && doX11Config=0
    }

    # Do the XF86Config/xorg.conf hack for those versions that require it
    if [ $dox11config -eq 1 ]
    then
        # Backup any xorg.conf files
        for i in $x11conf_files; do
          test -r "$i" -a ! -f "`dirname $i`/xorg.vbox.nobak" &&
              cp "$i" "$i.vbox"
        done
        if [ $dox11config15suse -eq 1 ]
        then
            cp /etc/X11/xorg.conf /etc/X11/xorg.conf.bak 2> /dev/null
            cp "$lib_dir/linux_xorg_suse11.conf" /etc/X11/xorg.conf 2> /dev/null
        elif [ $dox11config15 -eq 1 ]
        then
            "$lib_dir/x11config15.pl" >> $LOG 2>&1
            x11configured=0
            for x11configdir in /etc/X11 /etc /usr/etc/X11 /usr/lib/X11
            do
                if [ -e $x11configdir/xorg.conf -o -e $x11configdir/xorg.conf-4 ]
                then
                    x11configured=1
                fi
                if [ $x11configured -eq 0 ]
                then
                    cat > /etc/X11/xorg.conf << EOF
# Default xorg.conf for Xorg 1.5+ without PCI_TXT_IDS_PATH enabled.
#
# This file was created by VirtualBox Additions installer as it
# was unable to find any existing configuration file for X.

Section "Device"
        Identifier      "VirtualBox Video Card"
        Driver          "vboxvideo"
EndSection
EOF
                    touch /etc/X11/xorg.vbox.nobak
                fi
            done
        else
            "$lib_dir/x11config.pl" >> $LOG 2>&1
        fi
    fi

    # Certain Ubuntu/Debian versions use a special PCI-id file to identify
    # video drivers.  Some versions have the directory and don't use it.
    # Those versions can autoload vboxvideo though, so we don't need to
    # hack the configuration file for them.
    test -f /etc/debian_version -a -d /usr/share/xserver-xorg/pci &&
    {
        test -h -a ! -e "$share_dir/vboxvideo.ids" &&
            rm -rf "$share_dir/vboxvideo.ids"
        ln -s "$share_dir/vboxvideo.ids" /usr/share/xserver-xorg/pci 2>/dev/null
        test "$useHalForMouse" -eq 1 && doX11Config=0
    }

    # X.Org Server versions starting with 1.5 can do mouse auto-detection,
    # to make our lives easier and spare us the nasty hacks.
    if [ $useHalForMouse -eq 1 ]
    then
        # Install hal information about the mouse driver so that X.Org
        # knows to load it.
        if [ -d /etc/hal/fdi/policy ]
        then
            install -o 0 -g 0 -m 0644 "$share_dir/90-vboxguest.fdi" /etc/hal/fdi/policy
            # Delete the hal cache so that it notices our fdi file
            rm -r /var/cache/hald/fdi-cache 2> /dev/null
        fi
    fi
    # Install the guest OpenGL drivers
    if [ "$ARCH" = "amd64" ]
    then
        LIB=/usr/lib64
    else
        LIB=/usr/lib
    fi
    if [ -d /usr/lib64/dri ]
    then
        rm -f /usr/lib64/dri/vboxvideo_dri.so
        ln -s $LIB/VBoxOGL.so /usr/lib64/dri/vboxvideo_dri.so
    elif [ -d /usr/lib/dri ]
    then
        rm -f /usr/lib/dri/vboxvideo_dri.so
        ln -s $LIB/VBoxOGL.so /usr/lib/dri/vboxvideo_dri.so
    fi

    # And set up VBoxClient to start when the X session does
    install_x11_startup_app "$lib_dir/98vboxadd-xclient" "$share_dir/vboxclient.desktop" VBoxClient VBoxClient-all
}

# cleanup_script
cleanup()
{
    # Restore xorg.conf files as far as possible
    for i in $x11conf_files; do
        restored=0
        if test -f "`dirname $i`/xorg.vbox.nobak"; then
            rm -f "$i" 2> /dev/null
            restored=1
        elif test -r "$i.vbox"; then
            if ! grep -q -E "vboxvideo|vboxmouse" "$i.vbox"; then
                mv -f "$i.vbox" "$i"
                restored=1
            fi
        elif test -r "$i.bak"; then
            if ! grep -q -E "vboxvideo|vboxmouse" "$i.bak"; then
                mv -f "$i.bak" "$i"
                restored=1
            fi
        elif ! test -f "$i"; then
            restored=1
        fi
        test "$restored" = 1 &&
            rm -f "`dirname $i`/xorg.vbox.nobak" "$i.vbox" 2> /dev/null
        test "$restored" = 0 && cat << EOF
Failed to restore the X server configuration file $i.
Please make sure that you reconfigure your X server before it is started
again, as otherwise it may fail to start!
EOF
    done

    # Remove X.Org drivers
    find "$x11_modules_dir" /usr/lib64/xorg/modules /usr/lib/xorg/modules \
        /usr/X11R6/lib64/modules /usr/X11R6/lib/modules \
        /usr/X11R6/lib/X11/modules \
        '(' -name 'vboxvideo_drv*' -o -name 'vboxmouse_drv*' ')' \
        -exec rm -f '{}' ';' 2>/dev/null

    # Remove the link to vboxvideo_dri.so
    rm -f /usr/lib/dri/vboxvideo_dri.so /usr/lib64/dri/vboxvideo_dri.so 2>/dev/null

    # Remove VBoxClient autostart files
    rm /etc/X11/Xsession.d/98vboxadd-xclient 2>/dev/null
    rm /etc/X11/xinit.d/98vboxadd-xclient 2>/dev/null
    rm /etc/X11/xinit/xinitrc.d/98vboxadd-xclient.sh 2>/dev/null
    rm /etc/xdg/autostart/vboxclient.desktop 2>/dev/null
    rm /usr/share/autostart/vboxclient.desktop 2>/dev/null

    # Remove other files
    rm /etc/hal/fdi/policy/90-vboxguest.fdi 2>/dev/null
    rm /usr/share/xserver-xorg/pci/vboxvideo.ids 2>/dev/null
}

dmnstatus()
{
    if running_vboxguest; then
        echo "The VirtualBox Additions are currently running."
    else
        echo "The VirtualBox Additions are not currently running."
    fi
}

case "$1" in
start)
    start
    ;;
stop)
    stop
    ;;
restart)
    restart
    ;;
setup)
    setup
    ;;
cleanup)
    cleanup
    ;;
status)
    dmnstatus
    ;;
*)
    echo "Usage: $0 {start|stop|restart|status}"
    exit 1
esac

exit
