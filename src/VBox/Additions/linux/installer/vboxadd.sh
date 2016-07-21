#! /bin/sh
#
# Linux Additions kernel module init script ($Revision$)
#

#
# Copyright (C) 2006-2012 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#


# chkconfig: 345 10 90
# description: VirtualBox Linux Additions kernel modules
#
### BEGIN INIT INFO
# Provides:       vboxadd
# Required-Start:
# Required-Stop:
# Default-Start:  2 3 4 5
# Default-Stop:   0 1 6
# Description:    VirtualBox Linux Additions kernel modules
### END INIT INFO

## @todo This file duplicates a lot of script with vboxdrv.sh.  When making
# changes please try to reduce differences between the two wherever possible.

PATH=$PATH:/bin:/sbin:/usr/sbin
PACKAGE=VBoxGuestAdditions
LOG="/var/log/vboxadd-install.log"
MODPROBE=/sbin/modprobe
OLDMODULES="vboxguest vboxadd vboxsf vboxvfs vboxvideo"
SCRIPTNAME=vboxadd.sh
QUICKSETUP=

# These are getting hard-coded in more and more places...
test -z "${KERN_DIR}" && KERN_DIR="/lib/modules/`uname -r`/build"
test -z "${MODULE_DIR}" && MODULE_DIR="/lib/modules/`uname -r`/misc"
KERN_DIR_SUFFIX="${KERN_DIR#/lib/modules/}"
KERN_VER="${KERN_DIR_SUFFIX%/*}"

if $MODPROBE -c 2>/dev/null | grep -q '^allow_unsupported_modules  *0'; then
  MODPROBE="$MODPROBE --allow-unsupported-modules"
fi

# Check architecture
cpu=`uname -m`;
case "$cpu" in
  i[3456789]86|x86)
    cpu="x86"
    ldconfig_arch="(libc6)"
    lib_candidates="/usr/lib/i386-linux-gnu /usr/lib /lib"
    ;;
  x86_64|amd64)
    cpu="amd64"
    ldconfig_arch="(libc6,x86-64)"
    lib_candidates="/usr/lib/x86_64-linux-gnu /usr/lib64 /usr/lib /lib64 /lib"
    ;;
esac
for i in $lib_candidates; do
  if test -d "$i/VBoxGuestAdditions"; then
    lib_path=$i
    break
  fi
done

# Preamble for Gentoo
if [ "`which $0`" = "/sbin/rc" ]; then
    shift
fi

begin()
{
    test -n "${2}" && echo "${SCRIPTNAME}: ${1}."
    logger -t "${SCRIPTNAME}" "${1}."
}

succ_msg()
{
    logger -t "${SCRIPTNAME}" "${1}."
}

show_error()
{
    echo "${SCRIPTNAME}: failed: ${1}." >&2
    logger -t "${SCRIPTNAME}" "${1}."
}

fail()
{
    show_error "$1"
    exit 1
}

dev=/dev/vboxguest
userdev=/dev/vboxuser
config=/var/lib/VBoxGuestAdditions/config
owner=vboxadd
group=1

running_vboxguest()
{
    lsmod | grep -q "vboxguest[^_-]"
}

running_vboxadd()
{
    lsmod | grep -q "vboxadd[^_-]"
}

running_vboxsf()
{
    lsmod | grep -q "vboxsf[^_-]"
}

running_vboxvideo()
{
    lsmod | grep -q "vboxvideo[^_-]"
}

