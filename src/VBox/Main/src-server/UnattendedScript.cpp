/* $Id$ */
/** @file
 * Classes for reading/parsing/saving scripts for unattended installation.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_MAIN_UNATTENDED
#include "LoggingNew.h"
#include "VirtualBoxBase.h"
#include "AutoCaller.h"
#include <VBox/com/ErrorInfo.h>

#include "UnattendedScript.h"
#include "UnattendedImpl.h"

#include <iprt/errcore.h>

#include <iprt/ctype.h>
#include <iprt/file.h>
#include <iprt/vfs.h>
#include <iprt/getopt.h>
#include <iprt/path.h>

using namespace std;

#ifdef VBOX_WITH_UNATTENDED


/*********************************************************************************************************************************
*   UnattendedScriptTemplate Implementation                                                                                      *
*********************************************************************************************************************************/

UnattendedScriptTemplate::UnattendedScriptTemplate(Unattended *pUnattended, const char *pszDefaultTemplateFilename,
                                                   const char *pszDefaultFilename)
    : BaseTextScript(pUnattended, pszDefaultTemplateFilename, pszDefaultFilename), mpUnattended(pUnattended)
{
}


HRESULT UnattendedScriptTemplate::saveToString(Utf8Str &rStrDst)
{
    static const char s_szPrefix[]         = "@@VBOX_";
    static const char s_szPrefixInsert[]   = "@@VBOX_INSERT_";
    static const char s_szPrefixCond[]     = "@@VBOX_COND_";
    static const char s_szPrefixCondEnd[]  = "@@VBOX_COND_END@@";
    static const char s_szPrefixCondGuestOs[]     = "@@VBOX_GUEST_OS_COND_";
    static const char s_szPrefixCondGuestOsEnd[]  = "@@VBOX_GUEST_OS_COND_END@@";

    struct
    {
        bool    fSavedOutputting;
    }           aConds[8];
    unsigned    cConds      = 0;
    bool        fOutputting = true;
    HRESULT     hrc         = E_FAIL;
    size_t      offTemplate = 0;
    size_t      cchTemplate = mStrScriptFullContent.length();
    size_t      cchInternalCorrect = 0;//used in logic handling the placeholder @@VBOX_GUEST_OS_COND_XXX@@
    rStrDst.setNull();
    for (;;)
    {
        /*
         * Find the next placeholder and add any text before it to the output.
         */
        size_t offPlaceholder = mStrScriptFullContent.find(s_szPrefix, offTemplate);
        size_t cchToCopy = offPlaceholder != RTCString::npos ? offPlaceholder - offTemplate : cchTemplate - offTemplate;
        if (cchToCopy > 0)
        {
            if (fOutputting)
            {
                try
                {
                    rStrDst.append(mStrScriptFullContent, offTemplate + cchInternalCorrect, cchToCopy - + cchInternalCorrect);
                }
                catch (std::bad_alloc &)
                {
                    hrc = E_OUTOFMEMORY;
                    break;
                }
            }
            offTemplate += cchToCopy;
            cchInternalCorrect = 0;//don't forget to reset
        }

        /*
         * Process placeholder.
         */
        if (offPlaceholder != RTCString::npos)
        {
            /*
             * First we must find the end of the placeholder string.
             */
            const char *pszPlaceholder = mStrScriptFullContent.c_str() + offPlaceholder;
            size_t      cchPlaceholder = sizeof(s_szPrefix) - 1;
            char        ch;
            while (   offPlaceholder + cchPlaceholder < cchTemplate
                   && (ch = pszPlaceholder[cchPlaceholder]) != '\0'
                   && (   ch == '_'
                       || RT_C_IS_UPPER(ch)
                       || RT_C_IS_DIGIT(ch)) )
                cchPlaceholder++;

            if (   offPlaceholder + cchPlaceholder < cchTemplate
                && pszPlaceholder[cchPlaceholder] == '@')
            {
                cchPlaceholder++;
                if (   offPlaceholder + cchPlaceholder < cchTemplate
                    && pszPlaceholder[cchPlaceholder] == '@')
                    cchPlaceholder++;
            }

            if (   pszPlaceholder[cchPlaceholder - 1] != '@'
                || pszPlaceholder[cchPlaceholder - 2] != '@'
                || (   strncmp(pszPlaceholder, s_szPrefixInsert, sizeof(s_szPrefixInsert) - 1) != 0
                    && strncmp(pszPlaceholder, s_szPrefixCond,   sizeof(s_szPrefixCond)   - 1) != 0
                    && strncmp(pszPlaceholder, s_szPrefixCondGuestOs,   sizeof(s_szPrefixCondGuestOs) - 1) != 0) )
            {
                hrc = mpSetError->setError(E_FAIL, mpSetError->tr("Malformed template placeholder '%.*s'"),
                                           cchPlaceholder, pszPlaceholder);
                break;
            }

            offTemplate += cchPlaceholder;

            /*
             * @@VBOX_INSERT_XXX@@:
             */
            if ( strncmp(pszPlaceholder, s_szPrefixInsert, sizeof(s_szPrefixInsert) - 1) == 0 )
            {
                /*
                 * Get the placeholder value and add it to the output.
                 */
                RTCString strValue;
                hrc = getReplacement(pszPlaceholder, cchPlaceholder, fOutputting, strValue);
                if (SUCCEEDED(hrc))
                {
                    if (fOutputting)
                    {
                        try
                        {
                            rStrDst.append(strValue);
                        }
                        catch (std::bad_alloc &)
                        {
                            hrc = E_OUTOFMEMORY;
                            break;
                        }
                    }
                }
                else
                    break;
            }
            /*
             * @@VBOX_COND_END@@: Pop one item of the conditional stack.
             */
            else if ( strncmp(pszPlaceholder, s_szPrefixCondEnd, sizeof(s_szPrefixCondEnd) - 1U) == 0 )
            {
                if (cConds > 0)
                {
                    cConds--;
                    fOutputting = aConds[cConds].fSavedOutputting;
                }
                else
                {
                    hrc = mpSetError->setErrorBoth(E_FAIL, VERR_PARSE_ERROR,
                                                   mpSetError->tr("%s without @@VBOX_COND_XXX@@ at offset %zu (%#zx)"),
                                                   s_szPrefixCondEnd, offPlaceholder, offPlaceholder);
                    break;
                }
            }
            /*
             * @@VBOX_COND_XXX@@: Push the previous outputting state and combine it with the
             *                    one from the condition.
             */
            else if ( strncmp(pszPlaceholder, s_szPrefixCond, sizeof(s_szPrefixCond) - 1U) == 0 )
            {
                if (cConds + 1 < RT_ELEMENTS(aConds))
                {
                    aConds[cConds].fSavedOutputting = fOutputting;
                    bool fNewOutputting = fOutputting;
                    hrc = getConditional(pszPlaceholder, cchPlaceholder, &fNewOutputting);
                    if (SUCCEEDED(hrc))
                        fOutputting = fOutputting && fNewOutputting;
                    else
                        break;
                    cConds++;
                }
                else
                {
                    hrc = mpSetError->setErrorBoth(E_FAIL, VERR_PARSE_ERROR,
                                                   mpSetError->tr("Too deep conditional nesting at offset %zu (%#zx)"),
                                                   offPlaceholder, offPlaceholder);
                    break;
                }
            }
            /*
             * @@VBOX_GUEST_OS_COND_END@@: Pop one item of the conditional stack.
             */
            else if ( strncmp(pszPlaceholder, s_szPrefixCondGuestOsEnd, sizeof(s_szPrefixCondGuestOsEnd) - 1U) == 0 )
            {
                if (cConds > 0)
                {
                    cConds--;
                    fOutputting = aConds[cConds].fSavedOutputting;
                }
                else
                {
                    hrc = mpSetError->setErrorBoth(E_FAIL, VERR_PARSE_ERROR,
                                                   mpSetError->tr("%s without @@VBOX_GUEST_OS_COND_XXX@@ at offset %zu (%#zx)"),
                                                   s_szPrefixCondGuestOsEnd, offPlaceholder, offPlaceholder);
                    break;
                }
            }
            /*
             * @@VBOX_GUEST_OS_COND_XXX@@: Push the previous outputting state and combine it with the
             *                             one from the condition.
             */
            else
            {
                Assert(strncmp(pszPlaceholder, s_szPrefixCondGuestOs, sizeof(s_szPrefixCondGuestOs) - 1) == 0);
                if (cConds + 1 < RT_ELEMENTS(aConds))
                {
                    aConds[cConds].fSavedOutputting = fOutputting;
                    bool fNewOutputting = fOutputting;

                    //offTemplate is the beginning of content, offEndContent is the end of content
                    //@@PLACEHOLDER_BEGIN@@Content@@PLACEHOLDER_END@@
                    //                 ^       ^
                    //                 |       |
                    //         offTemplate  offEndContent
                    size_t offEndContent = mStrScriptFullContent.find(s_szPrefix, offTemplate);
                    size_t cchContent = offEndContent - offTemplate - 1;
                    hrc = getGuestOSConditional(pszPlaceholder, cchPlaceholder, cchContent, &cchInternalCorrect, &fNewOutputting);
                    if (SUCCEEDED(hrc))
                        fOutputting = fOutputting && fNewOutputting;
                    else
                        break;
                    cConds++;
                }
                else
                {
                    hrc = mpSetError->setErrorBoth(E_FAIL, VERR_PARSE_ERROR,
                                                   mpSetError->tr("Too deep conditional nesting at offset %zu (%#zx)"),
                                                   offPlaceholder, offPlaceholder);
                    break;
                }
            }
        }

        /*
         * Done?
         */
        if (offTemplate >= cchTemplate)
        {
            if (cConds == 0)
                return S_OK;
            if (cConds == 1)
                hrc = mpSetError->setErrorBoth(E_FAIL, VERR_PARSE_ERROR, mpSetError->tr("Missing @@VBOX_COND_END@@"));
            else
                hrc = mpSetError->setErrorBoth(E_FAIL, VERR_PARSE_ERROR, mpSetError->tr("Missing %u @@VBOX_COND_END@@"), cConds);
            break;
        }
    }

    /* failed */
    rStrDst.setNull();
    return hrc;
}

