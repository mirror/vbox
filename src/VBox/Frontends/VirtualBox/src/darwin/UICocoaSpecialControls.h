/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxCocoaSpecialControls class declaration
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___darwin_UICocoaSpecialControls_h__
#define ___darwin_UICocoaSpecialControls_h__

/* VBox includes */
#include "VBoxCocoaHelper.h"

/* Qt includes */
#include <QWidget>

/* Qt forward includes */
class QMacCocoaViewContainer;

/* Add typedefs for Cocoa types */
ADD_COCOA_NATIVE_REF(NSButton);
ADD_COCOA_NATIVE_REF(NSSegmentedControl);
ADD_COCOA_NATIVE_REF(NSSearchField);

class UICocoaWrapper: public QWidget
{
public:
    UICocoaWrapper(QWidget *pParent = 0);

protected:
    virtual void resizeEvent(QResizeEvent *pEvent);

    QMacCocoaViewContainer *m_pContainer;
};

class UICocoaButton: public UICocoaWrapper
{
    Q_OBJECT

public:
    enum CocoaButtonType
    {
        HelpButton,
        CancelButton,
        ResetButton
    };

    UICocoaButton(CocoaButtonType aType, QWidget *pParent = 0);
    ~UICocoaButton();

    QSize sizeHint() const;

    void setText(const QString& strText);
    void setToolTip(const QString& strTip);

    void onClicked();

signals:
    void clicked(bool fChecked = false);

private:
    /* Private member vars */
    NativeNSButtonRef m_pNativeRef;
};

class UICocoaSegmentedButton: public UICocoaWrapper
{
    Q_OBJECT

public:
    enum CocoaSegmentType
    {
        RoundRectSegment,
        TexturedRoundedSegment
    };

    UICocoaSegmentedButton(int count, CocoaSegmentType type = RoundRectSegment, QWidget *pParent = 0);
    ~UICocoaSegmentedButton();

    QSize sizeHint() const;

    void setTitle(int iSegment, const QString &strTitle);
    void setToolTip(int iSegment, const QString &strTip);
    void setIcon(int iSegment, const QIcon& icon);
    void setEnabled(int iSegment, bool fEnabled);

    void animateClick(int iSegment);
    void onClicked(int iSegment);

signals:
    void clicked(int iSegment, bool fChecked = false);

private:
    /* Private member vars */
    NativeNSSegmentedControlRef m_pNativeRef;
};

class UICocoaSearchField: public UICocoaWrapper
{
    Q_OBJECT

public:
    UICocoaSearchField(QWidget* pParent = 0);
    ~UICocoaSearchField();

    QSize sizeHint() const;

    QString text() const;
    void insert(const QString &strText);
    void setToolTip(const QString &strTip);
    void selectAll();

    void markError();
    void unmarkError();

    void onTextChanged(const QString &strText);

signals:
    void textChanged(const QString& strText);

private:
    /* Private member vars */
    NativeNSSearchFieldRef m_pNativeRef;
};

#endif /* ___darwin_UICocoaSpecialControls_h__ */

