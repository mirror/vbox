@ECHO OFF
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

REM Check the phase argument and jump to the right section of the file.
if "%1" == "PHASE1" goto phase1
if "%1" == "PHASE2" goto phase2
@echo ** error: invalid or missing parameter. Expected PHASE1 or PHASE2 as the first parameter to the script.
pause
cmd.exe
exit /b 1

REM
REM Phase 1 - Base system installation.
REM
:phase1
SET CDROM=S:

@echo on
@echo .
@echo Step 1.1 - Partition the disk.
@echo .
cd %CDROM%\os2image\disk_6
%CDROM%

lvm.exe /NEWMBR:1 && goto lvm_newmbr_ok
@echo ** error: Writing a new MBR on disk 1 failed.
goto lvm_failed
:lvm_newmbr_ok

@REM Depends the default drive name being "[ D1 ]".  However it's cosmetical,
@REM so we don't complain if this fails.
lvm.exe "/SETNAME:DRIVE,[ D1 ],BootDrive"

lvm.exe /CREATE:PARTITION,OS2Boot,1,1024,PRIMARY,BOOTABLE && goto lvm_create_partition_ok
@echo ** error: Creating boot partition on disk 1 failed.
goto lvm_failed
:lvm_create_partition_ok

lvm.exe /CREATE:VOLUME,COMPATIBILITY,BOOTOS2,C:,OS2Boot,1,OS2Boot && goto lvm_create_volume_ok
@echo ** error: Creating boot volume on disk 1 failed.
goto lvm_failed
:lvm_create_volume_ok

lvm.exe /SETSTARTABLE:VOLUME,OS2Boot && goto lvm_set_startable_ok
@echo ** error: Setting boot volume on disk 1 startable failed.
goto lvm_failed
:lvm_set_startable_ok

@REM Depending on the freespace automatically getting the name "[ FS1 ]".
lvm.exe "/CREATE:PARTITION,Data,1,LOGICAL,NotBootable,[ FS1 ]" && goto lvm_create_data_partition_ok
@echo ** error: Creating data partition on disk 1 failed.
goto lvm_failed
:lvm_create_data_partition_ok

lvm.exe /CREATE:VOLUME,LVM,D:,Data,1,Data && goto lvm_create_data_volume_ok
@echo ** error: Creating data volume on disk 1 failed.
goto lvm_failed
:lvm_create_data_volume_ok

REM pause
lvm.exe /QUERY
REM CMD.EXE
goto done_step1_1

:lvm_failed
@echo .
@echo An LVM operation failed (see above).
@echo The process requires a blank disk with no partitions.  Starting LVM
@echo so you can manually correct this.
@echo .
pause
lvm.exe
%CDROM%\cid\exe\os2\setboot.exe /B
exit

:done_step1_1

:step1_2
@echo .
@echo Step 1.2 - Format the volumes.
@echo .
cd %CDROM%\os2image\disk_3
%CDROM%

FORMAT.COM C: /FS:HPFS /V:OS2Boot < %CDROM%\VBoxCID\YES.TXT && goto format_boot_ok
@echo ** error: Formatting C: failed.
pause
:format_boot_ok

FORMAT.COM D: /FS:JFS /V:Data < %CDROM%\VBoxCID\YES.TXT && goto format_data_ok
@echo ** error: Formatting D: failed.
pause
:format_data_ok

cd \

