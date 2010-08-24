@echo off
rem Copyright (C) 2010 Oracle Corporation
rem
rem This file is part of VirtualBox Open Source Edition (OSE), as
rem available from http://www.virtualbox.org. This file is free software;
rem you can redistribute it and/or modify it under the terms of the GNU
rem General Public License (GPL) as published by the Free Software
rem Foundation, in version 2 as it comes in the "COPYING" file of the
rem VirtualBox OSE distribution. VirtualBox OSE is distributed in the
rem hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

set JAVA_HOME=c:\jdk
set JAVA=%JAVA_HOME%\bin\java
set JAVAC=%JAVA_HOME%\bin\javac
rem use Jacob after 1.15
set JACOB=s:\jacob-1.15-M3

%JAVAC% -cp %JACOB%\jacob.jar VBoxTest.java VBoxCallbacks.java
%JAVA% -cp %JACOB%\jacob.jar;. -Djava.library.path=%JACOB% VBoxTest