HRESULT UnattendedScriptTemplate::getReplacement(const char *pachPlaceholder, size_t cchPlaceholder,
                                                 bool fOutputting, RTCString &rValue)
{
    /*
     * Check for an escaping suffix.  Drop the '@@'.
     */
    size_t const cchFullPlaceholder = cchPlaceholder;
    enum
    {
        kValueEscaping_None,
        kValueEscaping_Bourne,
        kValueEscaping_XML_Element,
        kValueEscaping_XML_Attribute_Double_Quotes
    } enmEscaping;

#define PLACEHOLDER_ENDS_WITH(a_szSuffix) \
        (   cchPlaceholder > sizeof(a_szSuffix) - 1U \
         && memcmp(&pachPlaceholder[cchPlaceholder - sizeof(a_szSuffix) + 1U], a_szSuffix, sizeof(a_szSuffix) - 1U) == 0)
    if (PLACEHOLDER_ENDS_WITH("_SH@@"))
    {
        cchPlaceholder -= 3 + 2;
        enmEscaping = kValueEscaping_Bourne;
    }
    else if (PLACEHOLDER_ENDS_WITH("_ELEMENT@@"))
    {
        cchPlaceholder -= 8 + 2;
        enmEscaping = kValueEscaping_XML_Element;
    }
    else if (PLACEHOLDER_ENDS_WITH("_ATTRIB_DQ@@"))
    {
        cchPlaceholder -= 10 + 2;
        enmEscaping = kValueEscaping_XML_Attribute_Double_Quotes;
    }
    else
    {
        Assert(PLACEHOLDER_ENDS_WITH("@@"));
        cchPlaceholder -= 2;
        enmEscaping = kValueEscaping_None;
    }

    /*
     * Resolve and escape the value.
     */
    HRESULT hrc;
    try
    {
        switch (enmEscaping)
        {
            case kValueEscaping_None:
                hrc = getUnescapedReplacement(pachPlaceholder, cchPlaceholder, cchFullPlaceholder, fOutputting, rValue);
                if (SUCCEEDED(hrc))
                    return hrc;
                break;

            case kValueEscaping_Bourne:
            case kValueEscaping_XML_Element:
            case kValueEscaping_XML_Attribute_Double_Quotes:
            {
                RTCString strUnescaped;
                hrc = getUnescapedReplacement(pachPlaceholder, cchPlaceholder, cchFullPlaceholder, fOutputting, strUnescaped);
                if (SUCCEEDED(hrc))
                {
                    switch (enmEscaping)
                    {
                        case kValueEscaping_Bourne:
                        {
                            const char * const papszArgs[2] = { strUnescaped.c_str(), NULL };
                            char              *pszEscaped   = NULL;
                            int vrc = RTGetOptArgvToString(&pszEscaped, papszArgs, RTGETOPTARGV_CNV_QUOTE_BOURNE_SH);
                            if (RT_SUCCESS(vrc))
                            {
                                try
                                {
                                    rValue = pszEscaped;
                                    RTStrFree(pszEscaped);
                                    return S_OK;
                                }
                                catch (std::bad_alloc &)
                                {
                                    hrc = E_OUTOFMEMORY;
                                }
                                RTStrFree(pszEscaped);
                            }
                            break;
                        }

                        case kValueEscaping_XML_Element:
                            rValue.printf("%RMes", strUnescaped.c_str());
                            return S_OK;

                        case kValueEscaping_XML_Attribute_Double_Quotes:
                        {
                            RTCString strTmp;
                            strTmp.printf("%RMas", strUnescaped.c_str());
                            rValue = RTCString(strTmp, 1, strTmp.length() - 2);
                            return S_OK;
                        }

                        default:
                            hrc = E_FAIL;
                            break;
                    }
                }
                break;
            }

            default:
                AssertFailedStmt(hrc = E_FAIL);
                break;
        }
    }
    catch (std::bad_alloc &)
    {
        hrc = E_OUTOFMEMORY;
    }
    rValue.setNull();
    return hrc;
}