:step1_3
@echo .
@echo Step 1.3 - Putting response files and CID tools on C:
@echo .
mkdir C:\VBoxCID
mkdir C:\OS2
copy %CDROM%\cid\exe\os2\*.*            C:\VBoxCID
copy %CDROM%\cid\dll\os2\*.*            C:\VBoxCID
copy %CDROM%\os2image\disk_2\inst32.dll C:\VBoxCID
copy %CDROM%\VBoxCID\*.*                C:\VBoxCID && goto copy_1_ok
@echo ** error: Copying CID stuff from CDROM to C: failed (#1).
pause
:copy_1_ok
copy %CDROM%\VBoxCID.CMD                C:\VBoxCID && goto copy_2_ok
@echo ** error: Copying CID stuff from CDROM to C: failed (#2).
pause
:copy_2_ok

:step1_4
@echo .
@echo Step 1.4 - Start OS/2 CID installation.
@echo .
SET REMOTE_INSTALL_STATE=CAS_WARP4
cd C:\OS2
C:
C:\VBoxCID\OS2_UTIL.EXE -- C:\VBoxCID\SEMAINT.EXE /S:%CDROM%\os2image /B:C: /L1:C:\VBoxCID\1.4.1-Maint.log /T:C:\OS2
REM does not exit with status 0 on success.
goto semaint_ok
pause
:semaint_ok
REM CMD.EXE

cd C:\VBoxCID
C:
C:\VBoxCID\OS2_UTIL.EXE -- C:\VBoxCID\SEINST.EXE /S:%CDROM%\os2image /B:C: /L1:C:\VBoxCID\1.4.2-CIDInst.log /R:C:\VBoxCID\OS2.RSP /T:A:\
REM does not exit with status 0 on success.
goto seinst_ok
pause
:seinst_ok
REM CMD.EXE

:step1_5
@echo .
@echo Step 1.5 - Make C: bootable.
@echo .
C:
cd C:\OS2
SYSINSTX.COM C: && goto sysinstx_ok
pause
:sysinstx_ok

@echo Copying over patched OS2LDR from A:
attrib -R -H -S C:\OS2LDR
copy C:\OS2LDR C:\OS2LDR.Phase1
del  C:\OS2LDR
copy A:\OS2LDR C:\OS2LDR && goto copy_os2ldr_ok
pause
:copy_os2ldr_ok
attrib +R +H +S C:\OS2LDR

@REM This copy is for the end of phase 2 as someone replaces it.
copy A:\OS2LDR C:\VBoxCID && goto copy_os2ldr_2_ok
pause
:copy_os2ldr_2_ok
attrib +r C:\VBoxCID\OS2LDR

@echo Enabling Alt-F2 driver logging during boot.
@echo > "C:\ALTF2ON.$$$"

@echo Install startup.cmd for phase2.
@echo C:\VBoxCID\OS2_UTIL.EXE --tee-to-backdoor --tee-to-file C:\VBoxCID\Phase2.log --append -- C:\OS2\CMD.EXE /C C:\VBoxCID\VBoxCID.CMD PHASE2> C:\STARTUP.CMD && goto phase2_startup_ok
pause
:phase2_startup_ok

REM now reboot.
goto reboot


REM
REM Phase 2 - The rest of the installation running of the base install.
REM
:phase2
SET CDROM=E:
IF EXIST "%CDROM%\VBoxCID.CMD" goto phase2_found_cdrom
SET CDROM=D:
IF EXIST "%CDROM%\VBoxCID.CMD" goto phase2_found_cdrom
SET CDROM=F:
IF EXIST "%CDROM%\VBoxCID.CMD" goto phase2_found_cdrom
SET CDROM=G:
IF EXIST "%CDROM%\VBoxCID.CMD" goto phase2_found_cdrom
SET CDROM=H:
IF EXIST "%CDROM%\VBoxCID.CMD" goto phase2_found_cdrom
SET CDROM=S:
IF EXIST "%CDROM%\VBoxCID.CMD" goto phase2_found_cdrom
@echo ** error: Unable to find the CDROM drive
pause
CMD
SET CDROM=E:
:phase2_found_cdrom
cd C:\VBoxCID
C:

@echo on

:step2_1
@echo .
@echo Step 2.1 - Install the video driver.
@echo .
C:\VBoxCID\OS2_UTIL.EXE -- C:\OS2\INSTALL\DspInstl.EXE /PD:C:\OS2\INSTALL\GENGRADD.DSC /S:%CDROM%\OS2IMAGE /T:C: /RES:1024X768X16777216 /U
@REM does not exit with status 0 on success.
goto dspinstl_ok
pause
:dspinstl_ok

call VCfgCID.CMD /L1:C:\VBoxCID\2.1-Video.log /L2:C:\VBoxCID\2.1-Video-2.log /RES:1024X768X16777216
@REM TODO: Error: 1 Error getting current desktop mode
goto vcfgcid_ok
pause
:vcfgcid_ok
cd C:\VBoxCID
C:

:step2_2
@echo .
@echo Step 2.2 - Install multimedia.
@echo .
cd C:\mmtemp
C:
DIR
C:\VBoxCID\OS2_UTIL.EXE -- MInstall.EXE /M /R:C:\VBoxCID\MMOS2.RSP && goto mmos2_ok
@REM TODO: crashes
pause
:mmos2_ok
DIR
cd C:\VBoxCID

:step2_3
@echo .
@echo Step 2.3 - Install features.
@echo .
C:\VBoxCID\OS2_UTIL.EXE -- CLIFI.EXE /A:C /B:C: /S:%CDROM%\os2image\fi /R:C:\OS2\INSTALL\FIBASE.RSP /L1:C:\VBoxCID\2.2-FeatureInstaller.log /R2:C:\VBoxCID\OS2.RSP
@REM does not exit with status 0 on success.
goto features_ok
pause
:features_ok

:step2_4
@echo .
@echo Step 2.4 - Install Netscape.
@echo .
CD C:\VBoxCID
C:
%CDROM%
SET DPATH=%DPATH%;C:\NETSCAPE\SIUTIL;
C:\VBoxCID\OS2_UTIL.EXE -- %CDROM%\CID\SERVER\NETSCAPE\INSTALL.EXE /X /A:I /TU:C: /C:%CDROM%\CID\SERVER\NETSCAPE\NS46.ICF /S:%CDROM%\CID\SERVER\NETSCAPE /R:C:\VBoxCID\Netscape.RSP /L1:C:\VBoxCID\2.8-Netscape.log /L2:C:\VBoxCID\2.8-Netscape-2.log && goto netscape_ok
:netscape_ok
CD %CDROM%\
C:

:step2_5
@echo .
@echo Step 2.5 - Install feature installer.
@echo .
@REM No /L2: support.
@REM The /NN option is to make it not fail if netscape is missing.
C:\VBoxCID\OS2_UTIL.EXE -- C:\OS2\INSTALL\WSFI\FiSetup.EXE /B:C: /S:C:\OS2\INSTALL\WSFI\FISETUP /NN /L1:C:\VBoxCID\2.5-FiSetup.log && goto fisetup_ok
pause
:fisetup_ok

:step2_6
@echo .
@echo Step 2.6 - Install MPTS.
@echo .
@REM If we want to use non-standard drivers like the intel ones, copy the .NIF- and
@REM .OS2-files to C:\IBMCOM\MACS before launching the installer (needs creating first).
@REM Note! Does not accept /L2:.
@REM Note! Omitting /TU:C in hope that it solves the lan install failure (no netbeui configured in mpts).
CD %CDROM%\CID\SERVER\MPTS
%CDROM%
C:\VBoxCID\OS2_UTIL.EXE -- %CDROM%\CID\SERVER\MPTS\MPTS.EXE /R:C:\VBoxCID\MPTS.RSP /S:%CDROM%\CID\SERVER\MPTS /T:C: /L1:C:\VBoxCID\2.6-Mpts.log && goto mpts_ok
pause
:mpts_ok
CD %CDROM%\
C:

:step2_7
@echo .
@echo Step 2.7 - Install TCP/IP.
@echo .
CD %CDROM%\CID\SERVER\TCPAPPS
%CDROM%
C:\VBoxCID\OS2_UTIL.EXE -- CLIFI.EXE /A:C /B:C: /S:%CDROM%\CID\SERVER\TCPAPPS\INSTALL /R:%CDROM%\CID\SERVER\TCPAPPS\INSTALL\TCPINST.RSP /L1:C:\VBoxCID\2.7-tcp.log /L2:C:\VBoxCID\2.7-tcp-2.log
@REM does not exit with status 0 on success.
goto tcp_ok
pause
:tcp_ok
CD %CDROM%\
C:

CD %CDROM%\CID\SERVER\TCPAPPS\INSTALL
%CDROM%
C:\VBoxCID\OS2_UTIL.EXE -- %CDROM%\CID\SERVER\TCPAPPS\INSTALL\makecmd.exe C:\TCPIP en_US C:\MPTS && goto makecmd_ok
pause
:makecmd_ok
cd %CDROM%\

:step2_8
@echo .
@echo Step 2.8 - Install IBM LAN Requestor/Peer.
@echo .
SET REMOTE_INSTALL_STATE=CAS_OS/2 Peer
CD %CDROM%\CID\SERVER\IBMLS
%CDROM%
C:\VBoxCID\OS2_UTIL.EXE -- %CDROM%\CID\SERVER\IBMLS\LANINSTR.EXE /REQ /R:C:\VBoxCID\IBMLan.rsp /L1:C:\VBoxCID\2.8-IBMLan.log /L2:C:\VBoxCID\2.8-IBMLan-2.log && goto ibmlan_ok
:ibmlan_ok
CD %CDROM%\
C:

:step2_9
@echo .
@echo Step 2.9 - Install guest additions.
@echo .
@@VBOX_COND_IS_INSTALLING_ADDITIONS@@
mkdir C:\VBoxAdd
copy %CDROM%\VBoxAdditions\OS2\*.*  C:\VBoxAdd && goto ga_copy_ok
pause
:ga_copy_ok
@echo TODO: Write script editing Config.sys for GAs
@@VBOX_COND_ELSE@@
@echo Not requested. Skipping.
@@VBOX_COND_END@@

:step2_10
@echo .
@echo Step 2.10 - Install the test execution service (TXS).
@echo .
@@VBOX_COND_IS_INSTALLING_TEST_EXEC_SERVICE@@
mkdir C:\ValKit
mkdir D:\TestArea
copy %CDROM%\VBoxValidationKit\*.* C:\VBoxValKit && goto valkit_copy_1_ok
pause
:valkit_copy_1_ok
copy %CDROM%\VBoxValidationKit\os2\x86\*.* C:\VBoxValKit && goto valkit_copy_2_ok
pause
:valkit_copy_2_ok
@@VBOX_COND_ELSE@@
@echo Not requested. Skipping.
@@VBOX_COND_END@@


:step2_11
@echo .
@echo Step 2.11 - Install final startup.cmd and copy over OS2LDR again.
@echo .
attrib -r -h -s C:\STARTUP.CMD
copy C:\VBoxCID\STARTUP.CMD C:\ && goto final_startup_ok
pause
:final_startup_ok

attrib -r -h -s C:\OS2LDR
if not exist C:\VBoxCID\OS2LDR pause
if not exist C:\VBoxCID\OS2LDR goto final_os2ldr_ok
copy C:\OS2LDR C:\OS2LDR.Phase2
del  C:\OS2LDR
copy C:\VBoxCID\OS2LDR C:\OS2LDR && goto final_os2ldr_ok
pause
:final_os2ldr_ok
attrib +r +h +s C:\OS2LDR

:step2_12
@echo .
@echo Step 2.12 - Cleanup
@echo .
del /N C:\*.bio
del /N C:\*.i13
del /N C:\*.snp
del /N C:\CONFIG.ADD
mkdir C:\MMTEMP 2>nul
del /N C:\MMTEMP\*.*
@REM This is only needed if we don't install mmos2:
@REM for %%i in (acpadd2 azt16dd azt32dd csbsaud es1688dd es1788dd es1868dd es1888dd es688dd jazzdd mvprobdd mvprodd sb16d2 sbawed2 sbd2 sbp2d2 sbpd2) do del /N C:\MMTEMP\OS2\DRIVERS\%%i\*.*
@REM for %%i in (acpadd2 azt16dd azt32dd csbsaud es1688dd es1788dd es1868dd es1888dd es688dd jazzdd mvprobdd mvprodd sb16d2 sbawed2 sbd2 sbp2d2 sbpd2) do rmdir C:\MMTEMP\OS2\DRIVERS\%%i
@REM rmdir C:\MMTEMP\OS2\DRIVERS
@REM rmdir C:\MMTEMP\OS2
rmdir C:\MMTEMP
copy C:\CONFIG.SYS C:\VBoxCID || goto skip_sys_cleanup
del /N C:\*.SYS
copy C:\VBoxCID\CONFIG.SYS C:\
:skip_sys_cleanup

:step2_13
@@VBOX_COND_HAS_POST_INSTALL_COMMAND@@
@echo .
@echo Step 2.13 - Custom actions: "@@VBOX_INSERT_POST_INSTALL_COMMAND@@"
@echo .
cd C:\VBoxCID
C:
@@VBOX_INSERT_POST_INSTALL_COMMAND@@
@@VBOX_COND_END@@


REM
REM Reboot (common to both phases).
REM
:reboot
@echo .
@echo Reboot (%1)
@echo .
cd C:\OS2
C:

@echo debug
CMD.EXE

SETBOOT /IBD:C
pause
CMD.EXE

