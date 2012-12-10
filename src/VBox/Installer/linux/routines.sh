# Oracle VM VirtualBox
# VirtualBox installer shell routines
#

# Copyright (C) 2007-2011 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

ro_LOG_FILE=""
ro_X11_AUTOSTART="/etc/xdg/autostart"
ro_KDE_AUTOSTART="/usr/share/autostart"

# Aborts the script and prints an error message to stderr.
#
# syntax: abort message

abort() {
    echo 1>&2 "$1"
    exit 1
}

# Creates an empty log file and remembers the name for future logging
# operations
#
# syntax: create_log logfile

create_log() {
    ro_LOG_FILE="$1"
    if [ "$ro_LOG_FILE" = "" ]; then
        abort "create_log called without an argument!  Aborting..."
    fi
    # Create an empty file
    echo > "$ro_LOG_FILE" 2> /dev/null
    if [ ! -f "$ro_LOG_FILE" -o "`cat "$ro_LOG_FILE"`" != "" ]; then
        abort "Error creating log file!  Aborting..."
    fi
}

# Writes text to standard error
#
# Syntax: info text

info() {
    echo 1>&2 "$1"
}

# Writes text to the log file
#
# Syntax: log text

log() {
    if [ "$ro_LOG_FILE" = "" ]; then
        abort "Error!  Logging has not been set up yet!  Aborting..."
    fi
    echo "$1" >> $ro_LOG_FILE
    return 0
}

# Writes test to standard output and to the log file
#
# Syntax: infolog text

infolog() {
    info "$1"
    log "$1"
}

# Checks whether a module is loaded with a given string in its name.
#
# syntax: module_loaded string

module_loaded() {
    if [ "$1" = "" ]; then
        log "module_loaded called without an argument.  Aborting..."
        abort "Error in installer.  Aborting..."
    fi
    lsmod | grep -q $1
}

# Abort if we are not running as root

check_root() {
    if [ `id -u` -ne 0 ]; then
        abort "This program must be run with administrator privileges.  Aborting"
    fi
}

# Abort if a copy of VirtualBox is already running

check_running() {
    VBOXSVC_PID=`pidof VBoxSVC 2> /dev/null`
    if [ -n "$VBOXSVC_PID" ]; then
        if [ -f /etc/init.d/vboxweb-service ]; then
            kill -USR1 $VBOXSVC_PID
        fi
        sleep 1
        if pidof VBoxSVC > /dev/null 2>&1; then
            echo 1>&2 "A copy of VirtualBox is currently running.  Please close it and try again."
            abort "Please note that it can take up to ten seconds for VirtualBox to finish running."
        fi
    fi
}

# Do we have bzip2?

check_bzip2() {
    if ! ls /bin/bzip2 /usr/bin/bzip2 /usr/local/bin/bzip2 2> /dev/null | grep bzip2 > /dev/null; then
        echo 1>&2 "Please install the bzip2 utility."
        log "Please install bzip2."
        return 1
    fi
    return 0
}

# Do we have GNU make?

check_gmake() {
make --version 2>&1 | grep GNU > /dev/null
    if [ ! $? = 0 ]; then
        echo 1>&2 "Please install GNU make."
        log "Please install GNU make."
        return 1
    fi
    return 0
}

# Can we find the kernel source?

check_ksource() {
    ro_KBUILD_DIR=/lib/modules/`uname -r`/build
    if [ ! -d $ro_KBUILD_DIR/include ]; then
        ro_KBUILD_DIR=/usr/src/linux
        if [ ! -d $ro_KBUILD_DIR/include ]; then
            echo 1>&2 "Please install the build and header files for your current Linux kernel."
            echo 1>&2 "The current kernel version is `uname -r`"
            ro_KBUILD_DIR=""
            log "Could not find the Linux kernel header files - the directories"
            log "  /lib/modules/`uname -r`/build/include and /usr/src/linux/include"
            log "  do not exist."
            return 1
        fi
    fi
    return 0
}

# Is GCC installed?

