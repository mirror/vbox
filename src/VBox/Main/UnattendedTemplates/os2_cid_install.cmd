@ECHO ON
REM $Id$
REM REM @fileREM
REM VirtualBox CID Installation - main driver script for boot CD/floppy.
REM

REM
REM Copyright (C) 2004-2022 Oracle Corporation
REM
REM This file is part of VirtualBox Open Source Edition (OSE), as
REM available from http://www.virtualbox.org. This file is free software;
REM you can redistribute it and/or modify it under the terms of the GNU
REM General Public License (GPL) as published by the Free Software
REM Foundation, in version 2 as it comes in the "COPYING" file of the
REM VirtualBox OSE distribution. VirtualBox OSE is distributed in the
REM hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
REM

echo .
echo Step 1 - Partition the disk.
echo .
s:
cd s:\os2image\disk_6

lvm.exe /NEWMBR:1 && goto lvm_newmbr_ok
echo ** error: Writing a new MBR on disk 1 failed.
goto lvm_failed
:lvm_newmbr_ok

lvm.exe "/SETNAME:DRIVE,[ D1 ],BootDrive"

lvm.exe /CREATE:PARTITION,OS2Boot,1,1024,PRIMARY,BOOTABLE && goto lvm_create_partition_ok
echo ** error: Creating boot partition on disk 1 failed.
goto lvm_failed
:lvm_create_partition_ok

lvm.exe /CREATE:VOLUME,COMPATIBILITY,BOOTOS2,C:,OS2Boot,1,OS2Boot && goto lvm_create_volume_ok
echo ** error: Creating boot volume on disk 1 failed.
goto lvm_failed
:lvm_create_volume_ok

lvm.exe /SETSTARTABLE:VOLUME,OS2Boot && goto lvm_set_startable_ok
echo ** error: Setting boot volume on disk 1 startable failed.
goto lvm_failed
:lvm_set_startable_ok

lvm.exe "/CREATE:PARTITION,Data,1,LOGICAL,NotBootable,[ FS1 ]" && goto lvm_create_data_partition_ok
echo ** error: Creating data partition on disk 1 failed.
goto lvm_failed
:lvm_create_data_partition_ok

lvm.exe /CREATE:VOLUME,LVM,D:,Data,1,Data && goto lvm_create_data_volume_ok
echo ** error: Creating data volume on disk 1 failed.
goto lvm_failed
:lvm_create_data_volume_ok

REM pause
lvm.exe /QUERY
REM S:\OS2IMAGE\DISK_2\CMD.EXE
goto done_step1

:lvm_failed
echo .
echo An LVM operation failed (see above).
echo The process requires a blank disk with no partitions.  Starting LVM
echo so you can manually correct this.
echo .
PAUSE
lvm.exe
s:\cid\exe\os2\setboot.exe /B
exit

:done_step1

:step2
echo .
echo Step 2 - Format volumes.
echo .
cd s:\os2image\disk_3
s:

FORMAT C: /FS:HPFS /V:OS2Boot < S:\VBoxCID\YES.TXT && goto format_boot_ok
echo ** error: Formatting C: failed.
PAUSE
:format_boot_ok

FORMAT D: /FS:JFS /V:Data < S:\VBoxCID\YES.TXT && goto format_data_ok
echo ** error: Formatting D: failed.
PAUSE
:format_data_ok

:step3
echo .
echo Step 3 - Putting response files and CID tools on C:
echo .
mkdir C:\VBoxCID
mkdir C:\OS2
copy S:\cid\exe\os2\*.*            C:\VBoxCID
copy S:\cid\dll\os2\*.*            C:\VBoxCID
copy S:\os2image\disk_2\inst32.dll C:\VBoxCID
copy S:\VBoxCID\*.*                C:\VBoxCID && goto copy_ok
echo ** error: Copying CID stuff from CDROM to C: failed.
PAUSE
:copy_ok

:step4
echo .
echo Step 4 - Start OS/2 CID installation.
echo .
SET REMOTE_INSTALL_STATE=CAS_WARP4
C:
cd C:\OS2
C:\VBoxCID\SEMAINT.EXE /S:S:\os2image /B:C: /L1:C:\VBoxCID\Maint.log /T:C:\OS2
REM does not exit with status 0 on success.
goto semaint_ok
PAUSE
:semaint_ok
REM S:\OS2IMAGE\DISK_2\CMD.EXE

