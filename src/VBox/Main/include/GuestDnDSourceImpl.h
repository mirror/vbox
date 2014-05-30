/* $Id$ */
/** @file
 * VBox Console COM Class implementation - Guest drag'n drop source.
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

#ifndef ____H_GUESTDNDSOURCEIMPL
#define ____H_GUESTDNDSOURCEIMPL

#include "GuestDnDSourceWrap.h"
#include "GuestDnDPrivate.h"

class ATL_NO_VTABLE GuestDnDSource :
    public GuestDnDSourceWrap
{
public:
    /** @name COM and internal init/term/mapping cruft.
     * @{ */
    DECLARE_EMPTY_CTOR_DTOR(GuestDnDSource)

    int     init(const ComObjPtr<Guest>& pGuest);
    void    uninit(void);

    HRESULT FinalConstruct(void);
    void    FinalRelease(void);
    /** @}  */

private:

    /** Wrapped @name IDnDSource methods.
     * @{ */
    HRESULT dragIsPending(ULONG uScreenId, std::vector<com::Utf8Str> &aFormats, std::vector<DnDAction_T> &aAllowedActions, DnDAction_T *aDefaultAction);
    HRESULT drop(const com::Utf8Str &aFormat, DnDAction_T aAction, ComPtr<IProgress> &aProgress);
    HRESULT receiveData(std::vector<BYTE> &aData);
    /** @}  */

protected:

    /** @name Attributes.
     * @{ */
    /** Pointer to guest implementation. */
    const ComObjPtr<Guest>     m_pGuest;
    /** @}  */
};

#endif /* !____H_GUESTDNDSOURCEIMPL */

