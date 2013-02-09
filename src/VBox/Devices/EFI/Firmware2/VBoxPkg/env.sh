#!/bin/bash
# $Id$
# @file
# Environment Setup Script for VBoxPkg + EDK2 (OVMF).
#

#
# Copyright (C) 2010-2013 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

# todo2: patching edk's build environment

if [ -z "$PATH_DEVTOOLS" ]
then
    echo "Please run tools/env.sh from your VBox sources "
    exit 1
fi

export WORKSPACE=`pwd`

#CONF_FILE=Conf/tools_def.txt
CONF_FILE=BaseTools/Conf/tools_def.template
TARGET_CONF_FILE=Conf/tools_def.txt
TARGET_FILE=Conf/target.txt

TMP_CONF_DEFINE=/tmp/tools_def.defines.txt.$$
TMP_CONF_IASL=/tmp/tools_def.defines.iasl.txt.$$
TMP_CONF_DECL=/tmp/tools_def.defines.declarations.txt.$$

case $BUILD_PLATFORM in
    darwin)
        export IA32_PETOOLS_PREFIX=/opt/local/bin
        export X64_PETOOLS_PREFIX=$HOME/mingw-w64-bin_i686-darwin_20091111/bin
        export IPF_PETOOLS_PREFIX=__no_ipf__
        if [ "$USER" = "bird" ]; then
            export IA32_PETOOLS_PREFIX=/Users/bird/coding/tianocore/edk2/trunk-tools32/bin/i686-pc-mingw32-
            export X64_PETOOLS_PREFIX=/Users/bird/coding/tianocore/edk2/trunk-tools/bin/x86_64-pc-mingw32-
        fi
        
        sed -e "s/^\(DEFINE UNIXGCC_\)\(.*\)\(_PETOOLS_PREFIX\).*/\1\2\3 = ENV(\2_PETOOLS_PREFIX)/g" $CONF_FILE > $TMP_CONF_DEFINE
        sed -e "s/^\(DEFINE UNIX_\)\(IASL_BIN\).*/\1\2 = ENV(\2)/g" $TMP_CONF_DEFINE > $TMP_CONF_IASL
        #Not sure if any of the following is necessary/working with OVMF.
        sed -e "s/^\(\*_\)\(UNIXGCC_IA32_\)\(.*\)\(_PATH\).*\/\(.*\)$/\1\2\3\4 = DEF(\2PETOOLS_PREFIX)\/i386-mingw32-\5/g" $TMP_CONF_IASL > $TMP_CONF_DECL
        if [ "$USER" = "bird" ]; then
             sed -e "s/^\(\*_\)\(UNIXGCC_X64_\)\(.*\)\(_PATH\).*\/\(.*\)$/\1\2\3\4 = DEF(\2PETOOLS_PREFIX)\/x86_64-mingw32-\5/g" $TMP_CONF_IASL > $TMP_CONF_DECL
        else
             sed -e "s/^\(\*_\)\(UNIXGCC_X64_\)\(.*\)\(_PATH\).*\/\(.*\)$/\1\2\3\4 = DEF(\2PETOOLS_PREFIX)\/x86_64-mingw64-\5/g" $TMP_CONF_IASL > $TMP_CONF_DECL
        fi
        
        ;;
    linux)
    # Defines here suitable for mingw that comes with Ubuntu
    # Install as 'apt-get install mingw32-binutils mingw32 mingw32-runtime'
    if [ -z $IA32_PETOOLS_PREFIX ]
    then
       IA32_PETOOLS_PREFIX=/usr/bin/i586-mingw32msvc-
    fi
    if [ -z  $X64_PETOOLS_PREFIX ]
    then
        X64_PETOOLS_PREFIX=/usr/local/bin/x86_64-w64-mingw32-
    fi
    if [ -z  $IPF_PETOOLS_PREFIX ]
    then
        IPF_PETOOLS_PREFIX=__no_ipf__
    fi


    sed -e "s/^\(DEFINE UNIXGCC_\)\(.*\)\(_PETOOLS_PREFIX\).*/\1\2\3 = ENV(\2_PETOOLS_PREFIX)/g" $CONF_FILE  | \
        sed -e "s/^\(DEFINE UNIX_\)\(IASL_BIN\).*/\1\2 = ENV(\2)/g" - | \
        sed -e "s/\(.*\)-Wno-array-bounds\(.*\)/\1\2/g" - | \
        sed -e "s/^\(\*_\)\(UNIXGCC_X64_\)\(.*\)\(_PATH\).*\/\(.*\)$/\1\2\3\4 = DEF(\2PETOOLS_PREFIX)\/x86_64-mingw64-\5/g" - | \
        sed -e "s/^\(\*_\)\(UNIXGCC_IA32_\)\(.*\)\(_PATH\).*\/\(.*\)$/\1\2\3\4 = DEF(\2PETOOLS_PREFIX)\/i586-mingw32msvc-\5/g" - > $TMP_CONF_DECL

    # be smarter!
    PATH=`pwd`/BaseTools/BinWrappers/Linux-i686:$PATH
    ;;
    *)
    echo "port build setting to your Unix"
    exit 1
    ;;
