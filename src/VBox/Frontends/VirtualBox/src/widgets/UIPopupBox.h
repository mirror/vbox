/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIPopupBox class declaration
 */

/*
 * Copyright (C) 2010-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIPopupBox_h__
#define __UIPopupBox_h__

/* Global includes */
#include <QIcon>
#include <QWidget>

/* Global forward declarations */
class QLabel;

class UIPopupBox : public QWidget
{
    Q_OBJECT;

public:

    UIPopupBox(QWidget *pParent);
    ~UIPopupBox();

    void setTitle(const QString& strTitle);
    QString title() const;

    void setTitleIcon(const QIcon& icon);
    QIcon titleIcon() const;

    void setWarningIcon(const QIcon& icon);
    QIcon warningIcon() const;

    void setTitleLink(const QString& strLink);
    QString titleLink() const;

    void setTitleLinkEnabled(bool fEnabled);
    bool isTitleLinkEnabled() const;

    void setContentWidget(QWidget *pWidget);
    QWidget* contentWidget() const;

    void setOpen(bool fOpen);
    void toggleOpen();
    bool isOpen() const;

    void callForUpdateContentWidget() { emit sigUpdateContentWidget(); }

signals:

    void titleClicked(const QString &);
    void toggled(bool fOpened);
    void sigUpdateContentWidget();

protected:

    bool eventFilter(QObject *pWatched, QEvent *pEvent);

    void resizeEvent(QResizeEvent *pEvent);
    void mouseDoubleClickEvent(QMouseEvent *pEvent);
    void paintEvent(QPaintEvent *pEvent);

private:

    void updateHover(bool fForce = false);
    void recalc();

    /* Private member vars */
    QLabel *m_pTitleLabel;
    QString m_strTitle;
    QLabel *m_pTitleIcon;
    QLabel *m_pWarningIcon;
    QIcon m_titleIcon;
    QIcon m_warningIcon;
    QString m_strLink;
    bool m_fLinkEnabled;
    QWidget *m_pContentWidget;
    bool m_fOpen;

    QPainterPath *m_pLabelPath;
    const int m_aw;
    QPainterPath m_arrowPath;
    bool m_fHeaderHover;
};

#endif /* !__UIPopupBox_h__ */

