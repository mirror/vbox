#!/bin/sh

#
# Script to handle VirtualBox installation on a Linux host.
#
# Copyright (C) 2013-2015 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

# This script is invoked as part of the installation of VirtualBox on a Linux
# host system (see next paragraph for details).  When we install using the
# makeself installer it will be executed at installation time, whereas with RPM
# or deb packages it will be executed by the %install section of the template,
# respectively the binary rule from the rules file when the package is created.
# The plan is to gradually move anything here which is identical across the
# three installer types and to put new things in here straight away.  We should
# maintain an uninstall.sh script in parallel, but this will only be needed by
# the makeself installer as the other two deal with that automatically.  Perhaps
# once we have made some progress at factoring out common bits in the three
# installers we can even try doing it accross other POSIX platforms.
#
# The general idea (mine at least) of how this works/should work is that the
# build system installs everything needed for VirtualBox to run (provided all
# drivers it needs are currently loaded) into the output bin/ folder.  The
# Makeself installer duplicates this folder as /opt/VirtualBox (or whatever),
# the other two as <prefix>/lib/virtualbox (again, or whatever).  The installer
# then installs scripts into <prefix>/bin to start binaries in the duplicated
# main folder, builds drivers which are put into the kernel module directories
# and creates init/start-up scripts which load drivers and/or start binaries in
# the main folder.  As mentioned above, this installation is done at the time
# the package is created for RPM/deb packages and shouldn't create anything
# which is not supposed to be included in a package file list (other things can
# be done in a post-install step).

# Clean up before we start.
cr="
"
tab="   "
IFS=" ${cr}${tab}"
'unset' -f unalias
'unalias' -a 2>/dev/null
'unset' -f command
PATH=/bin:/sbin:/usr/bin:/usr/sbin:$PATH

# Exit on any errors.
set -e

# Get the folder we are installed to, as we need other files there.  May be
# relative to the current directory.
# I hope we never install to a folder with a new-line at the end of the name,
# but the code should(!) still work.
INSTALL_SOURCE=`expr "$0" : '\(.*/\)[^/][^/]*/*'`".."
. "${INSTALL_SOURCE}/scripts/generated.sh"

# Default settings values.
## Root of installation target file system.
ROOT=""
## Prefix to install to.
RELATIVE_PREFIX="/usr"
## Package name.
PACKAGE="VirtualBox"
## Init script folder.  Scripts will not be installed at this stage if empty.
INIT_FOLDER=""
## Do not install Qt front-end-related files.
NO_QT=""
## Do a headless installation.
HEADLESS=""
## Do not install the web service.
NO_WEB_SERVICE=""
## OSE installation - does this still mean anything?
OSE=""
## Do not create <prefix>/share/<package>.
NO_SHARE_PACKAGE=""
## Delete the helpers and scripts folders.
NO_HELPERS=""
## The command to use to run the Python interpreter.
PYTHON_COMMAND="python"
## The folder where the main VirtualBox binaries and libraries will be, relative
# to <prefix>.
INSTALL_FOLDER="/lib/${PACKAGE}"

while test "${#}" -gt 0; do
    case $1 in
    --prefix)
        test "${#}" -gt 1 ||
        {
            echo "${1} requires at least one argument." >&2
            exit 1
        }
        RELATIVE_PREFIX="${2}"
        shift
        ;;
    --root)
        test "${#}" -gt 1 ||
        {
            echo "${1} requires at least one argument." >&2
            exit 1
        }
        ROOT="${2}"
        shift
        ;;
    --package)
        test "${#}" -gt 1 ||
        {
            echo "${1} requires at least one argument." >&2
            exit 1
        }
        PACKAGE="${2}"
        shift
        ;;
    --init-folder)
        test "${#}" -gt 1 ||
        {
            echo "${1} requires at least one argument." >&2
            exit 1
        }
        INIT_FOLDER="${2}"
        shift
        ;;
    --no-qt)
        NO_QT="true"
        ;;
    --headless)
        HEADLESS="true"
        ;;
    --no-web-service)
        NO_WEB_SERVICE="true"
        ;;
    --ose)
        OSE="true"
        ;;
    --no-share-package)
        NO_SHARE_PACKAGE="true"
        ;;
    --no-helpers)
        NO_HELPERS="true"
        ;;
    --no-vbox-img)
        NO_VBOX_IMG="true"
        ;;
    --python-command)
        test "${#}" -gt 1 ||
        {
            echo "${1} requires at least one argument." >&2
            exit 1
        }
        PYTHON_COMMAND="${2}"
        shift
        ;;

    --install-folder)
        test "${#}" -gt 1 ||
        {
            echo "${1} requires at least one argument." >&2
            exit 1
        }
        INSTALL_FOLDER="${2}"
        shift
        ;;
    *)
        echo "Unknown argument ${1}."
        exit 1
        ;;
    esac
    shift