do_vboxguest_non_udev()
{
    if [ ! -c $dev ]; then
        maj=`sed -n 's;\([0-9]\+\) vboxguest;\1;p' /proc/devices`
        if [ ! -z "$maj" ]; then
            min=0
        else
            min=`sed -n 's;\([0-9]\+\) vboxguest;\1;p' /proc/misc`
            if [ ! -z "$min" ]; then
                maj=10
            fi
        fi
        test -z "$maj" && {
            rmmod vboxguest 2>/dev/null
            fail "Cannot locate the VirtualBox device"
        }

        mknod -m 0664 $dev c $maj $min || {
            rmmod vboxguest 2>/dev/null
            fail "Cannot create device $dev with major $maj and minor $min"
        }
    fi
    chown $owner:$group $dev 2>/dev/null || {
        rm -f $dev 2>/dev/null
        rm -f $userdev 2>/dev/null
        rmmod vboxguest 2>/dev/null
        fail "Cannot change owner $owner:$group for device $dev"
    }

    if [ ! -c $userdev ]; then
        maj=10
        min=`sed -n 's;\([0-9]\+\) vboxuser;\1;p' /proc/misc`
        if [ ! -z "$min" ]; then
            mknod -m 0666 $userdev c $maj $min || {
                rm -f $dev 2>/dev/null
                rmmod vboxguest 2>/dev/null
                fail "Cannot create device $userdev with major $maj and minor $min"
            }
            chown $owner:$group $userdev 2>/dev/null || {
                rm -f $dev 2>/dev/null
                rm -f $userdev 2>/dev/null
                rmmod vboxguest 2>/dev/null
                fail "Cannot change owner $owner:$group for device $userdev"
            }
        fi
    fi
}

start()
{
    begin "Starting the VirtualBox Guest Additions" console;
    # If we got this far assume that the slow set-up has been done.
    QUICKSETUP=yes
    if test -r $config; then
      . $config
    else
      fail "Configuration file $config not found"
    fi
    test -n "$INSTALL_DIR" -a -n "$INSTALL_VER" ||
      fail "Configuration file $config not complete"
    uname -r | grep -q -E '^2\.6|^3|^4' 2>/dev/null &&
        ps -A -o comm | grep -q '/*udevd$' 2>/dev/null ||
        no_udev=1
    running_vboxguest || {
        rm -f $dev || {
            fail "Cannot remove $dev"
        }

        rm -f $userdev || {
            fail "Cannot remove $userdev"
        }

        $MODPROBE vboxguest >/dev/null 2>&1 || {
            setup
            $MODPROBE vboxguest >/dev/null 2>&1 || {
                /sbin/rcvboxadd-x11 cleanup
                fail "modprobe vboxguest failed"
            }
        }
        case "$no_udev" in 1)
            sleep .5;;
        esac
    }
    case "$no_udev" in 1)
        do_vboxguest_non_udev;;
    esac

    running_vboxsf || {
        $MODPROBE vboxsf > /dev/null 2>&1 || {
            if dmesg | grep "VbglR0SfConnect failed" > /dev/null 2>&1; then
                show_error "Unable to start shared folders support.  Make sure that your VirtualBox build"
                show_error "supports this feature."
            else
                show_error "modprobe vboxsf failed"
            fi
        }
    }

    # Put the X.Org driver in place.  This is harmless if it is not needed.
    /sbin/rcvboxadd-x11 setup
    # Install the guest OpenGL drivers.  For now we don't support
    # multi-architecture installations
    rm -f /etc/ld.so.conf.d/00vboxvideo.conf
    ldconfig
    if /usr/bin/VBoxClient --check3d 2>/dev/null; then
        rm -f /var/lib/VBoxGuestAdditions/lib/system/tmp.so
        mkdir -m 0755 -p /var/lib/VBoxGuestAdditions/lib/system
        ldconfig -p | while read -r line; do
            case "${line}" in "libGL.so.1 ${ldconfig_arch} => "*)
                ln -s "${line#libGL.so.1 ${ldconfig_arch} => }" /var/lib/VBoxGuestAdditions/lib/system/tmp.so
                mv /var/lib/VBoxGuestAdditions/lib/system/tmp.so /var/lib/VBoxGuestAdditions/lib/system/libGL.so.1
                break
            esac
        done
        ldconfig -p | while read -r line; do
            case "${line}" in "libEGL.so.1 ${ldconfig_arch} => "*)
                ln -s "${line#libEGL.so.1 ${ldconfig_arch} => }" /var/lib/VBoxGuestAdditions/lib/system/tmp.so
                mv /var/lib/VBoxGuestAdditions/lib/system/tmp.so /var/lib/VBoxGuestAdditions/lib/system/libEGL.so.1
                break
            esac
        done
        ln -sf "${INSTALL_DIR}/lib/VBoxOGL.so" /var/lib/VBoxGuestAdditions/lib/libGL.so.1
        ln -sf "${INSTALL_DIR}/lib/VBoxEGL.so" /var/lib/VBoxGuestAdditions/lib/libEGL.so.1
        # SELinux for the OpenGL libraries, so that gdm can load them during the
        # acceleration support check.  This prevents an "Oh no, something has gone
        # wrong!" error when starting EL7 guests.
        if test -e /etc/selinux/config; then
            semanage fcontext -a -t lib_t "/var/lib/VBoxGuestAdditions/lib/libGL.so.1"
            semanage fcontext -a -t lib_t "/var/lib/VBoxGuestAdditions/lib/libEGL.so.1"
            chcon -h  -t lib_t "/var/lib/VBoxGuestAdditions/lib/libGL.so.1"
            chcon -h  -t lib_t  "/var/lib/VBoxGuestAdditions/lib/libEGL.so.1"
        fi
        echo "/var/lib/VBoxGuestAdditions/lib" > /etc/ld.so.conf.d/00vboxvideo.conf
    fi
    ldconfig

    # Mount all shared folders from /etc/fstab. Normally this is done by some
    # other startup script but this requires the vboxdrv kernel module loaded.
    # This isn't necessary anymore as the vboxsf module is autoloaded.
    # mount -a -t vboxsf

    succ_msg
    return 0
}

