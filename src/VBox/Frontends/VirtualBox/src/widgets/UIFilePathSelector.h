/* $Id$ */
/** @file
 * VBox Qt GUI - VirtualBox Qt extensions: UIFilePathSelector class declaration.
 */

/*
 * Copyright (C) 2008-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIFilePathSelector_h___
#define ___UIFilePathSelector_h___

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Qt includes: */
#include <QComboBox>

/* Forward declarations: */
class QILabel;
class QILineEdit;
class QHBoxLayout;
class QAction;
class QIToolButton;


/** QComboBox extension providing GUI with
  * possibility to choose/reflect file/folder path. */
class UIFilePathSelector: public QIWithRetranslateUI<QComboBox>
{
    Q_OBJECT;

public:

    /** Modes file-path selector operates in. */
    enum Mode
    {
        Mode_Folder = 0,
        Mode_File_Open,
        Mode_File_Save
    };

    /** Constructs file-path selector passing @a aParent to QComboBox base-class. */
    UIFilePathSelector (QWidget *aParent);
    /** Destructs file-path selector. */
   ~UIFilePathSelector();

    /** Defines the @a aMode to operate in. */
    void setMode (Mode aMode);
    /** Returns the mode to operate in. */
    Mode mode() const;

    /** Defines whether the possibility to edit the path is @a aOn. */
    void setEditable (bool aOn);
    /** Returns whether the path is editable. */
    bool isEditable() const;

    /** Defines whether the reseting to defauilt path is @a aEnabled. */
    void setResetEnabled (bool aEnabled);
    /** Returns whether the reseting to defauilt path is enabled. */
    bool isResetEnabled () const;

    /** Defines the file-dialog @a aTitle. */
    void setFileDialogTitle (const QString& aTitle);
    /** Returns the file-dialog title. */
    QString fileDialogTitle() const;

    /** Defines the file-dialog @a aFilters. */
    void setFileFilters (const QString& aFilters);
    /** Returns the file-dialog filters. */
    QString fileFilters() const;

    /** Defines the file-dialog default save @a aExt. */
    void setDefaultSaveExt (const QString &aExt);
    /** Returns the file-dialog default save extension. */
    QString defaultSaveExt() const;

    /** Resets path modified state to false. */
    void resetModified();
    /** Returns whether the path is modified. */
    bool isModified() const;
    /** Returns whether the path is selected. */
    bool isPathSelected() const;

    /** Returns the path. */
    QString path() const;

signals:

    /** Notify listeners about path changed. */
    void pathChanged (const QString &);

public slots:

    /** Defines the @a aPath and @a aRefreshText after that. */
    void setPath (const QString &aPath, bool aRefreshText = true);

    /** Defines the @a aHomeDir. */
    void setHomeDir (const QString &aHomeDir);

protected:

    /** Handles resize @a aEvent. */
    void resizeEvent (QResizeEvent *aEvent);

    /** Handles focus-in @a aEvent. */
    void focusInEvent (QFocusEvent *aEvent);
    /** Handles focus-out @a aEvent. */
    void focusOutEvent (QFocusEvent *aEvent);

    /** Preprocesses every @a aEv sent to @a aObj. */
    bool eventFilter (QObject *aObj, QEvent *aEv);

    /** Handles translation event. */
    void retranslateUi();

private slots:

    /** Handles combo-box activation. */
    void onActivated (int aIndex);

    /** Handles combo-box text editing. */
    void onTextEdited (const QString &aPath);

    /** Handles combo-box text copying. */
    void copyToClipboard();

    /** Refreshes combo-box text according to chosen path. */
    void refreshText();

private:

    /** Provokes change to @a aPath and @a aRefreshText after that. */
    void changePath (const QString &aPath, bool aRefreshText = true);

    /** Call for file-dialog to choose path. */
    void selectPath();

    /** Returns default icon. */
    QIcon defaultIcon() const;

    /** Returns full path @a aAbsolute if necessary. */
    QString fullPath (bool aAbsolute = true) const;

    /** Shrinks the reflected text to @a aWidth pixels. */
    QString shrinkText (int aWidth) const;

    /** Holds the copy action instance. */
    QAction *mCopyAction;

    /** Holds the mode to operate in. */
    Mode mMode;

    /** Holds the path. */
    QString mPath;
    /** Holds the home directory. */
    QString mHomeDir;

    /** Holds the file-dialog filters. */
    QString mFileFilters;
    /** Holds the file-dialog default save extension. */
    QString mDefaultSaveExt;
    /** Holds the file-dialog title. */
    QString mFileDialogTitle;

    /** Holds the cached text for empty path. */
    QString mNoneStr;
    /** Holds the cached tool-tip for empty path. */
    QString mNoneTip;

    /** Holds whether the path is editable. */
    bool mIsEditable;

    /** Holds whether we are in editable mode. */
    bool mIsEditableMode;
    /** Holds whether we are expecting mouse events. */
    bool mIsMouseAwaited;

    /** Holds whether the path is modified. */
    bool mModified;
};

#endif /* !___UIFilePathSelector_h___ */

