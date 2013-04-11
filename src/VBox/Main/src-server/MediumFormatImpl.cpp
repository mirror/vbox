/* $Id$ */
/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2008-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "MediumFormatImpl.h"
#include "AutoCaller.h"
#include "Logging.h"

#include <VBox/vd.h>

#include <iprt/cpp/utils.h>

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(MediumFormat)

HRESULT MediumFormat::FinalConstruct()
{
    return BaseFinalConstruct();
}

void MediumFormat::FinalRelease()
{
    uninit();

    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the hard disk format object.
 *
 * @param aVDInfo  Pointer to a backend info object.
 */
HRESULT MediumFormat::init(const VDBACKENDINFO *aVDInfo)
{
    LogFlowThisFunc(("aVDInfo=%p\n", aVDInfo));

    ComAssertRet(aVDInfo, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /* The ID of the backend */
    unconst(m.strId) = aVDInfo->pszBackend;
    /* The Name of the backend */
    /* Use id for now as long as VDBACKENDINFO hasn't any extra
     * name/description field. */
    unconst(m.strName) = aVDInfo->pszBackend;
    /* The capabilities of the backend. Assumes 1:1 mapping! */
    unconst(m.capabilities) = (MediumFormatCapabilities_T)aVDInfo->uBackendCaps;
    /* Save the supported file extensions in a list */
    if (aVDInfo->paFileExtensions)
    {
        PCVDFILEEXTENSION papExtension = aVDInfo->paFileExtensions;
        while (papExtension->pszExtension != NULL)
        {
            DeviceType_T devType;

            unconst(m.llFileExtensions).push_back(papExtension->pszExtension);

            switch(papExtension->enmType)
            {
                case VDTYPE_HDD:
                    devType = DeviceType_HardDisk;
                    break;
                case VDTYPE_DVD:
                    devType = DeviceType_DVD;
                    break;
                case VDTYPE_FLOPPY:
                    devType = DeviceType_Floppy;
                    break;
                default:
                    AssertMsgFailed(("Invalid enm type %d!\n", papExtension->enmType));
                    return E_INVALIDARG;
            }

            unconst(m.llDeviceTypes).push_back(devType);
            ++papExtension;
        }
    }
    /* Save a list of configure properties */
    if (aVDInfo->paConfigInfo)
    {
        PCVDCONFIGINFO pa = aVDInfo->paConfigInfo;
        /* Walk through all available keys */
        while (pa->pszKey != NULL)
        {
            Utf8Str defaultValue("");
            DataType_T dt;
            ULONG flags = static_cast <ULONG>(pa->uKeyFlags);
            /* Check for the configure data type */
            switch (pa->enmValueType)
            {
                case VDCFGVALUETYPE_INTEGER:
                {
                    dt = DataType_Int32;
                    /* If there is a default value get them in the right format */
                    if (pa->pszDefaultValue)
                        defaultValue = pa->pszDefaultValue;
                    break;
                }
                case VDCFGVALUETYPE_BYTES:
                {
                    dt = DataType_Int8;
                    /* If there is a default value get them in the right format */
                    if (pa->pszDefaultValue)
                    {
                        /* Copy the bytes over - treated simply as a string */
                        defaultValue = pa->pszDefaultValue;
                        flags |= DataFlags_Array;
                    }
                    break;
                }
                case VDCFGVALUETYPE_STRING:
                {
                    dt = DataType_String;
                    /* If there is a default value get them in the right format */
                    if (pa->pszDefaultValue)
                        defaultValue = pa->pszDefaultValue;
                    break;
                }

                default:
                    AssertMsgFailed(("Invalid enm type %d!\n", pa->enmValueType));
                    return E_INVALIDARG;
            }

            /// @todo add extendedFlags to Property when we reach the 32 bit
            /// limit (or make the argument ULONG64 after checking that COM is
            /// capable of defining enums (used to represent bit flags) that
            /// contain 64-bit values)
            ComAssertRet(pa->uKeyFlags == ((ULONG)pa->uKeyFlags), E_FAIL);

            /* Create one property structure */
            const Property prop = { Utf8Str(pa->pszKey),
                                    Utf8Str(""),
                                    dt,
                                    flags,
                                    defaultValue };
            unconst(m.llProperties).push_back(prop);
            ++pa;
        }
    }

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 * Uninitializes the instance and sets the ready flag to FALSE.
 * Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void MediumFormat::uninit()
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    unconst(m.llProperties).clear();
    unconst(m.llFileExtensions).clear();
    unconst(m.llDeviceTypes).clear();
    unconst(m.capabilities) = (MediumFormatCapabilities_T)0;
    unconst(m.strName).setNull();
    unconst(m.strId).setNull();
}

// IMediumFormat properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP MediumFormat::COMGETTER(Id)(BSTR *aId)
{
    CheckComArgOutPointerValid(aId);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* this is const, no need to lock */
    m.strId.cloneTo(aId);

    return S_OK;
}

STDMETHODIMP MediumFormat::COMGETTER(Name)(BSTR *aName)
{
    CheckComArgOutPointerValid(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* this is const, no need to lock */
    m.strName.cloneTo(aName);

    return S_OK;
}

STDMETHODIMP MediumFormat::COMGETTER(Capabilities)(ComSafeArrayOut(MediumFormatCapabilities_T, aCaps))
{
    CheckComArgOutSafeArrayPointerValid(aCaps);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    SafeArray<MediumFormatCapabilities_T> capabilities(sizeof(MediumFormatCapabilities_T)*8);

    for (ULONG i = 0; i < capabilities.size(); ++i)
    {
        ULONG temp = m.capabilities;
        temp &= 1<<i;
        capabilities [i] = (MediumFormatCapabilities_T)temp;
    }

    capabilities.detachTo(ComSafeArrayOutArg(aCaps));

    return S_OK;
}

STDMETHODIMP MediumFormat::DescribeFileExtensions(ComSafeArrayOut(BSTR, aFileExtensions),
                                                  ComSafeArrayOut(DeviceType_T, aDeviceTypes))
{
    CheckComArgOutSafeArrayPointerValid(aFileExtensions);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* this is const, no need to lock */
    com::SafeArray<BSTR> fileExtentions(m.llFileExtensions.size());
    int i = 0;
    for (StrList::const_iterator it = m.llFileExtensions.begin();
         it != m.llFileExtensions.end();
         ++it, ++i)
        (*it).cloneTo(&fileExtentions[i]);
    fileExtentions.detachTo(ComSafeArrayOutArg(aFileExtensions));

    com::SafeArray<DeviceType_T> deviceTypes(m.llDeviceTypes.size());
    i = 0;
    for (DeviceTypeList::const_iterator it = m.llDeviceTypes.begin();
         it != m.llDeviceTypes.end();
         ++it, ++i)
        deviceTypes[i] = (*it);
    deviceTypes.detachTo(ComSafeArrayOutArg(aDeviceTypes));

    return S_OK;
}

STDMETHODIMP MediumFormat::DescribeProperties(ComSafeArrayOut(BSTR, aNames),
                                              ComSafeArrayOut(BSTR, aDescriptions),
                                              ComSafeArrayOut(DataType_T, aTypes),
                                              ComSafeArrayOut(ULONG, aFlags),
                                              ComSafeArrayOut(BSTR, aDefaults))
{
    CheckComArgOutSafeArrayPointerValid(aNames);
    CheckComArgOutSafeArrayPointerValid(aDescriptions);
    CheckComArgOutSafeArrayPointerValid(aTypes);
    CheckComArgOutSafeArrayPointerValid(aFlags);
    CheckComArgOutSafeArrayPointerValid(aDefaults);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* this is const, no need to lock */
    size_t c = m.llProperties.size();
    com::SafeArray<BSTR>        propertyNames(c);
    com::SafeArray<BSTR>        propertyDescriptions(c);
    com::SafeArray<DataType_T>  propertyTypes(c);
    com::SafeArray<ULONG>       propertyFlags(c);
    com::SafeArray<BSTR>        propertyDefaults(c);

    int i = 0;
    for (PropertyList::const_iterator it = m.llProperties.begin();
         it != m.llProperties.end();
         ++it, ++i)
    {
        const Property &prop = (*it);
        prop.strName.cloneTo(&propertyNames[i]);
        prop.strDescription.cloneTo(&propertyDescriptions[i]);
        propertyTypes[i] = prop.type;
        propertyFlags[i] = prop.flags;
        prop.strDefaultValue.cloneTo(&propertyDefaults[i]);
    }

    propertyNames.detachTo(ComSafeArrayOutArg(aNames));
    propertyDescriptions.detachTo(ComSafeArrayOutArg(aDescriptions));
    propertyTypes.detachTo(ComSafeArrayOutArg(aTypes));
    propertyFlags.detachTo(ComSafeArrayOutArg(aFlags));
    propertyDefaults.detachTo(ComSafeArrayOutArg(aDefaults));

    return S_OK;
}

// IMediumFormat methods
/////////////////////////////////////////////////////////////////////////////

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
