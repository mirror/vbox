/* $Id$ */
/** @file
 * VirtualBox Translator.
 */

/*
 * Copyright (C) 2005-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_VirtualBoxTranslator_h
#define MAIN_INCLUDED_VirtualBoxTranslator_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <list>

#include <iprt/cpp/lock.h>
#include <VBox/com/AutoLock.h>


class QMTranslator;

class VirtualBoxTranslator
    : public util::RWLockHandle
{
public:

    virtual ~VirtualBoxTranslator();

    static VirtualBoxTranslator *instance();
    void release();

    /* Load language based on settings in the VirtualBox config */
    HRESULT loadLanguage(ComPtr<IVirtualBox> aVirtualBox);

    /**
     * Translates @a aSourceText to user language.
     *
     * @returns Translated string or @a aSourceText.  The returned string is
     *          valid only during lifetime of the this translator instance.
     */
    static const char *translate(const char *aContext,
                                 const char *aSourceText,
                                 const char *aComment = NULL,
                                 const int   aNum = -1);

    /**
     * Returns source text stored in the cache if exists.
     * Otherwise, the pszTranslation itself returned.
     */
    static const char *trSource(const char *aTranslation);

    /* Convenience function used by VirtualBox::init */
    int i_loadLanguage(const char *pszLang);
    int i_setLanguageFile(const char *aFileName);
    com::Utf8Str i_languageFile();


    static int32_t initCritSect();

private:
    static RTCRITSECTRW s_instanceRwLock;
    static VirtualBoxTranslator *s_pInstance;

    uint32_t m_cInstanceRefs;

    com::Utf8Str  m_strLangFileName;
    QMTranslator *m_pTranslator;
    /** String cache that all translation strings are stored in.
     * This is a add-only cache, which allows translate() to return C-strings w/o
     * needing to think about racing loadLanguage() wrt string lifetime. */
    RTSTRCACHE    m_hStrCache;
    /** RTStrCacheCreate status code. */
    int           m_rcCache;

    VirtualBoxTranslator();

    const char *i_translate(const char *aContext,
                            const char *aSourceText,
                            const char *aComment = NULL,
                            const int   aNum = -1);

    /* Returns instance if exists, returns NULL otherwise. */
    static VirtualBoxTranslator *i_instance();
};

#endif /* !MAIN_INCLUDED_VirtualBoxTranslator_h */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
