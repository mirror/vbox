/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * VirtualBox COM base class stubs
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ____H_AUTOCALLER
#define ____H_AUTOCALLER

#define COMMA_LOCKVAL_SRC_POS

class AutoInitSpan
{
public:
    AutoInitSpan(VirtualBoxBase *) {}
    bool isOk() { return true; }
    void setSucceeded() {}
};

class AutoUninitSpan
{
public:
    AutoUninitSpan(VirtualBoxBase *) {}
    bool uninitDone() { return false; }
};

class AutoCaller
{
public:
    AutoCaller(VirtualBoxBase *) {}
    int rc() { return S_OK; }
};

class AutoWriteLock
{
public:
    AutoWriteLock(VirtualBoxBase *) {}
};

#endif // !____H_AUTOCALLER
