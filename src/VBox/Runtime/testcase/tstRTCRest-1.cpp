/* $Id$ */
/** @file
 * IPRT Testcase - REST C++ classes.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/cpp/restbase.h>

#include <iprt/err.h>
#include <iprt/message.h>
#include <iprt/string.h>
#include <iprt/test.h>

#include <float.h> /* DBL_MIN, DBL_MAX */


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static RTTEST g_hTest;


static char *toJson(RTCRestObjectBase const *pObj)
{
    RTCString str;
    RTCRestOutputToString Dst(&str, false);
    pObj->serializeAsJson(Dst);

    static char s_szReturnBuffer[4096];
    RTStrCopy(s_szReturnBuffer, sizeof(s_szReturnBuffer), str.c_str());
    return s_szReturnBuffer;
}


static int deserializeFromJson(RTCRestObjectBase *pObj, const char *pszJson, PRTERRINFOSTATIC pErrInfo, const char *pszName)
{
    RTJSONVAL hValue;
    RTTESTI_CHECK_RC_OK_RET(RTJsonParseFromString(&hValue, pszJson, pErrInfo ? RTErrInfoInitStatic(pErrInfo) : NULL), rcCheck);
    RTCRestJsonPrimaryCursor Cursor(hValue, pszName, pErrInfo ? RTErrInfoInitStatic(pErrInfo) : NULL);
    return pObj->deserializeFromJson(Cursor.m_Cursor);
}


static int fromString(RTCRestObjectBase *pObj, const char *pszString, PRTERRINFOSTATIC pErrInfo, const char *pszName)
{
    RTCString strValue(pszString);
    return pObj->fromString(strValue, pszName, pErrInfo ? RTErrInfoInitStatic(pErrInfo) : NULL);
}


static void testBool(void)
{
    RTTestSub(g_hTest, "RTCRestBool");

    {
        RTCRestBool obj1;
        RTTESTI_CHECK(obj1.m_fValue == false);
        RTTESTI_CHECK(obj1.isNull() == false);
        RTTESTI_CHECK(strcmp(obj1.typeName(), "bool") == 0);
        RTTESTI_CHECK(obj1.typeClass() == RTCRestObjectBase::kTypeClass_Bool);
    }

    {
        RTCRestBool obj2(true);
        RTTESTI_CHECK(obj2.m_fValue == true);
        RTTESTI_CHECK(obj2.isNull() == false);
    }

    {
        RTCRestBool obj2(false);
        RTTESTI_CHECK(obj2.m_fValue == false);
        RTTESTI_CHECK(obj2.isNull() == false);
    }

    {
        /* Value assignments: */
        RTCRestBool obj3;
        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true);
        obj3.assignValue(true);
        RTTESTI_CHECK(obj3.m_fValue == true);
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true);
        obj3.assignValue(false);
        RTTESTI_CHECK(obj3.m_fValue == false);
        RTTESTI_CHECK(obj3.isNull() == false);

        obj3.assignValue(true);
        RTTESTI_CHECK(obj3.m_fValue == true);
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.resetToDefault(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.m_fValue == false);
        RTTESTI_CHECK(obj3.isNull() == false);

        obj3.assignValue(true);
        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK_RC(obj3.resetToDefault(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.m_fValue == false);
        RTTESTI_CHECK(obj3.isNull() == false);

        /* Copy assignments: */
        RTCRestBool obj3True(true);
        RTTESTI_CHECK(obj3True.m_fValue == true);
        RTTESTI_CHECK(obj3True.isNull() == false);
        RTCRestBool obj3False(false);
        RTTESTI_CHECK(obj3False.m_fValue == false);
        RTTESTI_CHECK(obj3False.isNull() == false);
        RTCRestBool obj3Null;
        obj3Null.setNull();
        RTTESTI_CHECK(obj3Null.m_fValue == false);
        RTTESTI_CHECK(obj3Null.isNull() == true);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK_RC(obj3.assignCopy(obj3True), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.m_fValue == true);
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.assignCopy(obj3Null), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.m_fValue == false);
        RTTESTI_CHECK(obj3.isNull() == true);

        RTTESTI_CHECK_RC(obj3.assignCopy(obj3False), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.m_fValue == false);
        RTTESTI_CHECK(obj3.isNull() == false);

        obj3 = obj3Null;
        RTTESTI_CHECK(obj3.m_fValue == false);
        RTTESTI_CHECK(obj3.isNull() == true);

        obj3 = obj3True;
        RTTESTI_CHECK(obj3.m_fValue == true);
        RTTESTI_CHECK(obj3.isNull() == false);

        obj3 = obj3Null;
        RTTESTI_CHECK(obj3.m_fValue == false);
        RTTESTI_CHECK(obj3.isNull() == true);

        obj3 = obj3False;
        RTTESTI_CHECK(obj3.m_fValue == false);
        RTTESTI_CHECK(obj3.isNull() == false);

        /* setNull implies resetToDefault: */
        obj3 = obj3True;
        RTTESTI_CHECK(obj3.m_fValue == true);
        RTTESTI_CHECK(obj3.isNull() == false);
        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true);
        RTTESTI_CHECK(obj3.m_fValue == false);

        /* Copy constructors: */
        {
            RTCRestBool obj3a(obj3True);
            RTTESTI_CHECK(obj3a.m_fValue == true);
            RTTESTI_CHECK(obj3a.isNull() == false);
        }
        {
            RTCRestBool obj3b(obj3False);
            RTTESTI_CHECK(obj3b.m_fValue == false);
            RTTESTI_CHECK(obj3b.isNull() == false);
        }
        {
            RTCRestBool obj3c(obj3Null);
            RTTESTI_CHECK(obj3c.m_fValue == false);
            RTTESTI_CHECK(obj3c.isNull() == true);
        }

        /* Serialization to json: */
        const char *pszJson = toJson(&obj3True);
        RTTESTI_CHECK_MSG(strcmp(pszJson, "true") == 0, ("pszJson=%s\n", pszJson));
        pszJson = toJson(&obj3False);
        RTTESTI_CHECK_MSG(strcmp(pszJson, "false") == 0, ("pszJson=%s\n", pszJson));
        pszJson = toJson(&obj3Null);
        RTTESTI_CHECK_MSG(strcmp(pszJson, "null") == 0, ("pszJson=%s\n", pszJson));

        /* Serialization to string. */
        RTCString str;
        str = "lead-in:";
        RTTESTI_CHECK_RC(obj3True.toString(&str, RTCRestObjectBase::kToString_Append), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("lead-in:true"), ("str=%s\n", str.c_str()));
        RTTESTI_CHECK_RC(obj3True.toString(&str), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("true"), ("str=%s\n", str.c_str()));

        str = "lead-in:";
        RTTESTI_CHECK_RC(obj3False.toString(&str, RTCRestObjectBase::kToString_Append), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("lead-in:false"), ("str=%s\n", str.c_str()));
        RTTESTI_CHECK_RC(obj3False.toString(&str), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("false"), ("str=%s\n", str.c_str()));

        str = "lead-in:";
        RTTESTI_CHECK_RC(obj3Null.toString(&str, RTCRestObjectBase::kToString_Append), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("lead-in:null"), ("str=%s\n", str.c_str()));
        RTTESTI_CHECK_RC(obj3Null.toString(&str), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("null"), ("str=%s\n", str.c_str()));
    }

    /* deserialize: */
    RTERRINFOSTATIC ErrInfo;
    {
        RTCRestBool obj4;
        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "false", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_fValue == false);
        RTTESTI_CHECK(obj4.isNull() == false);

        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "true", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_fValue == true);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "null", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_fValue == false);
        RTTESTI_CHECK(obj4.isNull() == true);

        /* object goes to default state on failure: */
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "0", &ErrInfo, RT_XSTR(__LINE__)), VERR_REST_WRONG_JSON_TYPE_FOR_BOOL);
        RTTESTI_CHECK(obj4.m_fValue == false);
        RTTESTI_CHECK(obj4.isNull() == false);
        RTTESTI_CHECK(RTErrInfoIsSet(&ErrInfo.Core));

        obj4.assignValue(true);
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "\"false\"", &ErrInfo, RT_XSTR(__LINE__)), VERR_REST_WRONG_JSON_TYPE_FOR_BOOL);
        RTTESTI_CHECK(obj4.m_fValue == false);
        RTTESTI_CHECK(obj4.isNull() == false);
        RTTESTI_CHECK(RTErrInfoIsSet(&ErrInfo.Core));

        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "[ null ]", NULL, RT_XSTR(__LINE__)), VERR_REST_WRONG_JSON_TYPE_FOR_BOOL);
        RTTESTI_CHECK(obj4.m_fValue == false);
        RTTESTI_CHECK(obj4.isNull() == false);

        obj4.setNull();
        RTTESTI_CHECK_RC(fromString(&obj4, "true", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_fValue == true);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, "false", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_fValue == false);
        RTTESTI_CHECK(obj4.isNull() == false);

        obj4.m_fValue = true;
        RTTESTI_CHECK_RC(fromString(&obj4, "null", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_fValue == false);
        RTTESTI_CHECK(obj4.isNull() == true);

        obj4.setNull();
        RTTESTI_CHECK_RC(fromString(&obj4, " TrUe ", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_fValue == true);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, "\tfAlSe;", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_fValue == false);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, "\r\nfAlSe\n;", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_fValue == false);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, "\r\tNuLl\n;", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_fValue == false);
        RTTESTI_CHECK(obj4.isNull() == true);

        RTTESTI_CHECK_RC(fromString(&obj4, "1", &ErrInfo, RT_XSTR(__LINE__)), VERR_REST_UNABLE_TO_PARSE_STRING_AS_BOOL);
        RTTESTI_CHECK(RTErrInfoIsSet(&ErrInfo.Core));

        RTTESTI_CHECK_RC(fromString(&obj4, "0", NULL, RT_XSTR(__LINE__)), VERR_REST_UNABLE_TO_PARSE_STRING_AS_BOOL);
    }
}

