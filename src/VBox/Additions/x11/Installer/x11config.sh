#!/bin/sh
#
# Guest Additions X11 config update script
#
# Copyright (C) 2006-2010 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

auto_mouse=""
new_mouse=""
no_bak=""
old_mouse_dev="/dev/psaux"

tab=`printf '\t'`

ALL_SECTIONS=\
'^[ '$tab']*[Ss][Ee][Cc][Tt][Ii][Oo][Nn][ '$tab']*'\
'"\([Ii][Nn][Pp][Uu][Tt][Dd][Ee][Vv][Ii][Cc][Ee]\|'\
'[Dd][Ee][Vv][Ii][Cc][Ee]\|'\
'[Ss][Ee][Rr][Vv][Ee][Rr][Ll][Aa][Yy][Oo][Uu][Tt]\|'\
'[Ss][Cc][Rr][Ee][Ee][Nn]\|'\
'[Mm][Oo][Nn][Ii][Tt][Oo][Rr]\|'\
'[Kk][Ee][Yy][Bb][Oo][Aa][Rr][Dd]\|'\
'[Pp][Oo][Ii][Nn][Tt][Ee][Rr]\)"'
# ^\s*Section\s*"(InputDevice|Device|ServerLayout|Screen|Monitor|Keyboard|Pointer)"

KBD_SECTION='^[ '$tab']*[Ss][Ee][Cc][Tt][Ii][Oo][Nn][ '$tab']*"'\
'[Ii][Nn][Pp][Uu][Tt][Dd][Ee][Vv][Ii][Cc][Ee]"' # ^\s*section\s*\"inputdevice\"

END_SECTION='[Ee][Nn][Dd][Ss][Ee][Cc][Tt][Ii][Oo][Nn]' # EndSection

OPT_XKB='^[ '$tab']*option[ '$tab'][ '$tab']*"xkb'

DRIVER_KBD='^[ '$tab']*[Dd][Rr][Ii][Vv][Ee][Rr][ '$tab'][ '$tab']*'\
'"\(kbd\|keyboard\)"'
# ^\s*driver\s+\"(kbd|keyboard)\"

reconfigure()
{
    cfg="$1"
    tmp="$cfg.vbox.tmp"
    test -w "$cfg" || { echo "$cfg does not exist"; return; }
    rm -f "$tmp"
    test ! -e "$tmp" || { echo "Failed to delete $tmp"; return; }
    touch "$tmp"
    test -w "$tmp" || { echo "Failed to create $tmp"; return; }
    xkb_opts="`cat "$cfg" | sed -n -e "/$KBD_SECTION/,/$END_SECTION/p" |
              grep -i "$OPT_XKB"`"
    kbd_drv="`cat "$cfg" | sed -n -e "/$KBD_SECTION/,/$END_SECTION/p" |
             sed -n -e "0,/$DRIVER_KBD/s/$DRIVER_KBD/\\1/p"`"
    cat > "$tmp" << EOF
# VirtualBox generated configuration file
# based on $cfg.
EOF
    cat "$cfg" | sed -e "/$ALL_SECTIONS/,/$END_SECTION/s/\\(.*\\)/# \\1/" >> "$tmp"
    test -n "$kbd_drv" && cat >> "$tmp" << EOF
Section "InputDevice"
  Identifier   "Keyboard[0]"
  Driver       "$kbd_drv"
$xkb_opts
  Option       "Protocol" "Standard"
  Option       "CoreKeyboard"
EndSection
EOF
    kbd_layout=""
    test -n "$kbd_drv" && kbd_layout='  InputDevice  "Keyboard[0]" "CoreKeyboard"'
    test -z "$auto_mouse" -a -z "$new_mouse" && cat >> $tmp << EOF

Section "InputDevice"
  Identifier  "Mouse[1]"
  Driver      "vboxmouse"
  Option      "Buttons" "9"
  Option      "Device" "$old_mouse_dev"
  Option      "Name" "VirtualBox Mouse"
  Option      "Protocol" "explorerps/2"
  Option      "Vendor" "Oracle Corporation"
  Option      "ZAxisMapping" "4 5"
  Option      "CorePointer"
EndSection

Section "ServerLayout"
  Identifier   "Layout[all]"
$kbd_layout
  InputDevice  "Mouse[1]" "CorePointer"
  Option       "Clone" "off"
  Option       "Xinerama" "off"
  Screen       "Screen[0]"
EndSection
EOF

    test -z "$auto_mouse" -a -n "$new_mouse" &&
        cat >> "$tmp" << EOF

Section "InputDevice"
  Driver       "mouse"
  Identifier   "Mouse[1]"
  Option       "Buttons" "9"
  Option       "Device" "/dev/input/mice"
  Option       "Name" "VirtualBox Mouse Buttons"
  Option       "Protocol" "explorerps/2"
  Option       "Vendor" "Oracle Corporation"
  Option       "ZAxisMapping" "4 5"
  Option       "CorePointer"
EndSection

Section "InputDevice"
  Driver       "evdev"
  Identifier   "Mouse[2]"
  Option       "Device" "/dev/input/vboxevent"
  Option       "Name" "VirtualBox mouse integration"
  Option       "Vendor" "Oracle Corporation"
  Option       "SendCoreEvents"
EndSection

Section "ServerLayout"
  Identifier   "Layout[all]"
  InputDevice  "Keyboard[0]" "CoreKeyboard"
  InputDevice  "Mouse[1]" "CorePointer"
  InputDevice  "Mouse[2]" "SendCoreEvents"
  Option       "Clone" "off"
  Option       "Xinerama" "off"
  Screen       "Screen[0]"
EndSection
EOF

    cat >> "$tmp" << EOF

Section "Monitor"
  Identifier   "Monitor[0]"
  ModelName    "VirtualBox Virtual Output"
  VendorName   "Oracle Corporation"
EndSection

Section "Device"
  BoardName    "VirtualBox Graphics"
  Driver       "vboxvideo"
  Identifier   "Device[0]"
  VendorName   "Oracle Corporation"
EndSection

Section "Screen"
  SubSection "Display"
    Depth      24
  EndSubSection
  Device       "Device[0]"
  Identifier   "Screen[0]"
  Monitor      "Monitor[0]"
EndSection
EOF

    test -n "$no_bak" -o -f "$cfg.vbox" || cp "$cfg" "$cfg.vbox"
    test -n "$no_bak" || mv "$cfg" "$cfg.bak"
    mv "$tmp" "$cfg"
}

while test -n "$1"
do
    case "$1" in
        --autoMouse)
            auto_mouse=1 ;;
        --newMouse)
            new_mouse=1 ;;
        --noBak)
            no_bak=1 ;;
        --nopsaux)
            old_mouse_dev="/dev/input/mice" ;;
        *)
            reconfigure "$1" ;;
    esac
    shift
done
