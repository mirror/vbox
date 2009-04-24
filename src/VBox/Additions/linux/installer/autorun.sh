#!/bin/sh
#
# Sun VirtualBox
# VirtualBox Guest Additions installation script for Linux

#
# Copyrign Microsystems, Inc.
#
# Sun Microsystems, Inc.
# All rights reserved
#

PATH=$PATH:/bin:/sbin:/usr/sbin

ostype=`uname -s`
if test "$ostype" != "Linux" && test "$ostype" != "SunOS" ; then
  echo "Linux/Solaris not detected."
  exit 1
fi

# 32-bit or 64-bit?
path=`dirname $0`
case `uname -m` in
  i[3456789]86|x86|i86pc)
    arch='x86'
    ;;
  x86_64|amd64|AMD64)
    arch='amd64'
    ;;
  *)
    echo "Unknown architecture `uname -m`."
    exit 1
    ;;
esac

# execute the installer
if test "$ostype" = "Linux"; then
    if test -f "$path/VBoxLinuxAdditions-$arch.run"; then
      exec gksu /bin/sh "$path/VBoxLinuxAdditions-$arch.run"
    fi

    # else: unknown failure
    echo "Linux guest additions installer not found -- try to start them manually."
    exit 1

elif test "$ostype" = "SunOS"; then

    # check for combined package
    installfile="$path/VBoxSolarisAdditions-amd64.pkg"
    if test -f "$installfile"; then

        # check for pkgadd bin
        pkgaddbin=pkgadd
        found=`which pkgadd | grep "no pkgadd"`
        if test ! -z "$found"; then
            if test -f "/usr/sbin/pkgadd"; then
                pkgaddbin=/usr/sbin/pkgadd
            else
                echo "Could not find pkgadd."
                exit 1
            fi
        fi

        # check for pfexec
        pfexecbin=pfexec
        found=`which pfexec | grep "no pfexec"`
        if test ! -z "$found"; then
            # Use su and prompt for password
            echo "Could not find pfexec."
            subin=`which su`
        else
            # check if pfexec can get the job done
            userid=`$pfexecbin id -u`
            if test $userid != "0"; then
                # pfexec exists but user has no pfexec privileges, switch to using su and prompting password
                subin=`which su`
            fi
        fi

        # create temporary admin file for autoinstall
        echo "basedir=default
runlevel=nocheck
conflict=quit
setuid=nocheck
action=nocheck
partial=quit
instance=unique
idepend=quit
rdepend=quit
space=quit
mail= 
" >> /tmp/vbox.autoinstall

        if test -f "/usr/bin/gnome-terminal"; then
            # use su/pfexec
            if test -z "$subin"; then
                /usr/bin/gnome-terminal --title "Installing VirtualBox Additions" --command "/bin/sh -c '$pfexecbin $pkgaddbin -G -d $installfile -n -a /tmp/vbox.autoinstall SUNWvboxguest; /bin/echo press ENTER to close this window; /bin/read'"
            else
                /usr/bin/gnome-terminal --title "Installing VirtualBox Additions: Root password required." --command "/bin/sh -c '$subin - root -c \"$pkgaddbin -G -d $installfile -n -a /tmp/vbox.autoinstall SUNWvboxguest\"; /bin/echo press ENTER to close this window; /bin/read'"
            fi
        else
            echo "gnome-terminal not found."
        fi

        # remove temporary autoinstall file
        rm -f /tmp/vbox.autoinstall
        exit 0
    fi

    echo "Solaris guest additions installer not found -- try to start them manually."
    exit 1
fi

