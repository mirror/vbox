#!/bin/sh
#
# innotek VirtualBox
# Permanent host interface creation script for Linux systems.

#
# Copyright (C) 2006-2007 innotek GmbH
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

# This script creates a new permanent host interface on a Linux system.  In
# fact, it does two things: it checks to see if the interface is present in
# the list of permanent interfaces (/etc/vbox/interfaces) and checks to see
# whether it can be created using VBoxTunctl.

appname=`basename $0`
interface=$1
user=$2
if [ "$user" = "-g" ]; then
  shift;
  group=$2
  user=+$group
fi
bridge=$3

appadd="VBoxAddIF"
appdel="VBoxDeleteIF"

echo "VirtualBox host networking interface creation utility, version _VERSION_"
echo "(C) 2005-2007 innotek GmbH"
echo "All rights reserved."

# Print out the correct usage instructions for the utility
usage() {
  if [ "$appname" = "$appadd" ]
  then
    echo 1>&2 ""
    echo 1>&2 "Usage: $appname <interface name>"
    echo 1>&2 "                [<user name>| -g <group name>] [<bridge name>]"
    echo 1>&2 "Create and register the permanent interface <interface name> for use by user"
    echo 1>&2 "<user name> (or group <group name> for linux kernels which support this)"
    echo 1>&2 "on the host system.  Optionally attach the interface to the network"
    echo 1>&2 "bridge <bridge name>.  <interface name> should take the form vbox<0-99>."
  elif [ "$appname" = "$appdel" ]
  then
    echo 1>&2 ""
    echo 1>&2 "Usage: $appname <interface name>"
    echo 1>&2 "Delete the permanent interface <interface name> from the host system."
  else
    echo 1>&2 ""
    echo 1>&2 "Your VirtualBox setup appears to be incorrect.  This utility should be called"
    echo 1>&2 "$appadd or $appdel."
  fi
}

valid_ifname() {
  if expr match "$1" "vbox[0-9][0-9]*$" > /dev/null 2>&1
  then
    return 0
  else
    return 1
  fi
}

# Check which name we were called under, and exit if it was not recognised.
if [ ! "$appname" = "$appadd" -a ! "$appname" = "$appdel" ]
then
  usage
  exit 1
fi

# Check that we have the right number of command line arguments
if [ "$appname" = "$appadd" ]
then
  if [ -z "$1" -o -z "$2" -o ! -z "$4" ]
  then
    usage
    exit 1
  fi
elif [ "$appname" = "$appdel" ]
then
  if [ -z "$1" -o ! -z "$2" ]
  then
    usage
    exit 1
  fi
fi
# Check that the interface name is valid if we are adding it.  If we are
# deleting it then the user will get a better error message later if it is
# invalid as the interface will not exist.
if [ "$appname" = "$appadd" ]
then
  if ! valid_ifname "$interface"
  then
    usage
    exit 1
  fi
fi


# Make sure that we can create files in the configuration directory
if [ ! -r /etc/vbox -o ! -w /etc/vbox -o ! -x /etc/vbox ]
then
  echo 1>&2 ""
  echo 1>&2 "This utility must be able to access the folder /etc/vbox/.  Please"
  echo 1>&2 "make sure that you have enough permissions to do this."
  exit 1
fi

# Make sure that the configuration file is accessible and that the interface
# is not already registered.
if [ -f /etc/vbox/interfaces ]
then
  # Make sure that the configuration file is read and writable
  if [ ! -r /etc/vbox/interfaces -o ! -w /etc/vbox/interfaces ]
  then
    echo 1>&2 ""
    echo 1>&2 "This utility must be able to read from and write to the file"
    echo 1>&2 "/etc/vbox/interfaces.  Please make sure that you have enough permissions to"
    echo 1>&2 "do this."
    exit 1
  fi
fi

