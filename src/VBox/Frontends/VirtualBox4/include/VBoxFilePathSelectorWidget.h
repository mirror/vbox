/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: VBoxFilePathSelectorWidget class declaration
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef __VBoxFilePathSelectorWidget_h__
#define __VBoxFilePathSelectorWidget_h__

#include "QIWithRetranslateUI.h"

/* Qt includes */
#include <QWidget>

#ifdef Q_WS_MAC
#define VBOX_USE_COMBOBOX_PATH_SELECTOR
#endif /* Q_WS_MAC */

class QLabel;
class QLineEdit;
class QToolButton;
class QFileIconProvider;
class QComboBox;

class VBoxFilePathSelectorWidget: public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    enum SelectorMode 
    { 
        PathMode = 0,
        FileMode
    };

    VBoxFilePathSelectorWidget (QWidget *aParent = NULL);
    ~VBoxFilePathSelectorWidget();

    void setMode (SelectorMode aMode);
    SelectorMode mode() const;

    void setLineEditWhatsThis (const QString &aText);
    void setSelectorWhatsThis (const QString &aText);
    void setResetWhatsThis (const QString &aText);

    bool isModified() const;

public slots:

    void setPath (const QString &aPath);
    QString path() const;

signals:

    void selectPath();
    void resetPath();

protected:

    void retranslateUi();

private slots:
     
    void cbActivated (int aIndex);

private:

    void init();
    QIcon defaultIcon() const;
    QString filePath (const QString &aName, bool bLast) const;

    /* Private member vars */
    SelectorMode mMode;
    QString      mPath;
    QString      mNoneStr;

#ifdef VBOX_USE_COMBOBOX_PATH_SELECTOR
    QComboBox   *mCbPath;
#else /* VBOX_USE_COMBOBOX_PATH_SELECTOR */
    QLabel      *mLbIcon;
    QLineEdit   *mLePath;
    QToolButton *mTbSelect;
    QToolButton *mTbReset;
#endif /* !VBOX_USE_COMBOBOX_PATH_SELECTOR */

    QFileIconProvider *mIconProvider;
};

#endif /* __VBoxFilePathSelectorWidget_h__ */

