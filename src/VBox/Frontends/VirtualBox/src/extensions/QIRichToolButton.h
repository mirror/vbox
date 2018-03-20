/* $Id$ */
/** @file
 * VBox Qt GUI - QIRichToolButton class declaration.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___QIRichToolButton_h___
#define ___QIRichToolButton_h___

/* Qt includes: */
#include <QWidget>

/* Forward declarations: */
class QLabel;
class QString;
class QIToolButton;

/** QWidget extension
  * representing tool-button with separate text-label. */
class QIRichToolButton : public QWidget
{
    Q_OBJECT;

signals:

    /** Notifies listeners about button click. */
    void sigClicked();

public:

    /** Constructs rich tool-button passing @a pParent to the base-class. */
    QIRichToolButton(QWidget *pParent = 0);

    /** Defines tool-button @a iconSize. */
    void setIconSize(const QSize &iconSize);
    /** Defines tool-button @a icon. */
    void setIcon(const QIcon &icon);
    /** Animates tool-button click. */
    void animateClick();

    /** Defines text-label @a strText. */
    void setText(const QString &strText);

protected:

    /** Handles paint @a pEvent. */
    virtual void paintEvent(QPaintEvent *pEvent) /* override */;
    /** Handles key-press @a pEvent. */
    virtual void keyPressEvent(QKeyEvent *pEvent) /* override */;
    /** Handles mouse-press @a pEvent. */
    virtual void mousePressEvent(QMouseEvent *pEvent) /* override */;

protected slots:

    /** Handles button-click. */
    virtual void sltButtonClicked() {}

private:

    /** Prepares all. */
    void prepare();

    /** Holds the tool-button instance. */
    QIToolButton *m_pButton;
    /** Holds the text-label instance. */
    QLabel       *m_pLabel;
    /** Holds the text for text-label instance. */
    QString       m_strName;
};

#endif /* !___QIRichToolButton_h___ */

