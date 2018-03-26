/* $Id$ */
/** @file
 * VBox Qt GUI - UIPopupPaneMessage class declaration.
 */

/*
 * Copyright (C) 2013-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIPopupPaneMessage_h___
#define ___UIPopupPaneMessage_h___

/* Qt includes: */
#include <QWidget>

/* Forward declarations: */
class QLabel;
class UIAnimation;

/** QWidget extension providing GUI with popup-pane message-pane prototype class. */
class UIPopupPaneMessage : public QWidget
{
    Q_OBJECT;
    Q_PROPERTY(QSize collapsedSizeHint READ collapsedSizeHint);
    Q_PROPERTY(QSize expandedSizeHint READ expandedSizeHint);
    Q_PROPERTY(QSize minimumSizeHint READ minimumSizeHint WRITE setMinimumSizeHint);

signals:

    /** Notifies about focus enter. */
    void sigFocusEnter();
    /** Notifies about focus enter. */
    void sigFocusLeave();

    /** Notifies about size-hint change. */
    void sigSizeHintChanged();

public:

    /** Constructs message-pane passing @a pParent to the base-class.
      * @param  strText   Brings the message text.
      * @param  fFcoused  Brings whether the pane is focused. */
    UIPopupPaneMessage(QWidget *pParent, const QString &strText, bool fFocused);

    /** Defines the message @a strText. */
    void setText(const QString &strText);

    /** Returns the message minimum size-hint. */
    QSize minimumSizeHint() const;
    /** Defines the message @a minimumSizeHint. */
    void setMinimumSizeHint(const QSize &minimumSizeHint);
    /** Lays the content out. */
    void layoutContent();

    /** Returns the collapsed size-hint. */
    QSize collapsedSizeHint() const { return m_collapsedSizeHint; }
    /** Returns the expanded size-hint. */
    QSize expandedSizeHint() const { return m_expandedSizeHint; }

private slots:

    /** Handles proposal for @a iWidth. */
    void sltHandleProposalForWidth(int iWidth);

    /** Handles focus enter. */
    void sltFocusEnter();
    /** Handles focus leave. */
    void sltFocusLeave();

private:

    /** Prepares all. */
    void prepare();
    /** Prepares content. */
    void prepareContent();
    /** Prepares animations. */
    void prepareAnimation();

    /** Updates size-hint. */
    void updateSizeHint();

    /** Adjusts @a font. */
    static QFont tuneFont(QFont font);

    /** Holds the layout margin. */
    const int m_iLayoutMargin;
    /** Holds the layout spacing. */
    const int m_iLayoutSpacing;

    /** Holds the label size-hint. */
    QSize m_labelSizeHint;
    /** Holds the collapsed size-hint. */
    QSize m_collapsedSizeHint;
    /** Holds the expanded size-hint. */
    QSize m_expandedSizeHint;
    /** Holds the minimum size-hint. */
    QSize m_minimumSizeHint;

    /** Holds the text. */
    QString m_strText;

    /** Holds the label instance. */
    QLabel *m_pLabel;

    /** Holds the desired label width. */
    int m_iDesiredLabelWidth;

    /** Holds whether message-pane is focused. */
    bool m_fFocused;

    /** Holds the animation instance. */
    UIAnimation *m_pAnimation;
};

#endif /* !___UIPopupPaneMessage_h___ */

