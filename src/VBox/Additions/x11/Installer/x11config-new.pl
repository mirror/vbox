#!/usr/bin/perl -w
#
# Sun VirtualBox
#
# Guest Additions X11 config update script
#
# Copyright (C) 2006-2009 Sun Microsystems, Inc.
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#
# Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
# Clara, CA 95054 USA or visit http://www.sun.com if you need
# additional information or have any questions.
#

my $use_hal = 0;
my $new_mouse = 0;
my $no_bak = 0;

foreach $arg (@ARGV)
{
    if (lc($arg) eq "--usehal")
    {
        $use_hal = 1;
    }
    elsif (lc($arg) eq "--newmouse")
    {
        $new_mouse = 1;
    }
    elsif (lc($arg) eq "--nobak")
    {
        $no_bak = 1;
    }
    else
    {
        my $cfg = $arg;
        my $CFG;
        my $xkbopts = "";
        my $kb_driver = "";
        if (open(CFG, $cfg))
        {
            my $TMP;
            my $temp = $cfg.".vbox.tmp";
            open(TMP, ">$temp") or die "Can't create $TMP: $!\n";

            my $in_section = 0;
            print TMP "# VirtualBox generated configuration file\n";
            print TMP "# based on $cfg.\n";

            while (defined ($line = <CFG>))
            {
                if ($line =~ /^\s*Section\s*"([a-zA-Z]+)"/i)
                {
                    my $section = lc($1);
                    if (   ($section eq "inputdevice")
                        || ($section eq "device")
                        || ($section eq "serverlayout")
                        || ($section eq "screen")
                        || ($section eq "monitor")
                        || ($section eq "keyboard")
                        || ($section eq "pointer"))
                    {
                        $in_section = 1;
                    }
                } else {
                    if ($line =~ /^\s*EndSection/i)
                    {
                        $line = "# " . $line unless $in_section eq 0;
                        $in_section = 0;
                    }
                }

                if ($in_section)
                {
                    # Remember XKB options
                    if ($line =~ /^\s*option\s+\"xkb/i)
                    {
                        $xkbopts = $xkbopts . $line;
                    }
                    # If we find a keyboard driver, remember it
                    if ($line =~ /^\s*driver\s+\"(kbd|keyboard)\"/i)
                    {
                        $kb_driver = $1;
                    }
                    $line = "# " . $line;
                }
                print TMP $line;
            }

            if ($kb_driver ne "")
            {
                print TMP <<EOF;
Section "InputDevice"
  Identifier   "keyboard[0]"
  Driver       "$kb_driver"
$xkbopts  Option       "Protocol" "Standard"
  Option       "CoreKeyboard"
EndSection
EOF
            }

            if (!$use_hal && !$new_mouse) {
                print TMP <<EOF;

Section "InputDevice"
  Identifier  "mouse"
  Driver      "vboxmouse"
  Option      "Buttons" "9"
  Option      "Device" "/dev/input/mice"
  Option      "Name" "VirtualBox Mouse Buttons"
  Option      "Protocol" "explorerps/2"
  Option      "Vendor" "Sun Microsystems Inc"
  Option      "ZAxisMapping" "4 5"
  Option      "CorePointer"
EndSection
EOF
            }

            if (!$use_hal && $new_mouse) {
                print TMP <<EOF;

Section "InputDevice"
  Driver       "mouse"
  Identifier   "Mouse[1]"
  Option       "Buttons" "9"
  Option       "Device" "/dev/input/mice"
  Option       "Name" "VirtualBox Mouse Buttons"
  Option       "Protocol" "explorerps/2"
  Option       "Vendor" "Sun Microsystems Inc"
  Option       "ZAxisMapping" "4 5"
  Option       "CorePointer"
EndSection

Section "InputDevice"
  Driver       "vboxmouse"
  Identifier   "Mouse[2]"
  Option       "Device" "/dev/vboxguest"
  Option       "Name" "VirtualBox Mouse"
  Option       "Vendor" "Sun Microsystems Inc"
  Option       "SendCoreEvents"
EndSection

Section "ServerLayout"
  Identifier   "Layout[all]"
  InputDevice  "Mouse[1]" "CorePointer"
  InputDevice  "Mouse[2]" "SendCoreEvents"
  Option       "Clone" "off"
  Option       "Xinerama" "off"
  Screen       "Screen[0]"
EndSection
EOF
            }

            print TMP <<EOF;

Section "Monitor"
  Identifier   "Monitor[0]"
  ModelName    "VirtualBox Virtual Output"
  VendorName   "Sun Microsystems Inc"
EndSection

Section "Device"
  BoardName    "VirtualBox Graphics"
  Driver       "vboxvideo"
  Identifier   "Device[0]"
  VendorName   "Sun Microsystems Inc"
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
            close(TMP);

            system("cp", $cfg, $cfg.".vbox") unless
                ($no_bak eq 1 || -e $cfg.".vbox");
            rename $cfg, $cfg.".bak" unless $no_bak eq 1;
            rename $temp, $cfg;
        }
    }
}
exit 0
