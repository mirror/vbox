#
# Spec file for creating VirtualBox rpm packages
#

#
# Copyright (C) 2006-2010 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License as published by the Free Software Foundation,
# in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
# distribution. VirtualBox OSE is distributed in the hope that it will
# be useful, but WITHOUT ANY WARRANTY of any kind.
#

%define %SPEC% 1
%{!?python_sitelib: %define python_sitelib %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib()")}

Summary:   Oracle VM VirtualBox
Name:      %NAME%
Version:   %BUILDVER%_%BUILDREL%
Release:   1
URL:       http://www.virtualbox.org/
Source:    VirtualBox.tar.bz2
License:   VirtualBox Personal Use and Evaluation License (PUEL)
Group:     Applications/System
Vendor:    Oracle Corporation
BuildRoot: %BUILDROOT%
Requires:  %LIBASOUND%

%if %{?rpm_suse:1}%{!?rpm_suse:0}
%debug_package
%endif

%MACROSPYTHON%


%description
VirtualBox is a powerful PC virtualization solution allowing
you to run a wide range of PC operating systems on your Linux
system. This includes Windows, Linux, FreeBSD, DOS, OpenBSD
and others. VirtualBox comes with a broad feature set and
excellent performance, making it the premier virtualization
software solution on the market.


%prep
%setup -q
DESTDIR=""
unset DESTDIR


%build


%install
# Mandriva: prevent replacing 'echo' by 'gprintf'
export DONT_GPRINTIFY=1
rm -rf $RPM_BUILD_ROOT
install -m 755 -d $RPM_BUILD_ROOT/sbin
install -m 755 -d $RPM_BUILD_ROOT%{_initrddir}
install -m 755 -d $RPM_BUILD_ROOT/lib/modules
install -m 755 -d $RPM_BUILD_ROOT/etc/vbox
install -m 755 -d $RPM_BUILD_ROOT/usr/bin
install -m 755 -d $RPM_BUILD_ROOT/usr/src
install -m 755 -d $RPM_BUILD_ROOT/usr/share/applications
install -m 755 -d $RPM_BUILD_ROOT/usr/share/pixmaps
install -m 755 -d $RPM_BUILD_ROOT%{_defaultdocdir}/virtualbox
install -m 755 -d $RPM_BUILD_ROOT/usr/lib/virtualbox
install -m 755 -d $RPM_BUILD_ROOT/usr/share/virtualbox
mv VBoxEFI32.fd $RPM_BUILD_ROOT/usr/lib/virtualbox || true
mv VBoxEFI64.fd $RPM_BUILD_ROOT/usr/lib/virtualbox || true
mv *.gc $RPM_BUILD_ROOT/usr/lib/virtualbox
mv *.r0 $RPM_BUILD_ROOT/usr/lib/virtualbox
mv *.rel $RPM_BUILD_ROOT/usr/lib/virtualbox || true
mv VBoxNetDHCP $RPM_BUILD_ROOT/usr/lib/virtualbox
mv VBoxNetAdpCtl $RPM_BUILD_ROOT/usr/lib/virtualbox
mv VBoxXPCOMIPCD $RPM_BUILD_ROOT/usr/lib/virtualbox
mv components $RPM_BUILD_ROOT/usr/lib/virtualbox/components
mv *.so $RPM_BUILD_ROOT/usr/lib/virtualbox
mv *.so.4 $RPM_BUILD_ROOT/usr/lib/virtualbox || true
mv VBoxTestOGL $RPM_BUILD_ROOT/usr/lib/virtualbox
mv vboxshell.py $RPM_BUILD_ROOT/usr/lib/virtualbox
(export VBOX_INSTALL_PATH=/usr/lib/virtualbox && \
  cd ./sdk/installer && \
  %{__python} ./vboxapisetup.py install --prefix %{_prefix} --root $RPM_BUILD_ROOT)
rm -rf sdk/installer
mv sdk $RPM_BUILD_ROOT/usr/lib/virtualbox
mv nls $RPM_BUILD_ROOT/usr/share/virtualbox
cp -a src $RPM_BUILD_ROOT/usr/share/virtualbox
mv VBox.sh $RPM_BUILD_ROOT/usr/bin/VBox
mv VBoxSysInfo.sh $RPM_BUILD_ROOT/usr/share/virtualbox
for i in VBoxManage VBoxSVC VBoxSDL VirtualBox VBoxHeadless vboxwebsrv webtest; do
  mv $i $RPM_BUILD_ROOT/usr/lib/virtualbox; done
for i in VBoxSDL VirtualBox VBoxHeadless VBoxNetDHCP VBoxNetAdpCtl; do
  chmod 4511 $RPM_BUILD_ROOT/usr/lib/virtualbox/$i; done
mv VBoxTunctl $RPM_BUILD_ROOT/usr/bin
for d in /lib/modules/*; do
  if [ -L $d/build ]; then
    rm -f /tmp/vboxdrv-Module.symvers
    ./src/vboxdrv/build_in_tmp \
      --save-module-symvers /tmp/vboxdrv-Module.symvers \
      KBUILD_VERBOSE= KERN_DIR=$d/build MODULE_DIR=$RPM_BUILD_ROOT/$d/misc -j4 \
      %INSTMOD%
    ./src/vboxnetflt/build_in_tmp \
      --use-module-symvers /tmp/vboxdrv-Module.symvers \
      KBUILD_VERBOSE= KERN_DIR=$d/build MODULE_DIR=$RPM_BUILD_ROOT/$d/misc -j4 \
      %INSTMOD%
    ./src/vboxnetadp/build_in_tmp \
      --use-module-symvers /tmp/vboxdrv-Module.symvers \
      KBUILD_VERBOSE= KERN_DIR=$d/build MODULE_DIR=$RPM_BUILD_ROOT/$d/misc -j4 \
      %INSTMOD%
  fi
done
mv kchmviewer $RPM_BUILD_ROOT/usr/lib/virtualbox
for i in rdesktop-vrdp.tar.gz rdesktop-vrdp-keymaps additions/VBoxGuestAdditions.iso; do
  mv $i $RPM_BUILD_ROOT/usr/share/virtualbox; done
if [ -d accessible ]; then
  mv accessible $RPM_BUILD_ROOT/usr/lib/virtualbox
fi
mv rdesktop-vrdp $RPM_BUILD_ROOT/usr/bin
install -D -m 755 vboxdrv.init $RPM_BUILD_ROOT%{_initrddir}/vboxdrv
%if %{?rpm_suse:1}%{!?rpm_suse:0}
ln -sf ../etc/init.d/vboxdrv $RPM_BUILD_ROOT/sbin/rcvboxdrv
%endif
install -D -m 755 vboxweb-service.init $RPM_BUILD_ROOT%{_initrddir}/vboxweb-service
%if %{?rpm_suse:1}%{!?rpm_suse:0}
ln -sf ../etc/init.d/vboxweb-service $RPM_BUILD_ROOT/sbin/rcvboxweb-service
%endif
ln -s VBox $RPM_BUILD_ROOT/usr/bin/VirtualBox
ln -s VBox $RPM_BUILD_ROOT/usr/bin/VBoxManage
ln -s VBox $RPM_BUILD_ROOT/usr/bin/VBoxSDL
ln -s VBox $RPM_BUILD_ROOT/usr/bin/VBoxVRDP
ln -s VBox $RPM_BUILD_ROOT/usr/bin/VBoxHeadless
ln -s VBox $RPM_BUILD_ROOT/usr/bin/vboxwebsrv
ln -s /usr/share/virtualbox/src/vboxdrv $RPM_BUILD_ROOT/usr/src/vboxdrv-%VER%
ln -s /usr/share/virtualbox/src/vboxnetflt $RPM_BUILD_ROOT/usr/src/vboxnetflt-%VER%
ln -s /usr/share/virtualbox/src/vboxnetadp $RPM_BUILD_ROOT/usr/src/vboxnetadp-%VER%
mv virtualbox.desktop $RPM_BUILD_ROOT/usr/share/applications/virtualbox.desktop
mv VBox.png $RPM_BUILD_ROOT/usr/share/pixmaps/VBox.png


%pre
# defaults
[ -r /etc/default/virtualbox ] && . /etc/default/virtualbox

# check for active VMs
if pidof VBoxSVC > /dev/null 2>&1; then
  echo "A copy of VirtualBox is currently running.  Please close it and try again. Please note"
  echo "that it can take up to ten seconds for VirtualBox (in particular the VBoxSVC daemon) to"
  echo "finish running."
  exit 1
fi

# check for old installation
if [ -r /etc/vbox/vbox.cfg ]; then
  . /etc/vbox/vbox.cfg
  if [ "x$INSTALL_DIR" != "x" -a -d "$INSTALL_DIR" ]; then
    echo "An old installation of VirtualBox was found. To install this package the"
    echo "old package has to be removed first. Have a look at /etc/vbox/vbox.cfg to"
    echo "determine the installation directory of the previous installation. After"
    echo "uninstalling the old package remove the file /etc/vbox/vbox.cfg."
    exit 1
  fi
fi

# XXX remove old modules from previous versions (disable with INSTALL_NO_VBOXDRV=1 in /etc/default/virtualbox)
if [ "$INSTALL_NO_VBOXDRV" != "1" ]; then
  find /lib/modules -name "vboxdrv\.*" 2>/dev/null|xargs rm -f 2> /dev/null || true
  find /lib/modules -name "vboxnetflt\.*" 2>/dev/null|xargs rm -f 2> /dev/null || true
  find /lib/modules -name "vboxnetadp\.*" 2>/dev/null|xargs rm -f 2> /dev/null || true
fi


%post
LOG="/var/log/vbox-install.log"

# defaults
[ -r /etc/default/virtualbox ] && . /etc/default/virtualbox

# remove old cruft
if [ -f /etc/init.d/vboxdrv.sh ]; then
  echo "Found old version of /etc/init.d/vboxdrv.sh, removing."
  rm /etc/init.d/vboxdrv.sh
fi
if [ -f /etc/vbox/vbox.cfg ]; then
  echo "Found old version of /etc/vbox/vbox.cfg, removing."
  rm /etc/vbox/vbox.cfg
fi
rm -f /etc/vbox/module_not_compiled

# install udev rule (disable with INSTALL_NO_UDEV=1 in /etc/default/virtualbox)
if [ -d /etc/udev/rules.d -a "$INSTALL_NO_UDEV" != "1" ]; then
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
  echo "KERNEL=${udev_fix}\"vboxdrv\", NAME=\"vboxdrv\", OWNER=\"root\", GROUP=\"root\", MODE=\"0600\"" \
    > /etc/udev/rules.d/10-vboxdrv.rules
  echo "SUBSYSTEM=${udev_fix}\"usb_device\", GROUP=\"vboxusers\", MODE=\"0664\"" \
    >> /etc/udev/rules.d/10-vboxdrv.rules
  echo "SUBSYSTEM=${udev_fix}\"usb\", ENV{DEVTYPE}==\"usb_device\", GROUP=\"vboxusers\", MODE=\"0664\"" \
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

# XXX SELinux: allow text relocation entries
%if %{?rpm_redhat:1}%{!?rpm_redhat:0}
if [ -x /usr/bin/chcon ]; then
  chcon -t texrel_shlib_t /usr/lib/virtualbox/*VBox* > /dev/null 2>&1
  chcon -t texrel_shlib_t /usr/lib/virtualbox/VirtualBox.so > /dev/null 2>&1
  chcon -t texrel_shlib_t /usr/lib/virtualbox/VRDPAuth.so > /dev/null 2>&1
  chcon -t texrel_shlib_t /usr/lib/virtualbox/components/VBox*.so > /dev/null 2>&1
  chcon -t java_exec_t    /usr/lib/virtualbox/VirtualBox > /dev/null 2>&1
  chcon -t java_exec_t    /usr/lib/virtualbox/VBoxSDL > /dev/null 2>&1
  chcon -t java_exec_t    /usr/lib/virtualbox/VBoxHeadless > /dev/null 2>&1
  chcon -t java_exec_t    /usr/lib/virtualbox/vboxwebsrv > /dev/null 2>&1
fi
%endif

# create users groups (disable with INSTALL_NO_GROUP=1 in /etc/default/virtualbox)
if [ "$INSTALL_NO_GROUP" != "1" ]; then
  echo
  echo "Creating group 'vboxusers'. VM users must be member of that group!"
  echo
  groupadd -f vboxusers 2> /dev/null
fi
%if %{?rpm_redhat:1}%{!?rpm_redhat:0}
/sbin/chkconfig --add vboxdrv
/sbin/chkconfig --add vboxweb-service
%endif
%if %{?rpm_suse:1}%{!?rpm_suse:0}
%{fillup_and_insserv -fy vboxdrv}
%{fillup_and_insserv -fy vboxweb-service}
%endif
%if %{?rpm_mdv:1}%{!?rpm_mdv:0}
/sbin/ldconfig
%_post_service vboxdrv
%_post_service vboxweb-service
%{update_desktop_database}
%update_menus
%endif

# try to build a kernel module (disable with INSTALL_NO_VBOXDRV=1 in /etc/default/virtualbox)
REGISTER_DKMS=1
if [ ! -f /lib/modules/`uname -r`/misc/vboxdrv.ko -a "$INSTALL_NO_VBOXDRV" != "1" ]; then
  # compile problem
  cat << EOF
No precompiled module for this kernel found -- trying to build one. Messages
emitted during module compilation will be logged to $LOG.

EOF
  rm -f /etc/vbox/module_not_compiled
  echo "** Compiling vboxdrv" > /var/log/vbox-install.log
  if ! /usr/share/virtualbox/src/vboxdrv/build_in_tmp \
    --save-module-symvers /tmp/vboxdrv-Module.symvers \
    --no-print-directory KBUILD_VERBOSE= \
    install >> /var/log/vbox-install.log 2>&1; then
    cat << EOF
Compilation of the kernel module FAILED! VirtualBox will not start until this
problem is fixed. Please consult $LOG to find out why the
kernel module does not compile. Most probably the kernel sources are not found.
Install them and execute

  /etc/init.d/vboxdrv setup

as root.

EOF
    touch /etc/vbox/module_not_compiled
  else
    echo "** Compiling vboxnetflt" >> /var/log/vbox-install.log
    if ! /usr/share/virtualbox/src/vboxnetflt/build_in_tmp \
      --use-module-symvers /tmp/vboxdrv-Module.symvers \
      --no-print-directory KBUILD_VERBOSE= \
      install >> /var/log/vbox-install.log 2>&1; then
      cat << EOF
Compilation of the kernel module FAILED! VirtualBox will not start until this
problem is fixed. Please consult $LOG to find out why the
kernel module does not compile. Most probably the kernel sources are not found.
Install them and execute

  /etc/init.d/vboxdrv setup

as root.

EOF
      touch /etc/vbox/module_not_compiled
    else
      echo "** Compiling vboxnetadp" >> /var/log/vbox-install.log
      if ! /usr/share/virtualbox/src/vboxnetadp/build_in_tmp \
        --use-module-symvers /tmp/vboxdrv-Module.symvers \
        --no-print-directory KBUILD_VERBOSE= \
        install >> /var/log/vbox-install.log 2>&1; then
        cat << EOF
Compilation of the kernel module FAILED! VirtualBox will not start until this
problem is fixed. Please consult $LOG to find out why the
kernel module does not compile. Most probably the kernel sources are not found.
Install them and execute

  /etc/init.d/vboxdrv setup

as root.

EOF
        touch /etc/vbox/module_not_compiled
      fi
    fi
  fi
  rm -f /tmp/vboxdrv-Module.symvers
  if [ ! -f /etc/vbox/module_not_compiled ]; then
    cat << EOF
Success!

EOF
    REGISTER_DKMS=
  fi
fi
# Register at DKMS. If the modules were built above, they are already registered
if [ -n "$REGISTER_DKMS" ]; then
  DKMS=`which dkms 2>/dev/null`
  if [ -n "$DKMS" ]; then
    for m in vboxdrv vboxnetflt vboxnetadp; do
      $DKMS status -m $m | while read line; do
        if echo "$line" | grep -q added > /dev/null ||
           echo "$line" | grep -q built > /dev/null ||
           echo "$line" | grep -q installed > /dev/null; then
             v=`echo "$line" | sed "s/$m,\([^,]*\)[,:].*/\1/;t;d"`
             $DKMS remove -m $m -v $v --all > /dev/null 2>&1
        fi
      done
      $DKMS add -m $m -v %VER% > /dev/null 2>&1
    done
  fi
fi
# if INSTALL_NO_VBOXDRV is set to 1, remove all shipped modules
if [ "$INSTALL_NO_VBOXDRV" = "1" ]; then
  rm -f /lib/modules/*/misc/vboxdrv.ko
  rm -f /lib/modules/*/misc/vboxnetflt.ko
  rm -f /lib/modules/*/misc/vboxnetadp.ko
fi
if lsmod | grep -q "vboxdrv[^_-]"; then
  /etc/init.d/vboxdrv stop || true
fi
if [ ! -f /etc/vbox/module_not_compiled ]; then
  depmod -a
  /etc/init.d/vboxdrv start > /dev/null
  /etc/init.d/vboxweb-service start > /dev/null
fi


%preun
# check for active VMs
if pidof VBoxSVC > /dev/null 2>&1; then
  echo "A copy of VirtualBox is currently running.  Please close it and try again. Please note"
  echo "that it can take up to ten seconds for VirtualBox (in particular the VBoxSVC daemon) to"
  echo "finish running."
  exit 1
fi

%if %{?rpm_suse:1}%{!?rpm_suse:0}
%stop_on_removal vboxweb-service
%stop_on_removal vboxdrv
%endif
%if %{?rpm_mdv:1}%{!?rpm_mdv:0}
%_preun_service vboxweb-service
%_preun_service vboxdrv
%endif
if [ "$1" = 0 ]; then
%if %{?rpm_redhat:1}%{!?rpm_redhat:0}
  /sbin/service vboxdrv stop > /dev/null
  /sbin/chkconfig --del vboxdrv
  /sbin/service vboxweb-service stop > /dev/null
  /sbin/chkconfig --del vboxweb-service
%endif
  rm -f /etc/udev/rules.d/10-vboxdrv.rules
  rm -f /etc/vbox/license_agreed
  rm -f /etc/vbox/module_not_compiled
fi
DKMS=`which dkms 2>/dev/null`
if [ -n "$DKMS" ]; then
  $DKMS remove -m vboxnetadp -v %VER% --all > /dev/null 2>&1 || true
  $DKMS remove -m vboxnetflt -v %VER% --all > /dev/null 2>&1 || true
  $DKMS remove -m vboxdrv -v %VER% --all > /dev/null 2>&1 || true
fi


%postun
%if %{?rpm_redhat:1}%{!?rpm_redhat:0}
if [ "$1" -ge 1 ]; then
  /sbin/service vboxdrv restart > /dev/null 2>&1
  /sbin/service vboxweb-service restart > /dev/null 2>&1
fi
%endif
%if %{?rpm_suse:1}%{!?rpm_suse:0}
%restart_on_update vboxdrv
%restart_on_update vboxweb-service
%insserv_cleanup
%endif
%if %{?rpm_mdv:1}%{!?rpm_mdv:0}
/sbin/ldconfig
%{clean_desktop_database}
%clean_menus
%endif


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root)
%doc LICENSE
%doc %LICENSE%
%doc UserManual*.pdf
%doc VirtualBox*.chm
%{_initrddir}/vboxdrv
%{_initrddir}/vboxweb-service
%{?rpm_suse: %{py_sitedir}/*}
%{!?rpm_suse: %{python_sitelib}/*}
%{?rpm_suse: /sbin/rcvboxdrv}
%{?rpm_suse: /sbin/rcvboxweb-service}
/lib/modules
/etc/vbox
/usr/bin
/usr/src/vbox*
/usr/lib/virtualbox
/usr/share/applications
/usr/share/pixmaps
/usr/share/virtualbox