# Parse the configuration file and create a new, updated one.
oldbridge=""
foundif=""
tempfile=/etc/vbox/interfaces.tmp
rm -f "$tempfile"
if [ -f /etc/vbox/interfaces ]
then
  while read line
  do
    set ""$line
    # If the line is a comment then ignore it
    if (expr match "$1" "#" > /dev/null || test -z "$1")
    then
      echo ""$line >> "$tempfile"
    else
      # Check that the line is correctly formed (an interface name plus one
      # or two non-comment entries, possibly followed by a comment).
      if ((expr match "$2" "#" > /dev/null) ||
          (! test -z "$4" && ! expr match "$4" "#" > /dev/null) ||
          (! valid_ifname "$1"))
      then
        echo 1>&2 ""
        echo 1>&2 "Removing badly formed line $line in /etc/vbox/interfaces."
      # If the interface to be created is already registered in the file, then
      # remove it and remember the fact
      elif [ "$1" = "$interface" ]
      then
        # Remember which bridge the interface was attached to, if any, and
        # do not write the line to the new configuration file.  Remember that
        # we have found the interface in the file.
        foundif=1
        oldbridge="$3"
      else
        echo ""$line >> "$tempfile"
      fi
    fi # The line was not a comment
  done < /etc/vbox/interfaces
else
  # Create the file /etc/vbox/interfaces and add some explanations as comments
  echo "# This file is for registering VirtualBox permanent host networking interfaces" > "$tempfile"
  echo "# and optionally adding them to network bridges on the host." >> "$tempfile"
  echo "# Each line should be of the format <interface name> <user name> [<bridge>]." >> "$tempfile"
  echo "" >> "$tempfile"
fi
mv -f "$tempfile" /etc/vbox/interfaces

# Add the new interface line to the file if so requested
if [ "$appname" = "$appadd" ]
then
  echo "$interface" "$user" "$bridge" >> /etc/vbox/interfaces
  echo ""
  if [ -n "$group" ]; then
      echo "Creating the permanent host networking interface \"$interface\" for group $group."
  else
      echo "Creating the permanent host networking interface \"$interface\" for user $user."
  fi
fi

# Remove the old interface (if it exists) from any bridge it was a part of and
# take the interface down
if [ ! -z "$oldbridge" ]
then
  brctl delif "$oldbridge" "$interface" > /dev/null 2>&1
fi
ifconfig "$interface" down > /dev/null 2>&1

# Delete the old interface if it exists
if ! VBoxTunctl -d "$interface" > /dev/null 2>&1
then
  echo 1>&2 ""
  echo 1>&2 "Failed to take down the old interface in order to replace it with the new one."
  echo 1>&2 "The interface may still be in use, or you may not currently have sufficient"
  echo 1>&2 "permissions to do this.  You can replace the interface manually using the"
  echo 1>&2 "VBoxTunctl command, or alternatively, the new interface will be created"
  echo 1>&2 "automatically next time you restart the host system."
  exit 1
else
  # Create the new interface and bring it up if we are adding it
  if [ "$appname" = "$appadd" ]
  then
    if [ -n "$group" ]; then
      if ! VBoxTunctl -t "$interface" -g "$group" > /dev/null 2>&1
      then
        echo 1>&2 ""
        echo 1>&2 "Failed to create the interface \"$interface\" for group $group.  Please check"
        echo 1>&2 "that you currently have sufficient permissions to do this."
        exit 1
      fi
    else
      if ! VBoxTunctl -t "$interface" -u "$user" > /dev/null 2>&1
      then
        echo 1>&2 ""
        echo 1>&2 "Failed to create the interface \"$interface\" for user $user.  Please check"
        echo 1>&2 "that you currently have sufficient permissions to do this."
        exit 1
      fi
    fi
    # On SUSE Linux Enterprise Server, the tunctl command does not take
    # effect at once, so we loop until it does.
    i=1
    while [ $i -le 10 ]
    do
      ifconfig "$interface" up > /dev/null 2>&1
      if ifconfig | grep "$interface" up > /dev/null 2>&1
      then
        i=11
      else
        i=`expr $i + 1`
        sleep .1
      fi
    done
    if [ ! -z "$bridge" ]
    then
      # And add it to a bridge if this was requested
      if ! brctl addif "$bridge" "$interface" > /dev/null 2>&1
      then
        echo 1>&2 ""
        echo 1>&2 "Failed to add the interface \"$interface\" to the bridge \"$bridge\"."
        echo 1>&2 "Make sure that the bridge exists and that you currently have sufficient"
        echo 1>&2 "permissions to do this."
        exit 1
      fi
    fi
  fi # $appname = $appadd
fi # VBoxTunctl -d succeeded

if [ "$appname" = "$appdel" -a -z "$foundif" ]
then
  echo 1>&2 ""
  echo 1>&2 "Warning: the utility could not find the registered interface \"$interface\"."
  exit 1
fi