check_gcc() {
    ro_gcc_version=`gcc --version 2> /dev/null | head -n 1`
    if [ "$ro_gcc_version" = "" ]; then
        echo 1>&2 "Please install the GNU compiler."
        log "Please install the GNU compiler."
        return 1
    fi # GCC installed
    return 0
}

# Is bash installed?  You never know...

check_bash() {
    if [ ! -x /bin/bash ]; then
        echo 1>&2 "Please install GNU bash."
        log "Please install GNU bash."
        return 1
    fi
    return 0
}

# Is perl installed?

check_perl() {
    if [ ! `perl -e 'print "test"' 2> /dev/null` = "test" ]; then
        echo 1>&2 "Please install perl."
        echo "Please install perl."
        return 1
    fi
    return 0
}

# What type of system are we running on?

check_system_type() {
    if [ ! "$ro_SYS_TYPE" = "" ]; then
        return 0
    elif [ -f /etc/debian_version ]; then
        ro_SYS_TYPE=debian
        ro_INIT_TYPE=sysv
    elif [ -f /etc/gentoo-release ]; then
        ro_SYS_TYPE=gentoo
        ro_INIT_TYPE=sysv
    elif [ -x /sbin/chkconfig ]; then
        ro_SYS_TYPE=redhat
        ro_INIT_TYPE=sysv
    elif [ -x /sbin/insserv ]; then
        ro_SYS_TYPE=suse
        ro_INIT_TYPE=sysv
    elif [ -f /etc/lfs-release -a -d /etc/rc.d/init.d ]; then
        ro_SYS_TYPE=lfs
        ro_INIT_TYPE=lfs
    elif [ -f /etc/pardus-release ]; then
        ro_SYS_TYPE=pardus
        ro_INIT_TYPE=pardus
    elif [ -f /etc/rc.d/rc.local ]; then
        ro_SYS_TYPE=unknown
        ro_INIT_TYPE=bsd
        ro_RC_LOCAL=/etc/rc.d/rc.local
    elif [ -f /etc/rc.local ]; then
        ro_SYS_TYPE=unknown
        ro_INIT_TYPE=bsd
        ro_RC_LOCAL=/etc/rc.local
    elif [ -d /etc/init.d ]; then
        ro_SYS_TYPE=unknown
        ro_INIT_TYPE=sysv
    else  # Perhaps we can determine what we need to know anyway though?
        echo 1>&2 "Unable to determine your Linux distribution"
        log "Unable to determine the Linux distribution"
        return 1
    fi
    return 0
}

# Hack to handle Mandriva's speedboot runlevel
copy_install_script() {
    if [ "$ro_INIT_TYPE" = "sysv" -a -r /etc/sysconfig/speedboot ]; then
        cp "$1" "$2" 2>/dev/null
    else
        sed -e 's/^\(#\s*chkconfig:\s*[0-9]*\)7/\1/' "$1" > "$2"
    fi
}

# Installs a file containing a shell script as an init script
#
# syntax: install_init_script file name
#
# where file is the file to be installed and
# name is the name of the new init script

install_init_script() {
    self="install_init_script"
    script=$1
    name=$2
    pardus_script=$name-pardus.py
    check_system_type
    test $? -ne 1 || return 1
    test -r "$script" || return 1
    test ! "$name" = "" || \
        { log "$self: invalid arguments" && return 1; }
    if [ "$ro_INIT_TYPE" = "pardus" ];then
        test -r "$pardus_script" || \
            { log "$self: Pardus service script missing" && return 1; }
    fi
    if [ "$ro_INIT_TYPE" = "sysv" ]; then
        copy_install_script "$script" "/etc/init.d/$name"
        chmod 755 "/etc/init.d/$name" 2> /dev/null
    elif [ "$ro_INIT_TYPE" = "bsd" ]; then
        copy_install_script "$script" "/etc/rc.d/rc.$name"
        chmod 755 "/etc/rc.d/rc.$name" 2> /dev/null
    elif [ "$ro_INIT_TYPE" = "lfs" ]; then
        copy_install_script "$script" "/etc/rc.d/init.d/$name"
        chmod 755 "/etc/rc.d/init.d/$name" 2> /dev/null
    elif [ "$ro_INIT_TYPE" = "pardus" ]; then
        copy_install_script "$script" "/usr/sbin/$name"
        chmod 755 "/usr/sbin/$name" 2> /dev/null
        hav register $name System.Service $pardus_script
    else
        log "install_init_script: error: unknown init type"
        return 1
    fi
    return 0
}

