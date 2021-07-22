/* $Id$ */
/** @file
 * VBox Qt GUI - UINotificationObjectItem class declaration.
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

#ifndef FEQT_INCLUDED_SRC_notificationcenter_UINotificationObjectItem_h
#define FEQT_INCLUDED_SRC_notificationcenter_UINotificationObjectItem_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* Forward declarations: */
class QHBoxLayout;
class QLabel;
class QVBoxLayout;
class QIRichTextLabel;
class QIToolButton;
class UINotificationObject;

/** QWidget-based notification-object item. */
class UINotificationObjectItem : public QWidget
{
    Q_OBJECT;

public:

    /** Constructs notification-object item, passing @a pParent to the base-class.
      * @param  pObject  Brings the notification-object this item created for. */
    UINotificationObjectItem(QWidget *pParent, UINotificationObject *pObject = 0);

protected:

    /** Handles any Qt @a pEvent. */
    virtual bool event(QEvent *pEvent) /* override */;

    /** Handles paint @a pEvent. */
    virtual void paintEvent(QPaintEvent *pEvent) /* override */;

    /** Holds the notification-object this item created for. */
    UINotificationObject *m_pObject;

    /** Holds the main layout instance. */
    QVBoxLayout     *m_pLayoutMain;
    /** Holds the upper layout instance. */
    QHBoxLayout     *m_pLayoutUpper;
    /** Holds the name label instance. */
    QLabel          *m_pLabelName;
    /** Holds the close button instance. */
    QIToolButton    *m_pButtonClose;
    /** Holds the details label instance. */
    QIRichTextLabel *m_pLabelDetails;

    /** Holds whether item is hovered. */
    bool  m_fHovered;
    /** Holds whether item is toggled. */
    bool  m_fToggled;
};

/** Notification-object factory. */
namespace UINotificationItem
{
    /** Creates notification-object of required type.
      * @param  pParent  Brings the parent constructed item being attached to.
      * @param  pObject  Brings the notification-object item being constructed for. */
    UINotificationObjectItem *create(QWidget *pParent, UINotificationObject *pObject);
}

#endif /* !FEQT_INCLUDED_SRC_notificationcenter_UINotificationObjectItem_h */
