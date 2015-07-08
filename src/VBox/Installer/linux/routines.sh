# Oracle VM VirtualBox
# VirtualBox installer shell routines
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

ro_LOG_FILE=""
ro_X11_AUTOSTART="/etc/xdg/autostart"
ro_KDE_AUTOSTART="/usr/share/autostart"

## Aborts the script and prints an error message to stderr.
#
# syntax: abort message

abort()
{
    echo 1>&2 "$1"
    exit 1
}

## Creates an empty log file and remembers the name for future logging
# operations
create_log()
{
    ## The path of the file to create.
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

## Writes text to standard error
#
# Syntax: info text
info()
{
    echo 1>&2 "$1"
}

## Writes text to the log file
#
# Syntax: log text
log()
{
    if [ "$ro_LOG_FILE" = "" ]; then
        abort "Error!  Logging has not been set up yet!  Aborting..."
    fi
    echo "$1" >> $ro_LOG_FILE
    return 0
}

## Writes test to standard output and to the log file
#
# Syntax: infolog text
infolog()
{
    info "$1"
    log "$1"
}

## Checks whether a module is loaded with a given string in its name.
#
# syntax: module_loaded string
module_loaded()
{
    if [ "$1" = "" ]; then
        log "module_loaded called without an argument.  Aborting..."
        abort "Error in installer.  Aborting..."
    fi
    lsmod | grep -q $1
}

## Abort if we are not running as root
check_root()
{
    if [ `id -u` -ne 0 ]; then
        abort "This program must be run with administrator privileges.  Aborting"
    fi
}

## Abort if a copy of VirtualBox is already running
check_running()
{
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

## Do we have bzip2?
check_bzip2()
{
    if ! ls /bin/bzip2 /usr/bin/bzip2 /usr/local/bin/bzip2 2> /dev/null | grep bzip2 > /dev/null; then
        echo 1>&2 "Please install the bzip2 utility."
        log "Please install bzip2."
        return 1
    fi
    return 0
}

## Do we have GNU make?
check_gmake()
{
make --version 2>&1 | grep GNU > /dev/null
    if [ ! $? = 0 ]; then
        echo 1>&2 "Please install GNU make."
        log "Please install GNU make."
        return 1
    fi
    return 0
}

## Can we find the kernel source?
check_ksource()
{
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

## Is GCC installed?
check_gcc()
{
    ro_gcc_version=`gcc --version 2> /dev/null | head -n 1`
    if [ "$ro_gcc_version" = "" ]; then
        echo 1>&2 "Please install the GNU compiler."
        log "Please install the GNU compiler."
        return 1
    fi # GCC installed
    return 0
}

## Is bash installed?  You never know...
check_bash()
{
    if [ ! -x /bin/bash ]; then
        echo 1>&2 "Please install GNU bash."
        log "Please install GNU bash."
        return 1
    fi
    return 0
}

## Is perl installed?
check_perl()
{
    if [ ! `perl -e 'print "test"' 2> /dev/null` = "test" ]; then
        echo 1>&2 "Please install perl."
        echo "Please install perl."
        return 1
    fi
    return 0
}

## Creates a systemd wrapper in /lib for an LSB init script
systemd_wrap_init_script()
{
    self="systemd_wrap_init_script"
    ## The init script to be installed.  The file may be copied or referenced.
    script="$(readlink -f -- "${1}")"
    ## Name for the service.
    name="$2"
    test -x "$script" && test ! "$name" = "" || \
        { log "$self: invalid arguments" && return 1; }
    test -d /usr/lib/systemd/system && unit_path=/usr/lib/systemd/system
    test -d /lib/systemd/system && unit_path=/lib/systemd/system
    test -n "${unit_path}" || \
        { log "$self: systemd unit path not found" && return 1; }
    description=`sed -n 's/# *Description: *\(.*\)/\1/p' "${script}"`
    required=`sed -n 's/# *Required-Start: *\(.*\)/\1/p' "${script}"`
    runlevels=`sed -n 's/# *Default-Start: *\(.*\)/\1/p' "${script}"`
    before=`for i in ${runlevels}; do printf "runlevel${i}.target "; done`
    after=`for i in ${required}; do printf "${i}.service "; done`
    cat > "${unit_path}/${name}.service" << EOF
[Unit]
SourcePath=${script}
Description=${description}
Before=${before}shutdown.target
After=${after}
Conflicts=shutdown.target

[Service]
Type=forking
Restart=no
TimeoutSec=5min
IgnoreSIGPIPE=no
KillMode=process
GuessMainPID=no
RemainAfterExit=yes
ExecStart=${script} start
ExecStop=${script} stop

[Install]
WantedBy=multi-user.target
EOF
}

## Installs a file containing a shell script as an init script
install_init_script()
{
    self="install_init_script"
    ## The init script to be installed.  The file may be copied or referenced.
    script="$1"
    ## Name for the service.
    name="$2"
    test -x "$script" && test ! "$name" = "" || \
        { log "$self: invalid arguments" && return 1; }
    test -d /lib/systemd/system || test -d /usr/lib/systemd/system && systemd_wrap_init_script "$script" "$name"
    if test -d /etc/rc.d/init.d
    then
        cp "$script" "/etc/rc.d/init.d/$name" 2> /dev/null
        chmod 755 "/etc/rc.d/init.d/$name" 2> /dev/null
    elif test -d /etc/init.d
    then
        cp "$script" "/etc/init.d/$name" 2> /dev/null
        chmod 755 "/etc/init.d/$name" 2> /dev/null
    fi
    return 0
}

## Remove the init script "name"
remove_init_script()
{
    self="remove_init_script"
    ## Name of the service to remove.
    name="$1"
    test ! "$name" = "" || \
        { log "$self: missing argument" && return 1; }
    rm -f /lib/systemd/system/"$name".service /usr/lib/systemd/system/"$name".service
    if test -d /etc/rc.d/init.d
    then
        rm -f "/etc/rc.d/init.d/$name" > /dev/null 2>&1
    elif test -d /etc/init.d
    then
        rm -f "/etc/init.d/$name" > /dev/null 2>&1
    fi
    return 0
}

## Perform an action on a service
do_sysvinit_action()
{
    self="do_sysvinit_action"
    ## Name of service to start.
    name="${1}"
    ## The action to perform, normally "start", "stop" or "status".
    action="${2}"
    test ! -z "${name}" && test ! -z "${action}" || \
        { log "${self}: missing argument" && return 1; }
    if test -x "`which systemctl 2>/dev/null`"
    then
        systemctl ${action} "${name}"
    elif test -x "`which service 2>/dev/null`"
    then
        service "${name}" ${action}
    elif test -x "`which invoke-rc.d 2>/dev/null`"
    then
        invoke-rc.d "${name}" ${action}
    elif test -x "/etc/rc.d/init.d/${name}"
    then
        "/etc/rc.d/init.d/${name}" "${action}"
    elif test -x "/etc/init.d/${name}"
    then
        "/etc/init.d/${name}" "${action}"
    fi
}

## Start a service
start_init_script()
{
    do_sysvinit_action "${1}" start
}

## Stop the init script "name"
stop_init_script()
{
    do_sysvinit_action "${1}" stop
}

## Extract chkconfig information from a sysvinit script.
get_chkconfig_info()
{
    ## The script to extract the information from.
    script="${1}"
    set `sed -n 's/# *chkconfig: *\([0-9]*\) *\(.*\)/\1 \2/p' "${script}"`
    ## Which runlevels should we start in?
    runlevels="${1}"
    ## How soon in the boot process will we start, from 00 (first) to 99
    start_order="${2}"
    ## How soon in the shutdown process will we stop, from 99 (first) to 00
    stop_order="${3}"
    test ! -z "${name}" || \
        { log "${self}: missing name" && return 1; }
    expr "${start_order}" + 0 > /dev/null 2>&1 && \
        expr 0 \<= "${start_order}" > /dev/null 2>&1 && \
        test `expr length "${start_order}"` -eq 2 > /dev/null 2>&1 || \
        { log "${self}: start sequence number must be between 00 and 99" && return 1; }
    expr "${stop_order}" + 0 > /dev/null 2>&1 && \
        expr 0 \<= "${stop_order}" > /dev/null 2>&1 && \
        test `expr length "${stop_order}"` -eq 2 > /dev/null 2>&1 || \
        { log "${self}: stop sequence number must be between 00 and 99" && return 1; }
    return 0
}

## Add a service to a runlevel
addrunlevel()
{
    self="addrunlevel"
    ## Service name.
    name="${1}"
    test -n "${name}" || \
        { log "${self}: missing argument" && return 1; }
    test -x "`which systemctl 2>/dev/null`" && \
        { systemctl enable "${name}"; return; }
    if test -x "/etc/rc.d/init.d/${name}"
    then
        init_d_path=/etc/rc.d
    elif test -x "/etc/init.d/${name}"
    then
        init_d_path=/etc
    else
        log "${self}: error: unknown init type or unknown service ${name}"
        return 1
    fi
    get_chkconfig_info "${init_d_path}/init.d/${name}" || return 1
    # Redhat based sysvinit systems
    if test -x "`which chkconfig 2>/dev/null`"
    then
        chkconfig --add "${name}" || {
            log "Failed to set up init script ${name}" && return 1
        }
    # SUSE-based sysvinit systems
    elif test -x "`which insserv 2>/dev/null`"
    then
        insserv "${name}" > /dev/null
    # Debian/Ubuntu-based systems
    elif test -x "`which update-rc.d 2>/dev/null`"
    then
        # Old Debians did not support dependencies
        update-rc.d "${name}" defaults "${start_order}" "${stop_order}" \
            > /dev/null 2>&1
    # Gentoo Linux
    elif test -x "`which rc-update 2>/dev/null`"; then
        rc-update add "${name}" default > /dev/null 2>&1
    # Generic sysvinit
    elif test -n "${init_d_path}/rc0.d"
    then
        for locali in 0 1 2 3 4 5 6
        do
            target="${init_d_path}/rc${locali}.d/K${stop_order}${name}"
            expr "${runlevels}" : ".*${locali}" >/dev/null && \
                target="${init_d_path}/rc${locali}.d/S${start_order}${name}"
            test -e "${init_d_path}/rc${locali}.d/"[KS][0-9]*"${name}" || \
                ln -fs "${init_d_path}/init.d/${name}" "${target}" 2> /dev/null
        done
    else
        log "${self}: error: unknown init type"
        return 1
    fi
    return 0
}


## Delete a service from a runlevel
delrunlevel()
{
    self="delrunlevel"
    ## Service name.
    name="${1}"
    test -n "${name}" || \
        { log "${self}: missing argument" && return 1; }
    if test -x "`which systemctl 2>/dev/null`"
    then
        systemctl disable "${name}"
    # Redhat-based systems
    elif test -x "/sbin/chkconfig"
    then
        /sbin/chkconfig --del "${name}" > /dev/null 2>&1
    # SUSE-based sysvinit systems
    elif test -x /sbin/insserv
    then
        /sbin/insserv -r "${name}" > /dev/null 2>&1
    # Debian/Ubuntu-based systems
    elif test -x "`which update-rc.d 2>/dev/null`"
    then
        update-rc.d -f "${name}" remove > /dev/null 2>&1
    # Gentoo Linux
    elif test -x "`which rc-update 2>/dev/null`"
    then
        rc-update del "${name}" > /dev/null 2>&1
    # Generic sysvinit
    elif test -d /etc/rc.d/init.d
    then
        rm /etc/rc.d/rc?.d/[SK]??"${name}" > /dev/null 2>&1
    elif test -d /etc/init.d
    then
        rm /etc/rc?.d/[SK]??"${name}" > /dev/null 2>&1
    else
        log "${self}: error: unknown init type"
        return 1
    fi
    return 0
}


terminate_proc() {
    PROC_NAME="${1}"
    SERVER_PID=`pidof $PROC_NAME 2> /dev/null`
    if [ "$SERVER_PID" != "" ]; then
        killall -TERM $PROC_NAME > /dev/null 2>&1
        sleep 2
    fi
}


maybe_run_python_bindings_installer() {
    VBOX_INSTALL_PATH="${1}"

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