# Remove the init script "name"
#
# syntax: remove_init_script name

remove_init_script() {
    self="remove_init_script"
    name=$1
    check_system_type
    test $? -ne 1 || return 1
    test ! "$name" = "" || \
        { log "$self: missing argument" && return 1; }
    if [ "$ro_INIT_TYPE" = "sysv" ]; then
        rm -f "/etc/init.d/$name" > /dev/null 2>&1
    elif [ "$ro_INIT_TYPE" = "bsd" ]; then
        rm -f "/etc/rc.d/rc.$name" > /dev/null 2>&1
    elif [ "$ro_INIT_TYPE" = "lfs" ]; then
        rm -f "/etc/rc.d/init.d/$name" > /dev/null 2>&1
    elif [ "$ro_INIT_TYPE" = "pardus" ]; then
        hav remove $name
        rm -f "/usr/sbin/$name" > /dev/null 2>&1
    else
        log "remove_init_script: error: unknown init type"
        return 1
    fi
    return 0
}

# Start the init script "name"
#
# syntax: start_init_script name

start_init_script() {
    self="start_init_script"
    name=$1
    check_system_type
    test $? -ne 1 || return 1
    test ! -z "$name" || \
        { log "$self: missing argument" && return 1; }
    if [ "$ro_INIT_TYPE" = "sysv" ]; then
        "/etc/init.d/$name" start >> $ro_LOG_FILE 2>&1
    elif [ "$ro_INIT_TYPE" = "bsd" ]; then
        "/etc/rc.d/rc.$name" start >> $ro_LOG_FILE  2>&1
    elif [ "$ro_INIT_TYPE" = "lfs" ]; then
        "/etc/rc.d/init.d/$name" start >> $ro_LOG_FILE 2>&1
    elif [ "$ro_INIT_TYPE" = "pardus" ]; then
        service $name on
    else
        log "$self: error: unknown init type"
        return 1
    fi
}

# Stop the init script "name"
#
# syntax: stop_init_script name

stop_init_script() {
    self=stop_init_script
    name=$1
    check_system_type
    test $? -ne 1 || return 1
    test ! -z "$name" || \
        { log "$self: missing argument" && return 1; }
    if [ "$ro_INIT_TYPE" = "sysv" ]; then
        "/etc/init.d/$name" stop >> $ro_LOG_FILE 2>&1
    elif [ "$ro_INIT_TYPE" = "bsd" ]; then
        "/etc/rc.d/rc.$name" stop >> $ro_LOG_FILE 2>&1
    elif [ "$ro_INIT_TYPE" = "lfs" ]; then
        "/etc/rc.d/init.d/$name" stop >> $ro_LOG_FILE 2>&1
    elif [ "$ro_INIT_TYPE" = "pardus" ]; then
        service $name off
    else
        log "$self: error: unknown init type"
        return 1
    fi
    return 0
}

# Add a service to a runlevel
#
# syntax: addrunlevel name start_order stop_order