class Int64Constants
{
public:
    Int64Constants() {}
    const char *getSubName()  const { return "RTCRestInt64"; }
    int64_t     getMin()      const { return INT64_MIN; }
    const char *getMinStr()   const { return "-9223372036854775808"; }
    int64_t     getMax()      const { return INT64_MAX; }
    const char *getMaxStr()   const { return "9223372036854775807"; }
    const char *getTypeName() const { return "int64_t"; }
    RTCRestObjectBase::kTypeClass getTypeClass() const { return RTCRestObjectBase::kTypeClass_Int64; }
};

class Int32Constants
{
public:
    Int32Constants() { }
    const char *getSubName()  const { return "RTCRestInt32"; }
    int32_t     getMin()      const { return INT32_MIN; }
    const char *getMinStr()   const { return "-2147483648"; }
    int32_t     getMax()      const { return INT32_MAX; }
    const char *getMaxStr()   const { return "2147483647"; }
    const char *getTypeName() const { return "int32_t"; }
    RTCRestObjectBase::kTypeClass getTypeClass() const { return RTCRestObjectBase::kTypeClass_Int32; }
};

class Int16Constants
{
public:
    Int16Constants() { }
    const char *getSubName()  const { return "RTCRestInt16"; }
    int16_t     getMin()      const { return INT16_MIN; }
    const char *getMinStr()   const { return "-32768"; }
    int16_t     getMax()      const { return INT16_MAX; }
    const char *getMaxStr()   const { return "32767"; }
    const char *getTypeName() const { return "int16_t"; }
    RTCRestObjectBase::kTypeClass getTypeClass() const { return RTCRestObjectBase::kTypeClass_Int16; }
};

template<typename RestType, typename IntType, typename ConstantClass>
void testInteger(void)
{
    ConstantClass const Consts;
    RTTestSub(g_hTest, Consts.getSubName());

    {
        RestType obj1;
        RTTESTI_CHECK(obj1.m_iValue == 0);
        RTTESTI_CHECK(obj1.isNull() == false);
        RTTESTI_CHECK(strcmp(obj1.typeName(), Consts.getTypeName()) == 0);
        RTTESTI_CHECK(obj1.typeClass() == Consts.getTypeClass());
    }

    {
        RestType obj2(2398);
        RTTESTI_CHECK(obj2.m_iValue == 2398);
        RTTESTI_CHECK(obj2.isNull() == false);
    }

    {
        RestType obj2(-7345);
        RTTESTI_CHECK(obj2.m_iValue == -7345);
        RTTESTI_CHECK(obj2.isNull() == false);
    }

    {
        /* Value assignments: */
        RestType obj3;
        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true);
        RTTESTI_CHECK(obj3.m_iValue == 0);
        obj3.assignValue(-1);
        RTTESTI_CHECK(obj3.m_iValue == -1);
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true);
        obj3.assignValue(42);
        RTTESTI_CHECK(obj3.m_iValue == 42);
        RTTESTI_CHECK(obj3.isNull() == false);

        obj3.assignValue(Consts.getMax());
        RTTESTI_CHECK(obj3.m_iValue == Consts.getMax());
        RTTESTI_CHECK(obj3.isNull() == false);

        obj3.assignValue(Consts.getMin());
        RTTESTI_CHECK(obj3.m_iValue == Consts.getMin());
        RTTESTI_CHECK(obj3.isNull() == false);

        /* Reset to default: */
        RTTESTI_CHECK_RC(obj3.resetToDefault(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.m_iValue == 0);
        RTTESTI_CHECK(obj3.isNull() == false);

        obj3.assignValue(42);
        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK_RC(obj3.resetToDefault(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.m_iValue == 0);
        RTTESTI_CHECK(obj3.isNull() == false);

        /* Copy assignments: */
        RestType obj3Max(Consts.getMax());
        RTTESTI_CHECK(obj3Max.m_iValue == Consts.getMax());
        RTTESTI_CHECK(obj3Max.isNull() == false);
        RestType obj3Min(Consts.getMin());
        RTTESTI_CHECK(obj3Min.m_iValue == Consts.getMin());
        RTTESTI_CHECK(obj3Min.isNull() == false);
        RestType obj3Null;
        obj3Null.setNull();
        RTTESTI_CHECK(obj3Null.m_iValue == 0);
        RTTESTI_CHECK(obj3Null.isNull() == true);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK_RC(obj3.assignCopy(obj3Max), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.m_iValue == Consts.getMax());
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.assignCopy(obj3Null), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.m_iValue == 0);
        RTTESTI_CHECK(obj3.isNull() == true);

        RTTESTI_CHECK_RC(obj3.assignCopy(obj3Min), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.m_iValue == Consts.getMin());
        RTTESTI_CHECK(obj3.isNull() == false);

        obj3 = obj3Null;
        RTTESTI_CHECK(obj3.m_iValue == 0);
        RTTESTI_CHECK(obj3.isNull() == true);

        obj3 = obj3Max;
        RTTESTI_CHECK(obj3.m_iValue == Consts.getMax());
        RTTESTI_CHECK(obj3.isNull() == false);

        obj3 = obj3Null;
        RTTESTI_CHECK(obj3.m_iValue == 0);
        RTTESTI_CHECK(obj3.isNull() == true);

        obj3 = obj3Min;
        RTTESTI_CHECK(obj3.m_iValue == Consts.getMin());
        RTTESTI_CHECK(obj3.isNull() == false);

        /* setNull implies resetToDefault: */
        obj3 = obj3Max;
        RTTESTI_CHECK(obj3.m_iValue == Consts.getMax());
        RTTESTI_CHECK(obj3.isNull() == false);
        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true);
        RTTESTI_CHECK(obj3.m_iValue == 0);

        /* Copy constructors: */
        {
            RestType obj3a(obj3Max);
            RTTESTI_CHECK(obj3a.m_iValue == Consts.getMax());
            RTTESTI_CHECK(obj3a.isNull() == false);
        }
        {
            RestType obj3b(obj3Min);
            RTTESTI_CHECK(obj3b.m_iValue == Consts.getMin());
            RTTESTI_CHECK(obj3b.isNull() == false);
        }
        {
            RestType obj3c(obj3Null);
            RTTESTI_CHECK(obj3c.m_iValue == 0);
            RTTESTI_CHECK(obj3c.isNull() == true);
        }

        /* Serialization to json: */
        const char *pszJson = toJson(&obj3Max);
        RTTESTI_CHECK_MSG(strcmp(pszJson, Consts.getMaxStr()) == 0, ("pszJson=%s\n", pszJson));
        pszJson = toJson(&obj3Min);
        RTTESTI_CHECK_MSG(strcmp(pszJson, Consts.getMinStr()) == 0, ("pszJson=%s\n", pszJson));
        pszJson = toJson(&obj3Null);
        RTTESTI_CHECK_MSG(strcmp(pszJson, "null") == 0, ("pszJson=%s\n", pszJson));

        /* Serialization to string. */
        RTCString str;
        RTCString strExpect;
        str = "lead-in:";
        RTTESTI_CHECK_RC(obj3Max.toString(&str, RTCRestObjectBase::kToString_Append), VINF_SUCCESS);
        strExpect.printf("lead-in:%s", Consts.getMaxStr());
        RTTESTI_CHECK_MSG(str.equals(strExpect), ("str=%s strExpect=%s\n", str.c_str(), strExpect.c_str()));
        RTTESTI_CHECK_RC(obj3Max.toString(&str), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals(Consts.getMaxStr()), ("str=%s\n", str.c_str()));

        str = "lead-in:";
        RTTESTI_CHECK_RC(obj3Min.toString(&str, RTCRestObjectBase::kToString_Append), VINF_SUCCESS);
        strExpect.printf("lead-in:%s", Consts.getMinStr());
        RTTESTI_CHECK_MSG(str.equals(strExpect), ("str=%s strExpect=%s\n", str.c_str(), strExpect.c_str()));
        RTTESTI_CHECK_RC(obj3Min.toString(&str), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals(Consts.getMinStr()), ("str=%s\n", str.c_str()));

        str = "lead-in:";
        RTTESTI_CHECK_RC(obj3Null.toString(&str, RTCRestObjectBase::kToString_Append), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("lead-in:null"), ("str=%s\n", str.c_str()));
        RTTESTI_CHECK_RC(obj3Null.toString(&str), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("null"), ("str=%s\n", str.c_str()));
    }

    /* deserialize: */
    RTERRINFOSTATIC ErrInfo;
    {
        /* from json: */
        RestType obj4;
        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "42", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == 42);
        RTTESTI_CHECK(obj4.isNull() == false);

        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "-22", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == -22);
        RTTESTI_CHECK(obj4.isNull() == false);

        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, Consts.getMaxStr(), &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == Consts.getMax());
        RTTESTI_CHECK(obj4.isNull() == false);

        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, Consts.getMinStr(), &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == Consts.getMin());
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "null", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == 0);
        RTTESTI_CHECK(obj4.isNull() == true);

        /* object goes to default state on failure: */
        obj4.assignValue(Consts.getMin());
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "0.0", &ErrInfo, RT_XSTR(__LINE__)), VERR_REST_WRONG_JSON_TYPE_FOR_INTEGER);
        RTTESTI_CHECK(obj4.m_iValue == 0);
        RTTESTI_CHECK(obj4.isNull() == false);
        RTTESTI_CHECK(RTErrInfoIsSet(&ErrInfo.Core));

        obj4.assignValue(Consts.getMax());
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "\"false\"", &ErrInfo, RT_XSTR(__LINE__)), VERR_REST_WRONG_JSON_TYPE_FOR_INTEGER);
        RTTESTI_CHECK(obj4.m_iValue == 0);
        RTTESTI_CHECK(obj4.isNull() == false);
        RTTESTI_CHECK(RTErrInfoIsSet(&ErrInfo.Core));

        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "[ null ]", NULL, RT_XSTR(__LINE__)), VERR_REST_WRONG_JSON_TYPE_FOR_INTEGER);
        RTTESTI_CHECK(obj4.m_iValue == 0);
        RTTESTI_CHECK(obj4.isNull() == false);

        /* from string: */
        obj4.setNull();
        RTTESTI_CHECK_RC(fromString(&obj4, "22", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == 22);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, "-42", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == -42);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, Consts.getMaxStr(), &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == Consts.getMax());
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, Consts.getMinStr(), &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == Consts.getMin());
        RTTESTI_CHECK(obj4.isNull() == false);

        obj4.m_iValue = 33;
        RTTESTI_CHECK_RC(fromString(&obj4, "null", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == 0);
        RTTESTI_CHECK(obj4.isNull() == true);

        obj4.m_iValue = 33;
        RTTESTI_CHECK_RC(fromString(&obj4, " nULl;", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == 0);
        RTTESTI_CHECK(obj4.isNull() == true);

        obj4.setNull();
        RTTESTI_CHECK_RC(fromString(&obj4, " 0x42 ", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == 0x42);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, "\t010\t", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == 8);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, "\r\t0X4FDB\t", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == 0x4fdb);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, "1.1", &ErrInfo, RT_XSTR(__LINE__)), VERR_TRAILING_CHARS);
        RTTESTI_CHECK(RTErrInfoIsSet(&ErrInfo.Core));

        RTTESTI_CHECK_RC(fromString(&obj4, "false", NULL, RT_XSTR(__LINE__)), VERR_NO_DIGITS);
    }
}


