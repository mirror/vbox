/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIAnimationFramework class implementation
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

/* Qt includes: */
#include <QWidget>
#include <QStateMachine>
#include <QPropertyAnimation>
#include <QSignalTransition>

/* GUI includes: */
#include "UIAnimationFramework.h"

QStateMachine* UIAnimationFramework::installPropertyAnimation(QWidget *pTarget, const char *pszPropertyName,
                                                              const char *pszValuePropertyNameStart, const char *pszValuePropertyNameFinal,
                                                              const char *pSignalForward, const char *pSignalBackward,
                                                              bool fReversive /*= false*/, int iAnimationDuration /*= 300*/)
{
    /* State-machine: */
    QStateMachine *pStateMachine = new QStateMachine(pTarget);
    /* State-machine 'start' state: */
    QState *pStateStart = new QState(pStateMachine);
    /* State-machine 'final' state: */
    QState *pStateFinal = new QState(pStateMachine);

    /* State-machine 'forward' animation: */
    QPropertyAnimation *pForwardAnimation = new QPropertyAnimation(pTarget, pszPropertyName, pStateMachine);
    pForwardAnimation->setEasingCurve(QEasingCurve(QEasingCurve::InOutCubic));
    pForwardAnimation->setDuration(iAnimationDuration);
    pForwardAnimation->setStartValue(pTarget->property(pszValuePropertyNameStart));
    pForwardAnimation->setEndValue(pTarget->property(pszValuePropertyNameFinal));
    /* State-machine 'backward' animation: */
    QPropertyAnimation *pBackwardAnimation = new QPropertyAnimation(pTarget, pszPropertyName, pStateMachine);
    pBackwardAnimation->setEasingCurve(QEasingCurve(QEasingCurve::InOutCubic));
    pBackwardAnimation->setDuration(iAnimationDuration);
    pBackwardAnimation->setStartValue(pTarget->property(pszValuePropertyNameFinal));
    pBackwardAnimation->setEndValue(pTarget->property(pszValuePropertyNameStart));

    /* State-machine state transitions: */
    QSignalTransition *pDefaultToHovered = pStateStart->addTransition(pTarget, pSignalForward, pStateFinal);
    pDefaultToHovered->addAnimation(pForwardAnimation);
    QSignalTransition *pHoveredToDefault = pStateFinal->addTransition(pTarget, pSignalBackward, pStateStart);
    pHoveredToDefault->addAnimation(pBackwardAnimation);

    /* Initial state is 'start': */
    pStateMachine->setInitialState(!fReversive ? pStateStart : pStateFinal);
    /* Start hover-machine: */
    pStateMachine->start();
    /* Return machine: */
    return pStateMachine;
}

