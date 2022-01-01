/* $Id$ */
/** @file
 * VBox Qt GUI - UIExtension namespace declaration.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_globals_UIExtension_h
#define FEQT_INCLUDED_SRC_globals_UIExtension_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UILibraryDefs.h"

/** Namespace with common extension pack stuff. */
namespace UIExtension
{
    /** Initiates the extension pack installation process.
      * @param  strFilePath      Brings the extension pack file path.
      * @param  strDigest        Brings the extension pack file digest.
      * @param  pParent          Brings the parent dialog reference.
      * @param  pstrExtPackName  Brings the extension pack name. */
    void SHARED_LIBRARY_STUFF install(QString const &strFilePath,
                                      QString const &strDigest,
                                      QWidget *pParent,
                                      QString *pstrExtPackName);
}

#endif /* !FEQT_INCLUDED_SRC_globals_UIExtension_h */