done

PREFIX="${ROOT}${RELATIVE_PREFIX}"

# Note: install(1) is not POSIX, but it is available on Linux, Darwin and all
# Solaris versions I could check (the oldest was 2.6).  It is a BSD command.
install -d -g 0 -o 0 "${PREFIX}/bin"
install -d -g 0 -o 0 "${PREFIX}/share/applications"
# We use this as our base install folder.
test -z "${NO_QT}" &&
    mv "${INSTALL_SOURCE}/virtualbox.desktop" "${PREFIX}/share/applications/"
install -d -g 0 -o 0 "${PREFIX}/share/pixmaps"
test -z "${NO_QT}" &&
    install -d -g 0 -o 0 "${PREFIX}/share/icons/hicolor"
test -z "${NO_QT}" &&
    cp "${INSTALL_SOURCE}/icons/128x128/virtualbox.png" "${PREFIX}/share/pixmaps/"
test -z "${NO_QT}" &&
    for i in "${INSTALL_SOURCE}/icons/"*; do
        folder=`expr "${i}/" : '.*/\([^/][^/]*/\)/*'`
        if test -f "${i}/virtualbox."*; then
            install -d -g 0 -o 0 "${PREFIX}/share/icons/hicolor/${folder}/apps"
            mv "${i}/virtualbox."* "${PREFIX}/share/icons/hicolor/${folder}/apps"
        fi
        install -d -g 0 -o 0 "${PREFIX}/share/icons/hicolor/${folder}/mimetypes"
        mv "${i}/"* "${PREFIX}/share/icons/hicolor/${folder}/mimetypes" 2>/dev/null || true
        rmdir "${i}"
    done
test -z "${NO_QT}" &&
    rmdir "${INSTALL_SOURCE}/icons"
if test -w "${INSTALL_SOURCE}/virtualbox.xml"; then
    install -d -g 0 -o 0 "${PREFIX}/share/mime/packages"
    mv "${INSTALL_SOURCE}/virtualbox.xml" "${PREFIX}/share/mime/packages/"
fi
mv "${INSTALL_SOURCE}/VBox.png" "${PREFIX}/share/pixmaps/"
test -n "${NO_QT}" &&
    test ! -r ${INSTALL_SOURCE}/VBoxTestOGL
test -n "${NO_QT}" &&
    test ! -r ${INSTALL_SOURCE}/nls
install -D -g 0 -o 0 -m 644 "${INSTALL_SOURCE}/VBox.sh" "${PREFIX}/bin/VBox"
rm "${INSTALL_SOURCE}/VBox.sh"
(
    cd "${INSTALL_SOURCE}/sdk/installer"
    export VBOX_INSTALL_PATH="${RELATIVE_PREFIX}${INSTALL_FOLDER}"
    "${PYTHON_COMMAND}" "vboxapisetup.py" install --root "${ROOT}" --prefix "${RELATIVE_PREFIX}"
)
rm -rf ${INSTALL_SOURCE}/sdk/installer
test -n "${HEADLESS}" &&
    test ! -r "${INSTALL_SOURCE}/VBoxSDL"
test -n "${NO_QT}" &&
    test ! -r "${INSTALL_SOURCE}/VirtualBox"
test -n "${NO_WEB_SERVICE}" &&
    test ! -r "${INSTALL_SOURCE}/vboxwebsrv"
test -n "${NO_VBOX_IMG}" &&
    test ! -r "${INSTALL_SOURCE}/vbox-img"
test -n "${NO_WEB_SERVICE}" &&
    test ! -r "${INSTALL_SOURCE}/webtest"
