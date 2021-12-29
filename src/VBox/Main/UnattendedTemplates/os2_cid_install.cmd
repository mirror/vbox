@ECHO ON
REM
REM VirtualBox CID Installation - main driver script for boot CD/floppy.
REM

echo .
echo Step 1 - Partition the disk.
echo .
s:
cd s:\os2image\disk_6

lvm.exe /NEWMBR:1
if ERRORLEVEL 0 goto lvm_newmbr_ok
echo ** error: Writing a new MBR on disk 1 failed.
goto lvm_failed
:lvm_newmbr_ok

lvm.exe "/SETNAME:DRIVE,[ D1 ],BootDrive"

lvm.exe /CREATE:PARTITION,OS2Boot,1,1024,PRIMARY,BOOTABLE
if ERRORLEVEL 0 goto lvm_create_partition_ok
echo ** error: Creating boot partition on disk 1 failed.
goto lvm_failed
:lvm_create_partition_ok

lvm.exe /CREATE:VOLUME,COMPATIBILITY,BOOTOS2,C:,OS2Boot,1,OS2Boot
if ERRORLEVEL 0 goto lvm_create_volume_ok
echo ** error: Creating boot volume on disk 1 failed.
goto lvm_failed
:lvm_create_volume_ok

lvm.exe /SETSTARTABLE:VOLUME,OS2Boot
if ERRORLEVEL 0 goto lvm_set_startable_ok
echo ** error: Setting boot volume on disk 1 startable failed.
goto lvm_failed
:lvm_set_startable_ok

lvm.exe "/CREATE:PARTITION,Data,1,LOGICAL,NotBootable,[ FS1 ]"
if ERRORLEVEL 0 goto lvm_create_data_partition_ok
echo ** error: Creating data partition on disk 1 failed.
goto lvm_failed
:lvm_create_data_partition_ok

lvm.exe /CREATE:VOLUME,LVM,D:,Data,1,Data
if ERRORLEVEL 0 goto lvm_create_data_volume_ok
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

FORMAT C: /FS:HPFS /V:OS2Boot < S:\VBoxCID\YES.TXT
if ERRORLEVEL 0 goto format_boot_ok
echo ** error: Formatting C: failed.
PAUSE
:format_boot_ok

FORMAT D: /FS:JFS /V:Data < S:\VBoxCID\YES.TXT
if ERRORLEVEL 0 goto format_data_ok
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
copy S:\VBoxCID\*.*                C:\VBoxCID
if ERRORLEVEL 0 goto copy_ok
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
REM if ERRORLEVEL 0 goto semaint_ok - doesn't return 0 on success?
REM PAUSE
:semaint_ok
REM S:\OS2IMAGE\DISK_2\CMD.EXE

cd C:\VBoxCID
C:\VBoxCID\SEINST.EXE /S:S:\os2image /B:C: /L1:C:\VBoxCID\CIDInst.log /R:C:\VBoxCID\OS2.RSP /T:A:\
if ERRORLEVEL 0 goto seinst_ok
PAUSE
:seinst_ok
S:\OS2IMAGE\DISK_2\CMD.EXE

:step5
echo .
echo Step 5 - Make C: bootable.
echo .
c:
cd c:\OS2
SYSINSTX C:
if ERRORLEVEL 0 goto sysinstx_ok
pause
:sysinstx_ok

echo Copying over patched OS2BOOT from A:
cd c:\
c:
attrib -R -H -S OS2BOOT
copy OS2BOOT OS2BOOT.ORG
copy a:\OS2BOOT C:\OS2BOOT
attrib +R +H +S OS2BOOT

echo Copying over final startup.cmd
ren C:\STARTUP.CMD C:\STARTUP.ORG
copy S:\VBoxCID\STARTUP.CMD C:\STARTUP.CMD

echo Enabling Alt-F2 driver logging during boot.
echo > "C:\ALTF2ON.$$$"

:step6
echo .
echo Step 6 - Cleanup
echo .
echo ** skipped

:step7
echo .
echo Step 7 - Post install actions
echo .
cd C:\VBoxCID
C:

:step8
echo .
echo Step 8 - Done.
echo .
cd C:\OS2
C:
SETBOOT /IBD:C
PAUSE
S:\OS2IMAGE\DISK_2\CMD.EXE

