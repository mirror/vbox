#!/bin/bash
# $Id$
## @file
# VirtualBox Validation Kit - testbox pxe config emitter.
#

#
# Copyright (C) 2006-2016 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#
# The contents of this file may alternatively be used under the terms
# of the Common Development and Distribution License Version 1.0
# (CDDL) only, as it comes in the "COPYING.CDDL" file of the
# VirtualBox OSE distribution, in which case the provisions of the
# CDDL are applicable instead of those of the GPL.
#
# You may elect to license modified versions of this file under the
# terms and conditions of either the GPL or the CDDL or both.
#


#
# Global Variables (config first).
#
MY_NFS_SERVER_IP="10.165.98.50"
MY_GATEWAY_IP="10.165.98.0"
MY_NETMASK="255.255.128.0"
MY_ETH_DEV="eth0"
MY_AUTO_CFG="any"
MY_DNS0_IP="10.165.246.33"
MY_DNS1_IP="192.135.82.44"

# options
MY_PXELINUX_CFG_DIR="/mnt/testbox-tftp/pxeclient.cfg"
MY_ACTION=""
MY_IP=""
MY_IP_HEX=""

#
# Parse arguments.
#
while test "$#" -ge 1; do
    MY_ARG=$1
    shift
    case "${MY_ARG}" in
        -c|--cfg-dir)
            MY_PXELINUX_CFG_DIR="$1";
            shift;
            if test -z "${MY_PXELINUX_CFG_DIR}"; then
                echo "syntax error: Empty pxeclient.cfg path." >&2;
                exit 2;
            fi
            ;;

        -h|--help)
            echo "usage: testbox-pxe-conf.sh: [-c /mnt/testbox-tftp/pxelinux.cfg] <ip> <action>";
            echo "Actions: backup, backup-again, restore, refresh-info, rescue";
            exit 0;
            ;;
        -*)
            echo "syntax error: Invalid option: ${MY_ARG}" >&2;
            exit 2;
            ;;

        *)  if test -z "$MY_ARG"; then
                echo "syntax error: Empty argument" >&2;
                exit 2;
            fi
            if test -z "${MY_IP}"; then
                MY_TMP=`echo "${MY_ARG}" | sed -e 's/\./ /g'`
                if printf "%02X%02X%02X%02X" ${MY_TMP} > /dev/null; then
                    MY_IP_HEX=`printf "%02X%02X%02X%02X" ${MY_TMP}`;
                    MY_IP="${MY_ARG}";
                else
                    echo "syntax error: Invalid IP: ${MY_ARG}" >&2;
                    exit 2;
                fi
            else
                if test -z "${MY_ACTION}"; then
                    case "${MY_ARG}" in
                        backup|backup-again|restore|refresh-info|rescue)
                            MY_ACTION="${MY_ARG}";
                            ;;
                        *)
                            echo "syntax error: Invalid action: ${MY_ARG}" >&2;
                            exit 2;
                            ;;
                    esac
                else
                    echo "syntax error: Too many arguments" >&2;
                    exit 2;
                fi
            fi
            ;;
    esac
done

if test -z "${MY_ACTION}"; then
    echo "syntax error: Insufficient arguments" >&2;
    exit 2;
fi
if test ! -d "${MY_PXELINUX_CFG_DIR}"; then
    echo "error: pxeclient.cfg path does not point to a directory: ${MY_PXELINUX_CFG_DIR}" >&2;
    exit 1;
fi
if test ! -f "${MY_PXELINUX_CFG_DIR}/default"; then
    echo "error: pxeclient.cfg path does contain a 'default' file: ${MY_PXELINUX_CFG_DIR}" >&2;
    exit 1;
fi


#
# Produce the file.
# Using echo here so we can split up the APPEND line more easily.
#
MY_CFG_FILE="${MY_PXELINUX_CFG_DIR}/${MY_IP_HEX}"
set +e
echo "PATH bios" > "${MY_CFG_FILE}";
echo "DEFAULT maintenance" >> "${MY_CFG_FILE}";
echo "LABEL maintenance" >> "${MY_CFG_FILE}";
echo "  MENU LABEL Maintenance (NFS)" >> "${MY_CFG_FILE}";
echo "  KERNEL maintenance-boot/vmlinuz-3.16.0-4-amd64" >> "${MY_CFG_FILE}";
echo -n "  APPEND initrd=maintenance-boot/initrd.img-3.16.0-4-amd64 testbox-action-${MY_ACTION}" >> "${MY_CFG_FILE}";
echo -n " ro aufs=tmpfs boot=nfs root=/dev/nfs" >> "${MY_CFG_FILE}";
echo -n " nfsroot=${MY_NFS_SERVER_IP}:/export/testbox-nfsroot,ro,tcp" >> "${MY_CFG_FILE}";
echo -n " nfsvers=3 nfsrootdebug" >> "${MY_CFG_FILE}";
echo -n " ip=${MY_IP}:${MY_NFS_SERVER_IP}:${MY_GATEWAY_IP}:${MY_NETMASK}:maintenance:${MY_ETH_DEV}:${MY_AUTO_CFG}:${MY_DNS0_IP}:${MY_DNS1_IP}" >> "${MY_CFG_FILE}";
echo "" >> "${MY_CFG_FILE}";
echo "LABEL local-boot" >> "${MY_CFG_FILE}";
echo "LOCALBOOT" >> "${MY_CFG_FILE}";
echo "Successfully generated ${MY_CFG_FILE}."
exit 0;

