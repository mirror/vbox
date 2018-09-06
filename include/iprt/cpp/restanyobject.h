/** @file
 * IPRT - C++ Representational State Transfer (REST) Any Object Class.
 */

/*
 * Copyright (C) 2008-2018 Oracle Corporation
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
 */

#ifndef ___iprt_cpp_restanyobject_h
#define ___iprt_cpp_restanyobject_h

#include <iprt/cpp/restbase.h>
#include <iprt/cpp/restarray.h>
#include <iprt/cpp/reststringmap.h>


/** @defgroup grp_rt_cpp_restanyobj C++ Representational State Transfer (REST) Any Object Class.
 * @ingroup grp_rt_cpp
 * @{
 */

/**
 * Wrapper object that can represent any kind of basic REST object.
 *
 * This class is the result of a couple of design choices made in our REST
 * data model.  If could have been avoided if we used pointers all over
 * the place and didn't rely entirely on the object specific implementations
 * of deserializeFromJson and fromString to do the deserializing or everything.
 *
 * The assumption, though, was that most of the data we're dealing with has a
 * known structure and maps to fixed types.  So, the data model was optimized
 * for that rather than flexiblity here.
 */
class RT_DECL_CLASS RTCRestAnyObject : public RTCRestObjectBase
{
public:
    /** Default constructor. */
    RTCRestAnyObject();
    /** Destructor. */
    virtual ~RTCRestAnyObject();

    /** Copy constructor. */
    RTCRestAnyObject(RTCRestAnyObject const &a_rThat);
    /** Copy assignment operator. */
    RTCRestAnyObject &operator=(RTCRestAnyObject const &a_rThat);

    /** Safe copy assignment method. */
    int assignCopy(RTCRestAnyObject const &a_rThat);
    /** Safe copy assignment method, boolean variant. */
    int assignCopy(RTCRestBool const &a_rThat);
    /** Safe copy assignment method, int64_t variant. */
    int assignCopy(RTCRestInt64 const &a_rThat);
    /** Safe copy assignment method, int32_t variant. */
    int assignCopy(RTCRestInt32 const &a_rThat);
    /** Safe copy assignment method, int16_t variant. */
    int assignCopy(RTCRestInt16 const &a_rThat);
    /** Safe copy assignment method, double variant. */
    int assignCopy(RTCRestDouble const &a_rThat);
    /** Safe copy assignment method, string variant. */
    int assignCopy(RTCRestString const &a_rThat);
    /** Safe copy assignment method, array variant. */
    int assignCopy(RTCRestArray<RTCRestAnyObject> const &a_rThat);
    /** Safe copy assignment method, string map variant. */
    int assignCopy(RTCRestStringMap<RTCRestAnyObject> const &a_rThat);

    /** Safe value assignment method, boolean variant. */
    int assignValue(bool a_fValue);
    /** Safe value assignment method, int64_t variant. */
    int assignValue(int64_t a_iValue);
    /** Safe value assignment method, int32_t variant. */
    int assignValue(int32_t a_iValue);
    /** Safe value assignment method, int16_t variant. */
    int assignValue(int16_t a_iValue);
    /** Safe value assignment method, double variant. */
    int assignValue(double a_iValue);
    /** Safe value assignment method, string variant. */
    int assignValue(RTCString const &a_rValue);
    /** Safe value assignment method, C-string variant. */
    int assignValue(const char *a_pszValue);

    /* Overridden methods: */
    virtual int setNull(void) RT_OVERRIDE;
    virtual int resetToDefault() RT_OVERRIDE;
    virtual RTCRestOutputBase &serializeAsJson(RTCRestOutputBase &a_rDst) const RT_OVERRIDE;
    virtual int deserializeFromJson(RTCRestJsonCursor const &a_rCursor) RT_OVERRIDE;
    virtual int toString(RTCString *a_pDst, uint32_t a_fFlags = kCollectionFormat_Unspecified) const RT_OVERRIDE;
    virtual int fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo = NULL,
                           uint32_t a_fFlags = kCollectionFormat_Unspecified) RT_OVERRIDE;
    virtual kTypeClass typeClass(void) const RT_OVERRIDE;
    virtual const char *typeName(void) const RT_OVERRIDE;

    /** Factory method. */
    static DECLCALLBACK(RTCRestObjectBase *) createInstance(void);

protected:
    /** The data. */
    RTCRestObjectBase *m_pData;
};

/** @} */

#endif