addrunlevel() {
    test ! -z "$1" || \
        { log "addrunlevel: missing argument(s)" && return 1; }
    check_system_type
    # Redhat based systems
    if [ "$ro_SYS_TYPE" = "redhat" ]
    then
        test -x "/sbin/chkconfig" || \
            { log "addrunlevel: /sbin/chkconfig not found" && return 1; }
        /sbin/chkconfig --del $1 > /dev/null 2>&1

        if /sbin/chkconfig -v > /dev/null 2>&1; then
            /sbin/chkconfig --level 35 $1 on || {
                log "Cannot add $1 to run levels: 35" && return 1
            }
        else
            /sbin/chkconfig $1 35 || {
                log "Cannot add $1 to run levels: 35" && return 1
            }
        fi
    # SUSE-base systems
    elif [ "$ro_SYS_TYPE" = "suse" ]
    then
        test -x /sbin/insserv || {
            log "addrunlevel: insserv not found" && return 1;
        }
        /sbin/insserv -r $1 > /dev/null 2>&1
        /sbin/insserv $1 > /dev/null
    # Debian/Ubuntu-based systems
    elif [ "$ro_SYS_TYPE" = "debian" ]; then
        test -x `which update-rc.d` || \
            { log "addrunlevel: update-rc.d not found" && return 1; }
        test ! -z "$2" || \
            { log "addrunlevel: missing second argument" && return 1; }
        test ! -z "$3" || \
            { log "addrunlevel: missing third argument" && return 1; }
        # Debian does not support dependencies currently -- use argument $2
        # for start sequence number and argument $3 for stop sequence number
        update-rc.d -f $1 remove > /dev/null 2>&1
        update-rc.d $1 defaults $2 $3 > /dev/null 2>&1
    # Gentoo Linux
    elif [ "$ro_SYS_TYPE" = "gentoo" ]; then
        test -x `which rc-update` || \
            { log "addrunlevel: update-rc.d not found" && return 1; }
        rc-update del $1 > /dev/null 2>&1
        rc-update add $1 default > /dev/null 2>&1
    # Linux from scratch, by the book
    elif [ "$ro_SYS_TYPE" = "lfs" ]; then
        test -x /etc/rc.d/init.d/$1 || \
            { log "addrunlevel: name argument must be a script in /etc/rc.d/init.d" && return 1; }
        P2=$2
        P3=$3
        # add leading zero
        if [ `expr length "$P2"` -eq 1 ]; then
            P2=`expr 0$P2`
        fi
        if [ `expr length "$P3"` -eq 1 ]; then
            P3=`expr 0$P3`
        fi
        expr "$P2" + 0 > /dev/null 2>&1 && expr 0 \<= "$P2" > /dev/null && \
            [ `expr length "$P2"` -eq 2 ] || \
            { log "addrunlevel: start sequence number must be between 00 and 99" && return 1; }
        expr "$P3" + 0 > /dev/null 2>&1 && expr 0 \<= "$P3" > /dev/null && \
            [ `expr length "$P3"` -eq 2 ] || \
            { log "addrunlevel: stop sequence number must be between 00 and 99" && return 1; }
        ln -fs "/etc/rc.d/init.d/$1" "/etc/rc.d/rc0.d/K`expr $P3`$1" > /dev/null 2>&1
        ln -fs "/etc/rc.d/init.d/$1" "/etc/rc.d/rc1.d/K`expr $P3`$1" > /dev/null 2>&1
        ln -fs "/etc/rc.d/init.d/$1" "/etc/rc.d/rc2.d/S`expr $P2`$1" > /dev/null 2>&1
        ln -fs "/etc/rc.d/init.d/$1" "/etc/rc.d/rc3.d/S`expr $P2`$1" > /dev/null 2>&1
        ln -fs "/etc/rc.d/init.d/$1" "/etc/rc.d/rc4.d/S`expr $P2`$1" > /dev/null 2>&1
        ln -fs "/etc/rc.d/init.d/$1" "/etc/rc.d/rc5.d/S`expr $P2`$1" > /dev/null 2>&1
        ln -fs "/etc/rc.d/init.d/$1" "/etc/rc.d/rc6.d/K`expr $P3`$1" > /dev/null 2>&1
    # BSD-based systems require changing the rc.local file to start a new service.
    elif [ "$ro_INIT_TYPE" = "bsd" ]; then
        if ! grep -q $1 $ro_RC_LOCAL
        then
            echo "# Start $1" >> $ro_RC_LOCAL
            echo "# If you do not wish this to be executed here then comment it out," >> $ro_RC_LOCAL
            echo "# and the installer will skip it next time." >> $ro_RC_LOCAL
            echo "if [ -x /etc/rc.d/rc.$1 ]; then" >> $ro_RC_LOCAL
            echo "    /etc/rc.d/rc.$1 start" >> $ro_RC_LOCAL
            echo "fi" >> $ro_RC_LOCAL
            echo "" >> $ro_RC_LOCAL
        fi
    # Probably most unknown Linux systems will be sysv type ones.  These can theoretically
    # be handled automatically if people give us information about them.
    elif [ "$ro_INIT_TYPE" = "sysv" ]; then
        echo 1>&2 "As our installer does not recognize your Linux distribution, we were unable to"
        echo 1>&2 "set up the initialization script $1 correctly.  The script has been copied"
        echo 1>&2 "copied to the /etc/init.d directory.  You should set up your system to start"
        echo 1>&2 "it at system start, or start it manually before using VirtualBox."
        echo 1>&2 ""
        echo 1>&2 "If you would like to help us add support for your distribution, please open a"
        echo 1>&2 "new ticket on http://www.virtualbox.org/wiki/Bugtracker."
    fi
    return 0
}