void testDouble(void)
{
    RTTestSub(g_hTest, "RTCRestDouble");
#define DBL_MAX_STRING  "1.7976931348623157e+308"
#define DBL_MIN_STRING  "2.2250738585072014e-308"

    {
        RTCRestDouble obj1;
        RTTESTI_CHECK(obj1.m_rdValue == 0.0);
        RTTESTI_CHECK(obj1.isNull() == false);
        RTTESTI_CHECK(strcmp(obj1.typeName(), "double") == 0);
        RTTESTI_CHECK(obj1.typeClass() == RTCRestObjectBase::kTypeClass_Double);
    }

    {
        RTCRestDouble obj2(2398.1);
        RTTESTI_CHECK(obj2.m_rdValue == 2398.1);
        RTTESTI_CHECK(obj2.isNull() == false);
    }

    {
        RTCRestDouble obj2(-7345.2);
        RTTESTI_CHECK(obj2.m_rdValue == -7345.2);
        RTTESTI_CHECK(obj2.isNull() == false);
    }

    {
        /* Value assignments: */
        RTCRestDouble obj3;
        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true);
        RTTESTI_CHECK(obj3.m_rdValue == 0.0);
        obj3.assignValue(-1.0);
        RTTESTI_CHECK(obj3.m_rdValue == -1.0);
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true);
        obj3.assignValue(42.42);
        RTTESTI_CHECK(obj3.m_rdValue == 42.42);
        RTTESTI_CHECK(obj3.isNull() == false);

        obj3.assignValue(DBL_MAX);
        RTTESTI_CHECK(obj3.m_rdValue == DBL_MAX);
        RTTESTI_CHECK(obj3.isNull() == false);

        obj3.assignValue(DBL_MIN);
        RTTESTI_CHECK(obj3.m_rdValue == DBL_MIN);
        RTTESTI_CHECK(obj3.isNull() == false);

        /* Reset to default: */
        RTTESTI_CHECK_RC(obj3.resetToDefault(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.m_rdValue == 0.0);
        RTTESTI_CHECK(obj3.isNull() == false);

        obj3.assignValue(42);
        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK_RC(obj3.resetToDefault(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.m_rdValue == 0.0);
        RTTESTI_CHECK(obj3.isNull() == false);

        /* Copy assignments: */
        RTCRestDouble obj3Max(DBL_MAX);
        RTTESTI_CHECK(obj3Max.m_rdValue == DBL_MAX);
        RTTESTI_CHECK(obj3Max.isNull() == false);
        RTCRestDouble obj3Min(DBL_MIN);
        RTTESTI_CHECK(obj3Min.m_rdValue == DBL_MIN);
        RTTESTI_CHECK(obj3Min.isNull() == false);
        RTCRestDouble obj3Null;
        obj3Null.setNull();
        RTTESTI_CHECK(obj3Null.m_rdValue == 0.0);
        RTTESTI_CHECK(obj3Null.isNull() == true);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK_RC(obj3.assignCopy(obj3Max), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.m_rdValue == DBL_MAX);
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.assignCopy(obj3Null), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.m_rdValue == 0.0);
        RTTESTI_CHECK(obj3.isNull() == true);

        RTTESTI_CHECK_RC(obj3.assignCopy(obj3Min), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.m_rdValue == DBL_MIN);
        RTTESTI_CHECK(obj3.isNull() == false);

        obj3 = obj3Null;
        RTTESTI_CHECK(obj3.m_rdValue == 0.0);
        RTTESTI_CHECK(obj3.isNull() == true);

        obj3 = obj3Max;
        RTTESTI_CHECK(obj3.m_rdValue == DBL_MAX);
        RTTESTI_CHECK(obj3.isNull() == false);

        obj3 = obj3Null;
        RTTESTI_CHECK(obj3.m_rdValue == 0.0);
        RTTESTI_CHECK(obj3.isNull() == true);

        obj3 = obj3Min;
        RTTESTI_CHECK(obj3.m_rdValue == DBL_MIN);
        RTTESTI_CHECK(obj3.isNull() == false);

        /* setNull implies resetToDefault: */
        obj3 = obj3Max;
        RTTESTI_CHECK(obj3.m_rdValue == DBL_MAX);
        RTTESTI_CHECK(obj3.isNull() == false);
        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true);
        RTTESTI_CHECK(obj3.m_rdValue == 0.0);

        /* Copy constructors: */
        {
            RTCRestDouble obj3a(obj3Max);
            RTTESTI_CHECK(obj3a.m_rdValue == DBL_MAX);
            RTTESTI_CHECK(obj3a.isNull() == false);
        }
        {
            RTCRestDouble obj3b(obj3Min);
            RTTESTI_CHECK(obj3b.m_rdValue == DBL_MIN);
            RTTESTI_CHECK(obj3b.isNull() == false);
        }
        {
            RTCRestDouble obj3c(obj3Null);
            RTTESTI_CHECK(obj3c.m_rdValue == 0.0);
            RTTESTI_CHECK(obj3c.isNull() == true);
        }

        /* Serialization to json: */
        const char *pszJson = toJson(&obj3Max);
        RTTESTI_CHECK_MSG(strcmp(pszJson, DBL_MAX_STRING) == 0, ("pszJson=%s\n", pszJson));
        pszJson = toJson(&obj3Min);
        RTTESTI_CHECK_MSG(strcmp(pszJson, DBL_MIN_STRING) == 0, ("pszJson=%s\n", pszJson));
        pszJson = toJson(&obj3Null);
        RTTESTI_CHECK_MSG(strcmp(pszJson, "null") == 0, ("pszJson=%s\n", pszJson));

        /* Serialization to string. */
        RTCString str;
        RTCString strExpect;
        str = "lead-in:";
        RTTESTI_CHECK_RC(obj3Max.toString(&str, RTCRestObjectBase::kToString_Append), VINF_SUCCESS);
        strExpect.printf("lead-in:%s", DBL_MAX_STRING);
        RTTESTI_CHECK_MSG(str.equals(strExpect), ("str=%s strExpect=%s\n", str.c_str(), strExpect.c_str()));
        RTTESTI_CHECK_RC(obj3Max.toString(&str), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals(DBL_MAX_STRING), ("str=%s\n", str.c_str()));

        str = "lead-in:";
        RTTESTI_CHECK_RC(obj3Min.toString(&str, RTCRestObjectBase::kToString_Append), VINF_SUCCESS);
        strExpect.printf("lead-in:%s", DBL_MIN_STRING);
        RTTESTI_CHECK_MSG(str.equals(strExpect), ("str=%s strExpect=%s\n", str.c_str(), strExpect.c_str()));
        RTTESTI_CHECK_RC(obj3Min.toString(&str), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals(DBL_MIN_STRING), ("str=%s\n", str.c_str()));

        str = "lead-in:";
        RTTESTI_CHECK_RC(obj3Null.toString(&str, RTCRestObjectBase::kToString_Append), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("lead-in:null"), ("str=%s\n", str.c_str()));
        RTTESTI_CHECK_RC(obj3Null.toString(&str), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("null"), ("str=%s\n", str.c_str()));
    }

    /* deserialize: */
    RTERRINFOSTATIC ErrInfo;
    {
        /* from json: */
        RTCRestDouble obj4;
        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "42.42", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_rdValue == 42.42);
        RTTESTI_CHECK(obj4.isNull() == false);

        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "-22.22", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_rdValue == -22.22);
        RTTESTI_CHECK(obj4.isNull() == false);

        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, DBL_MAX_STRING, &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_rdValue == DBL_MAX);
        RTTESTI_CHECK(obj4.isNull() == false);

        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, DBL_MIN_STRING, &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_rdValue == DBL_MIN);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "null", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_rdValue == 0.0);
        RTTESTI_CHECK(obj4.isNull() == true);

        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "14323", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_rdValue == 14323.0);
        RTTESTI_CHECK(obj4.isNull() == false);

        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "-234875", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_rdValue == -234875.0);
        RTTESTI_CHECK(obj4.isNull() == false);

        /* object goes to default state on failure: */
        obj4.assignValue(DBL_MIN);
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "false", &ErrInfo, RT_XSTR(__LINE__)), VERR_REST_WRONG_JSON_TYPE_FOR_DOUBLE);
        RTTESTI_CHECK(obj4.m_rdValue == 0.0);
        RTTESTI_CHECK(obj4.isNull() == false);
        RTTESTI_CHECK(RTErrInfoIsSet(&ErrInfo.Core));

        obj4.assignValue(DBL_MAX);
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "\"false\"", &ErrInfo, RT_XSTR(__LINE__)), VERR_REST_WRONG_JSON_TYPE_FOR_DOUBLE);
        RTTESTI_CHECK(obj4.m_rdValue == 0.0);
        RTTESTI_CHECK(obj4.isNull() == false);
        RTTESTI_CHECK(RTErrInfoIsSet(&ErrInfo.Core));

        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "[ null ]", NULL, RT_XSTR(__LINE__)), VERR_REST_WRONG_JSON_TYPE_FOR_DOUBLE);
        RTTESTI_CHECK(obj4.m_rdValue == 0.0);
        RTTESTI_CHECK(obj4.isNull() == false);

        /* from string: */
        obj4.setNull();
        RTTESTI_CHECK_RC(fromString(&obj4, "22.42", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_rdValue == 22.42);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, "-42.22", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_rdValue == -42.22);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, DBL_MAX_STRING, &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_rdValue == DBL_MAX);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, DBL_MIN_STRING, &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_rdValue == DBL_MIN);
        RTTESTI_CHECK(obj4.isNull() == false);

        obj4.m_rdValue = 33.33;
        RTTESTI_CHECK_RC(fromString(&obj4, "null", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_rdValue == 0.0);
        RTTESTI_CHECK(obj4.isNull() == true);

        obj4.m_rdValue = 33.33;
        RTTESTI_CHECK_RC(fromString(&obj4, " nULl;", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_rdValue == 0.0);
        RTTESTI_CHECK(obj4.isNull() == true);

        obj4.setNull();
        RTTESTI_CHECK_RC(fromString(&obj4, " 42.22 ", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_rdValue == 42.22);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, "\t010\t", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_rdValue ==10.0);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, "\r\t03495.344\t\r\n", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_rdValue == 3495.344);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, "1.1;", &ErrInfo, RT_XSTR(__LINE__)), VERR_TRAILING_CHARS);
        RTTESTI_CHECK(RTErrInfoIsSet(&ErrInfo.Core));

        RTTESTI_CHECK_RC(fromString(&obj4, "false", NULL, RT_XSTR(__LINE__)), VERR_NO_DIGITS);

        RTTESTI_CHECK_RC(fromString(&obj4, " 0x42 ", &ErrInfo, RT_XSTR(__LINE__)), VERR_TRAILING_CHARS);
        RTTESTI_CHECK(obj4.m_rdValue == 0.0);
        RTTESTI_CHECK(obj4.isNull() == false);
    }
}


