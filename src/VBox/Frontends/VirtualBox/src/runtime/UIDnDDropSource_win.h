/* $Id$ */
/** @file
 * VBox Qt GUI - UIDnDDropSource class declaration.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIDnDDropSource_win_h___
#define ___UIDnDDropSource_win_h___

/* COM includes: */
#include "COMEnums.h"

class UIDnDDropSource : public IDropSource
{
public:

    UIDnDDropSource(QWidget *pParent);
    virtual ~UIDnDDropSource(void);

public:

    uint32_t GetCurrentAction(void) { return muCurAction; }

public: /* IUnknown methods. */

    STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
    STDMETHOD_(ULONG, AddRef)(void);
    STDMETHOD_(ULONG, Release)(void);

public: /* IDropSource methods. */

    STDMETHOD(QueryContinueDrag)(BOOL fEscapePressed, DWORD dwKeyState);
    STDMETHOD(GiveFeedback)(DWORD dwEffect);

protected:

    LONG mRefCount;
    QWidget *mpParent;
    DWORD mdwCurEffect;
    Qt::DropActions muCurAction;
};

#endif /* ___UIDnDDropSource_win_h___ */

