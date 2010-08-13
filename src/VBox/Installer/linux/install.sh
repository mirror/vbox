#!/bin/sh
#
# Oracle VM VirtualBox
# VirtualBox linux installation script

#
# Copyright (C) 2007-2010 Oracle Corporation
#
# Oracle Corporation confidential
# All rights reserved
#

PATH=$PATH:/bin:/sbin:/usr/sbin

# Source functions needed by the installer
. ./routines.sh

LOG="/var/log/vbox-install.log"
VERSION="_VERSION_"
SVNREV="_SVNREV_"
BUILD="_BUILD_"
ARCH="_ARCH_"
HARDENED="_HARDENED_"
CONFIG_DIR="/etc/vbox"
CONFIG="vbox.cfg"
CONFIG_FILES="filelist"
DEFAULT_FILES=`pwd`/deffiles
GROUPNAME="vboxusers"
INSTALLATION_DIR="/opt/VirtualBox"
LICENSE_ACCEPTED=""
PREV_INSTALLATION=""
PYTHON="_PYTHON_"
ACTION=""
SELF=$1
DKMS=`which dkms 2> /dev/null`
RC_SCRIPT=0
if [ -n "$HARDENED" ]; then
    VBOXDRV_MODE=0600
    VBOXDRV_GRP="root"
else
    VBOXDRV_MODE=0660
    VBOXDRV_GRP=$GROUPNAME
fi
VBOXUSB_MODE=0664
VBOXUSB_GRP=$GROUPNAME


##############################################################################
# Helper routines                                                            #
##############################################################################

usage() {
    info ""
    info "Usage: install [<installation directory>] | uninstall"
    info ""
    info "Example:"
    info "$SELF install"
    exit 1
}

module_loaded() {
    lsmod | grep -q vboxdrv
}

# This routine makes sure that there is no previous installation of
# VirtualBox other than one installed using this install script or a
# compatible method.  We do this by checking for any of the VirtualBox
# applications in /usr/bin.  If these exist and are not symlinks into
# the installation directory, then we assume that they are from an
# incompatible previous installation.

## Helper routine: test for a particular VirtualBox binary and see if it
## is a link into a previous installation directory
##
## Arguments: 1) the binary to search for and
##            2) the installation directory (if any)
## Returns: false if an incompatible version was detected, true otherwise
check_binary() {
    binary=$1
    install_dir=$2
    test ! -e $binary 2>&1 > /dev/null ||
        ( test -n "$install_dir" &&
              readlink $binary 2>/dev/null | grep "$install_dir" > /dev/null
        )
}

## Main routine
##
## Argument: the directory where the previous installation should be
##           located.  If this is empty, then we will assume that any
##           installation of VirtualBox found is incompatible with this one.
## Returns: false if an incompatible installation was found, true otherwise
check_previous() {
    install_dir=$1
    # These should all be symlinks into the installation folder
    check_binary "/usr/bin/VirtualBox" "$install_dir" &&
    check_binary "/usr/bin/VBoxManage" "$install_dir" &&
    check_binary "/usr/bin/VBoxSDL" "$install_dir" &&
    check_binary "/usr/bin/VBoxVRDP" "$install_dir" &&
    check_binary "/usr/bin/VBoxHeadless" "$install_dir" &&
    check_binary "/usr/bin/vboxwebsrv" "$install_dir"
}

##############################################################################
# Main script                                                                #
##############################################################################

info "VirtualBox Version $VERSION r$SVNREV ($BUILD) installer"

check_root      # Make sure that we were invoked as root...
check_running   # ... and that no copy of VirtualBox was running at the time.

# Set up logging before anything else
create_log $LOG
log "VirtualBox $VERSION r$SVNREV installer, built $BUILD."
log ""
log "Testing system setup..."

# Sanity check: figure out whether build arch matches uname arch
cpu=`uname -m`;
case "$cpu" in
  i[3456789]86|x86)
    cpu="x86"
    ;;
  x86_64)
    cpu="amd64"
    ;;
esac
if [ "$cpu" != "$ARCH" ]; then
  info "Detected unsupported $cpu environment."
  log "Detected unsupported $cpu environment."
  exit 1
fi

