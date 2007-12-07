#! /bin/sh
# innotek VirtualBox
# Linux static host networking interface initialization
#

#
# Copyright (C) 2007 innotek GmbH
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

# chkconfig: 35 30 60
# description: VirtualBox permanent host networking setup
#
### BEGIN INIT INFO
# Provides:       vboxnet
# Required-Start: $network
# Required-Stop:
# Default-Start:  3 5
# Default-Stop:
# Description:    VirtualBox permanent host networking setup
### END INIT INFO

PATH=$PATH:/bin:/sbin:/usr/sbin
CONFIG="/etc/vbox/interfaces"
VARDIR="/var/run/VirtualBox"
VARFILE="/var/run/VirtualBox/vboxnet"
TAPDEV="/dev/net/tun"

if [ -f /etc/redhat-release ]; then
    system=redhat
elif [ -f /etc/SuSE-release ]; then
    system=suse
elif [ -f /etc/gentoo-release ]; then
    system=gentoo
else
    system=other
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
    . /sbin/functions.sh
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

fail() {
    if [ "$system" = "gentoo" ]; then
        eerror $1
        exit 1
    fi
    fail_msg
    echo "($1)"
    exit 1
}

running() {
    test -f "$VARFILE"
}

valid_ifname() {
    if expr match "$1" "vbox[0-9][0-9]*$" > /dev/null 2>&1
    then
      return 0
    else
      return 1
    fi
}

# Create all permanent TAP devices registered on the system, add them to a
# bridge if required and keep a record of proceedings in the file
# /var/run/VirtualBox/vboxnet.  If this file already exists, assume that the
# script has already been started and do nothing.
start_network() {
    begin "Starting VirtualBox host networking"
    # If the service is already running, return successfully.
    if [ -f "$VARFILE" ]
    then
      succ_msg
      return 0
    fi
    # Fail if we can't create our runtime record file
    if [ ! -d "$VARDIR" ]
    then
      if ! mkdir "$VARDIR" 2> /dev/null
      then
        fail_msg
        return 1
      fi
    fi
    if ! touch "$VARFILE" 2> /dev/null
    then
      fail_msg
      return 1
    fi
    # If there is no configuration file, report success
    if [ ! -f "$CONFIG" ]
    then
      succ_msg
      return 0
    fi
    # Fail if we can't read our configuration
    if [ ! -r "$CONFIG" ]
    then
      fail_msg
      return 1
    fi
    # Fail if we don't have tunctl
    if ! VBoxTunctl -h 2>&1 | grep VBoxTunctl > /dev/null
    then
      fail_msg
      return 1
    fi
    # Fail if we don't have the kernel tun device
    # Make sure that the tun module is loaded (Ubuntu 7.10 needs this)
    modprobe tun > /dev/null 2>&1
    if ! cat /proc/misc 2>/dev/null | grep tun > /dev/null
    then
      fail_msg
      return 1
    fi
    succ_msg
    # Read the configuration file entries line by line and create the
    # interfaces
    while read line
    do
      set ""$line
      # If the line is a comment then ignore it
      if ((! expr match "$1" "#" > /dev/null) && (! test -z "$1"))
      then
        # Check that the line is correctly formed (an interface name plus one
        # or two non-comment entries, possibly followed by a comment).
        if ((! expr match "$2" "#" > /dev/null) &&
            (test -z "$4" || expr match "$4" "#" > /dev/null) &&
            (valid_ifname "$1"))
        then
          # Try to create the interface
          if VBoxTunctl -t "$1" -u "$2" > /dev/null 2>&1
          then
            # On SUSE Linux Enterprise Server, the interface does not
            # appear immediately, so we loop trying to bring it up.
            i=1
            while [ $i -le 10 ]
            do
              ifconfig "$1" up 2> /dev/null
              if ifconfig | grep "$1" > /dev/null 2>&1
              then
                # Add the interface to a bridge if one was specified
                if [ ! -z "$3" ]
                then
                  if brctl addif "$3" "$1" 2> /dev/null
                  then
                    echo "$1 $2 $3" > "$VARFILE"
                  else
                    echo "$1 $2" > "$VARFILE"
                    echo "Warning - failed to add interface $1 to the bridge $3"
                  fi
                else
                  echo "$1 $2" > $VARFILE
                fi
                i=20
              else
                i=`expr $i + 1`
                sleep .1
              fi
            done
            if [ $i -ne 20 ]
            then
              echo "Warning - failed to bring up the interface $1"
            fi
          else
            echo "Warning - failed to create the interface $1 for the user $2"
          fi
        else
          echo "Warning - invalid line in $CONFIG:"
          echo "  $line"
        fi
      fi
    done < "$CONFIG"
    # Set /dev/net/tun to belong to the group vboxusers if it exists and does
    # yet belong to a group.
    if ls -g "$TAPDEV" 2>/dev/null | grep root 2>&1 > /dev/null
    then
      chgrp vboxusers "$TAPDEV"
      chmod 0660 "$TAPDEV"
    fi
    return 0
}