# Delete a service from a runlevel
#
# syntax: delrunlevel name

delrunlevel() {
    test ! -z "$1" || \
        { log "delrunlevel: missing argument" && return 1; }
    check_system_type
    # Redhat-based systems
    if [ "$ro_SYS_TYPE" = "redhat" ]
    then
        test -x "/sbin/chkconfig" || \
            { log "delrunlevel: /sbin/chkconfig not found" && return 1; }
        if /sbin/chkconfig --list $1 > /dev/null 2>&1; then
            /sbin/chkconfig --del $1 > /dev/null 2>&1 || {
                log "Cannot delete $1 from runlevels" && return 1
            }
        fi
    # SUSE-based systems
    elif [ "$ro_SYS_TYPE" = "suse" ]
    then
        test -x /sbin/insserv || {
            log "delrunlevel: insserv not found" && return 1;
        }
        /sbin/insserv -r $1 > /dev/null 2>&1
    # Debian/Ubuntu-based systems
    elif [ "$ro_SYS_TYPE" = "debian" ]; then
        test -x `which update-rc.d` || \
            { log "delrunlevel: update-rc.d not found" && return 1; }
        update-rc.d -f $1 remove > /dev/null 2>&1
    # Gentoo Linux
    elif [ "$ro_SYS_TYPE" = "gentoo" ]; then
        test -x `which rc-update` || \
            { log "delrunlevel: update-rc.d not found" && return 1; }
        rc-update del "$1" > /dev/null 2>&1
    # Linux from scratch, by the book
    elif [ "$ro_SYS_TYPE" = "lfs" ]; then
        rm "/etc/rc0.d/K??$1" > /dev/null 2>&1
        rm "/etc/rc1.d/K??$1" > /dev/null 2>&1
        rm "/etc/rc2.d/S??$1" > /dev/null 2>&1
        rm "/etc/rc3.d/S??$1" > /dev/null 2>&1
        rm "/etc/rc4.d/S??$1" > /dev/null 2>&1
        rm "/etc/rc5.d/S??$1" > /dev/null 2>&1
        rm "/etc/rc6.d/K??$1" > /dev/null 2>&1
    # Unknown sysv-type system
    elif [ "$ro_INIT_TYPE" = "sysv" ]; then
        echo 1>&2 "Please remove remove references to the initialization script"
        echo 1>&2 "/etc/init.d/$1 to complete the uninstallation."
    fi
    # BSD-type systems will just not start the script if it is not there.
    # Assume that BSD users understand their system.
    return 0
}

# Do initial setup of an installed service
#
# syntax: setup_init_script name

