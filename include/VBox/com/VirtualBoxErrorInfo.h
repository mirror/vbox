/* $Id$ */

/** @file
 * MS COM / XPCOM Abstraction Layer:
 * VirtualBoxErrorInfo COM class declaration
 */

/*
 * Copyright (C) 2008-2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_com_VirtualBoxErrorInfo_h
#define ___VBox_com_VirtualBoxErrorInfo_h

#include "VBox/com/defs.h"
#include "VBox/com/string.h"
#include "VBox/com/ptr.h"
#include "VBox/com/Guid.h"

/// @todo this is for IVirtualBoxErrorInfo, see the @todo below.
#include "VBox/com/VirtualBox.h"

namespace com
{

/**
 * The VirtualBoxErrorInfo class implements the IVirtualBoxErrorInfo interface
 * that provides extended error information about interface/component method
 * invocation.
 *
 * @todo Rename IVirtualBoxErrorInfo/VirtualBoxErrorInfo to something like
 *       IExtendedErrorInfo since it's not actually VirtualBox-dependent any
 *       more. This will also require to create IExtendedErrorInfo.idl/h etc to
 *       let adding this class to custom type libraries.
 */
class ATL_NO_VTABLE VirtualBoxErrorInfo
    : public CComObjectRootEx <CComMultiThreadModel>
    , public IVirtualBoxErrorInfo
{
public:

    DECLARE_NOT_AGGREGATABLE (VirtualBoxErrorInfo)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP (VirtualBoxErrorInfo)
        COM_INTERFACE_ENTRY (IErrorInfo)
        COM_INTERFACE_ENTRY (IVirtualBoxErrorInfo)
    END_COM_MAP()

    VirtualBoxErrorInfo() : mResultCode (S_OK) {}

    // public initializer/uninitializer for internal purposes only

    HRESULT init(HRESULT aResultCode,
                 const GUID *aIID,
                 const char *aComponent,
                 const Utf8Str &strText,
                 IVirtualBoxErrorInfo *aNext = NULL);

    // IVirtualBoxErrorInfo properties
    STDMETHOD(COMGETTER(ResultCode)) (LONG *aResultCode);
    STDMETHOD(COMGETTER(InterfaceID)) (BSTR *aIID);
    STDMETHOD(COMGETTER(Component)) (BSTR *aComponent);
    STDMETHOD(COMGETTER(Text)) (BSTR *aText);
    STDMETHOD(COMGETTER(Next)) (IVirtualBoxErrorInfo **aNext);

#if !defined (VBOX_WITH_XPCOM)

    HRESULT init (IErrorInfo *aInfo);

    STDMETHOD(GetGUID) (GUID *guid);
    STDMETHOD(GetSource) (BSTR *source);
    STDMETHOD(GetDescription) (BSTR *description);
    STDMETHOD(GetHelpFile) (BSTR *pBstrHelpFile);
    STDMETHOD(GetHelpContext) (DWORD *pdwHelpContext);

#else /* !defined (VBOX_WITH_XPCOM) */

    HRESULT init (nsIException *aInfo);

    NS_DECL_NSIEXCEPTION
#endif

private:

    HRESULT mResultCode;
    Bstr mText;
    Guid mIID;
    Bstr mComponent;
    ComPtr <IVirtualBoxErrorInfo> mNext;
};

/**
 * The VirtualBoxErrorInfoGlue class glues two IVirtualBoxErrorInfo chains by
 * attaching the head of the second chain to the tail of the first one.
 *
 * This is done by wrapping around each member of the first chain and
 * substituting the next attribute implementation.
 */
class ATL_NO_VTABLE VirtualBoxErrorInfoGlue
    : public CComObjectRootEx <CComMultiThreadModel>
    , public IVirtualBoxErrorInfo
{
public:

    DECLARE_NOT_AGGREGATABLE (VirtualBoxErrorInfoGlue)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP (VirtualBoxErrorInfoGlue)
        COM_INTERFACE_ENTRY (IErrorInfo)
        COM_INTERFACE_ENTRY (IVirtualBoxErrorInfo)
    END_COM_MAP()

    VirtualBoxErrorInfoGlue() {}

    // public initializer/uninitializer for internal purposes only

    HRESULT init (IVirtualBoxErrorInfo *aReal, IVirtualBoxErrorInfo *aNext);

protected:

    HRESULT protectedInit (IVirtualBoxErrorInfo *aReal, IVirtualBoxErrorInfo *aNext);

private:

    // IVirtualBoxErrorInfo properties
    COM_FORWARD_IVirtualBoxErrorInfo_GETTER_ResultCode_TO_OBJ (mReal)
    COM_FORWARD_IVirtualBoxErrorInfo_GETTER_InterfaceID_TO_OBJ (mReal)
    COM_FORWARD_IVirtualBoxErrorInfo_GETTER_Component_TO_OBJ (mReal)
    COM_FORWARD_IVirtualBoxErrorInfo_GETTER_Text_TO_OBJ (mReal)
    STDMETHOD(COMGETTER(Next)) (IVirtualBoxErrorInfo **aNext);

#if !defined (VBOX_WITH_XPCOM)

    STDMETHOD(GetGUID) (GUID *guid) { return mReal->GetGUID (guid); }
    STDMETHOD(GetSource) (BSTR *source) { return mReal->GetSource (source); }
    STDMETHOD(GetDescription) (BSTR *description) { return mReal->GetDescription (description); }
    STDMETHOD(GetHelpFile) (BSTR *pBstrHelpFile) { return mReal->GetHelpFile (pBstrHelpFile); }
    STDMETHOD(GetHelpContext) (DWORD *pdwHelpContext) { return mReal->GetHelpContext (pdwHelpContext); }

#else /* !defined (VBOX_WITH_XPCOM) */

    NS_FORWARD_NSIEXCEPTION (mReal->)

#endif

private:

    ComPtr <IVirtualBoxErrorInfo> mReal;
    ComPtr <IVirtualBoxErrorInfo> mNext;
};

} /* namespace com */

#endif /* ___VBox_com_VirtualBoxErrorInfo_h */