# Check that the system is setup correctly for the installation
have_bzip2="`check_bzip2; echo $?`"     # Do we have bzip2?
have_gmake="`check_gmake; echo $?`"     # Do we have GNU make?
have_ksource="`check_ksource; echo $?`" # Can we find the kernel source?
have_gcc="`check_gcc; echo $?`"         # Is GCC installed?

if [ $have_bzip2 -eq 1 -o $have_gmake -eq 1 -o $have_ksource -eq 1 \
     -o $have_gcc -eq 1 ]; then
    info "Problems were found which would prevent VirtualBox from installing."
    info "Please correct these problems and try again."
    log "Giving up due to the problems mentioned above."
    exit 1
else
    log "System setup appears correct."
    log ""
fi

# Sensible default actions
ACTION="install"
BUILD_MODULE="true"
while true
do
    if [ "$2" = "" ]; then
        break
    fi
    shift
    case "$1" in
        install)
            ACTION="install"
            ;;

        uninstall)
            ACTION="uninstall"
            ;;

        force)
            FORCE_UPGRADE=1
            ;;
        license_accepted_unconditionally)
            # Legacy option
            ;;
        no_module)
            BUILD_MODULE=""
            ;;
        *)
            if [ "$ACTION" = "" ]; then
                info "Unknown command '$1'."
                usage
            fi
            if [ "`echo $1|cut -c1`" != "/" ]; then
                info "Please specify an absolute path"
                usage
            fi
            INSTALLATION_DIR="$1"
            ;;
    esac
done