cd C:\VBoxCID
C:\VBoxCID\SEINST.EXE /S:S:\os2image /B:C: /L1:C:\VBoxCID\CIDInst.log /R:C:\VBoxCID\OS2.RSP /T:A:\
REM does not exit with status 0 on success.
goto seinst_ok
PAUSE
:seinst_ok
REM S:\OS2IMAGE\DISK_2\CMD.EXE

:step5
echo .
echo Step 5 - Make C: bootable.
echo .
C:
cd C:\OS2
SYSINSTX C: && goto sysinstx_ok
pause
:sysinstx_ok

echo Copying over patched OS2LDR from A:
attrib -R -H -S C:\OS2LDR
copy C:\OS2LDR C:\OS2LDR.ORG
del  C:\OS2LDR
copy A:\OS2LDR C:\OS2LDR && goto copy_os2ldr_ok
pause
:copy_os2ldr_ok
attrib +R +H +S C:\OS2LDR

echo Copying over final startup.cmd
if exist C:\STARTUP.CMD ren C:\STARTUP.CMD C:\STARTUP.ORG
copy S:\VBoxCID\STARTUP.CMD C:\STARTUP.CMD

echo Enabling Alt-F2 driver logging during boot.
echo > "C:\ALTF2ON.$$$"

:step6
echo .
echo Step 6 - Cleanup
echo .
cd C:\
del /N C:\*.bio
del /N C:\*.i13
del /N C:\*.snp
del /N C:\CONFIG.ADD
mkdir C:\MMTEMP 2>nul
del /N C:\MMTEMP\*.*
for %%i in (acpadd2 azt16dd azt32dd csbsaud es1688dd es1788dd es1868dd es1888dd es688dd jazzdd mvprobdd mvprodd sb16d2 sbawed2 sbd2 sbp2d2 sbpd2) do del /N C:\MMTEMP\OS2\DRIVERS\%%i\*.*
for %%i in (acpadd2 azt16dd azt32dd csbsaud es1688dd es1788dd es1868dd es1888dd es688dd jazzdd mvprobdd mvprodd sb16d2 sbawed2 sbd2 sbp2d2 sbpd2) do rmdir C:\MMTEMP\OS2\DRIVERS\%%i
rmdir C:\MMTEMP\OS2\DRIVERS
rmdir C:\MMTEMP\OS2
rmdir C:\MMTEMP
copy C:\CONFIG.SYS C:\VBoxCID && del /N C:\*.SYS
copy C:\VBoxCID\CONFIG.SYS C:\

:step7
echo .
echo Step 7 - Install guest additions.
echo .
@@VBOX_COND_IS_INSTALLING_ADDITIONS@@
mkdir C:\VBoxAdd
copy S:\VBoxAdditions\OS2\*.*  C:\VBoxAdd && goto ga_copy_ok
pause
:ga_copy_ok
echo TODO: Write script editing Config.sys for GAs
@@VBOX_COND_ELSE@@
echo Not requested. Skipping.
@@VBOX_COND_END@@

:step8
echo .
echo Step 8 - Install the test execution service (TXS).
echo .
@@VBOX_COND_IS_INSTALLING_TEST_EXEC_SERVICE@@
mkdir C:\ValKit
mkdir D:\TestArea
copy S:\VBoxValidationKit\*.* C:\VBoxValKit && goto valkit_copy_1_ok
pause
:valkit_copy_1_ok
copy S:\VBoxValidationKit\os2\x86\*.* C:\VBoxValKit && goto valkit_copy_2_ok
pause
:valkit_copy_2_ok
@@VBOX_COND_ELSE@@
echo Not requested. Skipping.
@@VBOX_COND_END@@

:step9
@@VBOX_COND_HAS_POST_INSTALL_COMMAND@@
echo .
echo Step 9 - Custom actions: "@@VBOX_INSERT_POST_INSTALL_COMMAND@@"
echo .
cd C:\VBoxCID
C:
@@VBOX_INSERT_POST_INSTALL_COMMAND@@
@@VBOX_COND_END@@


:done
echo .
echo Finally Done.  Now we reboot.
echo .
cd C:\OS2
C:
SETBOOT /IBD:C
PAUSE
S:\OS2IMAGE\DISK_2\CMD.EXE