void testString(const char *pszDummy, ...)
{
    RTTestSub(g_hTest, "RTCRestString");

    {
        RTCRestString obj1;
        RTTESTI_CHECK(obj1.isEmpty());
        RTTESTI_CHECK(obj1.isNull() == false);
        RTTESTI_CHECK(strcmp(obj1.typeName(), "RTCString") == 0);
        RTTESTI_CHECK(obj1.typeClass() == RTCRestObjectBase::kTypeClass_String);
    }

    {
        RTCRestString obj2(RTCString("2398.1"));
        RTTESTI_CHECK(obj2 == "2398.1");
        RTTESTI_CHECK(obj2.isNull() == false);
    }

    {
        RTCRestString obj2("-7345.2");
        RTTESTI_CHECK(obj2 == "-7345.2");
        RTTESTI_CHECK(obj2.isNull() == false);
    }

    {
        /* Value assignments: */
        RTCRestString obj3;
        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true);
        RTTESTI_CHECK(obj3.isEmpty());
        obj3 = "-1.0";
        RTTESTI_CHECK(obj3 == "-1.0");
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true && obj3.isEmpty());
        obj3 = RTCString("-2.0");
        RTTESTI_CHECK(obj3 == "-2.0");
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true && obj3.isEmpty());
        obj3 = RTCRestString("-3.0");
        RTTESTI_CHECK(obj3 == "-3.0");
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true && obj3.isEmpty());
        RTTESTI_CHECK_RC(obj3.assignNoThrow(RTCRestString("4.0")), VINF_SUCCESS);
        RTTESTI_CHECK(obj3 == "4.0");
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true && obj3.isEmpty());
        RTTESTI_CHECK_RC(obj3.assignNoThrow("-4.0"), VINF_SUCCESS);
        RTTESTI_CHECK(obj3 == "-4.0");
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true && obj3.isEmpty());
        RTTESTI_CHECK_RC(obj3.assignNoThrow(RTCRestString("0123456789"), 3, 5), VINF_SUCCESS);
        RTTESTI_CHECK(obj3 == "34567");
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true && obj3.isEmpty());
        RTTESTI_CHECK_RC(obj3.assignNoThrow("0123456789", 4), VINF_SUCCESS);
        RTTESTI_CHECK(obj3 == "0123");
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true && obj3.isEmpty());
        RTTESTI_CHECK_RC(obj3.assignNoThrow(8, 'x'), VINF_SUCCESS);
        RTTESTI_CHECK(obj3 == "xxxxxxxx");
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true && obj3.isEmpty());
        RTTESTI_CHECK_RC(obj3.printfNoThrow("%d%s%d", 42, "asdf", 22), VINF_SUCCESS);
        RTTESTI_CHECK(obj3 == "42asdf22");
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true && obj3.isEmpty());
        va_list va;
        va_start(va, pszDummy);
        RTTESTI_CHECK_RC(obj3.printfVNoThrow("asdf", va), VINF_SUCCESS);
        va_end(va);
        RTTESTI_CHECK(obj3 == "asdf");
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true && obj3.isEmpty());
        obj3.assign(RTCRestString("4.0"));
        RTTESTI_CHECK(obj3 == "4.0");
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true && obj3.isEmpty());
        obj3.assign("-4.0");
        RTTESTI_CHECK(obj3 == "-4.0");
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true && obj3.isEmpty());
        obj3.assign(RTCRestString("0123456789"), 3, 5);
        RTTESTI_CHECK(obj3 == "34567");
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true && obj3.isEmpty());
        obj3.assign("0123456789", 4);
        RTTESTI_CHECK(obj3 == "0123");
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true && obj3.isEmpty());
        obj3.assign(8, 'x');
        RTTESTI_CHECK(obj3 == "xxxxxxxx");
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true && obj3.isEmpty());
        obj3.printf("%d%s%d", 42, "asdf", 22);
        RTTESTI_CHECK(obj3 == "42asdf22");
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true && obj3.isEmpty());
        va_start(va, pszDummy);
        obj3.printfV("asdf", va);
        va_end(va);
        RTTESTI_CHECK(obj3 == "asdf");
        RTTESTI_CHECK(obj3.isNull() == false);

        /* Reset to default: */
        RTTESTI_CHECK_RC(obj3.resetToDefault(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isEmpty());
        RTTESTI_CHECK(obj3.isNull() == false);

        obj3 = "1";
        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK_RC(obj3.resetToDefault(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isEmpty());
        RTTESTI_CHECK(obj3.isNull() == false);

        /* Copy assignments: */
        RTCRestString const obj3Max("max");
        RTTESTI_CHECK(obj3Max == "max");
        RTTESTI_CHECK(obj3Max.isNull() == false);
        RTCRestString obj3Null;
        obj3Null.setNull();
        RTTESTI_CHECK(obj3Null.isEmpty());
        RTTESTI_CHECK(obj3Null.isNull() == true);
        RTCRestString obj3Empty;
        RTTESTI_CHECK(obj3Empty.isEmpty());
        RTTESTI_CHECK(obj3Empty.isNull() == false);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true && obj3.isEmpty());
        RTTESTI_CHECK_RC(obj3.assignCopy(obj3Max), VINF_SUCCESS);
        RTTESTI_CHECK(obj3 == "max");
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true && obj3.isEmpty());
        RTTESTI_CHECK_RC(obj3.assignCopy(obj3Null), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isEmpty());
        RTTESTI_CHECK(obj3.isNull() == true);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true && obj3.isEmpty());
        RTTESTI_CHECK_RC(obj3.assignCopy(obj3Empty), VINF_SUCCESS);
        RTTESTI_CHECK(obj3 == "");
        RTTESTI_CHECK(obj3.isEmpty());
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true && obj3.isEmpty());
        RTTESTI_CHECK_RC(obj3.assignCopy(RTCString("11.0")), VINF_SUCCESS);
        RTTESTI_CHECK(obj3 == "11.0");
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true && obj3.isEmpty());
        RTTESTI_CHECK_RC(obj3.assignCopy("12.0"), VINF_SUCCESS);
        RTTESTI_CHECK(obj3 == "12.0");
        RTTESTI_CHECK(obj3.isNull() == false);

        /* setNull implies resetToDefault: */
        obj3 = obj3Max;
        RTTESTI_CHECK(obj3 == "max");
        RTTESTI_CHECK(obj3.isNull() == false);
        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true);
        RTTESTI_CHECK(obj3.isEmpty());

        /* Copy constructors: */
        {
            RTCRestString obj3a(obj3Max);
            RTTESTI_CHECK(obj3a == "max");
            RTTESTI_CHECK(obj3a.isNull() == false);
        }
        {
            RTCRestString const obj3c(obj3Null);
            RTTESTI_CHECK(obj3c.isEmpty());
            RTTESTI_CHECK(obj3c.isNull() == true);
        }

        /* Serialization to json: */
        const char *pszJson = toJson(&obj3Max);
        RTTESTI_CHECK_MSG(strcmp(pszJson, "\"max\"") == 0, ("pszJson=%s\n", pszJson));
        pszJson = toJson(&obj3Null);
        RTTESTI_CHECK_MSG(strcmp(pszJson, "null") == 0, ("pszJson=%s\n", pszJson));
        pszJson = toJson(&obj3Empty);
        RTTESTI_CHECK_MSG(strcmp(pszJson, "\"\"") == 0, ("pszJson=%s\n", pszJson));

        /* Serialization to string. */
        RTCString str;
        RTCString strExpect;
        str = "lead-in:";
        RTTESTI_CHECK_RC(obj3Max.toString(&str, RTCRestObjectBase::kToString_Append), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("lead-in:max"), ("str=%s\n", str.c_str()));
        RTTESTI_CHECK_RC(obj3Max.toString(&str), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("max"), ("str=%s\n", str.c_str()));

        str = "lead-in:";
        RTTESTI_CHECK_RC(obj3Empty.toString(&str, RTCRestObjectBase::kToString_Append), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("lead-in:"), ("str=%s\n", str.c_str()));
        RTTESTI_CHECK_RC(obj3Empty.toString(&str), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals(""), ("str=%s\n", str.c_str()));

        str = "lead-in:";
        RTTESTI_CHECK_RC(obj3Null.toString(&str, RTCRestObjectBase::kToString_Append), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("lead-in:"), ("str=%s\n", str.c_str()));
        RTTESTI_CHECK_RC(obj3Null.toString(&str), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals(""), ("str=%s\n", str.c_str()));
    }

    /* deserialize: */
    RTERRINFOSTATIC ErrInfo;
    {
        /* from json: */
        RTCRestString obj4;
        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "\"42.42\"", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4 == "42.42");
        RTTESTI_CHECK(obj4.isNull() == false);

        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "\"-22.22\"", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4 == "-22.22");
        RTTESTI_CHECK(obj4.isNull() == false);

        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "\"maximum\"", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4 == "maximum");
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "null", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.isEmpty());
        RTTESTI_CHECK(obj4.isNull() == true);

        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "\"\\u0020\"", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4 == " ");
        RTTESTI_CHECK(obj4.isNull() == false);

        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "\"\\u004f\\u004D\\u0047\\u0021 :-)\"",
                                             &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4 == "OMG! :-)");
        RTTESTI_CHECK(obj4.isNull() == false);

        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "\"42:\\uD801\\udC37\\ud852\\uDf62:42\"",  /* U+10437 U+24B62 */
                                             &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4 == "42:" "\xf0\x90\x90\xb7" "\xf0\xa4\xad\xa2" ":42");
        RTTESTI_CHECK(obj4.isNull() == false);

        /* object goes to default state on failure: */
        obj4 = "asdf";
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "false", &ErrInfo, RT_XSTR(__LINE__)), VERR_REST_WRONG_JSON_TYPE_FOR_STRING);
        RTTESTI_CHECK(obj4.isEmpty());
        RTTESTI_CHECK(obj4.isNull() == false);
        RTTESTI_CHECK(RTErrInfoIsSet(&ErrInfo.Core));

        obj4 = "asdf";
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "1", &ErrInfo, RT_XSTR(__LINE__)), VERR_REST_WRONG_JSON_TYPE_FOR_STRING);
        RTTESTI_CHECK(obj4.isEmpty());
        RTTESTI_CHECK(obj4.isNull() == false);
        RTTESTI_CHECK(RTErrInfoIsSet(&ErrInfo.Core));

        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "[ null ]", NULL, RT_XSTR(__LINE__)), VERR_REST_WRONG_JSON_TYPE_FOR_STRING);
        RTTESTI_CHECK(obj4.isEmpty());
        RTTESTI_CHECK(obj4.isNull() == false);

        /* from string: */
        obj4.setNull();
        RTTESTI_CHECK_RC(fromString(&obj4, "22.42", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4 == "22.42");
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, "-42.22", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4 == "-42.22");
        RTTESTI_CHECK(obj4.isNull() == false);

        obj4.setNull();
        RTTESTI_CHECK_RC(fromString(&obj4, "null", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4 == "null");
        RTTESTI_CHECK(obj4.isNull() == false);
    }
}


