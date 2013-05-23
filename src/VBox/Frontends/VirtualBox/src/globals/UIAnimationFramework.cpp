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

/* static */
UIAnimation* UIAnimation::installPropertyAnimation(QWidget *pTarget, const char *pszPropertyName,
                                                   const char *pszValuePropertyNameStart, const char *pszValuePropertyNameFinal,
                                                   const char *pszSignalForward, const char *pszSignalReverse,
                                                   bool fReverse /*= false*/, int iAnimationDuration /*= 300*/)
{
    /* Return newly created animation-machine: */
    return new UIAnimation(pTarget, pszPropertyName,
                           pszValuePropertyNameStart, pszValuePropertyNameFinal,
                           pszSignalForward, pszSignalReverse,
                           fReverse, iAnimationDuration);
}

UIAnimation::UIAnimation(QWidget *pParent, const char *pszPropertyName,
                         const char *pszValuePropertyNameStart, const char *pszValuePropertyNameFinal,
                         const char *pszSignalForward, const char *pszSignalReverse,
                         bool fReverse, int iAnimationDuration)
    : QObject(pParent)
    , m_pszPropertyName(pszPropertyName)
    , m_pszValuePropertyNameStart(pszValuePropertyNameStart), m_pszValuePropertyNameFinal(pszValuePropertyNameFinal)
    , m_pszSignalForward(pszSignalForward), m_pszSignalReverse(pszSignalReverse)
    , m_fReverse(fReverse), m_iAnimationDuration(iAnimationDuration)
    , m_pAnimationMachine(0), m_pForwardAnimation(0), m_pReverseAnimation(0)
{
    /* Prepare: */
    prepare();
}

void UIAnimation::prepare()
{
    /* Prepare animation-machine: */
    m_pAnimationMachine = new QStateMachine(this);
    /* Create 'start' and 'final' states: */
    QState *pStateStart = new QState(m_pAnimationMachine);
    QState *pStateFinal = new QState(m_pAnimationMachine);

    /* Prepare 'forward' animation: */
    m_pForwardAnimation = new QPropertyAnimation(parent(), m_pszPropertyName, m_pAnimationMachine);
    m_pForwardAnimation->setEasingCurve(QEasingCurve(QEasingCurve::InOutCubic));
    m_pForwardAnimation->setDuration(m_iAnimationDuration);
    /* Prepare 'reverse' animation: */
    m_pReverseAnimation = new QPropertyAnimation(parent(), m_pszPropertyName, m_pAnimationMachine);
    m_pReverseAnimation->setEasingCurve(QEasingCurve(QEasingCurve::InOutCubic));
    m_pReverseAnimation->setDuration(m_iAnimationDuration);

    /* Prepare state-transitions: */
    QSignalTransition *pStartToFinal = pStateStart->addTransition(parent(), m_pszSignalForward, pStateFinal);
    pStartToFinal->addAnimation(m_pForwardAnimation);
    QSignalTransition *pFinalToStart = pStateFinal->addTransition(parent(), m_pszSignalReverse, pStateStart);
    pFinalToStart->addAnimation(m_pReverseAnimation);

    /* Fetch animation-borders: */
    update();

    /* Choose initial state: */
    m_pAnimationMachine->setInitialState(!m_fReverse ? pStateStart : pStateFinal);
    /* Start animation-machine: */
    m_pAnimationMachine->start();
}

void UIAnimation::update()
{
    /* Update 'forward' animation: */
    m_pForwardAnimation->setStartValue(parent()->property(m_pszValuePropertyNameStart));
    m_pForwardAnimation->setEndValue(parent()->property(m_pszValuePropertyNameFinal));
    /* Update 'reverse' animation: */
    m_pReverseAnimation->setStartValue(parent()->property(m_pszValuePropertyNameFinal));
    m_pReverseAnimation->setEndValue(parent()->property(m_pszValuePropertyNameStart));
}

