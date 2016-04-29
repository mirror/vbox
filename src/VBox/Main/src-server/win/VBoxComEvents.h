/* $Id$ */
/** @file
 *
 *  Declaration of COM Events Helper routines.
 */

/*
 * Copyright (C) 2010-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBOXCOMEVENTS_h__
#define __VBOXCOMEVENTS_h__

#include <map>

#include "VBox/com/string.h"
#include "VBox/com/guid.h"

#include <VBox/err.h>


class ComEventDesc
{
public:
 ComEventDesc()
   : mArgc(0), mArgs(0), mPos(0)
 {}
 ~ComEventDesc()
 {
   if (mArgs)
      delete [] mArgs;
 }

 void init(const char *name, int argc)
 {
   // copies content
   mName = name;
   mArgc = argc;
   if (mArgs)
     delete [] mArgs;
   mArgs = new tagVARIANT[mArgc];
   mPos = argc - 1;
 }

 template <class T>
 ComEventDesc &add(T v)
 {
   Assert(mPos>= 0);
   mArgs[mPos] = v;
   mPos--;
   return *this;
 }

private:
 com::Utf8Str mName;
 int          mArgc;
 tagVARIANT  *mArgs;
 int          mPos;

 friend class ComEventsHelper;
};

class ComEventsHelper
{
public:
    ComEventsHelper();
    ~ComEventsHelper();

    HRESULT init(const com::Guid &aGuid);
    HRESULT lookup(com::Utf8Str &aName, DISPID *did);
    HRESULT fire(IDispatch *aObj, ComEventDesc &desc, tagVARIANT *pResult);

private:
    typedef std::map<com::Utf8Str, DISPID> ComEventsMap;

    ComEventsMap evMap;
};

#endif /* __VBOXCOMEVENTS_h__ */