test -r "${INSTALL_SOURCE}/VBoxDTrace" &&
    mv "${INSTALL_SOURCE}/VBoxDTrace" "${PREFIX}/bin"
mv "${INSTALL_SOURCE}/VBoxTunctl" "${PREFIX}/bin"
test -n "${OSE}" || test -n "${NO_QT}" &&
    test ! -r ${INSTALL_SOURCE}/kchmviewer
test -z "${OSE}" && test -z "${HEADLESS}" &&
    mv "${INSTALL_SOURCE}/rdesktop-vrdp" "${PREFIX}/bin"
if test -z "${NO_SHARE_PACKAGE}"; then
    install -d -g 0 -o 0 "${PREFIX}/share/${PACKAGE}"
    mv "${INSTALL_SOURCE}/VBoxSysInfo.sh" "${PREFIX}/share/${PACKAGE}"
    mv "${INSTALL_SOURCE}/VBoxCreateUSBNode.sh" "${PREFIX}/share/${PACKAGE}"
    mv "${INSTALL_SOURCE}/src" "${PREFIX}/share/${PACKAGE}"
    test -z "${NO_QT}" &&
        mv "${INSTALL_SOURCE}/nls" "${PREFIX}/share/${PACKAGE}"
    mv "${INSTALL_SOURCE}/additions/VBoxGuestAdditions.iso" "${PREFIX}/share/${PACKAGE}"
    # The original code did not fail if this file did not exist.
    test -z "${OSE}" && test -f "${INSTALL_SOURCE}/rdesktop-vrdp.tar.gz" &&
        mv "${INSTALL_SOURCE}/rdesktop-vrdp.tar.gz" "${PREFIX}/share/${PACKAGE}"
    test -z "${OSE}" && test -z "${HEADLESS}" &&
        mv "${INSTALL_SOURCE}/rdesktop-vrdp-keymaps" "${PREFIX}/share/${PACKAGE}"
    ln -s "../share/virtualbox/src/vboxhost" "${PREFIX}/src/vboxhost-${VBOX_VERSION_STRING}"
else
    mv "${INSTALL_SOURCE}/src/vboxhost" "${PREFIX}/src/vboxhost-${VBOX_VERSION_STRING}"
fi
test -z "${NO_QT}" && ln -s "VBox" "${PREFIX}/bin/VirtualBox"
test -z "${NO_QT}" && ln -sf "VBox" "${PREFIX}/bin/virtualbox"
ln -s "VBox" "${PREFIX}/bin/VBoxManage"
ln -sf "VBox" "${PREFIX}/bin/vboxmanage"
test -z "${HEADLESS}" && ln -s "VBox" "${PREFIX}/bin/VBoxSDL"
test -z "${HEADLESS}" && ln -sf "VBox" "${PREFIX}/bin/vboxsdl"
test -z "${OSE}" && ln -s "VBox" "${PREFIX}/bin/VBoxVRDP"
ln -s "VBox" "${PREFIX}/bin/VBoxHeadless"
ln -sf "VBox" "${PREFIX}/bin/vboxheadless"
ln -s "VBox" "${PREFIX}/bin/VBoxBalloonCtrl"
ln -sf "VBox" "${PREFIX}/bin/vboxballoonctrl"
ln -s "VBox" "${PREFIX}/bin/VBoxAutostart"
ln -s "VBox" "${PREFIX}/bin/vboxautostart"
test -z "${NO_WEB_SERVICE}" && ln -s "VBox" "${PREFIX}/bin/vboxwebsrv"
echo "NO_VBOX_IMG = ${NO_VBOX_IMG}"
test -z "${NO_VBOX_IMG}" && ln -sv "${RELATIVE_PREFIX}${INSTALL_FOLDER}/vbox-img" "${PREFIX}/bin/vbox-img"
rmdir ${INSTALL_SOURCE}/additions
rm "${INSTALL_SOURCE}/scripts/install.sh"
## @todo Move this to a make file.
install -d -g 0 -o 0 "${INSTALL_SOURCE}/ExtensionPacks"
# For now.
test -n "${NO_HELPERS}" &&
    rm -r "${INSTALL_SOURCE}/helpers"
# And the very last bit.
test -n "${NO_HELPERS}" &&
    rm -r "${INSTALL_SOURCE}/scripts"
exit 0