setup_init_script()
{
    self=setup_init_script
    name=$1
    spaces=`printf " %b" "\t"`
    check_system_type
    test $? -ne 1 || return 1
    test ! -z "$name" ||
        { log "$self: missing argument" && return 1; }
    if [ "$ro_INIT_TYPE" = "sysv" ]; then
        scriptname="/etc/init.d/$name"
    elif [ "$ro_INIT_TYPE" = "bsd" ]; then
        scriptname="/etc/rc.d/rc.$name"
    elif [ "$ro_INIT_TYPE" = "lfs" ]; then
        scriptname="/etc/rc.d/init.d/$name"
    elif [ "$ro_INIT_TYPE" = "pardus" ]; then
        scriptname="/usr/sbin/$name"
    else
        log "$self: error: unknown init type"
        return 1
    fi
    # Add the init script to the default runlevel
    # This is complicated by the fact that we need to pass older Debian-based
    # systems the start and stop order numbers, which we extract from the
    # Redhat chkconfig information.
    if test "$ro_INIT_TYPE" = "sysv" -a -r "$scriptname"; then
        orders=`grep '^#['"$spaces"']*chkconfig:' "$scriptname" |
            sed -e 's/^#['"$spaces"']*chkconfig:['"$spaces"']*[0-9]*['"$spaces"']*//'`
        expr "$orders" : '.*[0-9][0-9]*['"$spaces"']['"$spaces"']*[0-9][0-9]*$' > /dev/null ||
            {
                log "$self: bad or missing chkconfig line in init script $scriptname"
                return 1
            }
        # $orders is deliberately not quoted here
        addrunlevel "$name" $orders
    else
        addrunlevel "$name"
    fi
    test -r "$scriptname" &&
        grep -q '^#['"$spaces"']*setup_script['"$spaces"']*$' "$scriptname" &&
        "$scriptname" setup
    return 0
}

# Do pre-removal cleanup of an installed service
#
# syntax: cleanup_init_script name

cleanup_init()
{
    self=cleanup_init_script
    name=$1
    spaces=`printf " %b" "\t"`
    check_system_type
    test $? -ne 1 || return 1
    test ! -z "$name" || \
        { log "$self: missing argument" && return 1; }
    if [ "$ro_INIT_TYPE" = "sysv" ]; then
        scriptname="/etc/init.d/$name"
    elif [ "$ro_INIT_TYPE" = "bsd" ]; then
        scriptname="/etc/rc.d/rc.$name"
    elif [ "$ro_INIT_TYPE" = "lfs" ]; then
        scriptname="/etc/rc.d/init.d/$name"
    elif [ "$ro_INIT_TYPE" = "pardus" ]; then
        scriptname="/usr/sbin/$name"
    else
        log "$self: error: unknown init type"
        return 1
    fi
    test -r "$scriptname" &&
        grep -q '^#['"$spaces"']*cleanup_script['"$spaces"']*$' "$scriptname" &&
        "$scriptname" cleanup >> $ro_LOG_FILE 2>&1
    delrunlevel "$name"
    return 0
}


terminate_proc() {
    PROC_NAME=$1
    SERVER_PID=`pidof $PROC_NAME 2> /dev/null`
    if [ "$SERVER_PID" != "" ]; then
        killall -TERM $PROC_NAME > /dev/null 2>&1
        sleep 2
    fi
}


maybe_run_python_bindings_installer() {
    VBOX_INSTALL_PATH=$1

    PYTHON=python
    if [ ! `python -c 'print "test"' 2> /dev/null` = "test" ]; then
        echo  1>&2 "Python not available, skipping bindings installation."
        return 1
    fi

    echo  1>&2 "Python found: $PYTHON, installing bindings..."
    # Pass install path via environment
    export VBOX_INSTALL_PATH
    $SHELL -c "cd $VBOX_INSTALL_PATH/sdk/installer && $PYTHON vboxapisetup.py install"
    # remove files created during build
    rm -rf $VBOX_INSTALL_PATH/sdk/installer/build

    return 0
}

