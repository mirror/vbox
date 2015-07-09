#!/bin/sh
#
# Oracle VM VirtualBox
# VirtualBox linux installation script

#
# Copyright (C) 2007-2015 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

PATH=$PATH:/bin:/sbin:/usr/sbin

# Include routines and utilities needed by the installer
. ./routines.sh
#include installer-common.sh

LOG="/var/log/vbox-install.log"
VERSION="_VERSION_"
SVNREV="_SVNREV_"
BUILD="_BUILD_"
ARCH="_ARCH_"
HARDENED="_HARDENED_"
# The "BUILD_" prefixes prevent the variables from being overwritten when we
# read the configuration from the previous installation.
BUILD_BUILDTYPE="_BUILDTYPE_"
BUILD_USERNAME="_USERNAME_"
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
    info "Usage: install | uninstall"
    info ""
    info "Example:"
    info "$SELF install"
    exit 1
}

module_loaded() {
    lsmod | grep -q "vboxdrv[^_-]"
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
    check_binary "/usr/bin/VBoxDTrace" "$install_dir" &&
    check_binary "/usr/bin/VBoxBalloonCtrl" "$install_dir" &&
    check_binary "/usr/bin/VBoxAutostart" "$install_dir" &&
    check_binary "/usr/bin/vboxwebsrv" "$install_dir" &&
    check_binary "/usr/bin/vbox-img" "$install_dir" &&
    check_binary "/sbin/rcvboxdrv" "$install_dir"
}

##############################################################################
# Main script                                                                #
##############################################################################

info "VirtualBox Version $VERSION r$SVNREV ($BUILD) installer"


# Make sure that we were invoked as root...
check_root

# Set up logging before anything else
create_log $LOG

# Now stop the autostart service otherwise it will keep VBoxSVC running
stop_init_script vboxautostart-service

# Now stop the ballon control service otherwise it will keep VBoxSVC running
stop_init_script vboxballoonctrl-service

# Now stop the web service otherwise it will keep VBoxSVC running
stop_init_script vboxweb-service

# Now check if no VBoxSVC daemon is running
check_running

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
            info "Specifying an installation path is not allowed -- using /opt/VirtualBox!"
            ;;
    esac
done

