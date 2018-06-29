#!/bin/sh
apt-get install chrpath g++ make iasl libidl-dev libsdl1.2-dev \
    libsdl-ttf2.0-dev libpam0g-dev libssl-dev libpulse-dev doxygen \
    libasound2-dev xsltproc libxml2-dev libxml2-utils unzip \
    libxrandr-dev libxinerama-dev libcap-dev python-dev \
    libxmu-dev libxcursor-dev libcurl4-openssl-dev libdevmapper-dev \
    libvpx-dev qttools5-dev-tools libqt5opengl5-dev libqt5x11extras5-dev \
    texlive texlive-latex-extra texlive-fonts-extra g++-multilib
# Ubuntu only
grep Ubuntu /etc/lsb-release 2>/dev/null >&2 &&
    apt-get install linux-headers-generic
# apt-get install wine linux-headers-`uname -r`  # Not for chroot installs.

