/* $Id$ */
/** @file
 * VBoxManage - VirtualBox's command-line interface.
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

#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint2.h>
#include <VBox/com/EventQueue.h>

#include <VBox/com/VirtualBox.h>

#include "VBoxManage.h"

#include <iprt/asm.h>

/* missing XPCOM <-> COM wrappers */
#ifndef STDMETHOD_
# define STDMETHOD_(ret, meth) NS_IMETHOD_(ret) meth
#endif
#ifndef NS_GET_IID
# define NS_GET_IID(I) IID_##I
#endif
#ifndef RT_OS_WINDOWS
#define IUnknown nsISupports
#endif

using namespace com;

/**
 * Quick IUSBDevice implementation for detaching / attaching
 * devices to the USB Controller.
 */
class MyUSBDevice : public IUSBDevice
{
public:
    // public initializer/uninitializer for internal purposes only
    MyUSBDevice(uint16_t a_u16VendorId, uint16_t a_u16ProductId, uint16_t a_bcdRevision, uint64_t a_u64SerialHash, const char *a_pszComment)
        :  m_usVendorId(a_u16VendorId), m_usProductId(a_u16ProductId),
           m_bcdRevision(a_bcdRevision), m_u64SerialHash(a_u64SerialHash),
           m_bstrComment(a_pszComment),
           m_cRefs(0)
    {
    }

    STDMETHOD_(ULONG, AddRef)(void)
    {
        return ASMAtomicIncU32(&m_cRefs);
    }
    STDMETHOD_(ULONG, Release)(void)
    {
        ULONG cRefs = ASMAtomicDecU32(&m_cRefs);
        if (!cRefs)
            delete this;
        return cRefs;
    }
    STDMETHOD(QueryInterface)(const IID &iid, void **ppvObject)
    {
        Guid guid(iid);
        if (guid == Guid(NS_GET_IID(IUnknown)))
            *ppvObject = (IUnknown *)this;
        else if (guid == Guid(NS_GET_IID(IUSBDevice)))
            *ppvObject = (IUSBDevice *)this;
        else
            return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }

    STDMETHOD(COMGETTER(Id))(OUT_GUID a_pId)                    { return E_NOTIMPL; }
    STDMETHOD(COMGETTER(VendorId))(USHORT *a_pusVendorId)       { *a_pusVendorId    = m_usVendorId;     return S_OK; }
    STDMETHOD(COMGETTER(ProductId))(USHORT *a_pusProductId)     { *a_pusProductId   = m_usProductId;    return S_OK; }
    STDMETHOD(COMGETTER(Revision))(USHORT *a_pusRevision)       { *a_pusRevision    = m_bcdRevision;    return S_OK; }
    STDMETHOD(COMGETTER(SerialHash))(ULONG64 *a_pullSerialHash) { *a_pullSerialHash = m_u64SerialHash;  return S_OK; }
    STDMETHOD(COMGETTER(Manufacturer))(BSTR *a_pManufacturer)   { return E_NOTIMPL; }
    STDMETHOD(COMGETTER(Product))(BSTR *a_pProduct)             { return E_NOTIMPL; }
    STDMETHOD(COMGETTER(SerialNumber))(BSTR *a_pSerialNumber)   { return E_NOTIMPL; }
    STDMETHOD(COMGETTER(Address))(BSTR *a_pAddress)             { return E_NOTIMPL; }

private:
    /** The vendor id of this USB device. */
    USHORT m_usVendorId;
    /** The product id of this USB device. */
    USHORT m_usProductId;
    /** The product revision number of this USB device.
     * (high byte = integer; low byte = decimal) */
    USHORT m_bcdRevision;
    /** The USB serial hash of the device. */
    uint64_t m_u64SerialHash;
    /** The user comment string. */
    Bstr     m_bstrComment;
    /** Reference counter. */
    uint32_t volatile m_cRefs;
};


// types
///////////////////////////////////////////////////////////////////////////////

template <typename T>
class Nullable
{
public:

    Nullable() : mIsNull (true) {}
    Nullable (const T &aValue, bool aIsNull = false)
        : mIsNull (aIsNull), mValue (aValue) {}

