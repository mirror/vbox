/* $Id$ */
/** @file
 * VBox Qt GUI - UINotificationCenter class declaration.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_notificationcenter_UINotificationCenter_h
#define FEQT_INCLUDED_SRC_notificationcenter_UINotificationCenter_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QUuid>
#include <QWidget>

/* GUI includes: */
#include "UILibraryDefs.h"
#include "UINotificationObjects.h"

/* Forward declarations: */
class QHBoxLayout;
class QPainter;
class QStateMachine;
class QVBoxLayout;
class QIToolButton;
class UINotificationModel;
class UINotificationObject;

/** QWidget-based notification-center overlay. */
class SHARED_LIBRARY_STUFF UINotificationCenter : public QWidget
{
    Q_OBJECT;
    Q_PROPERTY(int animatedValue READ animatedValue WRITE setAnimatedValue);

signals:

    /** Requests sliding state-machine to open overlay. */
    void sigOpen();
    /** Requests sliding state-machine to close overlay. */
    void sigClose();

public:

    /** Creates notification-center for passed @a pParent. */
    static void create(QWidget *pParent);
    /** Destroys notification-center. */
    static void destroy();
    /** Returns notification-center singleton instance. */
    static UINotificationCenter *instance();

    /** Appends a notification @a pObject to intenal model. */
    QUuid append(UINotificationObject *pObject);
    /** Revokes a notification object referenced by @a uId from intenal model. */
    void revoke(const QUuid &uId);

protected:

    /** Constructs notification-center passing @a pParent to the base-class. */
    UINotificationCenter(QWidget *pParent);
    /** Destructs notification-center. */
    virtual ~UINotificationCenter() /* override final */;

    /** Preprocesses any Qt @a pEvent for passed @a pObject. */
    virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override final */;

    /** Handles any Qt @a pEvent. */
    virtual bool event(QEvent *pEvent) /* override final */;

    /** Handles paint @a pEvent. */
    virtual void paintEvent(QPaintEvent *pEvent) /* override final */;

private slots:

    /** Issues request to make open button @a fToggled. */
    void sltHandleOpenButtonToggled(bool fToggled);

    /** Handles signal about model being updated. */
    void sltModelChanged();

private:

    /** Prepares everything. */
    void prepare();
    /** Prepares model. */
    void prepareModel();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Prepares sliding state-machine. */
    void prepareStateMachineSliding();

    /** Paints background using pre-configured @a pPainter. */
    void paintBackground(QPainter *pPainter);
    /** Paints frame using pre-configured @a pPainter. */
    void paintFrame(QPainter *pPainter);

    /** Defines animated @a iValue. */
    void setAnimatedValue(int iValue);
    /** Returns animated value. */
    int animatedValue() const;

    /** Adjusts geometry. */
    void adjustGeometry();
    /** Adjusts mask. */
    void adjustMask();

    /** Holds the notification-center singleton instance. */
    static UINotificationCenter *s_pInstance;

    /** Holds the model instance. */
    UINotificationModel *m_pModel;

    /** Holds the main layout instance. */
    QVBoxLayout  *m_pLayoutMain;
    /** Holds the open button layout instance. */
    QHBoxLayout  *m_pLayoutOpenButton;
    /** Holds the open button instance. */
    QIToolButton *m_pOpenButton;
    /** Holds the items layout instance. */
    QVBoxLayout  *m_pLayoutItems;

    /** Holds the sliding state-machine instance. */
    QStateMachine *m_pStateMachineSliding;
    /** Holds the sliding animation current value. */
    int            m_iAnimatedValue;
};

/** Singleton notification-center 'official' name. */
inline UINotificationCenter &notificationCenter() { return *UINotificationCenter::instance(); }

#endif /* !FEQT_INCLUDED_SRC_notificationcenter_UINotificationCenter_h */
