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

/* VBox includes */
#include "QIWithRetranslateUI.h"

/* Qt includes */
#include <QComboBox>

/* VBox forward declarations */
class QILabel;

/* Qt forward declarations */
class QFileIconProvider;
class QAction;
class QPushButton;

////////////////////////////////////////////////////////////////////////////////
// VBoxFilePathSelectorWidget

class VBoxFilePathSelectorWidget: public QIWithRetranslateUI<QComboBox>
{
    Q_OBJECT;

public:

    enum Mode
    {
        Mode_Folder = 0,
        Mode_File_Open,
        Mode_File_Save
    };

    VBoxFilePathSelectorWidget (QWidget *aParent);
   ~VBoxFilePathSelectorWidget();

    void setMode (Mode aMode);
    Mode mode() const;

    void setEditable (bool aOn);
    bool isEditable() const;

    void setResetEnabled (bool aEnabled);
    bool isResetEnabled () const;

    void setFileDialogTitle (const QString& aTitle);
    QString fileDialogTitle() const;

    void setFileFilters (const QString& aFilters);
    QString fileFilters() const;

    void setDefaultSaveExt (const QString &aExt);
    QString defaultSaveExt() const;

    void resetModified();
    bool isModified() const;
    bool isPathSelected() const;

    QString path() const;

signals:
    void pathChanged (const QString &);

public slots:

    void setPath (const QString &aPath, bool aRefreshText = true);
    void setHomeDir (const QString &aHomeDir);

protected:

    void resizeEvent (QResizeEvent *aEvent);
    void focusInEvent (QFocusEvent *aEvent);
    void focusOutEvent (QFocusEvent *aEvent);
    bool eventFilter (QObject *aObj, QEvent *aEv);
    void retranslateUi();

private slots:

    void onActivated (int aIndex);
    void onTextEdited (const QString &aPath);
    void copyToClipboard();
    void refreshText();

private:

    void changePath (const QString &aPath, bool aRefreshText = true);
    void selectPath();
    QIcon defaultIcon() const;
    QString fullPath (bool aAbsolute = true) const;
    QString shrinkText (int aWidth) const;

    /* Private member vars */
    QFileIconProvider *mIconProvider;
    QAction *mCopyAction;
    Mode mMode;
    QString mPath;
    QString mHomeDir;
    QString mFileFilters;
    QString mDefaultSaveExt;
    QString mFileDialogTitle;
    QString mNoneStr;
    QString mNoneTip;
    bool mIsEditable;
    bool mIsEditableMode;
    bool mIsMouseAwaited;

    bool mModified;
};

////////////////////////////////////////////////////////////////////////////////
// VBoxEmptyFileSelector

class VBoxEmptyFileSelector: public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:
    VBoxEmptyFileSelector (QWidget *aParent = NULL);

    void setPath (const QString& aPath);
    QString path() const;

    bool isModified () const { return mIsModified; }
    void resetModified () { mIsModified = false; }

    void setFileDialogTitle (const QString& aTitle);
    QString fileDialogTitle() const;

    void setFileFilters (const QString& aFilters);
    QString fileFilters() const;

    void setHomeDir (const QString& aDir);
    QString homeDir() const;

signals:
    void pathChanged (QString);

protected:
    void retranslateUi();

private slots:
    void choose();

private:
    /* Private member vars */
    QILabel *mLabel;
    QPushButton *mSelectButton;
    QString mFileDialogTitle;
    QString mFileFilters;
    QString mHomeDir;
    bool mIsModified;
    QString mPath;
};

#endif /* __VBoxFilePathSelectorWidget_h__ */

