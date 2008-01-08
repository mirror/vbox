#!/usr/bin/perl -w
#
# innotek VirtualBox
#
# Linux Additions X11 config update script
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


my $temp="/tmp/xorg.conf";
my @cfg_files = ("/etc/X11/xorg.conf-4", "/etc/X11/xorg.conf", "/etc/xorg.conf",
                 "/usr/etc/X11/xorg.conf-4", "/usr/etc/X11/xorg.conf", "/usr/lib/X11/xorg.conf-4",
                 "/usr/lib/X11/xorg.conf", "/etc/X11/XF86Config-4", "/etc/X11/XF86Config",
                 "/etc/XF86Config", "/usr/X11R6/etc/X11/XF86Config-4", "/usr/X11R6/etc/X11/XF86Config",
                 "/usr/X11R6/lib/X11/XF86Config-4", "/usr/X11R6/lib/X11/XF86Config");
my $CFG;
my $TMP;

my $count_config = 0;

foreach $cfg (@cfg_files)
{

    if (open(CFG, $cfg))
    {
        open(TMP, ">$temp") or die "Can't create $TMP: $!\n";

        my $have_mouse = 0;
        my $in_section = 0;

        while (defined ($line = <CFG>))
        {
            if ($line =~ /^\s*Section\s*"([a-zA-Z]+)"/i)
            {
                my $section = lc($1);
                if (($section eq "inputdevice") || ($section eq "device"))
                {
                    $in_section = 1;
                }
                if ($section eq "serverlayout")
                {
                    $in_layout = 1;
                }
            } else {
                if ($line =~ /^\s*EndSection/i)
                {
                    $in_section = 0;
                    $in_layout = 0;
                }
            }

            if ($in_section)
            {
                if ($line =~ /^\s*driver\s+\"(?:mouse|vboxmouse)\"/i)
                {
                    $line = "    Driver      \"vboxmouse\"\n    Option      \"CorePointer\"\n";
                    $have_mouse = 1
                }

                # Other drivers sending events interfere badly with pointer integration
                if ($line =~ /^\s*option\s+\"(?:alwayscore|sendcoreevents|corepointer)\"/i)
                {
                    $line = "";
                }

                if ($line =~ /^\s*driver\s+\"(?:fbdev|vga|vesa|vboxvideo|ChangeMe)\"/i)
                {
                    $line = "    Driver      \"vboxvideo\"\n";
                }
            }
            if ($in_layout)
            {
                # Other drivers sending events interfere badly with pointer integration
                if (   $line =~ /^\s*inputdevice.*\"(?:alwayscore|sendcoreevents)\"/i)
                {
                    $line = "";
                }
            }
            print TMP $line;
        }

        rename $cfg, $cfg.".bak";
        system("cp $temp $cfg");
        unlink $temp;

        if ($have_mouse == 0) {
            system("echo >> $cfg");
            system("echo 'Section \"InputDevice\"' >> $cfg");
            system("echo '        Identifier  \"VBoxMouse\"' >> $cfg");
            system("echo '        Driver      \"vboxmouse\"' >> $cfg");
            system("echo '        Option      \"CorePointer\"' >> $cfg");
            system("echo 'EndSection' >> $cfg");
        }
        $config_count++;
    }
}

$config_count != 0 or die "Could not find any X11 configuration files";