# Shut down VirtualBox host networking and remove all permanent TAP
# interfaces.  This action will fail if some interfaces could not be removed.
stop_network() {
    begin "Shutting down VirtualBox host networking"
    # If there is no runtime record file, assume that the service is not
    # running.
    if [ ! -f "$VARFILE" ]
    then
      succ_msg
      return 0
    fi
    # Fail if we can't read our runtime record file or write to the
    # folder it is located in
    if [ ! -r "$VARFILE" -o ! -w "$VARDIR" ]
    then
      fail_msg
      return 1
    fi
    # Fail if we don't have tunctl
    if ! VBoxTunctl -h 2>&1 | grep VBoxTunctl > /dev/null
    then
      fail_msg
      return 1
    fi
    # Read the runtime record file entries line by line and delete the
    # interfaces.  The format of the runtime record file is not checked for
    # errors.
    while read line
    do
      set ""$line
      # Remove the interface from a bridge if it is part of one
      if [ ! -z "$3" ]
      then
        brctl delif "$3" "$1" 2> /dev/null
      fi
      # Remove the interface.  Roll back everything and fail if this is not
      # possible
      if (! ifconfig "$1" down 2> /dev/null ||
          ! VBoxTunctl -d "$1" > /dev/null 2>&1)
      then
        while read line
        do
          set ""$line
          VBoxTunctl -t "$1" -u "$2" > /dev/null 2>&1
          ifconfig "$1" up 2> /dev/null
          if [ ! -z "$3" ]
          then
            brctl addif "$3" "$1"
          fi
        done < "$VARFILE"
        fail_msg
        return 1
      fi
    done < "$VARFILE"
    rm -f "$VARFILE" 2> /dev/null
    succ_msg
    return 0
}

# Shut down VirtualBox host networking and remove all permanent TAP
# interfaces.  This action will succeed even if not all interfaces could be
# removed.  It is only intended for exceptional circumstances such as
# uninstalling VirtualBox.
force_stop_network() {
    begin "Shutting down VirtualBox host networking"
    # If there is no runtime record file, assume that the service is not
    # running.
    if [ ! -f "$VARFILE" ]
    then
      succ_msg
      return 0
    fi
    # Fail if we can't read our runtime record file or write to the
    # folder it is located in
    if [ ! -r "$VARFILE" -o ! -w "$VARDIR" ]
    then
      fail_msg
      return 1
    fi
    # Fail if we don't have tunctl
    if ! VBoxTunctl -h 2>&1 | grep VBoxTunctl > /dev/null
    then
      fail_msg
      return 1
    fi
    # Read the runtime record file entries line by line and delete the
    # interfaces.  The format of the runtime record file is not checked for
    # errors.
    while read line
    do
      set ""$line
      # Remove the interface from a bridge if it is part of one
      if [ ! -z "$3" ]
      then
        brctl delif "$3" "$1" 2> /dev/null
      fi
      # Remove the interface.
      ifconfig "$1" down 2> /dev/null
      VBoxTunctl -d "$1" > /dev/null 2>&1
    done < "$VARFILE"
    rm -f "$VARFILE" 2> /dev/null
    succ_msg
    return 0
}

start() {
    start_network
}

stop() {
    stop_network
}

force_stop() {
    force_stop_network
}

restart() {
    stop_network && start_network
}

status() {
    if running; then
        echo "VirtualBox host networking is loaded."
    else
        echo "VirtualBox host networking is not loaded."
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
force-reload)
    restart
    ;;
force-stop)
    force_stop
    ;;
status)
    status
    ;;
*)
    echo "Usage: `basename $0` {start|stop|restart|force-reload|status}"
    exit 1
esac

exit