stop()
{
    begin "Stopping VirtualBox Additions" console;
    if test -r /etc/ld.so.conf.d/00vboxvideo.conf; then
        rm /etc/ld.so.conf.d/00vboxvideo.conf
        ldconfig
    fi
    if ! umount -a -t vboxsf 2>/dev/null; then
        fail "Cannot unmount vboxsf folders"
    fi
    if running_vboxsf; then
        rmmod vboxsf 2>/dev/null || fail "Cannot unload module vboxsf"
    fi
    if running_vboxguest; then
        rmmod vboxguest 2>/dev/null || fail "Cannot unload module vboxguest"
        rm -f $userdev || fail "Cannot unlink $userdev"
        rm -f $dev || fail "Cannot unlink $dev"
    fi
    succ_msg
    return 0
}

restart()
{
    stop && start
    return 0
}

## Update the initramfs.  Debian and Ubuntu put the graphics driver in, and
# need the touch(1) command below.  Everyone else that I checked just need
# the right module alias file from depmod(1) and only use the initramfs to
# load the root filesystem, not the boot splash.  update-initramfs works
# for the first two and dracut for every one else I checked.  We are only
# interested in distributions recent enough to use the KMS vboxvideo driver.
## @param $1  kernel version to update for.
update_module_dependencies()
{
    depmod "${1}"
    rm -f "/lib/modules/${1}/initrd/vboxvideo"
    test -d "/lib/modules/${1}/initrd" &&
        test -f "/lib/modules/${1}/misc/vboxvideo.ko" &&
        touch "/lib/modules/${1}/initrd/vboxvideo"
    test -n "${QUICKSETUP}" && return
    if type dracut >/dev/null 2>&1; then
        dracut -f "/boot/initramfs-${1}.img" "${1}"
    elif type update-initramfs >/dev/null 2>&1; then
        update-initramfs -u -k "${1}"
    fi
}