void testDate()
{
    RTTestSub(g_hTest, "RTCRestDate");
    int64_t const iRecent    = INT64_C(1536580687739632500);
    int64_t const iRecentSec = INT64_C(1536580687000000000);
    RTTIMESPEC TimeSpec;

#define CHECK_DATE(a_obj, a_fNull, a_fOkay, a_i64Nano, a_sz, a_fUtc) \
    do { \
        RTTESTI_CHECK((a_obj).isOkay() == (a_fOkay)); \
        if ((a_obj).getEpochNano() != (a_i64Nano)) \
            RTTestIFailed("line " RT_XSTR(__LINE__) ": getEpochNano=%RI64, expected %RI64", (a_obj).getEpochNano(), (int64_t)(a_i64Nano)); \
        if (!(a_obj).getString().equals(a_sz)) \
            RTTestIFailed("line " RT_XSTR(__LINE__) ": getString=%s, expected %s", (a_obj).getString().c_str(), a_sz); \
        RTTESTI_CHECK((a_obj).isUtc() == (a_fUtc)); \
        RTTESTI_CHECK((a_obj).isNull() == (a_fNull)); \
    } while (0)
#define CHECK_DATE_FMT(a_obj, a_fNull, a_fOkay, a_i64Nano, a_sz, a_fUtc, a_enmFormat) \
    do { \
        CHECK_DATE(a_obj, a_fNull, a_fOkay, a_i64Nano, a_sz, a_fUtc); \
        if ((a_obj).getFormat() != (a_enmFormat)) \
            RTTestIFailed("line " RT_XSTR(__LINE__) ": getFormat=%d, expected %d (%s)", (a_obj).getFormat(), (a_enmFormat), #a_enmFormat); \
    } while (0)

    {
        RTCRestDate obj1;
        CHECK_DATE(obj1, true, false, 0, "", true);
        RTTESTI_CHECK(strcmp(obj1.typeName(), "RTCRestDate") == 0);
        RTTESTI_CHECK(obj1.typeClass() == RTCRestObjectBase::kTypeClass_Date);
    }

    {
        /* Value assignments: */
        RTCRestDate obj3;
        RTTESTI_CHECK_RC(obj3.assignValue(RTTimeSpecSetNano(&TimeSpec, 0), RTCRestDate::kFormat_Rfc3339), VINF_SUCCESS);
        CHECK_DATE(obj3, false, true, 0, "1970-01-01T00:00:00Z", true);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        CHECK_DATE(obj3, true, false, 0, "", true);
        RTTESTI_CHECK_RC(obj3.assignValueRfc3339(RTTimeSpecSetNano(&TimeSpec, 0)), VINF_SUCCESS);
        CHECK_DATE(obj3, false, true, 0, "1970-01-01T00:00:00Z", true);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        CHECK_DATE(obj3, true, false, 0, "", true);
        RTTESTI_CHECK_RC(obj3.assignValueRfc2822(RTTimeSpecSetNano(&TimeSpec, 0)), VINF_SUCCESS);
        CHECK_DATE(obj3, false, true, 0, "Thu, 1 Jan 1970 00:00:00 -0000", true);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK_RC(obj3.assignValueRfc7131(RTTimeSpecSetNano(&TimeSpec, 0)), VINF_SUCCESS);
        CHECK_DATE(obj3, false, true, 0, "Thu, 1 Jan 1970 00:00:00 GMT", true);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK_RC(obj3.assignValue(RTTimeSpecSetNano(&TimeSpec, 0), RTCRestDate::kFormat_Rfc2822), VINF_SUCCESS);
        CHECK_DATE(obj3, false, true, 0, "Thu, 1 Jan 1970 00:00:00 -0000", true);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK_RC(obj3.assignValue(RTTimeSpecSetNano(&TimeSpec, 0), RTCRestDate::kFormat_Rfc7131), VINF_SUCCESS);
        CHECK_DATE(obj3, false, true, 0, "Thu, 1 Jan 1970 00:00:00 GMT", true);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK_RC(obj3.assignValue(RTTimeSpecSetNano(&TimeSpec, 0), RTCRestDate::kFormat_Rfc3339_Fraction_9), VINF_SUCCESS);
        CHECK_DATE(obj3, false, true, 0, "1970-01-01T00:00:00.000000000Z", true);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK_RC(obj3.assignValue(RTTimeSpecSetNano(&TimeSpec, 0), RTCRestDate::kFormat_Rfc3339_Fraction_6), VINF_SUCCESS);
        CHECK_DATE(obj3, false, true, 0, "1970-01-01T00:00:00.000000Z", true);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK_RC(obj3.assignValue(RTTimeSpecSetNano(&TimeSpec, 0), RTCRestDate::kFormat_Rfc3339_Fraction_3), VINF_SUCCESS);
        CHECK_DATE(obj3, false, true, 0, "1970-01-01T00:00:00.000Z", true);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK_RC(obj3.assignValue(RTTimeSpecSetNano(&TimeSpec, 0), RTCRestDate::kFormat_Rfc3339_Fraction_2), VINF_SUCCESS);
        CHECK_DATE(obj3, false, true, 0, "1970-01-01T00:00:00.00Z", true);

        /* Format changes: */
        RTTESTI_CHECK_RC(obj3.assignValue(RTTimeSpecSetNano(&TimeSpec, 59123456789), RTCRestDate::kFormat_Rfc3339_Fraction_9), VINF_SUCCESS);
        CHECK_DATE(obj3, false, true, 59123456789, "1970-01-01T00:00:59.123456789Z", true);
        RTTESTI_CHECK_RC(obj3.setFormat(RTCRestDate::kFormat_Rfc2822), VINF_SUCCESS);
        CHECK_DATE(obj3, false, true, 59123456789, "Thu, 1 Jan 1970 00:00:59 -0000", true);
        RTTESTI_CHECK_RC(obj3.setFormat(RTCRestDate::kFormat_Rfc7131), VINF_SUCCESS);
        CHECK_DATE(obj3, false, true, 59123456789, "Thu, 1 Jan 1970 00:00:59 GMT", true);
        RTTESTI_CHECK_RC(obj3.setFormat(RTCRestDate::kFormat_Rfc3339), VINF_SUCCESS);
        CHECK_DATE(obj3, false, true, 59123456789, "1970-01-01T00:00:59Z", true);
        RTTESTI_CHECK_RC(obj3.setFormat(RTCRestDate::kFormat_Rfc3339_Fraction_2), VINF_SUCCESS);
        CHECK_DATE(obj3, false, true, 59123456789, "1970-01-01T00:00:59.12Z", true);
        RTTESTI_CHECK_RC(obj3.setFormat(RTCRestDate::kFormat_Rfc3339_Fraction_3), VINF_SUCCESS);
        CHECK_DATE(obj3, false, true, 59123456789, "1970-01-01T00:00:59.123Z", true);
        RTTESTI_CHECK_RC(obj3.setFormat(RTCRestDate::kFormat_Rfc3339_Fraction_6), VINF_SUCCESS);
        CHECK_DATE(obj3, false, true, 59123456789, "1970-01-01T00:00:59.123456Z", true);
        RTTESTI_CHECK_RC(obj3.setFormat(RTCRestDate::kFormat_Rfc3339_Fraction_9), VINF_SUCCESS);
        CHECK_DATE(obj3, false, true, 59123456789, "1970-01-01T00:00:59.123456789Z", true);

        /* Reset to default and setNull works identically: */
        RTTESTI_CHECK_RC(obj3.resetToDefault(), VINF_SUCCESS);
        CHECK_DATE(obj3, true, false, 0, "", true);

        RTTESTI_CHECK_RC(obj3.assignValue(RTTimeSpecSetNano(&TimeSpec, 0), RTCRestDate::kFormat_Rfc3339_Fraction_2), VINF_SUCCESS);
        CHECK_DATE(obj3, false, true, 0, "1970-01-01T00:00:00.00Z", true);
        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        CHECK_DATE(obj3, true, false, 0, "", true);

        /* Copy assignments: */
        RTCRestDate obj3Epoch_3339_9;
        RTTESTI_CHECK_RC(obj3Epoch_3339_9.assignValue(RTTimeSpecSetNano(&TimeSpec, 0), RTCRestDate::kFormat_Rfc3339_Fraction_9), VINF_SUCCESS);
        CHECK_DATE(obj3Epoch_3339_9, false, true, 0, "1970-01-01T00:00:00.000000000Z", true);

        RTCRestDate obj3Epoch_7131;
        RTTESTI_CHECK_RC(obj3Epoch_7131.assignValue(RTTimeSpecSetNano(&TimeSpec, 0), RTCRestDate::kFormat_Rfc7131), VINF_SUCCESS);
        CHECK_DATE(obj3Epoch_7131, false, true, 0, "Thu, 1 Jan 1970 00:00:00 GMT", true);

        RTCRestDate obj3Recent_3339;
        RTTESTI_CHECK_RC(obj3Recent_3339.assignValue(RTTimeSpecSetNano(&TimeSpec, iRecent), RTCRestDate::kFormat_Rfc3339), VINF_SUCCESS);
        CHECK_DATE(obj3Recent_3339, false, true, iRecent, "2018-09-10T11:58:07Z", true);

        RTCRestDate obj3Recent_2822;
        RTTESTI_CHECK_RC(obj3Recent_2822.assignValue(RTTimeSpecSetNano(&TimeSpec, iRecent), RTCRestDate::kFormat_Rfc2822), VINF_SUCCESS);
        CHECK_DATE(obj3Recent_2822, false, true, iRecent, "Mon, 10 Sep 2018 11:58:07 -0000", true);

        RTCRestDate const obj3Null;
        CHECK_DATE(obj3Null, true, false, 0, "", true);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK_RC(obj3.assignCopy(obj3Epoch_3339_9), VINF_SUCCESS);
        CHECK_DATE(obj3, false, true, 0, "1970-01-01T00:00:00.000000000Z", true);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK_RC(obj3.assignCopy(obj3Epoch_7131), VINF_SUCCESS);
        CHECK_DATE(obj3, false, true, 0, "Thu, 1 Jan 1970 00:00:00 GMT", true);

        RTTESTI_CHECK_RC(obj3.assignCopy(obj3Recent_3339), VINF_SUCCESS);
        CHECK_DATE(obj3Recent_2822, false, true, iRecent, "Mon, 10 Sep 2018 11:58:07 -0000", true);

        RTTESTI_CHECK_RC(obj3.assignCopy(obj3Null), VINF_SUCCESS);
        CHECK_DATE(obj3, true, false, 0, "", true);

        obj3 = obj3Recent_2822;
        CHECK_DATE(obj3Recent_2822, false, true, iRecent, "Mon, 10 Sep 2018 11:58:07 -0000", true);

        obj3 = obj3Epoch_3339_9;
        CHECK_DATE(obj3, false, true, 0, "1970-01-01T00:00:00.000000000Z", true);

        obj3 = obj3Null;
        CHECK_DATE(obj3, true, false, 0, "", true);

        /* Copy constructors: */
        {
            RTCRestDate obj3a(obj3Epoch_3339_9);
            CHECK_DATE(obj3a, false, true, 0, "1970-01-01T00:00:00.000000000Z", true);
        }
        {
            RTCRestDate obj3b(obj3Epoch_7131);
            CHECK_DATE(obj3b, false, true, 0, "Thu, 1 Jan 1970 00:00:00 GMT", true);
        }
        {
            RTCRestDate obj3c(obj3Recent_3339);
            CHECK_DATE(obj3Recent_3339, false, true, iRecent, "2018-09-10T11:58:07Z", true);
        }
        {
            RTCRestDate obj3d(obj3Recent_2822);
            CHECK_DATE(obj3d, false, true, iRecent, "Mon, 10 Sep 2018 11:58:07 -0000", true);
        }
        {
            RTCRestDate obj3e(obj3Null);
            CHECK_DATE(obj3e, true, false, 0, "", true);
        }

        /* Serialization to json: */
        const char *pszJson = toJson(&obj3Epoch_3339_9);
        RTTESTI_CHECK_MSG(strcmp(pszJson, "\"1970-01-01T00:00:00.000000000Z\"") == 0, ("pszJson=%s\n", pszJson));
        pszJson = toJson(&obj3Epoch_7131);
        RTTESTI_CHECK_MSG(strcmp(pszJson, "\"Thu, 1 Jan 1970 00:00:00 GMT\"") == 0, ("pszJson=%s\n", pszJson));
        pszJson = toJson(&obj3Recent_3339);
        RTTESTI_CHECK_MSG(strcmp(pszJson, "\"2018-09-10T11:58:07Z\"") == 0, ("pszJson=%s\n", pszJson));
        pszJson = toJson(&obj3Recent_2822);
        RTTESTI_CHECK_MSG(strcmp(pszJson, "\"Mon, 10 Sep 2018 11:58:07 -0000\"") == 0, ("pszJson=%s\n", pszJson));
        pszJson = toJson(&obj3Null);
        RTTESTI_CHECK_MSG(strcmp(pszJson, "null") == 0, ("pszJson=%s\n", pszJson));

        /* Serialization to string. */
        RTCString str;
        str = "lead-in:";
        RTTESTI_CHECK_RC(obj3Epoch_7131.toString(&str, RTCRestObjectBase::kToString_Append), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("lead-in:Thu, 1 Jan 1970 00:00:00 GMT"), ("str=%s\n", str.c_str()));
        RTTESTI_CHECK_RC(obj3Epoch_7131.toString(&str), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("Thu, 1 Jan 1970 00:00:00 GMT"), ("str=%s\n", str.c_str()));

        str = "lead-in:";
        RTTESTI_CHECK_RC(obj3Recent_3339.toString(&str, RTCRestObjectBase::kToString_Append), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("lead-in:2018-09-10T11:58:07Z"), ("str=%s\n", str.c_str()));
        RTTESTI_CHECK_RC(obj3Recent_3339.toString(&str), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("2018-09-10T11:58:07Z"), ("str=%s\n", str.c_str()));

        str = "lead-in:";
        RTTESTI_CHECK_RC(obj3Null.toString(&str, RTCRestObjectBase::kToString_Append), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("lead-in:null"), ("str=%s\n", str.c_str()));
        RTTESTI_CHECK_RC(obj3Null.toString(&str), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("null"), ("str=%s\n", str.c_str()));
    }

    /* deserialize: */
    RTERRINFOSTATIC ErrInfo;
    {
        RTCRestDate obj4;
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "\"Thu, 1 Jan 1970 00:00:00 GMT\"", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        CHECK_DATE_FMT(obj4, false, true, 0, "Thu, 1 Jan 1970 00:00:00 GMT", true, RTCRestDate::kFormat_Rfc7131);

        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "\"Thu, 1 Jan 1970 00:00:00.0000 GMT\"", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        CHECK_DATE_FMT(obj4, false, true, 0, "Thu, 1 Jan 1970 00:00:00.0000 GMT", true, RTCRestDate::kFormat_Rfc7131);

        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "\"1 Jan 1970 00:00:00 GMT\"", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        CHECK_DATE_FMT(obj4, false, true, 0, "1 Jan 1970 00:00:00 GMT", true, RTCRestDate::kFormat_Rfc7131);

        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "\"1 Jan 1970 00:00:00\"", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        CHECK_DATE_FMT(obj4, false, true, 0, "1 Jan 1970 00:00:00", false, RTCRestDate::kFormat_Rfc2822);

        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "\"1 Jan 070 00:00:00\"", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        CHECK_DATE_FMT(obj4, false, true, 0, "1 Jan 070 00:00:00", false, RTCRestDate::kFormat_Rfc2822);

        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "\"2018-09-10T11:58:07Z\"", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        CHECK_DATE_FMT(obj4, false, true, iRecentSec , "2018-09-10T11:58:07Z", true, RTCRestDate::kFormat_Rfc3339);

        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "\"1 Jan 70 00:00:00\"", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        CHECK_DATE_FMT(obj4, false, true, 0, "1 Jan 70 00:00:00", false, RTCRestDate::kFormat_Rfc2822);

        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "\"2018-09-10T11:58:07.739632500Z\"", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        CHECK_DATE_FMT(obj4, false, true, iRecent, "2018-09-10T11:58:07.739632500Z", true, RTCRestDate::kFormat_Rfc3339_Fraction_9);

        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "null", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        CHECK_DATE_FMT(obj4, true, false, 0, "", true, RTCRestDate::kFormat_Rfc3339_Fraction_9);

        /* object goes to default state if not string and to non-okay if string: */
        RTTESTI_CHECK_RC(obj4.assignValue(RTTimeSpecSetNano(&TimeSpec, iRecent), RTCRestDate::kFormat_Rfc2822), VINF_SUCCESS);
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "true", &ErrInfo, RT_XSTR(__LINE__)), VERR_REST_WRONG_JSON_TYPE_FOR_DATE);
        CHECK_DATE_FMT(obj4, true, false, 0, "", true, RTCRestDate::kFormat_Rfc2822);

        RTTESTI_CHECK_RC(obj4.assignValue(RTTimeSpecSetNano(&TimeSpec, iRecent), RTCRestDate::kFormat_Rfc2822), VINF_SUCCESS);
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "\"string\"", &ErrInfo, RT_XSTR(__LINE__)), VWRN_REST_UNABLE_TO_DECODE_DATE);
        CHECK_DATE_FMT(obj4, false, false, 0, "string", false, RTCRestDate::kFormat_Rfc2822);

        RTTESTI_CHECK_RC(obj4.assignValue(RTTimeSpecSetNano(&TimeSpec, iRecent), RTCRestDate::kFormat_Rfc2822), VINF_SUCCESS);
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "\"0x199 string\"", &ErrInfo, RT_XSTR(__LINE__)), VWRN_REST_UNABLE_TO_DECODE_DATE);
        CHECK_DATE_FMT(obj4, false, false, 0, "0x199 string", false, RTCRestDate::kFormat_Rfc2822);

        RTTESTI_CHECK_RC(obj4.assignValue(RTTimeSpecSetNano(&TimeSpec, iRecent), RTCRestDate::kFormat_Rfc2822), VINF_SUCCESS);
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "[ null ]", &ErrInfo, RT_XSTR(__LINE__)), VERR_REST_WRONG_JSON_TYPE_FOR_DATE);
        CHECK_DATE_FMT(obj4, true, false, 0, "", true, RTCRestDate::kFormat_Rfc2822);

        RTTESTI_CHECK_RC(obj4.assignValue(RTTimeSpecSetNano(&TimeSpec, iRecent), RTCRestDate::kFormat_Rfc2822), VINF_SUCCESS);
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "{ \"foo\": 1 }", &ErrInfo, RT_XSTR(__LINE__)), VERR_REST_WRONG_JSON_TYPE_FOR_DATE);
        CHECK_DATE_FMT(obj4, true, false, 0, "", true, RTCRestDate::kFormat_Rfc2822);

        /* From string: */
        obj4.setNull();
        RTTESTI_CHECK_RC(fromString(&obj4, "Thu, 1 Jan 1970 00:00:00 GMT", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        CHECK_DATE_FMT(obj4, false, true, 0, "Thu, 1 Jan 1970 00:00:00 GMT", true, RTCRestDate::kFormat_Rfc7131);

        RTTESTI_CHECK_RC(fromString(&obj4, "Mon, 10 Sep 2018 11:58:07 -0000", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        CHECK_DATE_FMT(obj4, false, true, iRecentSec, "Mon, 10 Sep 2018 11:58:07 -0000", true, RTCRestDate::kFormat_Rfc2822);

        RTTESTI_CHECK_RC(fromString(&obj4, "\t\n\rnull;\r\n\t", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        CHECK_DATE_FMT(obj4, true, false, 0, "", true, RTCRestDate::kFormat_Rfc2822);

        RTTESTI_CHECK_RC(fromString(&obj4, "Mon, 10 Sep 2018 11:58:07 +0000", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        CHECK_DATE_FMT(obj4, false, true, iRecentSec, "Mon, 10 Sep 2018 11:58:07 +0000", false, RTCRestDate::kFormat_Rfc2822);

        RTTESTI_CHECK_RC(fromString(&obj4, "1970-01-01T00:00:00.000000000Z", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        CHECK_DATE_FMT(obj4, false, true, 0, "1970-01-01T00:00:00.000000000Z", true, RTCRestDate::kFormat_Rfc3339_Fraction_9);

        RTTESTI_CHECK_RC(fromString(&obj4, "10 Sep 2018 11:58:07 -0000", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        CHECK_DATE_FMT(obj4, false, true, iRecentSec, "10 Sep 2018 11:58:07 -0000", true, RTCRestDate::kFormat_Rfc2822);

        RTTESTI_CHECK_RC(fromString(&obj4, "null", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        CHECK_DATE_FMT(obj4, true, false, 0, "", true, RTCRestDate::kFormat_Rfc2822);

        RTTESTI_CHECK_RC(fromString(&obj4, "Mon, 10 Sep 18 11:58:07 -0000", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        CHECK_DATE_FMT(obj4, false, true, iRecentSec, "Mon, 10 Sep 18 11:58:07 -0000", true, RTCRestDate::kFormat_Rfc2822);

        RTTESTI_CHECK_RC(fromString(&obj4, "fa;se", &ErrInfo, RT_XSTR(__LINE__)), VERR_REST_UNABLE_TO_DECODE_DATE);
        CHECK_DATE_FMT(obj4, false, false, 0, "fa;se", false, RTCRestDate::kFormat_Rfc2822);

        RTTESTI_CHECK_RC(fromString(&obj4, "10 Sep 18 11:58:07", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        CHECK_DATE_FMT(obj4, false, true, iRecentSec, "10 Sep 18 11:58:07", false, RTCRestDate::kFormat_Rfc2822);

        obj4.setNull();
        RTTESTI_CHECK_RC(fromString(&obj4, "10 Sep 118 11:58:07", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        CHECK_DATE_FMT(obj4, false, true, iRecentSec, "10 Sep 118 11:58:07", false, RTCRestDate::kFormat_Rfc2822);

    }
}


int main()
{
    RTEXITCODE rcExit = RTTestInitAndCreate("tstRTRest-1", &g_hTest);
    if (rcExit == RTEXITCODE_SUCCESS )
    {
        testBool();
        testInteger<RTCRestInt64, int64_t, Int64Constants>();
        testInteger<RTCRestInt32, int32_t, Int32Constants>();
        testInteger<RTCRestInt16, int16_t, Int16Constants>();
        testDouble();
        testString("dummy", 1, 2);
        testDate();

        rcExit = RTTestSummaryAndDestroy(g_hTest);
    }
    return rcExit;
}

