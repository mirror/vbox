/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserSearchWidget class declaration.
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_manager_chooser_UIChooserSearchWidget_h
#define FEQT_INCLUDED_SRC_manager_chooser_UIChooserSearchWidget_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QHBoxLayout;
class QILineEdit;
class QIToolButton;
class UISearchLineEdit;

/** QWidget extension used as virtual machine search widget in the VM Chooser-pane. */
class UIChooserSearchWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigRedoSearch(const QString &strSearchTerm, int iItemSearchFlags);
    /** Is being signalled as next/prev tool buttons are pressed. @a fIsNext is true
      * for the next and false for the previous case. */
    void sigScrollToMatch(bool fIsNext);
    /** Is used for signalling show/hide event from this to parent. */
    void sigToggleVisibility(bool fIsVisible);

public:

    UIChooserSearchWidget(QWidget *pParent);
    /** Forward @a iMatchCount to UISearchLineEdit. */
    void setMatchCount(int iMatchCount);
    /** Forward @a iScrollToIndex to UISearchLineEdit. */
    void setScroolToIndex(int iScrollToIndex);
    /** Appends the @a strSearchText to the current (if any) search text. */
    void appendToSearchString(const QString &strSearchText);
    /** Repeats the last search again. */
    void redoSearch();

protected:

    virtual void showEvent(QShowEvent *pEvent) /* override */;
    virtual void hideEvent(QHideEvent *pEvent) /* override */;
    virtual void retranslateUi() /* override */;
    virtual bool eventFilter(QObject *pWatched, QEvent *pEvent) /* override */;

public slots:

private slots:

    /** Emits sigRedoSearch thuse causes a re-search. */
    void sltHandleSearchTermChange(const QString &strSearchTerm);
    void sltHandleScroolToButtonClick();
    /** Emits sigToggleVisibility, */
    void sltHandleCloseButtonClick();

private:

    void prepareWidgets();
    void prepareConnections();

    /** @name Member widgets.
      * @{ */
        UISearchLineEdit  *m_pLineEdit;
        QHBoxLayout       *m_pMainLayout;
        QIToolButton      *m_pScrollToNextMatchButton;
        QIToolButton      *m_pScrollToPreviousMatchButton;
        QIToolButton      *m_pCloseButton;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_manager_chooser_UIChooserSearchWidget_h */
