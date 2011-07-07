@echo off
rem $Id$
rem rem @file
rem Windows NT batch script for launching gen-slickedit-workspace.sh
rem

rem
rem Copyright (C) 2009 Oracle Corporation
rem
rem Oracle Corporation confidential
rem All rights reserved
rem

setlocal ENABLEEXTENSIONS
setlocal

rem
rem gen-slickedit-workspace.sh should be in the same directory as this script.
rem
set MY_SCRIPT=%~dp0gen-slickedit-workspace.sh
if exist "%MY_SCRIPT%" goto found
echo gen-slickedit-workspace.cmd: failed to find gen-slickedit-workspace.sh in "%~dp0".
goto end

rem
rem Found it, convert slashes and tell kmk_ash to interpret it.
rem
:found
set MY_SCRIPT=%MY_SCRIPT:\=/%
set MY_ARGS=%*
if ".%MY_ARGS%." NEQ ".." set MY_ARGS=%MY_ARGS:\=/%
kmk_ash %MY_SCRIPT% --windows-host %MY_ARGS%

:end
endlocal
endlocal