HRESULT UnattendedScriptTemplate::getUnescapedReplacement(const char *pachPlaceholder, size_t cchPlaceholder,
                                                          size_t cchFullPlaceholder, bool fOutputting, RTCString &rValue)
{
    RT_NOREF(fOutputting);
#define IS_PLACEHOLDER_MATCH(a_szMatch) \
        (   cchPlaceholder == sizeof("@@VBOX_INSERT_" a_szMatch) - 1U \
         && memcmp(pachPlaceholder, "@@VBOX_INSERT_" a_szMatch, sizeof("@@VBOX_INSERT_" a_szMatch) - 1U) == 0)

    if (IS_PLACEHOLDER_MATCH("USER_LOGIN"))
        rValue = mpUnattended->i_getUser();
    else if (IS_PLACEHOLDER_MATCH("USER_PASSWORD"))
        rValue = mpUnattended->i_getPassword();
    else if (IS_PLACEHOLDER_MATCH("ROOT_PASSWORD"))
        rValue = mpUnattended->i_getPassword();
    else if (IS_PLACEHOLDER_MATCH("USER_FULL_NAME"))
        rValue = mpUnattended->i_getFullUserName();
    else if (IS_PLACEHOLDER_MATCH("PRODUCT_KEY"))
        rValue = mpUnattended->i_getProductKey();
    else if (IS_PLACEHOLDER_MATCH("POST_INSTALL_COMMAND"))
        rValue = mpUnattended->i_getPostInstallCommand();
    else if (IS_PLACEHOLDER_MATCH("IMAGE_INDEX"))
        rValue.printf("%u", mpUnattended->i_getImageIndex());
    else if (IS_PLACEHOLDER_MATCH("OS_ARCH"))
        rValue = mpUnattended->i_isGuestOs64Bit() ? "amd64" : "x86";
    else if (IS_PLACEHOLDER_MATCH("OS_ARCH2"))
        rValue = mpUnattended->i_isGuestOs64Bit() ? "x86_64" : "x86";
    else if (IS_PLACEHOLDER_MATCH("OS_ARCH3"))
        rValue = mpUnattended->i_isGuestOs64Bit() ? "x86_64" : "i386";
    else if (IS_PLACEHOLDER_MATCH("OS_ARCH4"))
        rValue = mpUnattended->i_isGuestOs64Bit() ? "x86_64" : "i486";
    else if (IS_PLACEHOLDER_MATCH("OS_ARCH6"))
        rValue = mpUnattended->i_isGuestOs64Bit() ? "x86_64" : "i686";
    else if (IS_PLACEHOLDER_MATCH("GUEST_OS_VERSION"))
        rValue = mpUnattended->i_getDetectedOSVersion();
    else if (IS_PLACEHOLDER_MATCH("GUEST_OS_MAJOR_VERSION"))
    {
        Utf8Str strOsVer(mpUnattended->i_getDetectedOSVersion());
        RTCList<RTCString> partList = strOsVer.split(".");
        if (partList.size() < 1)
        {
            rValue.setNull();
            return mpSetError->setErrorBoth(E_FAIL, VERR_NOT_FOUND, mpSetError->tr("Unknown guest OS major version '%s'"),
                                            partList.at(0).c_str());
        }
        rValue = partList.at(0);
    }
    else if (IS_PLACEHOLDER_MATCH("TIME_ZONE_UX"))
        rValue = mpUnattended->i_getTimeZoneInfo()
               ? mpUnattended->i_getTimeZoneInfo()->pszUnixName : mpUnattended->i_getTimeZone();
    else if (IS_PLACEHOLDER_MATCH("TIME_ZONE_WIN_NAME"))
    {
        PCRTTIMEZONEINFO pInfo = mpUnattended->i_getTimeZoneInfo();
        if (pInfo)
            rValue = pInfo->pszWindowsName ? pInfo->pszWindowsName : "GMT";
        else
            rValue = mpUnattended->i_getTimeZone();
    }
    else if (IS_PLACEHOLDER_MATCH("TIME_ZONE_WIN_INDEX"))
    {
        PCRTTIMEZONEINFO pInfo = mpUnattended->i_getTimeZoneInfo();
        if (pInfo)
            rValue.printf("%u", pInfo->idxWindows ? pInfo->idxWindows : 85 /*GMT*/);
        else
            rValue = mpUnattended->i_getTimeZone();
    }
    else if (IS_PLACEHOLDER_MATCH("LOCALE"))
        rValue = mpUnattended->i_getLocale();
    else if (IS_PLACEHOLDER_MATCH("DASH_LOCALE"))
    {
        rValue = mpUnattended->i_getLocale();
        Assert(rValue[2] == '_');
        rValue.replace(2, 1, "-");
    }
    else if (IS_PLACEHOLDER_MATCH("LANGUAGE"))
        rValue = mpUnattended->i_getLanguage();
    else if (IS_PLACEHOLDER_MATCH("COUNTRY"))
        rValue = mpUnattended->i_getCountry();
    else if (IS_PLACEHOLDER_MATCH("HOSTNAME_FQDN"))
        rValue = mpUnattended->i_getHostname();
    else if (IS_PLACEHOLDER_MATCH("HOSTNAME_WITHOUT_DOMAIN"))
        rValue.assign(mpUnattended->i_getHostname(), 0, mpUnattended->i_getHostname().find("."));
    else if (IS_PLACEHOLDER_MATCH("HOSTNAME_WITHOUT_DOMAIN_MAX_15"))
        rValue.assign(mpUnattended->i_getHostname(), 0, RT_MIN(mpUnattended->i_getHostname().find("."), 15));
    else if (IS_PLACEHOLDER_MATCH("HOSTNAME_DOMAIN"))
        rValue.assign(mpUnattended->i_getHostname(), mpUnattended->i_getHostname().find(".") + 1);
    else if (IS_PLACEHOLDER_MATCH("PROXY"))
        rValue = mpUnattended->i_getProxy();
    else
    {
        rValue.setNull();
        return mpSetError->setErrorBoth(E_FAIL, VERR_NOT_FOUND, mpSetError->tr("Unknown template placeholder '%.*s'"),
                                        cchFullPlaceholder, pachPlaceholder);
    }
    return S_OK;
#undef IS_PLACEHOLDER_MATCH
}

