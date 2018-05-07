#!/bin/sh -e

#
# Copyright (C) 2018 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

# What this script does:
usage_msg="\
Usage: `basename ${0}` [--no-docs]

Install the dependencies needed for building VirtualBox on a Linux
system.  Initial support for Enterprise Linux derivatives, additional
distributions will be added as needed.  There are no plans to add
support for or to accept patches for distributions we do not package.
The \`--no-docs\' parameter is used to prevent installation of TeX
packages.  Installing TeX is not supported on EL5, so the flag is
ignored there."

# To repeat: there are no plans to add support for or to accept patches
# for distributions we do bot package.

unset NODOCS
egrepignore=\
"Setting up Install Process|already installed and latest version|Nothing to do"

usage()
{
    echo "${usage_msg}"
    exit "${1}"
}

while test -n "${1}"; do
    case "${1}" in
    --no-docs)
        NODOCS=1
        shift ;;
    -h|--help)
        usage 0 ;;
    *)
        echo "Unknown parameter ${1}" >&2
        usage 1 ;;
    esac
done

LC_ALL=C
export LC_ALL
PATH=/sbin:/usr/sbin:$PATH

if test -f /etc/redhat-release; then
  yum install -y bzip2 gcc-c++ glibc-devel gzip libcap-devel \
    libIDL-devel libxslt-devel libXmu-devel \
    make mkisofs openssl-devel pam-devel \
    python-devel qt-devel rpm-build \
    wget kernel kernel-devel \
    tar libpng-devel | egrep -v  "${egrepignore}"
  # Not EL5
  if ! grep -q "release 5" /etc/redhat-release; then
    yum install libcurl-devel libstdc++-static libvpx-devel \
      pulseaudio-libs-devel SDL-static texlive-latex texlive-latex-bin \
      texlive-ec texlive-collection-fontsrecommended
      texlive-pdftex-def texlive-fancybox device-mapper-devel \
      glibc-static zlib-static glibc-devel.i686 libstdc++.i686 \
      qt5-qttools-devel qt5-qtx11extras-devel | egrep -v  "${egrepignore}"
    if test -z "$NODOCS"; then
      yum install texlive-latex texlive-latex-bin texlive-ec \
        texlive-pdftex-def texlive-fancybox device-mapper-devel |
        egrep -v  "${egrepignore}"
      if test ! -f /usr/share/texmf/tex/latex/bera/beramono.sty; then
        mkdir -p /usr/share/texmf/tex/latex/bera
        pushd /usr/share/texmf/tex/latex/bera
        wget http://www.tug.org/texlive/devsrc/Master/texmf-dist/tex/latex/bera/beramono.sty
        texhash
        popd
      fi
    fi
  fi
  # EL5
  if grep -q "release 5" /etc/redhat-release; then
    yum install -y curl-devel SDL-devel libstdc++-devel.i386 \
      openssh-clients which gcc44-c++ | egrep -v  "${egrepignore}"
    ln -sf /usr/bin/gcc44 /usr/local/bin/gcc
    ln -sf /usr/bin/g++44 /usr/local/bin/g++
    if ! rpm -q python26 > /dev/null; then
      pythonpkgs="\
http://archives.fedoraproject.org/pub/archive/epel/5/x86_64/python26-2.6.8-2.el5.x86_64.rpm \
http://archives.fedoraproject.org/pub/archive/epel/5/x86_64/python26-libs-2.6.8-2.el5.x86_64.rpm \
http://archives.fedoraproject.org/pub/archive/epel/5/x86_64/python26-devel-2.6.8-2.el5.x86_64.rpm
http://archives.fedoraproject.org/pub/archive/epel/5/x86_64/libffi-3.0.5-1.el5.x86_64.rpm"
      tmpdir=`mktemp -d`
      pushd ${tmpdir}
      wget ${pythonpkgs}
      rpm -i *.rpm
      popd
      rm -r ${tmpdir}
      ln -sf /usr/bin/python2.6 /usr/local/bin/python
    fi
    if ! rpm -q SDL_ttf-devel > /dev/null; then
      sdlpkgs="\
http://archives.fedoraproject.org/pub/archive/epel/5/x86_64/SDL_ttf-2.0.8-3.el5.x86_64.rpm \
http://archives.fedoraproject.org/pub/archive/epel/5/x86_64/SDL_ttf-devel-2.0.8-3.el5.x86_64.rpm"
      tmpdir=`mktemp -d`
      pushd ${tmpdir}
      wget ${sdlpkgs}
      rpm -i *.rpm
      popd
      rm -r ${tmpdir}
    fi
    if test ! -f /usr/local/include/pulse/pulseaudio.h; then
      tmpdir=`mktemp -d`
      pushd ${tmpdir}
      wget --no-check-certificate https://freedesktop.org/software/pulseaudio/releases/pulseaudio-11.1.tar.gz
      tar -x -z -f pulseaudio-11.1.tar.gz pulseaudio-11.1/src/pulse
      mkdir -p /usr/local/include/pulse
      cp pulseaudio-11.1/src/pulse/*.h /usr/local/include/pulse
      popd
      rm -r ${tmpdir}
    fi
  fi
fi
