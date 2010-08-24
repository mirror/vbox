@echo off
REM  Copyright (C) 2010 Oracle Corporation
REM
REM  This file is part of VirtualBox Open Source Edition (OSE), as
REM  available from http://www.virtualbox.org. This file is free software;
REM  you can redistribute it and/or modify it under the terms of the GNU
REM  General Public License (GPL) as published by the Free Software
REM  Foundation, in version 2 as it comes in the "COPYING" file of the
REM  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
REM  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
REM

set JAVA_HOME=c:\jdk
set JAVA=%JAVA_HOME%\bin\java
set JAVAC=%JAVA_HOME%\bin\javac
rem use Jacob after 1.15
set JACOB=s:\jacob-1.15-M3

%JAVAC% -cp %JACOB%\jacob.jar VBoxTest.java VBoxCallbacks.java
%JAVA% -cp %JACOB%\jacob.jar;. -Djava.library.path=%JACOB% VBoxTest
