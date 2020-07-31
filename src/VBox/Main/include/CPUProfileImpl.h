/* $Id$ */
/** @file
 * VirtualBox Main - interface for CPU profiles, VBoxSVC.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_CPUProfileImpl_h
#define MAIN_INCLUDED_CPUProfileImpl_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "CPUProfileWrap.h"

struct CPUMDBENTRY;

/**
 * A CPU profile.
 */
class ATL_NO_VTABLE CPUProfile
    : public CPUProfileWrap
{
public:
    /** @name COM and internal init/term/mapping cruft
     * @{ */
    DECLARE_EMPTY_CTOR_DTOR(CPUProfile)
    HRESULT FinalConstruct();
    void    FinalRelease();
    HRESULT initFromDbEntry(struct CPUMDBENTRY const *a_pDbEntry) RT_NOEXCEPT;
    void    uninit();
    /** @} */

    bool    i_match(CPUArchitecture_T a_enmArchitecture, CPUArchitecture_T a_enmSecondaryArch,
                    const com::Utf8Str &a_strNamePattern) const RT_NOEXCEPT;

private:
    /** @name Wrapped ICPUProfile attributes
     * @{ */
    HRESULT getName(com::Utf8Str &aName) RT_OVERRIDE;
    HRESULT getFullName(com::Utf8Str &aFullName) RT_OVERRIDE;
    HRESULT getArchitecture(CPUArchitecture_T *aArchitecture) RT_OVERRIDE;
    /** @} */

    /** @name Data
     * @{ */
    com::Utf8Str        m_strName;
    com::Utf8Str        m_strFullName;
    CPUArchitecture_T   m_enmArchitecture;
    /** @} */
};

#endif /* !MAIN_INCLUDED_CPUProfileImpl_h */

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