if [ "$ACTION" = "install" ]; then
    # Find previous installation
    if [ ! -r $CONFIG_DIR/$CONFIG ]; then
        mkdir -p $CONFIG_DIR
        touch $CONFIG_DIR/$CONFIG
    else
        . $CONFIG_DIR/$CONFIG
        PREV_INSTALLATION=$INSTALL_DIR
    fi
    if ! check_previous $INSTALL_DIR
    then
        info
        info "You appear to have a version of VirtualBox on your system which was installed"
        info "from a different source or using a different type of installer (or a damaged"
        info "installation of VirtualBox).  We strongly recommend that you remove it before"
        info "installing this version of VirtualBox."
        info
        info "Do you wish to continue anyway? [yes or no]"
        read reply dummy
        if ! expr "$reply" : [yY] && ! expr "$reply" : [yY][eE][sS]
        then
            info
            info "Cancelling installation."
            log "User requested cancellation of the installation"
            exit 1
        fi
    fi

    # Terminate Server and VBoxNetDHCP if running
    terminate_proc VBoxSVC
    terminate_proc VBoxNetDHCP

    # Remove previous installation
    if [ -n "$PREV_INSTALLATION" -a -z "$FORCE_UPGRADE" -a ! "$VERSION" = "$INSTALL_VER" ] &&
      expr "$INSTALL_VER" "<" "1.6.0" > /dev/null 2>&1
    then
        info
        info "If you are upgrading from VirtualBox 1.5 or older and if some of your virtual"
        info "machines have saved states, then the saved state information will be lost"
        info "after the upgrade and will have to be discarded.  If you do not want this then"
        info "you can cancel the upgrade now."
        info
        info "Do you wish to continue? [yes or no]"
        read reply dummy
        if ! expr "$reply" : [yY] && ! expr "$reply" : [yY][eE][sS]
        then
            info
            info "Cancelling upgrade."
            log "User requested cancellation of the installation"
            exit 1
        fi
    fi

    if [ ! "$VERSION" = "$INSTALL_VER" -a ! "$BUILD_MODULE" = "true" -a -n "$DKMS" ]
    then
        # Not doing this can confuse dkms
        info "Rebuilding the kernel module after version change"
        BUILD_MODULE=true
    fi

    if [ -n "$PREV_INSTALLATION" ]; then
        [ -n "$INSTALL_REV" ] && INSTALL_REV=" r$INSTALL_REV"
        info "Removing previous installation of VirtualBox $INSTALL_VER$INSTALL_REV from $PREV_INSTALLATION"
        log "Removing previous installation of VirtualBox $INSTALL_VER$INSTALL_REV from $PREV_INSTALLATION"
        log ""

        stop_init_script vboxnet
        delrunlevel vboxnet > /dev/null 2>&1
        if [ "$BUILD_MODULE" = "true" ]; then
            stop_init_script vboxdrv
            if [ -n "$DKMS" ]
            then
                $DKMS remove -m vboxdrv -v $INSTALL_VER --all > /dev/null 2>&1
                $DKMS remove -m vboxnetflt -v $INSTALL_VER --all > /dev/null 2>&1
                $DKMS remove -m vboxnetadp -v $INSTALL_VER --all > /dev/null 2>&1
            fi
            # OSE doesn't always have the initscript
            rmmod vboxnetadp > /dev/null 2>&1
            rmmod vboxnetflt > /dev/null 2>&1
            rmmod vboxdrv > /dev/null 2>&1

            module_loaded && {
                info "Warning: could not stop VirtualBox kernel module."
                info "Please restart your system to apply changes."
                log "Unable to remove the old VirtualBox kernel module."
                log "  An old version of VirtualBox may be running."
            }
        else
            VBOX_DONT_REMOVE_OLD_MODULES=1
        fi

        VBOX_NO_UNINSTALL_MESSAGE=1
        . ./uninstall.sh

    fi

    info "Installing VirtualBox to $INSTALLATION_DIR"
    log "Installing VirtualBox to $INSTALLATION_DIR"
    log ""

    # Verify the archive
    mkdir -p $INSTALLATION_DIR
    bzip2 -d -c VirtualBox.tar.bz2 | tar -tf - > $CONFIG_DIR/$CONFIG_FILES
    RETVAL=$?
    if [ $RETVAL != 0 ]; then
        rmdir $INSTALLATION_DIR 2> /dev/null
        rm -f $CONFIG_DIR/$CONFIG 2> /dev/null
        rm -f $CONFIG_DIR/$CONFIG_FILES 2> /dev/null
        log 'Error running "bzip2 -d -c VirtualBox.tar.bz2 | tar -tf - > '"$CONFIG_DIR/$CONFIG_FILES"'".'
        abort "Error installing VirtualBox.  Installation aborted"
    fi

    # Create installation directory and install
    bzip2 -d -c VirtualBox.tar.bz2 | tar -xf - -C $INSTALLATION_DIR
    RETVAL=$?
    if [ $RETVAL != 0 ]; then
        cwd=`pwd`
        cd $INSTALLATION_DIR
        rm -f `cat $CONFIG_DIR/$CONFIG_FILES` 2> /dev/null
        cd $pwd
        rmdir $INSTALLATION_DIR 2> /dev/null
        rm -f $CONFIG_DIR/$CONFIG 2> /dev/null
        log 'Error running "bzip2 -d -c VirtualBox.tar.bz2 | tar -xf - -C '"$INSTALLATION_DIR"'".'
        abort "Error installing VirtualBox.  Installation aborted"
    fi

    cp uninstall.sh routines.sh $INSTALLATION_DIR
    echo "routines.sh" >> $CONFIG_DIR/$CONFIG_FILES
    echo "uninstall.sh" >> $CONFIG_DIR/$CONFIG_FILES

    # XXX SELinux: allow text relocation entries
    if [ -x /usr/bin/chcon ]; then
        chcon -t texrel_shlib_t $INSTALLATION_DIR/VBox* > /dev/null 2>&1
        chcon -t texrel_shlib_t $INSTALLATION_DIR/VRDPAuth.so > /dev/null 2>&1
        chcon -t texrel_shlib_t $INSTALLATION_DIR/VirtualBox.so > /dev/null 2>&1
        chcon -t texrel_shlib_t $INSTALLATION_DIR/components/VBox*.so > /dev/null 2>&1
        chcon -t java_exec_t    $INSTALLATION_DIR/VirtualBox > /dev/null 2>&1
        chcon -t java_exec_t    $INSTALLATION_DIR/VBoxSDL > /dev/null 2>&1
        chcon -t java_exec_t    $INSTALLATION_DIR/VBoxHeadless > /dev/null 2>&1
        chcon -t java_exec_t    $INSTALLATION_DIR/VBoxNetDHCP > /dev/null 2>&1
        chcon -t java_exec_t    $INSTALLATION_DIR/vboxwebsrv > /dev/null 2>&1
        chcon -t java_exec_t    $INSTALLATION_DIR/webtest > /dev/null 2>&1
    fi

    # Hardened build: Mark selected binaries set-user-ID-on-execution,
    #                 create symlinks for working around unsupported $ORIGIN/.. in VBoxC.so (setuid),
    #                 and finally make sure the directory is only writable by the user (paranoid).
    if [ -n "$HARDENED" ]; then
        test -e $INSTALLATION_DIR/VirtualBox    && chmod 4511 $INSTALLATION_DIR/VirtualBox
        test -e $INSTALLATION_DIR/VBoxSDL       && chmod 4511 $INSTALLATION_DIR/VBoxSDL
        test -e $INSTALLATION_DIR/VBoxHeadless  && chmod 4511 $INSTALLATION_DIR/VBoxHeadless
        test -e $INSTALLATION_DIR/VBoxNetDHCP   && chmod 4511 $INSTALLATION_DIR/VBoxNetDHCP

        ln -sf $INSTALLATION_DIR/VBoxVMM.so   $INSTALLATION_DIR/components/VBoxVMM.so
        ln -sf $INSTALLATION_DIR/VBoxREM.so   $INSTALLATION_DIR/components/VBoxREM.so
        ln -sf $INSTALLATION_DIR/VBoxRT.so    $INSTALLATION_DIR/components/VBoxRT.so
        ln -sf $INSTALLATION_DIR/VBoxDDU.so   $INSTALLATION_DIR/components/VBoxDDU.so
        ln -sf $INSTALLATION_DIR/VBoxXPCOM.so $INSTALLATION_DIR/components/VBoxXPCOM.so

        chmod go-w $INSTALLATION_DIR
    fi

    # This binary needs to be suid root in any case, even if not hardened
    test -e $INSTALLATION_DIR/VBoxNetAdpCtl && chmod 4511 $INSTALLATION_DIR/VBoxNetAdpCtl

    # Install runlevel scripts
    install_init_script vboxdrv.sh vboxdrv
    delrunlevel vboxdrv > /dev/null 2>&1
    addrunlevel vboxdrv 20 80 # This may produce useful output

    # Create users group
    groupadd $GROUPNAME 2> /dev/null

    # Create symlinks to start binaries
    ln -sf $INSTALLATION_DIR/VBox.sh /usr/bin/VirtualBox
    ln -sf $INSTALLATION_DIR/VBox.sh /usr/bin/VBoxManage
    ln -sf $INSTALLATION_DIR/VBox.sh /usr/bin/VBoxSDL
    ln -sf $INSTALLATION_DIR/VBox.sh /usr/bin/VBoxVRDP
    ln -sf $INSTALLATION_DIR/VBox.sh /usr/bin/VBoxHeadless
    ln -sf $INSTALLATION_DIR/VBox.sh /usr/bin/vboxwebsrv
    ln -sf $INSTALLATION_DIR/VBox.png /usr/share/pixmaps/VBox.png
    ln -sf $INSTALLATION_DIR/virtualbox.desktop /usr/share/applications/virtualbox.desktop
    ln -sf $INSTALLATION_DIR/rdesktop-vrdp /usr/bin/rdesktop-vrdp
    ln -sf $INSTALLATION_DIR/src/vboxdrv /usr/src/vboxdrv-_VERSION_
    ln -sf $INSTALLATION_DIR/src/vboxnetflt /usr/src/vboxnetflt-_VERSION_
    ln -sf $INSTALLATION_DIR/src/vboxnetadp /usr/src/vboxnetadp-_VERSION_

    # If Python is available, install Python bindings
    if [ -n "$PYTHON" ]; then
      maybe_run_python_bindings_installer $INSTALLATION_DIR
    fi

    # Create udev description file
    if [ -d /etc/udev/rules.d ]; then
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
        # Write udev rules
        echo "KERNEL=${udev_fix}\"vboxdrv\", NAME=\"vboxdrv\", OWNER=\"root\", GROUP=\"$VBOXDRV_GRP\", MODE=\"$VBOXDRV_MODE\"" \
          > /etc/udev/rules.d/10-vboxdrv.rules
        echo "SUBSYSTEM=${udev_fix}\"usb_device\", GROUP=\"$VBOXUSB_GRP\", MODE=\"$VBOXUSB_MODE\"" \
          >> /etc/udev/rules.d/10-vboxdrv.rules
        echo "SUBSYSTEM=${udev_fix}\"usb\", ENV{DEVTYPE}==\"usb_device\", GROUP=\"$VBOXUSB_GRP\", MODE=\"$VBOXUSB_MODE\"" \
          >> /etc/udev/rules.d/10-vboxdrv.rules
    fi
    # Remove old udev description file
    if [ -f /etc/udev/rules.d/60-vboxdrv.rules ]; then
        rm -f /etc/udev/rules.d/60-vboxdrv.rules 2> /dev/null
    fi

    # Push the permissions to the USB device nodes.  One of these should match.
    # Rather nasty to use udevadm trigger for this, but I don't know of any
    # better way.
    udevadm trigger --subsystem-match=usb > /dev/null 2>&1
    udevtrigger --subsystem-match=usb > /dev/null 2>&1
    udevtrigger --subsystem-match=usb_device > /dev/null 2>&1
    udevplug -Busb > /dev/null 2>&1

    # Make kernel module
    MODULE_FAILED="false"
    if [ "$BUILD_MODULE" = "true" ]
    then
        info "Building the VirtualBox vboxdrv kernel module"
        log "Output from the module build process (the Linux kernel build system) follows:"
        cur=`pwd`
        log ""
        cd $INSTALLATION_DIR/src/vboxdrv
        ./build_in_tmp \
          --save-module-symvers /tmp/vboxdrv-Module.symvers \
          --no-print-directory install >> $LOG 2>&1
        RETVAL=$?
        if [ $RETVAL -ne 0 ]
        then
            info "Failed to build the vboxdrv kernel module."
            info "Please check the log file $LOG for more information."
            MODULE_FAILED="true"
            RC_SCRIPT=1
        else
            info "Building the VirtualBox netflt kernel module"
            log "Output from the module build process (the Linux kernel build system) follows:"
            cd $INSTALLATION_DIR/src/vboxnetflt
            ./build_in_tmp \
              --use-module-symvers /tmp/vboxdrv-Module.symvers \
              --no-print-directory install >> $LOG 2>&1
            RETVAL=$?
            if [ $RETVAL -ne 0 ]
            then
                info "Failed to build the vboxnetflt kernel module."
                info "Please check the log file $LOG for more information."
                MODULE_FAILED="true"
                RC_SCRIPT=1
            else
                info "Building the VirtualBox netadp kernel module"
                log "Output from the module build process (the Linux kernel build system) follows:"
                cd $INSTALLATION_DIR/src/vboxnetadp
                ./build_in_tmp \
                    --use-module-symvers /tmp/vboxdrv-Module.symvers \
                    --no-print-directory install >> $LOG 2>&1
                RETVAL=$?
                if [ $RETVAL -ne 0 ]
                then
                    info "Failed to build the vboxnetadp kernel module."
                    info "Please check the log file $LOG for more information."
                    MODULE_FAILED="true"
                    RC_SCRIPT=1
                fi
            fi
        fi
        # cleanup
        rm -f /tmp/vboxdrv-Module.symvers
        # Start VirtualBox kernel module
        if [ $RETVAL -eq 0 ] && ! start_init_script vboxdrv; then
            info "Failed to load the kernel module."
            MODULE_FAILED="true"
            RC_SCRIPT=1
        fi
        log ""
        log "End of the output from the Linux kernel build system."
        cd $cur
    fi

    echo "# VirtualBox installation directory" > $CONFIG_DIR/$CONFIG
    echo "INSTALL_DIR='$INSTALLATION_DIR'" >> $CONFIG_DIR/$CONFIG
    echo "# VirtualBox version" >> $CONFIG_DIR/$CONFIG
    echo "INSTALL_VER='$VERSION'" >> $CONFIG_DIR/$CONFIG
    echo "INSTALL_REV='$SVNREV'" >> $CONFIG_DIR/$CONFIG
    info ""
    if [ ! "$MODULE_FAILED" = "true" ]
    then
        info "VirtualBox has been installed successfully."
    else
        info "VirtualBox has been installed successfully, but the kernel module could not"
        info "be built.  When you have fixed the problems preventing this, execute"
        info "  /etc/init.d/vboxdrv setup"
        info "as administrator to build it."
    fi
    info ""
    info "You will find useful information about using VirtualBox in the user manual"
    info "  $INSTALLATION_DIR/UserManual.pdf"
    info "and in the user FAQ"
    info "  http://www.virtualbox.org/wiki/User_FAQ"
    info ""
    info "We hope that you enjoy using VirtualBox."
    info ""
    log "Installation successful"
elif [ "$ACTION" = "uninstall" ]; then
    . ./uninstall.sh
fi
exit $RC_SCRIPT