# Remove any existing VirtualBox guest kernel modules from the disk, but not
# from the kernel as they may still be in use
cleanup_modules()
{
    begin "Removing existing VirtualBox kernel modules"
    # We no longer support DKMS, remove any leftovers.
    for i in vboxguest vboxadd vboxsf vboxvfs vboxvideo; do
        rm -rf "/var/lib/dkms/${i}"*
    done
    for i in $OLDMODULES; do
        find /lib/modules -name $i\* | xargs rm 2>/dev/null
    done
    # Remove leftover module folders.
    for i in /lib/modules/*/misc; do
        test -d "${i}" && rmdir -p "${i}" 2>/dev/null
    done
    succ_msg
}

# Build and install the VirtualBox guest kernel modules
setup_modules()
{
    # don't stop the old modules here -- they might be in use
    test -z "${QUICKSETUP}" && cleanup_modules
    # This does not work for 2.4 series kernels.  How sad.
    test -n "${QUICKSETUP}" && test -f "${MODULE_DIR}/vboxguest.ko" && return 0
    begin "Building the VirtualBox Guest Additions kernel modules"

    begin "Building the main Guest Additions module"
    if ! $BUILDINTMP \
        --save-module-symvers /tmp/vboxguest-Module.symvers \
        --module-source $MODULE_SRC/vboxguest \
        --no-print-directory install >> $LOG 2>&1; then
        show_error "Look at $LOG to find out what went wrong"
        return 1
    fi
    succ_msg
    begin "Building the shared folder support module"
    if ! $BUILDINTMP \
        --use-module-symvers /tmp/vboxguest-Module.symvers \
        --module-source $MODULE_SRC/vboxsf \
        --no-print-directory install >> $LOG 2>&1; then
        show_error  "Look at $LOG to find out what went wrong"
        return 1
    fi
    succ_msg
    begin "Building the graphics driver module"
    if ! $BUILDINTMP \
        --use-module-symvers /tmp/vboxguest-Module.symvers \
        --module-source $MODULE_SRC/vboxvideo \
        --no-print-directory install >> $LOG 2>&1; then
        show_error "Look at $LOG to find out what went wrong"
    fi
    succ_msg
    update_module_dependencies "${KERN_VER}"
    return 0
}

# Do non-kernel bits needed for the kernel modules to work properly (user
# creation, udev, mount helper...)
extra_setup()
{
    begin "Doing non-kernel setup of the Guest Additions"
    echo "Creating user for the Guest Additions." >> $LOG
    # This is the LSB version of useradd and should work on recent
    # distributions
    useradd -d /var/run/vboxadd -g 1 -r -s /bin/false vboxadd >/dev/null 2>&1
    # And for the others, we choose a UID ourselves
    useradd -d /var/run/vboxadd -g 1 -u 501 -o -s /bin/false vboxadd >/dev/null 2>&1

    # Add a group "vboxsf" for Shared Folders access
    # All users which want to access the auto-mounted Shared Folders have to
    # be added to this group.
    groupadd -r -f vboxsf >/dev/null 2>&1

    # Create udev description file
    if [ -d /etc/udev/rules.d ]; then
        echo "Creating udev rule for the Guest Additions kernel module." >> $LOG
        udev_call=""
        udev_app=`which udevadm 2> /dev/null`
        if [ $? -eq 0 ]; then
            udev_call="${udev_app} version 2> /dev/null"
        else
            udev_app=`which udevinfo 2> /dev/null`
            if [ $? -eq 0 ]; then
                udev_call="${udev_app} -V 2> /dev/null"
            fi
        fi
        udev_fix="="
        if [ "${udev_call}" != "" ]; then
            udev_out=`${udev_call}`
            udev_ver=`expr "$udev_out" : '[^0-9]*\([0-9]*\)'`
            if [ "$udev_ver" = "" -o "$udev_ver" -lt 55 ]; then
               udev_fix=""
            fi
        fi
        ## @todo 60-vboxadd.rules -> 60-vboxguest.rules ?
        echo "KERNEL=${udev_fix}\"vboxguest\", NAME=\"vboxguest\", OWNER=\"vboxadd\", MODE=\"0660\"" > /etc/udev/rules.d/60-vboxadd.rules
        echo "KERNEL=${udev_fix}\"vboxuser\", NAME=\"vboxuser\", OWNER=\"vboxadd\", MODE=\"0666\"" >> /etc/udev/rules.d/60-vboxadd.rules
    fi

    # Put mount.vboxsf in the right place
    ln -sf "$lib_path/$PACKAGE/mount.vboxsf" /sbin
    # And an rc file to re-build the kernel modules and re-set-up the X server.
    ln -sf "$lib_path/$PACKAGE/vboxadd" /sbin/rcvboxadd
    ln -sf "$lib_path/$PACKAGE/vboxadd-x11" /sbin/rcvboxadd-x11
    # And a post-installation script for rebuilding modules when a new kernel
    # is installed.
    mkdir -p /etc/kernel/postinst.d /etc/kernel/prerm.d
    cat << EOF > /etc/kernel/postinst.d/vboxadd
#!/bin/sh
test -d "/lib/modules/\${1}/build" || exit 0
KERN_DIR="/lib/modules/\${1}/build" MODULE_DIR="/lib/modules/\${1}/misc" \
/sbin/rcvboxadd quicksetup
exit 0
EOF
    cat << EOF > /etc/kernel/prerm.d/vboxadd
#!/bin/sh
for i in ${OLDMODULES}; do rm -f /lib/modules/"\${1}"/misc/"\${i}".ko; done
rmdir -p /lib/modules/"\$1"/misc 2>/dev/null
exit 0
EOF
    chmod 0755 /etc/kernel/postinst.d/vboxadd /etc/kernel/prerm.d/vboxadd
    # SELinux security context for the mount helper.
    if test -e /etc/selinux/config; then
        # This is correct.  semanage maps this to the real path, and it aborts
        # with an error, telling you what you should have typed, if you specify
        # the real path.  The "chcon" is there as a back-up in case this is
        # different on old guests.
        semanage fcontext -a -t mount_exec_t "/usr/lib/$PACKAGE/mount.vboxsf"
        chcon -t mount_exec_t "$lib_path/$PACKAGE/mount.vboxsf"
    fi
    succ_msg
}

# setup_script
setup()
{
    begin "Building Guest Additions kernel modules" console
    if test -r $config; then
      . $config
    else
      fail "Configuration file $config not found"
    fi
    test -n "$INSTALL_DIR" -a -n "$INSTALL_VER" ||
      fail "Configuration file $config not complete"
    export BUILD_TYPE
    export USERNAME

    rm -f $LOG
    MODULE_SRC="$INSTALL_DIR/src/vboxguest-$INSTALL_VER"
    BUILDINTMP="$MODULE_SRC/build_in_tmp"
    chcon -t bin_t "$BUILDINTMP" > /dev/null 2>&1

    if setup_modules; then
        mod_succ=0
    else
        mod_succ=1
        show_error "Please check that you have gcc, make, the header files for your Linux kernel and possibly perl installed."
    fi
    test -n "${QUICKSETUP}" && return "${mod_succ}"
    extra_setup
    if [ "$mod_succ" -eq "0" ]; then
        if running_vboxguest || running_vboxadd; then
            begin "You should restart your guest to make sure the new modules are actually used" console
        fi
    fi
    return "${mod_succ}"
}

# cleanup_script
cleanup()
{
    if test -r $config; then
      . $config
      test -n "$INSTALL_DIR" -a -n "$INSTALL_VER" ||
        fail "Configuration file $config not complete"
    else
      fail "Configuration file $config not found"
    fi

    # Delete old versions of VBox modules.
    cleanup_modules
    for i in /lib/modules/*; do
        update_module_dependencies "${i#/lib/modules/}"
    done

    # Remove old module sources
    for i in $OLDMODULES; do
      rm -rf /usr/src/$i-*
    done

    # Clean-up X11-related bits
    /sbin/rcvboxadd-x11 cleanup

    # Remove other files
    rm /sbin/mount.vboxsf 2>/dev/null
    rm /sbin/rcvboxadd 2>/dev/null
    rm /sbin/rcvboxadd-x11 2>/dev/null
    rm -f /etc/kernel/postinst.d/vboxadd /etc/kernel/prerm.d/vboxadd
    rmdir -p /etc/kernel/postinst.d /etc/kernel/prerm.d 2>/dev/null
    rm /etc/udev/rules.d/60-vboxadd.rules 2>/dev/null
    rm -f /lib/modules/*/initrd/vboxvideo
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
    setup && start
    ;;
quicksetup)
    QUICKSETUP=yes
    setup
    ;;
cleanup)
    cleanup
    ;;
status)
    dmnstatus
    ;;
*)
    echo "Usage: $0 {start|stop|restart|status|setup|quicksetup|cleanup}"
    exit 1
esac

exit