HRESULT UnattendedScriptTemplate::getGuestOSConditional(const char *pachPlaceholder,
                                                        size_t cchPlaceholder,
                                                        size_t cchContent,
                                                        size_t *cchCorrect,
                                                        bool *pfOutputting)
{
#define IS_PLACEHOLDER_MATCH(a_szMatch) \
        (   cchPlaceholder == sizeof("@@VBOX_GUEST_OS_COND_" a_szMatch "@@") - 1U \
         && memcmp(pachPlaceholder, "@@VBOX_GUEST_OS_COND_" a_szMatch "@@", sizeof("@@VBOX_GUEST_OS_COND_" a_szMatch "@@") - 1U) == 0)

    if ( IS_PLACEHOLDER_MATCH("VERSION") )
    {
        Utf8Str strT(pachPlaceholder + cchPlaceholder, cchContent);
        RTCList<RTCString> partList = strT.split("**");
        Utf8Str strRequiredOSVersion;
        if (partList.size() == 2)//when the version is placed together with the placeholder in one line in the file
        {
            //The case when the string has been splitted on the 2 parts:
            //1. OS version
            //2. Actual content
            strRequiredOSVersion.assign(partList.at(0));
            //cchCorrect = "**" + length of OS version string + "**"
            *cchCorrect = 2 + partList.at(0).length() + 2;// must be subtracted from the cchContent
        }
        else if (partList.size() == 3)//when the version is placed on a standalone line in the file
        {
            //The case when the string has been splitted on the 3 parts:
            //1. Empty string or string with only "\n"
            //2. OS version
            //3. Actual content
            strRequiredOSVersion.assign(partList.at(1));
            *cchCorrect = 2 + partList.at(0).length() + partList.at(1).length() + 2;// must be subtracted from the cchContent
        }
        else//case with wrong string syntax
        {
            *cchCorrect = 0;
            *pfOutputting = false;
            LogRel(("Malformed content of the template @@VBOX_GUEST_OS_COND_VERSION@@\n"));
            return S_OK;
        }

        if (strRequiredOSVersion.isEmpty())
            *pfOutputting = false;
        else
        {
            Utf8Str strDetectedOSVersion = mpUnattended->i_getDetectedOSVersion();
            RTCList<RTCString> partListRequired = strRequiredOSVersion.split(".");
            RTCList<RTCString> partListDetected = strDetectedOSVersion.split(".");
            *pfOutputting = false;//initially is set to "false"

            /** @todo r=vvp: Should we check the string with a requested OS version for digits?
             *        (with RTLocCIsDigit()) */
            //Major version must be presented
            if ( partListDetected.at(0).toUInt32() >= partListRequired.at(0).toUInt32() )//comparison major versions
            {
                //OS major versions are equal or detected guest OS major version is greater. Go further.
                if (partListDetected.size() > 1 && partListRequired.size() > 1)//comparison minor versions
                {
                    if (partListDetected.at(1).toUInt32() >= partListRequired.at(1).toUInt32())
                        //OS minor versions are equal or detected guest OS minor version is greater. Go further.
                        *pfOutputting = true;
                    else
                        //The detected guest OS minor version is less than the requested one.
                        *pfOutputting = false;
                }
                else
                    //OS minor versions are absent.
                    *pfOutputting = true;
            }
            else
                //The detected guest OS major version is less than the requested one.
                *pfOutputting = false;
        }
    }
    else
        return mpSetError->setErrorBoth(E_FAIL, VERR_NOT_FOUND, mpSetError->tr("Unknown conditional placeholder '%.*s'"),
                                        cchPlaceholder, pachPlaceholder);
    return S_OK;
#undef IS_PLACEHOLDER_MATCH
}