    bool isNull() const { return mIsNull; };
    void setNull (bool aIsNull = true) { mIsNull = aIsNull; }

    operator const T&() const { return mValue; }

    Nullable &operator= (const T &aValue)
    {
        mValue = aValue;
        mIsNull = false;
        return *this;
    }

private:

    bool mIsNull;
    T mValue;
};

/** helper structure to encapsulate USB filter manipulation commands */
struct USBFilterCmd
{
    struct USBFilter
    {
        USBFilter ()
            : mAction (USBDeviceFilterAction_Null)
            {}

        Bstr mName;
        Nullable <bool> mActive;
        Bstr mVendorId;
        Bstr mProductId;
        Bstr mRevision;
        Bstr mManufacturer;
        Bstr mProduct;
        Bstr mRemote;
        Bstr mSerialNumber;
        Nullable <ULONG> mMaskedInterfaces;
        USBDeviceFilterAction_T mAction;
    };

    enum Action { Invalid, Add, Modify, Remove };

    USBFilterCmd() : mAction (Invalid), mIndex (0), mGlobal (false) {}

    Action mAction;
    uint32_t mIndex;
    /** flag whether the command target is a global filter */
    bool mGlobal;
    /** machine this command is targeted at (null for global filters) */
    ComPtr<IMachine> mMachine;
    USBFilter mFilter;
};

