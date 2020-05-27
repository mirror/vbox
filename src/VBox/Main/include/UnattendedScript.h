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

#ifndef MAIN_INCLUDED_UnattendedScript_h
#define MAIN_INCLUDED_UnattendedScript_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "TextScript.h"

using namespace xml;

class Unattended;


/**
 * Generic unattended text script template editor.
 *
 * This just perform variable replacements, no other editing possible.
 *
 * Everything happens during saveToString, parse is a noop.
 */
class UnattendedScriptTemplate : public BaseTextScript
{
protected:
    /** Where to get the replacement strings from. */
    Unattended *mpUnattended;

public:
    UnattendedScriptTemplate(Unattended *pUnattended, const char *pszDefaultTemplateFilename, const char *pszDefaultFilename);
    virtual ~UnattendedScriptTemplate()             {}

    HRESULT parse()                                 { return S_OK; }
    HRESULT saveToString(Utf8Str &rStrDst);

protected:
    /**
     * Gets the replacement value for the given placeholder.
     *
     * @returns COM status code.
     * @param   pachPlaceholder The placholder string.  Not zero terminated.
     * @param   cchPlaceholder  The length of the placeholder.
     * @param   fOutputting     Indicates whether we actually need the correct value
     *                          or is just syntax checking excluded template parts.
     * @param   rValue          Where to return the value.
     */
    HRESULT getReplacement(const char *pachPlaceholder, size_t cchPlaceholder, bool fOutputting, RTCString &rValue);

    /**
     * Overridable worker for getReplacement.
     *
     * @returns COM status code.
     * @param   pachPlaceholder     The placholder string.  Not zero terminated.
     * @param   cchPlaceholder      The length of the placeholder.
     * @param   cchFullPlaceholder  The full placeholder length, including suffixes
     *                              indicating how it should be escaped (for error
     *                              messages).
     * @param   fOutputting         Indicates whether we actually need the correct
     *                              value or is just syntax checking excluded
     *                              template parts.  Intended for voiding triggering
     *                              sanity checks regarding which replacements
     *                              should be used and not (e.g. no Guest Additions
     *                              path when installing GAs aren't enabled).
     * @param   rValue              Where to return the value.
     * @throws  std::bad_alloc
     */
    virtual HRESULT getUnescapedReplacement(const char *pachPlaceholder, size_t cchPlaceholder,
                                            size_t cchFullPlaceholder, bool fOutputting, RTCString &rValue);


    /**
     * Get the result of a conditional.
     *
     * @returns COM status code.
     * @param   pachPlaceholder     The placholder string.  Not zero terminated.
     * @param   cchPlaceholder      The length of the placeholder.
     * @param   pfOutputting        Where to return the result of the conditional.
     *                              This holds the current outputting state on input
     *                              in case someone want to sanity check anything.
     */
    virtual HRESULT getConditional(const char *pachPlaceholder, size_t cchPlaceholder, bool *pfOutputting);
};

#if 0 /* convert when we fix SUSE */
/**
 * SUSE unattended XML file editor.
 */
class UnattendedSUSEXMLScript : public UnattendedXMLScript
{
public:
    UnattendedSUSEXMLScript(VirtualBoxBase *pSetError, const char *pszDefaultFilename = "autoinst.xml")
        : UnattendedXMLScript(pSetError, pszDefaultFilename) {}
    ~UnattendedSUSEXMLScript() {}

    HRESULT parse();

protected:
    HRESULT setFieldInElement(xml::ElementNode *pElement, const DataId enmDataId, const Utf8Str &rStrValue);

private:
    //////////////////New functions//////////////////////////////
    /** @throws std::bad_alloc */
    HRESULT LoopThruSections(const xml::ElementNode *pelmRoot);
    /** @throws std::bad_alloc */
    HRESULT HandleUserAccountsSection(const xml::ElementNode *pelmSection);
    /** @throws std::bad_alloc */
    Utf8Str createProbableValue(const DataId enmDataId, const xml::ElementNode *pCurElem);
    /** @throws std::bad_alloc */
    Utf8Str createProbableUserHomeDir(const xml::ElementNode *pCurElem);
    //////////////////New functions//////////////////////////////
};
#endif


#endif /* !MAIN_INCLUDED_UnattendedScript_h */