HRESULT UnattendedScriptTemplate::getConditional(const char *pachPlaceholder, size_t cchPlaceholder, bool *pfOutputting)
{
#define IS_PLACEHOLDER_MATCH(a_szMatch) \
        (   cchPlaceholder == sizeof("@@VBOX_COND_" a_szMatch "@@") - 1U \
         && memcmp(pachPlaceholder, "@@VBOX_COND_" a_szMatch "@@", sizeof("@@VBOX_COND_" a_szMatch "@@") - 1U) == 0)

    /* Install Guest Additions: */
    if (IS_PLACEHOLDER_MATCH("IS_INSTALLING_ADDITIONS"))
        *pfOutputting = mpUnattended->i_getInstallGuestAdditions();
    else if (IS_PLACEHOLDER_MATCH("IS_NOT_INSTALLING_ADDITIONS"))
        *pfOutputting = !mpUnattended->i_getInstallGuestAdditions();
    /* User == Administrator: */
    else if (IS_PLACEHOLDER_MATCH("IS_USER_LOGIN_ADMINISTRATOR"))
        *pfOutputting = mpUnattended->i_getUser().compare("Administrator", RTCString::CaseInsensitive) == 0;
    else if (IS_PLACEHOLDER_MATCH("IS_USER_LOGIN_NOT_ADMINISTRATOR"))
        *pfOutputting = mpUnattended->i_getUser().compare("Administrator", RTCString::CaseInsensitive) != 0;
    /* Install TXS: */
    else if (IS_PLACEHOLDER_MATCH("IS_INSTALLING_TEST_EXEC_SERVICE"))
        *pfOutputting = mpUnattended->i_getInstallTestExecService();
    else if (IS_PLACEHOLDER_MATCH("IS_NOT_INSTALLING_TEST_EXEC_SERVICE"))
        *pfOutputting = !mpUnattended->i_getInstallTestExecService();
    /* Post install command: */
    else if (IS_PLACEHOLDER_MATCH("HAS_POST_INSTALL_COMMAND"))
        *pfOutputting = mpUnattended->i_getPostInstallCommand().isNotEmpty();
    else if (IS_PLACEHOLDER_MATCH("HAS_NO_POST_INSTALL_COMMAND"))
        *pfOutputting = !mpUnattended->i_getPostInstallCommand().isNotEmpty();
    /* Product key: */
    else if (IS_PLACEHOLDER_MATCH("HAS_PRODUCT_KEY"))
        *pfOutputting = mpUnattended->i_getProductKey().isNotEmpty();
    else if (IS_PLACEHOLDER_MATCH("HAS_NO_PRODUCT_KEY"))
        *pfOutputting = !mpUnattended->i_getProductKey().isNotEmpty();
    /* Minimal installation: */
    else if (IS_PLACEHOLDER_MATCH("IS_MINIMAL_INSTALLATION"))
        *pfOutputting = mpUnattended->i_isMinimalInstallation();
    else if (IS_PLACEHOLDER_MATCH("IS_NOT_MINIMAL_INSTALLATION"))
        *pfOutputting = !mpUnattended->i_isMinimalInstallation();
    /* Is RTC using UTC (i.e. set to UTC time on startup): */
    else if (IS_PLACEHOLDER_MATCH("IS_RTC_USING_UTC"))
        *pfOutputting = mpUnattended->i_isRtcUsingUtc();
    else if (IS_PLACEHOLDER_MATCH("IS_NOT_RTC_USING_UTC"))
        *pfOutputting = !mpUnattended->i_isRtcUsingUtc();
    else if (IS_PLACEHOLDER_MATCH("HAS_PROXY"))
        *pfOutputting = mpUnattended->i_getProxy().isNotEmpty();
    else
        return mpSetError->setErrorBoth(E_FAIL, VERR_NOT_FOUND, mpSetError->tr("Unknown conditional placeholder '%.*s'"),
                                        cchPlaceholder, pachPlaceholder);
    return S_OK;
#undef IS_PLACEHOLDER_MATCH
}