int handleUSBFilter (HandlerArg *a)
{
    HRESULT rc = S_OK;
    USBFilterCmd cmd;

    /* at least: 0: command, 1: index, 2: --target, 3: <target value> */
    if (a->argc < 4)
        return errorSyntax(USAGE_USBFILTER, "Not enough parameters");

    /* which command? */
    cmd.mAction = USBFilterCmd::Invalid;
    if      (!strcmp(a->argv[0], "add"))     cmd.mAction = USBFilterCmd::Add;
    else if (!strcmp(a->argv[0], "modify"))  cmd.mAction = USBFilterCmd::Modify;
    else if (!strcmp(a->argv[0], "remove"))  cmd.mAction = USBFilterCmd::Remove;

    if (cmd.mAction == USBFilterCmd::Invalid)
        return errorSyntax(USAGE_USBFILTER, "Invalid parameter '%s'", a->argv[0]);

    /* which index? */
    if (VINF_SUCCESS !=  RTStrToUInt32Full (a->argv[1], 10, &cmd.mIndex))
        return errorSyntax(USAGE_USBFILTER, "Invalid index '%s'", a->argv[1]);

    switch (cmd.mAction)
    {
        case USBFilterCmd::Add:
        case USBFilterCmd::Modify:
        {
            /* at least: 0: command, 1: index, 2: --target, 3: <target value>, 4: --name, 5: <name value> */
            if (a->argc < 6)
            {
                if (cmd.mAction == USBFilterCmd::Add)
                    return errorSyntax(USAGE_USBFILTER_ADD, "Not enough parameters");

                return errorSyntax(USAGE_USBFILTER_MODIFY, "Not enough parameters");
            }

            // set Active to true by default
            // (assuming that the user sets up all necessary attributes
            // at once and wants the filter to be active immediately)
            if (cmd.mAction == USBFilterCmd::Add)
                cmd.mFilter.mActive = true;

            for (int i = 2; i < a->argc; i++)
            {
                if (   !strcmp(a->argv[i], "--target")
                    || !strcmp(a->argv[i], "-target"))
                {
                    if (a->argc <= i + 1 || !*a->argv[i+1])
                        return errorArgument("Missing argument to '%s'", a->argv[i]);
                    i++;
                    if (!strcmp(a->argv[i], "global"))
                        cmd.mGlobal = true;
                    else
                    {
                        /* assume it's a UUID of a machine */
                        rc = a->virtualBox->GetMachine(Bstr(a->argv[i]), cmd.mMachine.asOutParam());
                        if (FAILED(rc) || !cmd.mMachine)
                        {
                            /* must be a name */
                            CHECK_ERROR_RET(a->virtualBox, FindMachine(Bstr(a->argv[i]), cmd.mMachine.asOutParam()), 1);
                        }
                    }
                }
                else if (   !strcmp(a->argv[i], "--name")
                         || !strcmp(a->argv[i], "-name"))
                {
                    if (a->argc <= i + 1 || !*a->argv[i+1])
                        return errorArgument("Missing argument to '%s'", a->argv[i]);
                    i++;
                    cmd.mFilter.mName = a->argv[i];
                }
                else if (   !strcmp(a->argv[i], "--active")
                         || !strcmp(a->argv[i], "-active"))
                {
                    if (a->argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", a->argv[i]);
                    i++;
                    if (!strcmp(a->argv[i], "yes"))
                        cmd.mFilter.mActive = true;
                    else if (!strcmp(a->argv[i], "no"))
                        cmd.mFilter.mActive = false;
                    else
                        return errorArgument("Invalid --active argument '%s'", a->argv[i]);
                }
                else if (   !strcmp(a->argv[i], "--vendorid")
                         || !strcmp(a->argv[i], "-vendorid"))
                {
                    if (a->argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", a->argv[i]);
                    i++;
                    cmd.mFilter.mVendorId = a->argv[i];
                }
                else if (   !strcmp(a->argv[i], "--productid")
                         || !strcmp(a->argv[i], "-productid"))
                {
                    if (a->argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", a->argv[i]);
                    i++;
                    cmd.mFilter.mProductId = a->argv[i];
                }
                else if (   !strcmp(a->argv[i], "--revision")
                         || !strcmp(a->argv[i], "-revision"))
                {
                    if (a->argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", a->argv[i]);
                    i++;
                    cmd.mFilter.mRevision = a->argv[i];
                }
                else if (   !strcmp(a->argv[i], "--manufacturer")
                         || !strcmp(a->argv[i], "-manufacturer"))
                {
                    if (a->argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", a->argv[i]);
                    i++;
                    cmd.mFilter.mManufacturer = a->argv[i];
                }
                else if (   !strcmp(a->argv[i], "--product")
                         || !strcmp(a->argv[i], "-product"))
                {
                    if (a->argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", a->argv[i]);
                    i++;
                    cmd.mFilter.mProduct = a->argv[i];
                }
                else if (   !strcmp(a->argv[i], "--remote")
                         || !strcmp(a->argv[i], "-remote"))
                {
                    if (a->argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", a->argv[i]);
                    i++;
                    cmd.mFilter.mRemote = a->argv[i];
                }
                else if (   !strcmp(a->argv[i], "--serialnumber")
                         || !strcmp(a->argv[i], "-serialnumber"))
                {
                    if (a->argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", a->argv[i]);
                    i++;
                    cmd.mFilter.mSerialNumber = a->argv[i];
                }
                else if (   !strcmp(a->argv[i], "--maskedinterfaces")
                         || !strcmp(a->argv[i], "-maskedinterfaces"))
                {
                    if (a->argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", a->argv[i]);
                    i++;
                    uint32_t u32;
                    rc = RTStrToUInt32Full(a->argv[i], 0, &u32);
                    if (RT_FAILURE(rc))
                        return errorArgument("Failed to convert the --maskedinterfaces value '%s' to a number, rc=%Rrc", a->argv[i], rc);
                    cmd.mFilter.mMaskedInterfaces = u32;
                }
                else if (   !strcmp(a->argv[i], "--action")
                         || !strcmp(a->argv[i], "-action"))
                {
                    if (a->argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", a->argv[i]);
                    i++;
                    if (!strcmp(a->argv[i], "ignore"))
                        cmd.mFilter.mAction = USBDeviceFilterAction_Ignore;
                    else if (!strcmp(a->argv[i], "hold"))
                        cmd.mFilter.mAction = USBDeviceFilterAction_Hold;
                    else
                        return errorArgument("Invalid USB filter action '%s'", a->argv[i]);
                }
                else
                    return errorSyntax(cmd.mAction == USBFilterCmd::Add ? USAGE_USBFILTER_ADD : USAGE_USBFILTER_MODIFY,
                                       "Unknown option '%s'", a->argv[i]);
            }

            if (cmd.mAction == USBFilterCmd::Add)
            {
                // mandatory/forbidden options
                if (   cmd.mFilter.mName.isEmpty()
                    ||
                       (   cmd.mGlobal
                        && cmd.mFilter.mAction == USBDeviceFilterAction_Null
                       )
                    || (   !cmd.mGlobal
                        && !cmd.mMachine)
                    || (   cmd.mGlobal
                        && cmd.mFilter.mRemote)
                   )
                {
                    return errorSyntax(USAGE_USBFILTER_ADD, "Mandatory options not supplied");
                }
            }
            break;
        }

        case USBFilterCmd::Remove:
        {
            /* at least: 0: command, 1: index, 2: --target, 3: <target value> */
            if (a->argc < 4)
                return errorSyntax(USAGE_USBFILTER_REMOVE, "Not enough parameters");

            for (int i = 2; i < a->argc; i++)
            {
                if (   !strcmp(a->argv[i], "--target")
                    || !strcmp(a->argv[i], "-target"))
                {
                    if (a->argc <= i + 1 || !*a->argv[i+1])
                        return errorArgument("Missing argument to '%s'", a->argv[i]);
                    i++;
                    if (!strcmp(a->argv[i], "global"))
                        cmd.mGlobal = true;
                    else
                    {
                        /* assume it's a UUID of a machine */
                        rc = a->virtualBox->GetMachine(Bstr(a->argv[i]), cmd.mMachine.asOutParam());
                        if (FAILED(rc) || !cmd.mMachine)
                        {
                            /* must be a name */
                            CHECK_ERROR_RET(a->virtualBox, FindMachine(Bstr(a->argv[i]), cmd.mMachine.asOutParam()), 1);
                        }
                    }
                }
            }

            // mandatory options
            if (!cmd.mGlobal && !cmd.mMachine)
                return errorSyntax(USAGE_USBFILTER_REMOVE, "Mandatory options not supplied");

            break;
        }

        default: break;
    }

    USBFilterCmd::USBFilter &f = cmd.mFilter;

    ComPtr <IHost> host;
    ComPtr <IUSBController> ctl;
    if (cmd.mGlobal)
        CHECK_ERROR_RET (a->virtualBox, COMGETTER(Host) (host.asOutParam()), 1);
    else
    {
        Bstr uuid;
        cmd.mMachine->COMGETTER(Id)(uuid.asOutParam());
        /* open a session for the VM */
        CHECK_ERROR_RET (a->virtualBox, OpenSession(a->session, uuid), 1);
        /* get the mutable session machine */
        a->session->COMGETTER(Machine)(cmd.mMachine.asOutParam());
        /* and get the USB controller */
        CHECK_ERROR_RET (cmd.mMachine, COMGETTER(USBController) (ctl.asOutParam()), 1);
    }

    switch (cmd.mAction)
    {
        case USBFilterCmd::Add:
        {
            if (cmd.mGlobal)
            {
                ComPtr <IHostUSBDeviceFilter> flt;
                CHECK_ERROR_BREAK (host, CreateUSBDeviceFilter (f.mName, flt.asOutParam()));

                if (!f.mActive.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Active) (f.mActive));
                if (!f.mVendorId.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(VendorId) (f.mVendorId.setNullIfEmpty()));
                if (!f.mProductId.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(ProductId) (f.mProductId.setNullIfEmpty()));
                if (!f.mRevision.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Revision) (f.mRevision.setNullIfEmpty()));
                if (!f.mManufacturer.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Manufacturer) (f.mManufacturer.setNullIfEmpty()));
                if (!f.mSerialNumber.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(SerialNumber) (f.mSerialNumber.setNullIfEmpty()));
                if (!f.mMaskedInterfaces.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(MaskedInterfaces) (f.mMaskedInterfaces));

                if (f.mAction != USBDeviceFilterAction_Null)
                    CHECK_ERROR_BREAK (flt, COMSETTER(Action) (f.mAction));

                CHECK_ERROR_BREAK (host, InsertUSBDeviceFilter (cmd.mIndex, flt));
            }
            else
            {
                ComPtr <IUSBDeviceFilter> flt;
                CHECK_ERROR_BREAK (ctl, CreateDeviceFilter (f.mName, flt.asOutParam()));

                if (!f.mActive.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Active) (f.mActive));
                if (!f.mVendorId.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(VendorId) (f.mVendorId.setNullIfEmpty()));
                if (!f.mProductId.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(ProductId) (f.mProductId.setNullIfEmpty()));
                if (!f.mRevision.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Revision) (f.mRevision.setNullIfEmpty()));
                if (!f.mManufacturer.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Manufacturer) (f.mManufacturer.setNullIfEmpty()));
                if (!f.mRemote.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Remote) (f.mRemote.setNullIfEmpty()));
                if (!f.mSerialNumber.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(SerialNumber) (f.mSerialNumber.setNullIfEmpty()));
                if (!f.mMaskedInterfaces.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(MaskedInterfaces) (f.mMaskedInterfaces));

                CHECK_ERROR_BREAK (ctl, InsertDeviceFilter (cmd.mIndex, flt));
            }
            break;
        }
        case USBFilterCmd::Modify:
        {
            if (cmd.mGlobal)
            {
                SafeIfaceArray <IHostUSBDeviceFilter> coll;
                CHECK_ERROR_BREAK (host, COMGETTER(USBDeviceFilters) (ComSafeArrayAsOutParam(coll)));

                ComPtr <IHostUSBDeviceFilter> flt = coll[cmd.mIndex];

                if (!f.mName.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Name) (f.mName.setNullIfEmpty()));
                if (!f.mActive.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Active) (f.mActive));
                if (!f.mVendorId.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(VendorId) (f.mVendorId.setNullIfEmpty()));
                if (!f.mProductId.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(ProductId) (f.mProductId.setNullIfEmpty()));
                if (!f.mRevision.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Revision) (f.mRevision.setNullIfEmpty()));
                if (!f.mManufacturer.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Manufacturer) (f.mManufacturer.setNullIfEmpty()));
                if (!f.mSerialNumber.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(SerialNumber) (f.mSerialNumber.setNullIfEmpty()));
                if (!f.mMaskedInterfaces.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(MaskedInterfaces) (f.mMaskedInterfaces));

                if (f.mAction != USBDeviceFilterAction_Null)
                    CHECK_ERROR_BREAK (flt, COMSETTER(Action) (f.mAction));
            }
            else
            {
                SafeIfaceArray <IUSBDeviceFilter> coll;
                CHECK_ERROR_BREAK (ctl, COMGETTER(DeviceFilters) (ComSafeArrayAsOutParam(coll)));

                ComPtr <IUSBDeviceFilter> flt = coll[cmd.mIndex];

                if (!f.mName.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Name) (f.mName.setNullIfEmpty()));
                if (!f.mActive.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Active) (f.mActive));
                if (!f.mVendorId.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(VendorId) (f.mVendorId.setNullIfEmpty()));
                if (!f.mProductId.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(ProductId) (f.mProductId.setNullIfEmpty()));
                if (!f.mRevision.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Revision) (f.mRevision.setNullIfEmpty()));
                if (!f.mManufacturer.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Manufacturer) (f.mManufacturer.setNullIfEmpty()));
                if (!f.mRemote.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Remote) (f.mRemote.setNullIfEmpty()));
                if (!f.mSerialNumber.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(SerialNumber) (f.mSerialNumber.setNullIfEmpty()));
                if (!f.mMaskedInterfaces.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(MaskedInterfaces) (f.mMaskedInterfaces));
            }
            break;
        }
        case USBFilterCmd::Remove:
        {
            if (cmd.mGlobal)
            {
                ComPtr <IHostUSBDeviceFilter> flt;
                CHECK_ERROR_BREAK (host, RemoveUSBDeviceFilter (cmd.mIndex, flt.asOutParam()));
            }
            else
            {
                ComPtr <IUSBDeviceFilter> flt;
                CHECK_ERROR_BREAK (ctl, RemoveDeviceFilter (cmd.mIndex, flt.asOutParam()));
            }
            break;
        }
        default:
            break;
    }

    if (cmd.mMachine)
    {
        /* commit and close the session */
        CHECK_ERROR(cmd.mMachine, SaveSettings());
        a->session->Close();
    }

    return SUCCEEDED (rc) ? 0 : 1;
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
