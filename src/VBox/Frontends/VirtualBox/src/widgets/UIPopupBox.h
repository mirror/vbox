/* $Id$ */
/** @file
 * VBox Qt GUI - UIPopupBox/UIPopupBoxGroup classes declaration.
 */

/*
 * Copyright (C) 2010-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIPopupBoxStuff_h___
#define ___UIPopupBoxStuff_h___

/* Qt includes: */
#include <QIcon>
#include <QWidget>

/* GUI includes: */
#include "UILibraryDefs.h"

/* Forward declarations: */
class QEvent;
class QIcon;
class QLabel;
class QMouseEvent;
class QObject;
class QPainterPath;
class QPaintEvent;
class QResizeEvent;
class QString;
class QWidget;


/** QWidget extension,
  * wrapping content-widget with nice collapsable frame. */
class SHARED_LIBRARY_STUFF UIPopupBox : public QWidget
{
    Q_OBJECT;

signals:

    /** Notifies about title with @a strLink was clicked. */
    void sigTitleClicked(const QString &strLink);

    /** Notifies box was toggled and currently @a fOpened. */
    void sigToggled(bool fOpened);

    /** Asks to update contents widget. */
    void sigUpdateContentWidget();

    /** Notify about box is hovered. */
    void sigGotHover();

public:

    /** Construct popup-box passing @a pParent to the base-class. */
    UIPopupBox(QWidget *pParent);
    /** Destruct popup-box. */
    virtual ~UIPopupBox() /* override */;

    /** Defines title @a icon. */
    void setTitleIcon(const QIcon &icon);
    /** Returns title icon. */
    QIcon titleIcon() const;

    /** Defines warning @a icon. */
    void setWarningIcon(const QIcon &icon);
    /** Returns warnings icon. */
    QIcon warningIcon() const;

    /** Defines @a strTitle. */
    void setTitle(const QString &strTitle);
    /** Returns title. */
    QString title() const;

    /** Defines title @a strLink. */
    void setTitleLink(const QString &strLink);
    /** Returns title link. */
    QString titleLink() const;
    /** Defines whether title link is @a fEnabled. */
    void setTitleLinkEnabled(bool fEnabled);
    /** Returns whether title link is enabled. */
    bool isTitleLinkEnabled() const;

    /** Defines content @a pWidget. */
    void setContentWidget(QWidget *pWidget);
    /** Returns content widget. */
    QWidget *contentWidget() const;

    /** Defines whether box is @a fOpened. */
    void setOpen(bool fOpened);
    /** Toggles current opened state. */
    void toggleOpen();
    /** Returns whether box is opened. */
    bool isOpen() const;

    /** Calls for content iwdget update. */
    void callForUpdateContentWidget() { emit sigUpdateContentWidget(); }

protected:

    /** Handles any Qt @a pEvent. */
    virtual bool event(QEvent *pEvent) /* override */;

    /** Pre-handles standard Qt @a pEvent for passed @a pObject. */
    virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;

    /** Handles resize @a pEvent. */
    virtual void resizeEvent(QResizeEvent *pEvent) /* override */;

    /** Handles paint @a pEvent. */
    virtual void paintEvent(QPaintEvent *pEvent) /* override */;

    /** Handles mouse double-click @a pEvent. */
    virtual void mouseDoubleClickEvent(QMouseEvent *pEvent) /* override */;

private:

    /** Updates title icon. */
    void updateTitleIcon();
    /** Updates warning icon. */
    void updateWarningIcon();
    /** Updates title. */
    void updateTitle();
    /** Updates hovered state. */
    void updateHover();
    /** Revokes hovered state. */
    void revokeHover();
    /** Toggles hovered state to @a fHeaderHover. */
    void toggleHover(bool fHeaderHover);
    /** Recalculates geometry. */
    void recalc();

    /** Holds the title icon label. */
    QLabel *m_pTitleIcon;
    /** Holds the warning icon label. */
    QLabel *m_pWarningIcon;
    /** Holds the title label. */
    QLabel *m_pTitleLabel;

    /** Holds the title icon. */
    QIcon    m_titleIcon;
    /** Holds the warning icon. */
    QIcon    m_warningIcon;
    /** Holds the title text. */
    QString  m_strTitle;
    /** Holds the link icon. */
    QString  m_strLink;

    /** Holds whether the link is enabled. */
    bool m_fLinkEnabled : 1;
    /** Holds whether box is opened. */
    bool m_fOpened      : 1;
    /** Holds whether header is hovered. */
    bool m_fHovered     : 1;

    /** Holds the content widget. */
    QWidget *m_pContentWidget;

    /** Holds the label painter path. */
    QPainterPath *m_pLabelPath;

    /** Holds the arrow width. */
    const int m_iArrowWidth;
    /** Holds the arrow painter-path. */
    QPainterPath m_arrowPath;

    /** Allow popup-box group to access private API. */
    friend class UIPopupBoxGroup;
};


/** QObject extension,
  * provides a container to organize groups of popup-boxes. */
class SHARED_LIBRARY_STUFF UIPopupBoxGroup : public QObject
{
    Q_OBJECT;

public:

    /** Construct popup-box passing @a pParent to the base-class. */
    UIPopupBoxGroup(QObject *pParent);
    /** Destruct popup-box. */
    virtual ~UIPopupBoxGroup() /* override */;

    /** Adds @a pPopupBox into group. */
    void addPopupBox(UIPopupBox *pPopupBox);

private slots:

    /** Handles group hovering. */
    void sltHoverChanged();

private:

    /** Holds the list of popup-boxes. */
    QList<UIPopupBox*> m_list;
};


#endif /* !___UIPopupBoxStuff_h___ */