#endif /* VBOX_WITH_UNATTENDED */
#if 0 /* Keeping this a reference */


/*********************************************************************************************************************************
*   UnattendedSUSEXMLScript Implementation                                                                                       *
*********************************************************************************************************************************/

HRESULT UnattendedSUSEXMLScript::parse()
{
    HRESULT hrc = UnattendedXMLScript::parse();
    if (SUCCEEDED(hrc))
    {
        /*
         * Check that we've got the right root element type.
         */
        const xml::ElementNode *pelmRoot = mDoc.getRootElement();
        if (   pelmRoot
            && strcmp(pelmRoot->getName(), "profile") == 0)
        {
            /*
             * Work thought the sections.
             */
            try
            {
                LoopThruSections(pelmRoot);
                hrc = S_OK;
            }
            catch (std::bad_alloc &)
            {
                hrc = E_OUTOFMEMORY;
            }
        }
        else if (pelmRoot)
            hrc = mpSetError->setError(E_FAIL, mpSetError->tr("XML document root element is '%s' instead of 'profile'"),
                                       pelmRoot->getName());
        else
            hrc = mpSetError->setError(E_FAIL, mpSetError->tr("Missing XML root element"));
    }
    return hrc;
}

HRESULT UnattendedSUSEXMLScript::setFieldInElement(xml::ElementNode *pElement, const DataId enmDataId, const Utf8Str &rStrValue)
{
    /*
     * Don't set empty values.
     */
    if (rStrValue.isEmpty())
    {
        Utf8Str strProbableValue;
        try
        {
            strProbableValue = createProbableValue(enmDataId, pElement);
        }
        catch (std::bad_alloc &)
        {
            return E_OUTOFMEMORY;
        }
        return UnattendedXMLScript::setFieldInElement(pElement, enmDataId, strProbableValue);
    }
    return UnattendedXMLScript::setFieldInElement(pElement, enmDataId, rStrValue);
}