esac

export IA32_PETOOLS_PREFIX
export X64_PETOOLS_PREFIX
export IPF_PETOOLS_PREFIX

cp $TMP_CONF_DECL $TARGET_CONF_FILE

if [ -z "$FIRMWARE_ARCH" ]; then
    FIRMWARE_ARCH="IA32"
fi
case  "$FIRMWARE_ARCH"  in
    "IA32") active=OvmfPkg/OvmfPkgIa32.dsc;;
    "X64" ) active=OvmfPkg/OvmfPkgX64.dsc;;
esac
echo "ACTIVE_PLATFORM = " $active > $TARGET_FILE
case "$BUILD_TYPE" in
    "debug") target=DEBUG;;
    "release") target=RELEASE;;
    "profile") target=RELEASE;;
    *) echo "unsupported build type "$BUILD_TYPE;;
esac
echo "TARGET = " $target >> $TARGET_FILE
echo "TARGET_ARCH = " $FIRMWARE_ARCH  >> $TARGET_FILE
echo "TOOL_CHAIN_CONF = Conf/tools_def.txt" >> $TARGET_FILE
echo "TOOL_CHAIN_TAG = UNIXGCC" >> $TARGET_FILE
echo "MAX_CONCURRENT_THREAD_NUMBER = 1" >> $TARGET_FILE
echo "MULTIPLE_THREAD = Disable" >> $TARGET_FILE
echo "BUILD_RULE_CONF = Conf/build_rule.txt" >> $TARGET_FILE

export IASL_BIN=$PATH_DEVTOOLS/$BUILD_PLATFORM.$BUILD_PLATFORM_ARCH/bin/iasl

VBOX_ROOT=$PATH_DEVTOOLS/..
if [ ! -f "$VBOX_ROOT/Config.kmk" ]; then
    echo "Warning! $VBOX_ROOT doesn't seem to be the root of the VirtualBox source tree!"
fi

[ ! -d VBoxPkg/Include/VBox ] && ln -s $VBOX_ROOT/include/VBox VBoxPkg/Include/VBox
[ ! -d VBoxPkg/Include/iprt ] && ln -s $VBOX_ROOT/include/iprt VBoxPkg/Include/iprt
[ ! -f VBoxPkg/Include/version-generated.h ] && ln -s $VBOX_ROOT/out/$BUILD_PLATFORM.$BUILD_PLATFORM_ARCH/$BUILD_TYPE/version-generated.h VBoxPkg/Include/version-generated.h
[ ! -f VBoxPkg/Include/product-generated.h ] && ln -s $VBOX_ROOT/out/$BUILD_PLATFORM.$BUILD_PLATFORM_ARCH/$BUILD_TYPE/product-generated.h VBoxPkg/Include/product-generated.h
[ ! -f VBoxPkg/VBoxPUELLogo/puel_logo.bmp ] && ln -s $VBOX_ROOT/src/VBox/Devices/Graphics/BIOS/puel_logo.bmp VBoxPkg/VBoxPUELLogo/puel_logo.bmp

# Tools should be ready at this point.
export EDK_TOOLS_PATH=$WORKSPACE/BaseTools
. BaseTools/BuildEnv

if [ -z "$VBOXPKG_ENV_NON_INTERECTIVE" ]
then
    export ARCH=IA32

    RC=/tmp/efidev$$
    rm -f $RC
    echo "if [ ~/.bashrc ]; then . ~/.bashrc; fi" > $RC
    echo "export PS1='\[\033[01;32m\]VBox/trunk/EFI \u@\h \[\033[01;34m\]\W \$ \[\033[00m\]'" >> $RC
    $SHELL --init-file $RC
    rm -f $RC
fi
