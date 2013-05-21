/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIAnimationFramework class declaration
 */

/*
 * Copyright (C) 2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIAnimationFramework_h__
#define __UIAnimationFramework_h__

/* Forward declaration: */
class QStateMachine;

/* UIAnimationFramework namespace: */
namespace UIAnimationFramework
{
    /* API: Animation stuff: */
    QStateMachine* installPropertyAnimation(QWidget *pTarget, const char *pszPropertyName,
                                            const char *pszValuePropertyNameStart, const char *pszValuePropertyNameFinal,
                                            const char *pSignalForward, const char *pSignalBackward,
                                            bool fReversive = false, int iAnimationDuration = 300);
}

#endif /* __UIAnimationFramework_h__ */
