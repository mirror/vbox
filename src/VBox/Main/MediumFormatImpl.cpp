/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#include "MediumFormatImpl.h"
#include "Logging.h"

#include <VBox/VBoxHDD.h>

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (MediumFormat)

HRESULT MediumFormat::FinalConstruct()
{
    return S_OK;
}

void MediumFormat::FinalRelease()
{
    uninit();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the hard disk format object.
 *
 * @param aVDInfo  Pointer to a backend info object.
 */
HRESULT MediumFormat::init (const VDBACKENDINFO *aVDInfo)
{
    LogFlowThisFunc (("aVDInfo=%p\n", aVDInfo));

    ComAssertRet (aVDInfo, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    /* The ID of the backend */
    unconst (m.id) = aVDInfo->pszBackend;
    /* The Name of the backend */
    /* Use id for now as long as VDBACKENDINFO hasn't any extra
     * name/description field. */
    unconst (m.name) = aVDInfo->pszBackend;
    /* The capabilities of the backend */
    unconst (m.capabilities) = aVDInfo->uBackendCaps;
    /* Save the supported file extensions in a list */
    if (aVDInfo->papszFileExtensions)
    {
        const char *const *papsz = aVDInfo->papszFileExtensions;
        while (*papsz != NULL)
        {
            unconst (m.fileExtensions).push_back (*papsz);
            ++ papsz;
        }
    }
    /* Save a list of configure properties */
    if (aVDInfo->paConfigInfo)
    {
        PCVDCONFIGINFO pa = aVDInfo->paConfigInfo;
        /* Walk through all available keys */
        while (pa->pszKey != NULL)
        {
            Utf8Str defaultValue ("");
            DataType_T dt;
            ULONG flags = static_cast <ULONG> (pa->uKeyFlags);
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
            ComAssertRet (pa->uKeyFlags == ((ULONG) pa->uKeyFlags), E_FAIL);

            /* Create one property structure */
            const Property prop = { Utf8Str (pa->pszKey),
                                    Utf8Str (""),
                                    dt,
                                    flags,
                                    defaultValue };
            unconst (m.properties).push_back (prop);
            ++ pa;
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
    LogFlowThisFunc (("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    unconst (m.properties).clear();
    unconst (m.fileExtensions).clear();
    unconst (m.capabilities) = 0;
    unconst (m.name).setNull();
    unconst (m.id).setNull();
}

// IMediumFormat properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP MediumFormat::COMGETTER(Id)(BSTR *aId)
{
    CheckComArgOutPointerValid(aId);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* this is const, no need to lock */
    m.id.cloneTo (aId);

    return S_OK;
}

STDMETHODIMP MediumFormat::COMGETTER(Name)(BSTR *aName)
{
    CheckComArgOutPointerValid(aName);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* this is const, no need to lock */
    m.name.cloneTo (aName);

    return S_OK;
}

STDMETHODIMP MediumFormat::
COMGETTER(FileExtensions)(ComSafeArrayOut (BSTR, aFileExtensions))
{
    if (ComSafeArrayOutIsNull (aFileExtensions))
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* this is const, no need to lock */
    com::SafeArray <BSTR> fileExtentions (m.fileExtensions.size());
    int i = 0;
    for (BstrList::const_iterator it = m.fileExtensions.begin();
        it != m.fileExtensions.end(); ++ it, ++ i)
        (*it).cloneTo (&fileExtentions [i]);
    fileExtentions.detachTo (ComSafeArrayOutArg (aFileExtensions));

    return S_OK;
}

STDMETHODIMP MediumFormat::COMGETTER(Capabilities)(ULONG *aCaps)
{
    CheckComArgOutPointerValid(aCaps);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* m.capabilities is const, no need to lock */

    /// @todo add COMGETTER(ExtendedCapabilities) when we reach the 32 bit
    /// limit (or make the argument ULONG64 after checking that COM is capable
    /// of defining enums (used to represent bit flags) that contain 64-bit
    /// values)
    ComAssertRet (m.capabilities == ((ULONG) m.capabilities), E_FAIL);

    *aCaps = (ULONG) m.capabilities;

    return S_OK;
}

STDMETHODIMP MediumFormat::DescribeProperties(ComSafeArrayOut (BSTR, aNames),
                                                ComSafeArrayOut (BSTR, aDescriptions),
                                                ComSafeArrayOut (DataType_T, aTypes),
                                                ComSafeArrayOut (ULONG, aFlags),
                                                ComSafeArrayOut (BSTR, aDefaults))
{
    CheckComArgSafeArrayNotNull(aNames);
    CheckComArgSafeArrayNotNull(aDescriptions);
    CheckComArgSafeArrayNotNull(aTypes);
    CheckComArgSafeArrayNotNull(aFlags);
    CheckComArgSafeArrayNotNull(aDefaults);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* this is const, no need to lock */
    com::SafeArray <BSTR> propertyNames (m.properties.size());
    com::SafeArray <BSTR> propertyDescriptions (m.properties.size());
    com::SafeArray <DataType_T> propertyTypes (m.properties.size());
    com::SafeArray <ULONG> propertyFlags (m.properties.size());
    com::SafeArray <BSTR> propertyDefaults (m.properties.size());

    int i = 0;
    for (PropertyList::const_iterator it = m.properties.begin();
         it != m.properties.end(); ++ it, ++ i)
    {
        const Property &prop = (*it);
        prop.name.cloneTo (&propertyNames [i]);
        prop.description.cloneTo (&propertyDescriptions [i]);
        propertyTypes [i] = prop.type;
        propertyFlags [i] = prop.flags;
        prop.defaultValue.cloneTo (&propertyDefaults [i]);
    }

    propertyNames.detachTo (ComSafeArrayOutArg (aNames));
    propertyDescriptions.detachTo (ComSafeArrayOutArg (aDescriptions));
    propertyTypes.detachTo (ComSafeArrayOutArg (aTypes));
    propertyFlags.detachTo (ComSafeArrayOutArg (aFlags));
    propertyDefaults.detachTo (ComSafeArrayOutArg (aDefaults));

    return S_OK;
}

// IMediumFormat methods
/////////////////////////////////////////////////////////////////////////////

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
