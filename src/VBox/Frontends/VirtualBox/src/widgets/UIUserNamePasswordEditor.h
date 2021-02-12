/* $Id$ */
/** @file
 * VBox Qt GUI - UIUserNamePasswordEditor class declaration.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_widgets_UIUserNamePasswordEditor_h
#define FEQT_INCLUDED_SRC_widgets_UIUserNamePasswordEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QIcon>
#include <QWidget>

/* Local includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QGridLayout;
class QLabel;
class QLineEdit;
class UIPasswordLineEdit;

class UIUserNamePasswordEditor : public QIWithRetranslateUI<QWidget>
{

    Q_OBJECT;

signals:

    /** this is emitted whenever the content of one of the line edits is changed. */
    void sigSomeTextChanged();

public:

    UIUserNamePasswordEditor(QWidget *pParent = 0);

    QString userName() const;
    void setUserName(const QString &strUserName);

    QString password() const;
    void setPassword(const QString &strPassword);

    /** Returns false if username or password fields are empty, or password fields do not match. */
    bool isComplete();
    /** Sets m_fForceUnmark flag. see it for more info. */
    void setForceUnmark(bool fForceUnmark);

    /** When fEnabled true place holder texts for the line edits are shown. */
    void setPlaceholderTextEnabled(bool fEnabled);
    void setLabelsVisible(bool fVisible);

protected:

    void retranslateUi();

private slots:

    void sltHandlePasswordVisibility(bool fPasswordVisible);
    void sltSomeTextChanged();

private:

    void prepare();
    template <class T>
    void addLineEdit(int &iRow, QLabel *&pLabel, T *&pLineEdit, QGridLayout *pLayout);
    /** Changes @p pLineEdit's base color to indicate an error or reverts it to the original color. */
    void markLineEdit(QLineEdit *pLineEdit, bool fError);

    bool isUserNameComplete();
    bool isPasswordComplete();

    QLineEdit          *m_pUserNameLineEdit;
    UIPasswordLineEdit *m_pPasswordLineEdit;
    UIPasswordLineEdit *m_pPasswordRepeatLineEdit;

    QLabel *m_pUserNameLabel;
    QLabel *m_pPasswordLabel;
    QLabel *m_pPasswordRepeatLabel;
    QColor m_orginalLineEditBaseColor;
    /** When true line edits are not marked even if they have to be. */
    bool m_fForceUnmark;
    bool m_fShowPlaceholderText;
    bool m_fLabelsVisible;

    QString m_strPasswordError;
};

#endif /* !FEQT_INCLUDED_SRC_widgets_UIUserNamePasswordEditor_h */