install_autologon() {
    info "Installing auto-logon support ..."

    ## Parameters:
    # Greeter directory. Defaults to /usr/share/xgreeters.
    greeter_dir="$1"
    # LightDM config. Defaults to /etc/lightdm/lightdm.conf.
    lightdm_config="$2"
    # Whether to force installation if non-compatible distribution
    # is detected.
    force="$3"
    
    # Check for Ubuntu and derivates. @todo Debian?
    distros="Ubuntu UbuntuStudio Edubuntu Kubuntu Lubuntu Mythbuntu Xubuntu"
    ## @todo Map Linux Mint versions to Ubuntu ones.

    ## @todo Move the distro check to a routine / globals as soon as
    ##       we have other distribution-dependent stuff.
    which lsb_release &>/dev/null
    if test "$?" -ne "0"; then
        info "Error: lsb_release not found (path set?), skipping auto-logon installation"
        return 1
    fi
    distro_name=$(lsb_release -si)
    distro_ver=$(lsb_release -sr)

    for distro_cur in ${distros}; do
        if test "$distro_name" = "$distro_cur"; then
            distro_found="true"
            break
        fi
    done

    if test -z "$distro_found"; then
        if ! test "$force" = "force"; then
            info "Error: Unsupported distribution \"$distro_name\" found, skipping auto-logon installation"
            return 1
        fi
        info "Warning: Unsupported distribution \"$distro_name\" found"
    else
        # Do we have Ubuntu 11.10 or greater?
        # Use AWK for comparison since we run on plan sh.
        echo | awk 'END { exit ( !('"$distro_ver >= 11.10"') ); }'
        if test "$?" -ne "0"; then
            if ! test "$force" = "force"; then
                info "Error: Version $distro_ver of \"$distro_name\" not supported, skipping auto-logon installation"
                return 1
            fi
            info "Warning: Unsupported \"$distro_name\" version $distro_ver found"
        fi
    fi

    # Install dependencies (lightdm and FLTK 1.3+) using apt-get.
    which apt-get &>/dev/null
    if test "$?" -ne "0"; then
        info "Error: apt-get not found (path set?), skipping auto-logon installation"
        return 1
    fi
    info "Checking and installing necessary dependencies ..."
    apt-get -qqq -y install libfltk1.3 libfltk-images1.3 || return 1
    apt-get -qqq -y install lightdm || return 1

    # Check for LightDM config.
    if ! test -f "$lightdm_config"; then
        info "Error: LightDM config \"$lightdm_config\" not found (LightDM installed?), skipping auto-logon installation"
        return 1
    fi 

    # Check for /usr/share/xgreeters.
    if ! test -d "$greeter_dir"; then
        if ! test "$force" = "force"; then
            info "Error: Directory \"$greeter_dir\" does not exist, skipping auto-logon installation"
            return 1
        fi
        info "Warning: Directory \"$greeter_dir\" does not exist, creating it"
        mkdir -p -m 755 "$greeter_dir" || return 1
    fi
    
    # Link to required greeter files into $greeter_dir.
    add_symlink "$INSTALLATION_DIR/share/VBoxGuestAdditions/vbox-greeter.desktop" "$greeter_dir/vbox-greeter.desktop"

    # Backup and activate greeter config.
    if ! test -f "$lightdm_config.vbox-backup"; then
        info "Backing up LightDM configuration file ..."
        cp "$lightdm_config" "$lightdm_config.vbox-backup" || return 1
        chmod 644 "$lightdm_config.vbox-backup" || return 1
    fi
    sed -i -e 's/^\s*greeter-session\s*=/greeter-sessio**n=vbox-greeter/g' "$lightdm_config" || return 1
    chmod 644 "$lightdm_config" || return 1

    info "Auto-logon installation successful"
    return 0
}

remove_autologon() {
    if test -z "$LIGHTDM_CONFIG"; then
        return 0
    fi
    info "Un-installing auto-logon support ..."

    # Switch back to original greeter.
    if test -f "$LIGHTDM_CONFIG.vbox-backup"; then
        mv "$LIGHTDM_CONFIG.vbox-backup" "$LIGHTDM_CONFIG"
        if test "$?" -ne "0"; then
            info "Warning: Could not restore original LightDM config \"$LIGHTDM_CONFIG\""
        fi 
    fi

    # Remove greeter directory (if not empty).
    rm "$LIGHTDM_GREETER_DIR" 2>/dev/null

    return 0
}