HRESULT UnattendedSUSEXMLScript::LoopThruSections(const xml::ElementNode *pelmRoot)
{
    xml::NodesLoop loopChildren(*pelmRoot);
    const xml::ElementNode *pelmOuterLoop;
    while ((pelmOuterLoop = loopChildren.forAllNodes()) != NULL)
    {
        const char *pcszElemName = pelmOuterLoop->getName();
        if (!strcmp(pcszElemName, "users"))
        {
            xml::NodesLoop loopUsers(*pelmOuterLoop);
            const xml::ElementNode *pelmUser;
            while ((pelmUser = loopUsers.forAllNodes()) != NULL)
            {
                HRESULT hrc = HandleUserAccountsSection(pelmUser);
                if (FAILED(hrc))
                    return hrc;
            }
        }
    }
    return S_OK;
}

HRESULT UnattendedSUSEXMLScript::HandleUserAccountsSection(const xml::ElementNode *pelmSection)
{
    xml::NodesLoop loopUser(*pelmSection);

    const xml::ElementNode *pelmCur;
    while ((pelmCur = loopUser.forAllNodes()) != NULL)
    {
        const char *pszValue = pelmCur->getValue();
#ifdef LOG_ENABLED
        if (!RTStrCmp(pelmCur->getName(), "uid"))
            LogRelFunc(("UnattendedSUSEXMLScript::HandleUserAccountsSection profile/users/%s/%s = %s\n",
                        pelmSection->getName(), pelmCur->getName(), pszValue));
#endif

        if (!RTStrCmp(pszValue, "$homedir"))
            mNodesForCorrectionMap.insert(make_pair(USERHOMEDIR_ID, pelmCur));

        if (!RTStrCmp(pszValue, "$user"))
            mNodesForCorrectionMap.insert(make_pair(USERNAME_ID, pelmCur));

        if (!RTStrCmp(pszValue, "$password"))
            mNodesForCorrectionMap.insert(make_pair(USERPASSWORD_ID, pelmCur));
    }
    return S_OK;
}

Utf8Str UnattendedSUSEXMLScript::createProbableValue(const DataId enmDataId, const xml::ElementNode *pCurElem)
{
    const xml::ElementNode *pElem = pCurElem;

    switch (enmDataId)
    {
        case USERHOMEDIR_ID:
//          if ((pElem = pElem->findChildElement("home")))
//          {
                return createProbableUserHomeDir(pElem);
//          }
            break;
        default:
            break;
    }

    return Utf8Str::Empty;
}

Utf8Str UnattendedSUSEXMLScript::createProbableUserHomeDir(const xml::ElementNode *pCurElem)
{
    Utf8Str strCalcValue;
    const xml::ElementNode *pElem = pCurElem->findNextSibilingElement("username");
    if (pElem)
    {
        const char *pszValue = pElem->getValue();
        strCalcValue = "/home/";
        strCalcValue.append(pszValue);
    }

    return strCalcValue;
}
#endif /* just for reference */

