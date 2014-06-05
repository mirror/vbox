/* $Id$ */
/** @file
 * VBox Console COM Class implementation - Guest drag'n drop target.
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

#ifndef ____H_GUESTDNDTARGETIMPL
#define ____H_GUESTDNDTARGETIMPL

#include "GuestDnDTargetWrap.h"
#include "GuestDnDPrivate.h"

class ATL_NO_VTABLE GuestDnDTarget :
    public GuestDnDTargetWrap,
    protected GuestDnDBase
{
public:
    /** @name COM and internal init/term/mapping cruft.
     * @{ */
    DECLARE_EMPTY_CTOR_DTOR(GuestDnDTarget)

    int     init(const ComObjPtr<Guest>& pGuest);
    void    uninit(void);

    HRESULT FinalConstruct(void);
    void    FinalRelease(void);
    /** @}  */

private:

    /** Private wrapped @name IDnDBase methods.
     * @{ */
    HRESULT isFormatSupported(const com::Utf8Str &aFormat, BOOL *aSupported);
    HRESULT getFormats(std::vector<com::Utf8Str> &aFormats);
    HRESULT addFormats(const std::vector<com::Utf8Str> &aFormats);
    HRESULT removeFormats(const std::vector<com::Utf8Str> &aFormats);
    /** @}  */

    /** Private wrapped @name IDnDTarget methods.
     * @{ */
    HRESULT enter(ULONG aScreenId, ULONG ax, ULONG aY, DnDAction_T aDefaultAction, const std::vector<DnDAction_T> &aAllowedActions, const std::vector<com::Utf8Str> &aFormats, DnDAction_T *aResultAction);
    HRESULT move(ULONG aScreenId, ULONG aX, ULONG aY, DnDAction_T aDefaultAction, const std::vector<DnDAction_T> &aAllowedActions, const std::vector<com::Utf8Str> &aFormats, DnDAction_T *aResultAction);
    HRESULT leave(ULONG aScreenId);
    HRESULT drop(ULONG aScreenId, ULONG aX, ULONG aY, DnDAction_T aDefaultAction, const std::vector<DnDAction_T> &aAllowedActions, const std::vector<com::Utf8Str> &aFormats, com::Utf8Str &aFormat, DnDAction_T *aResultAction);
    HRESULT sendData(ULONG aScreenId, const com::Utf8Str &aFormat, const std::vector<BYTE> &aData, ComPtr<IProgress> &aProgress);
    /** @}  */

protected:

    /** @name Attributes.
     * @{ */
    /** Pointer to guest implementation. */
    const ComObjPtr<Guest>     m_pGuest;
    /** @}  */
};

#endif /* !____H_GUESTDNDTARGETIMPL */