if [ "$ACTION" = "install" ]; then
    # Choose a proper umask
    umask 022

    # Find previous installation
    if [ ! -r $CONFIG_DIR/$CONFIG ]; then
        mkdir -p -m 755 $CONFIG_DIR
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
    terminate_proc VBoxNetNAT

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

        stop_init_script vboxnet >/dev/null 2>&1  # Do we need this?
        delrunlevel vboxnet > /dev/null 2>&1
        if [ "$BUILD_MODULE" = "true" ]; then
            stop_init_script vboxdrv
            if [ -n "$DKMS" ]
            then
                $DKMS remove -m vboxhost -v $INSTALL_VER --all > /dev/null 2>&1
                $DKMS remove -m vboxdrv -v $INSTALL_VER --all > /dev/null 2>&1
                $DKMS remove -m vboxnetflt -v $INSTALL_VER --all > /dev/null 2>&1
                $DKMS remove -m vboxnetadp -v $INSTALL_VER --all > /dev/null 2>&1
            fi
            # OSE doesn't always have the initscript
            rmmod vboxpci > /dev/null 2>&1
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
    mkdir -p -m 755 $INSTALLATION_DIR
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
    set_selinux_permissions "$INSTALLATION_DIR" \
                            "$INSTALLATION_DIR"

    # Hardened build: Mark selected binaries set-user-ID-on-execution,
    #                 create symlinks for working around unsupported $ORIGIN/.. in VBoxC.so (setuid),
    #                 and finally make sure the directory is only writable by the user (paranoid).
    if [ -n "$HARDENED" ]; then
        test -e $INSTALLATION_DIR/VirtualBox     && chmod 4511 $INSTALLATION_DIR/VirtualBox
        test -e $INSTALLATION_DIR/VBoxSDL        && chmod 4511 $INSTALLATION_DIR/VBoxSDL
        test -e $INSTALLATION_DIR/VBoxHeadless   && chmod 4511 $INSTALLATION_DIR/VBoxHeadless
        test -e $INSTALLATION_DIR/VBoxNetDHCP    && chmod 4511 $INSTALLATION_DIR/VBoxNetDHCP
        test -e $INSTALLATION_DIR/VBoxNetNAT     && chmod 4511 $INSTALLATION_DIR/VBoxNetNAT

        ln -sf $INSTALLATION_DIR/VBoxVMM.so   $INSTALLATION_DIR/components/VBoxVMM.so
        ln -sf $INSTALLATION_DIR/VBoxRT.so    $INSTALLATION_DIR/components/VBoxRT.so

        chmod go-w $INSTALLATION_DIR
    fi

    # This binaries need to be suid root in any case, even if not hardened
    test -e $INSTALLATION_DIR/VBoxNetAdpCtl && chmod 4511 $INSTALLATION_DIR/VBoxNetAdpCtl
    test -e $INSTALLATION_DIR/VBoxVolInfo && chmod 4511 $INSTALLATION_DIR/VBoxVolInfo

    # Install runlevel scripts
    install_init_script $INSTALLATION_DIR/vboxdrv.sh vboxdrv 2>> $LOG
    install_init_script $INSTALLATION_DIR/vboxballoonctrl-service.sh vboxballoonctrl-service 2>> $LOG
    install_init_script $INSTALLATION_DIR/vboxautostart-service.sh vboxautostart-service 2>> $LOG
    install_init_script $INSTALLATION_DIR/vboxweb-service.sh vboxweb-service 2>> $LOG

    # Write the configuration. Do this before we call /etc/init.d/vboxdrv setup!
    echo "# VirtualBox installation directory" > $CONFIG_DIR/$CONFIG
    echo "INSTALL_DIR='$INSTALLATION_DIR'" >> $CONFIG_DIR/$CONFIG
    echo "# VirtualBox version" >> $CONFIG_DIR/$CONFIG
    echo "INSTALL_VER='$VERSION'" >> $CONFIG_DIR/$CONFIG
    echo "INSTALL_REV='$SVNREV'" >> $CONFIG_DIR/$CONFIG
    echo "# Build type and user name for logging purposes" >> $CONFIG_DIR/$CONFIG
    echo "BUILD_TYPE='$BUILD_BUILDTYPE'" >> $CONFIG_DIR/$CONFIG
    echo "USERNAME='$BUILD_USERNAME'" >> $CONFIG_DIR/$CONFIG

    delrunlevel vboxdrv > /dev/null 2>&1
    addrunlevel vboxdrv 2>> $LOG # This may produce useful output
    delrunlevel vboxballoonctrl-service > /dev/null 2>&1
    addrunlevel vboxballoonctrl-service 2>> $LOG # This may produce useful output
    delrunlevel vboxautostart-service > /dev/null 2>&1
    addrunlevel vboxautostart-service 2>> $LOG # This may produce useful output
    delrunlevel vboxweb-service > /dev/null 2>&1
    addrunlevel vboxweb-service 2>> $LOG # This may produce useful output

    # Create users group
    groupadd -r -f $GROUPNAME 2> /dev/null

    # Create symlinks to start binaries
    ln -sf $INSTALLATION_DIR/VBox.sh /usr/bin/VirtualBox
    ln -sf $INSTALLATION_DIR/VBox.sh /usr/bin/VBoxManage
    ln -sf $INSTALLATION_DIR/VBox.sh /usr/bin/VBoxSDL
    ln -sf $INSTALLATION_DIR/VBox.sh /usr/bin/VBoxVRDP
    ln -sf $INSTALLATION_DIR/VBox.sh /usr/bin/VBoxHeadless
    ln -sf $INSTALLATION_DIR/VBox.sh /usr/bin/VBoxBalloonCtrl
    ln -sf $INSTALLATION_DIR/VBox.sh /usr/bin/VBoxAutostart
    ln -sf $INSTALLATION_DIR/VBox.sh /usr/bin/vboxwebsrv
    ln -sf $INSTALLATION_DIR/vbox-img /usr/bin/vbox-img
    ln -sf $INSTALLATION_DIR/VBox.png /usr/share/pixmaps/VBox.png
    if [ -f $INSTALLATION_DIR/VBoxDTrace ]; then
        ln -sf $INSTALLATION_DIR/VBox.sh /usr/bin/VBoxDTrace
    fi
    ln -sf $INSTALLATION_DIR/vboxdrv.sh /sbin/rcvboxdrv
    # Unity and Nautilus seem to look here for their icons
    ln -sf $INSTALLATION_DIR/icons/128x128/virtualbox.png /usr/share/pixmaps/virtualbox.png
    ln -sf $INSTALLATION_DIR/virtualbox.desktop /usr/share/applications/virtualbox.desktop
    ln -sf $INSTALLATION_DIR/virtualbox.xml /usr/share/mime/packages/virtualbox.xml
    ln -sf $INSTALLATION_DIR/rdesktop-vrdp /usr/bin/rdesktop-vrdp
    ln -sf $INSTALLATION_DIR/src/vboxhost /usr/src/vboxhost-_VERSION_

    # Convenience symlinks. The creation fails if the FS is not case sensitive
    ln -sf VirtualBox /usr/bin/virtualbox > /dev/null 2>&1
    ln -sf VBoxManage /usr/bin/vboxmanage > /dev/null 2>&1
    ln -sf VBoxSDL /usr/bin/vboxsdl > /dev/null 2>&1
    ln -sf VBoxHeadless /usr/bin/vboxheadless > /dev/null 2>&1
    if [ -f $INSTALLATION_DIR/VBoxDTrace ]; then
        ln -sf VBoxDTrace /usr/bin/vboxdtrace > /dev/null 2>&1
    fi

    # Icons
    cur=`pwd`
    cd $INSTALLATION_DIR/icons
    for i in *; do
        cd $i
        if [ -d /usr/share/icons/hicolor/$i ]; then
            for j in *; do
                if expr "$j" : "virtualbox\..*" > /dev/null; then
                    dst=apps
                else
                    dst=mimetypes
                fi
                if [ -d /usr/share/icons/hicolor/$i/$dst ]; then
                    ln -s $INSTALLATION_DIR/icons/$i/$j /usr/share/icons/hicolor/$i/$dst/$j
                    echo /usr/share/icons/hicolor/$i/$dst/$j >> $CONFIG_DIR/$CONFIG_FILES
                fi
            done
        fi
        cd -
    done
    cd $cur

    # Update the MIME database
    update-mime-database /usr/share/mime 2>/dev/null

    # Update the desktop database
    update-desktop-database -q 2>/dev/null

    # If Python is available, install Python bindings
    if [ -n "$PYTHON" ]; then
      maybe_run_python_bindings_installer $INSTALLATION_DIR
    fi

    install_device_node_setup "$VBOXDRV_GRP" "$VBOXDRV_MODE" "$INSTALLATION_DIR"

    # Make kernel module
    MODULE_FAILED="false"
    if [ "$BUILD_MODULE" = "true" ]
    then
        info "Building the VirtualBox kernel modules"
        log "Output from the module build process (the Linux kernel build system) follows:"
        cur=`pwd`
        log ""
        ./vboxdrv.sh setup
        # Start VirtualBox kernel module
        if [ $RETVAL -eq 0 ] && ! start_init_script vboxdrv; then
            info "Failed to load the kernel module."
            MODULE_FAILED="true"
            RC_SCRIPT=1
        fi
        start_init_script vboxballoonctrl-service
        start_init_script vboxautostart-service
        start_init_script vboxweb-service
        log ""
        log "End of the output from the Linux kernel build system."
        cd $cur
    fi

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
