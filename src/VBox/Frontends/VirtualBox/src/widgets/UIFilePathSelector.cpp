/* $Id$ */
/** @file
 * VBox Qt GUI - VirtualBox Qt extensions: UIFilePathSelector class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* GUI includes: */
# include "QIFileDialog.h"
# include "QIToolButton.h"
# include "QILabel.h"
# include "QILineEdit.h"
# include "UIIconPool.h"
# include "UIFilePathSelector.h"
# include "VBoxGlobal.h"

/* Other VBox includes: */
# include <iprt/assert.h>

/* Qt includes: */
# include <QAction>
# include <QApplication>
# include <QClipboard>
# include <QDir>
# include <QFocusEvent>
# include <QHBoxLayout>
# include <QLineEdit>
# include <QTimer>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

enum
{
    PathId = 0,
    SelectId,
    ResetId
};


/** Returns first position of difference between passed strings. */
static int differFrom (const QString &aS1, const QString &aS2)
{
    if (aS1 == aS2)
        return -1;

    int minLength = qMin (aS1.size(), aS2.size());
    int index = 0;
    for (index = 0; index < minLength ; ++ index)
        if (aS1 [index] != aS2 [index])
            break;
    return index;
}

UIFilePathSelector::UIFilePathSelector (QWidget *aParent)
    : QIWithRetranslateUI<QComboBox> (aParent)
    , mCopyAction (new QAction (this))
    , mMode (Mode_Folder)
    , mHomeDir (QDir::current().absolutePath())
    , mIsEditable (true)
    , mIsEditableMode (false)
    , mIsMouseAwaited (false)
    , mModified (false)
{
    /* Populate items */
    insertItem (PathId, "");
    insertItem (SelectId, "");
    insertItem (ResetId, "");

    /* Attaching known icons */
    setItemIcon(SelectId, UIIconPool::iconSet(":/select_file_16px.png"));
    setItemIcon(ResetId, UIIconPool::iconSet(":/eraser_16px.png"));

    /* Setup context menu */
    addAction (mCopyAction);
    mCopyAction->setShortcut (QKeySequence (QKeySequence::Copy));
    mCopyAction->setShortcutContext (Qt::WidgetShortcut);

    /* Initial Setup */
    setInsertPolicy (QComboBox::NoInsert);
    setContextMenuPolicy (Qt::ActionsContextMenu);
    setMinimumWidth (200);

    /* Setup connections */
    connect (this, SIGNAL (activated (int)), this, SLOT (onActivated (int)));
    connect (mCopyAction, SIGNAL (triggered (bool)), this, SLOT (copyToClipboard()));

    /* Editable by default */
    setEditable (true);

    /* Applying language settings */
    retranslateUi();
}

UIFilePathSelector::~UIFilePathSelector()
{
}

void UIFilePathSelector::setMode (Mode aMode)
{
    mMode = aMode;
}

UIFilePathSelector::Mode UIFilePathSelector::mode() const
{
    return mMode;
}

void UIFilePathSelector::setEditable (bool aOn)
{
    mIsEditable = aOn;

    if (mIsEditable)
    {
        QComboBox::setEditable (true);
        Assert (lineEdit());
        connect (lineEdit(), SIGNAL (textEdited (const QString &)),
                 this, SLOT (onTextEdited (const QString &)));

        /* Installing necessary event filters */
        lineEdit()->installEventFilter (this);
    }
    else
    {
        if (lineEdit())
        {
            /* Installing necessary event filters */
            lineEdit()->installEventFilter (this);
            disconnect (lineEdit(), SIGNAL (textEdited (const QString &)),
                        this, SLOT (onTextEdited (const QString &)));
        }
        QComboBox::setEditable (false);
    }
}

bool UIFilePathSelector::isEditable() const
{
    return mIsEditable;
}

void UIFilePathSelector::setResetEnabled (bool aEnabled)
{
    if (!aEnabled && count() - 1 == ResetId)
        removeItem (ResetId);
    else if (aEnabled && count() - 1 == ResetId - 1)
    {
        insertItem (ResetId, "");
        setItemIcon(ResetId, UIIconPool::iconSet(":/eraser_16px.png"));
    }
    retranslateUi();
}

bool UIFilePathSelector::isResetEnabled() const
{
    return (count() - 1  == ResetId);
}

void UIFilePathSelector::resetModified()
{
    mModified = false;
}

bool UIFilePathSelector::isModified() const
{
    return mModified;
}

void UIFilePathSelector::setFileDialogTitle (const QString& aTitle)
{
    mFileDialogTitle = aTitle;
}

QString UIFilePathSelector::fileDialogTitle() const
{
    return mFileDialogTitle;
}

void UIFilePathSelector::setFileFilters (const QString& aFilters)
{
    mFileFilters = aFilters;
}

QString UIFilePathSelector::fileFilters() const
{
    return mFileFilters;
}

void UIFilePathSelector::setDefaultSaveExt (const QString &aExt)
{
    mDefaultSaveExt = aExt;
}

QString UIFilePathSelector::defaultSaveExt() const
{
    return mDefaultSaveExt;
}

bool UIFilePathSelector::isPathSelected() const
{
    return (currentIndex() == PathId);
}

QString UIFilePathSelector::path() const
{
    return mPath;
}

void UIFilePathSelector::setPath (const QString &aPath, bool aRefreshText /* = true */)
{
    mPath = aPath.isEmpty() ? QString::null :
            QDir::toNativeSeparators (aPath);
    if (aRefreshText)
        refreshText();
}

void UIFilePathSelector::setHomeDir (const QString &aHomeDir)
{
    mHomeDir = aHomeDir;
}

void UIFilePathSelector::resizeEvent (QResizeEvent *aEvent)
{
    QIWithRetranslateUI<QComboBox>::resizeEvent (aEvent);
    refreshText();
}

void UIFilePathSelector::focusInEvent (QFocusEvent *aEvent)
{
    if (isPathSelected())
    {
        if (mIsEditable)
            mIsEditableMode = true;
        if (aEvent->reason() == Qt::MouseFocusReason)
            mIsMouseAwaited = true;
        else
            refreshText();
    }
    QIWithRetranslateUI<QComboBox>::focusInEvent (aEvent);
}

void UIFilePathSelector::focusOutEvent (QFocusEvent *aEvent)
{
    if (isPathSelected())
    {
        mIsEditableMode = false;
        refreshText();
    }
    QIWithRetranslateUI<QComboBox>::focusOutEvent (aEvent);
}

bool UIFilePathSelector::eventFilter (QObject *aObj, QEvent *aEv)
{
    if (mIsMouseAwaited && (aEv->type() == QEvent::MouseButtonPress))
        QTimer::singleShot (0, this, SLOT (refreshText()));

    return QIWithRetranslateUI<QComboBox>::eventFilter (aObj, aEv);
}

void UIFilePathSelector::retranslateUi()
{
    /* How do we interpret the "nothing selected" item? */
    if (isResetEnabled())
    {
        mNoneStr = tr ("<reset to default>");
        mNoneTip = tr ("The actual default path value will be displayed after "
                       "accepting the changes and opening this window again.");
    }
    else
    {
        mNoneStr = tr ("<not selected>");
        mNoneTip = tr ("Please use the <b>Other...</b> item from the drop-down "
                       "list to select a path.");
    }

    /* Retranslate 'path' item */
    if (mPath.isNull())
    {
        setItemText (PathId, mNoneStr);
        setItemData (PathId, mNoneTip, Qt::ToolTipRole);
        setToolTip (mNoneTip);
    }

    /* Retranslate 'select' item */
    setItemText (SelectId, tr ("Other..."));

    /* Retranslate 'reset' item */
    if (count() - 1 == ResetId)
        setItemText (ResetId, tr ("Reset"));

    /* Set tooltips of the above two items based on the mode */
    switch (mMode)
    {
        case Mode_Folder:
            setItemData (SelectId,
                         tr ("Displays a window to select a different folder."),
                         Qt::ToolTipRole);
            setItemData (ResetId,
                         tr ("Resets the folder path to the default value."),
                         Qt::ToolTipRole);
            break;
        case Mode_File_Open:
        case Mode_File_Save:
            setItemData (SelectId,
                         tr ("Displays a window to select a different file."),
                         Qt::ToolTipRole);
            setItemData (ResetId,
                         tr ("Resets the file path to the default value."),
                         Qt::ToolTipRole);
            break;
        default:
            AssertFailedBreak();
    }

    /* Retranslate copy action */
    mCopyAction->setText (tr ("&Copy"));
}

void UIFilePathSelector::onActivated (int aIndex)
{
    switch (aIndex)
    {
        case SelectId:
        {
            selectPath();
            break;
        }
        case ResetId:
        {
            changePath (QString::null);
            break;
        }
        default:
            break;
    }
    setCurrentIndex (PathId);
    setFocus();
}

void UIFilePathSelector::onTextEdited (const QString &aPath)
{
    changePath (aPath, false /* refresh text? */);
}

void UIFilePathSelector::copyToClipboard()
{
    QString text (fullPath());
    /* Copy the current text to the selection and global clipboard. */
    if (QApplication::clipboard()->supportsSelection())
        QApplication::clipboard()->setText (text, QClipboard::Selection);
    QApplication::clipboard()->setText (text, QClipboard::Clipboard);
}

void UIFilePathSelector::changePath (const QString &aPath,
                                             bool aRefreshText /* = true */)
{
    QString oldPath = mPath;
    setPath (aPath, aRefreshText);
    if (!mModified && mPath != oldPath)
        mModified = true;
    emit pathChanged (aPath);
}

void UIFilePathSelector::selectPath()
{
    /* Preparing initial directory. */
    QString initDir = mPath.isNull() ? mHomeDir :
        QIFileDialog::getFirstExistingDir (mPath);
    if (initDir.isNull())
        initDir = mHomeDir;

    QString selPath;
    switch (mMode)
    {
        case Mode_File_Open:
            selPath = QIFileDialog::getOpenFileName (initDir, mFileFilters, parentWidget(), mFileDialogTitle); break;
        case Mode_File_Save:
            {
                selPath = QIFileDialog::getSaveFileName (initDir, mFileFilters, parentWidget(), mFileDialogTitle);
                if (!selPath.isEmpty() && QFileInfo (selPath).suffix().isEmpty())
                    selPath = QString ("%1.%2").arg (selPath).arg (mDefaultSaveExt);
                break;
            }
        case Mode_Folder:
            selPath = QIFileDialog::getExistingDirectory (initDir, parentWidget(), mFileDialogTitle); break;
    }

    if (selPath.isNull())
        return;

    selPath.remove (QRegExp ("[\\\\/]$"));
    changePath (selPath);
}

QIcon UIFilePathSelector::defaultIcon() const
{
    if (mMode == Mode_Folder)
        return vboxGlobal().icon(QFileIconProvider::Folder);
    else
        return vboxGlobal().icon(QFileIconProvider::File);
}

QString UIFilePathSelector::fullPath (bool aAbsolute /* = true */) const
{
    if (mPath.isNull())
        return mPath;

    QString result;
    switch (mMode)
    {
        case Mode_Folder:
            result = aAbsolute ? QDir (mPath).absolutePath() :
                                 QDir (mPath).path();
            break;
        case Mode_File_Open:
        case Mode_File_Save:
            result = aAbsolute ? QFileInfo (mPath).absoluteFilePath() :
                                 QFileInfo (mPath).filePath();
            break;
        default:
            AssertFailedBreak();
    }
    return QDir::toNativeSeparators (result);
}

QString UIFilePathSelector::shrinkText (int aWidth) const
{
    QString fullText (fullPath (false));
    if (fullText.isEmpty())
        return fullText;

    int oldSize = fontMetrics().width (fullText);
    int indentSize = fontMetrics().width ("x...x");

    /* Compress text */
    int start = 0;
    int finish = 0;
    int position = 0;
    int textWidth = 0;
    do {
        textWidth = fontMetrics().width (fullText);
        if (textWidth + indentSize > aWidth)
        {
            start = 0;
            finish = fullText.length();

            /* Selecting remove position */
            QRegExp regExp ("([\\\\/][^\\\\^/]+[\\\\/]?$)");
            int newFinish = regExp.indexIn (fullText);
            if (newFinish != -1)
                finish = newFinish;
            position = (finish - start) / 2;

            if (position == finish)
               break;

            fullText.remove (position, 1);
        }
    } while (textWidth + indentSize > aWidth);

    fullText.insert (position, "...");
    int newSize = fontMetrics().width (fullText);

    return newSize < oldSize ? fullText : fullPath (false);
}

void UIFilePathSelector::refreshText()
{
    if (mIsEditable && mIsEditableMode)
    {
        /* Cursor positioning variables */
        int curPos = -1;
        int diffPos = -1;
        int fromRight = -1;

        if (mIsMouseAwaited)
        {
            /* Store the cursor position */
            curPos = lineEdit()->cursorPosition();
            diffPos = differFrom (lineEdit()->text(), mPath);
            fromRight = lineEdit()->text().size() - curPos;
        }

        /* In editable mode there should be no any icon
         * and text have be corresponding real stored path
         * which can be absolute or relative. */
        if (lineEdit()->text() != mPath)
            setItemText (PathId, mPath);
        setItemIcon (PathId, QIcon());
        setToolTip (mMode == Mode_Folder ?
            tr ("Holds the folder path.") :
            tr ("Holds the file path."));

        if (mIsMouseAwaited)
        {
            mIsMouseAwaited = false;

            /* Restore the position to the right of dots */
            if (diffPos != -1 && curPos >= diffPos + 3)
                lineEdit()->setCursorPosition (lineEdit()->text().size() -
                                               fromRight);
            /* Restore the position to the center of text */
            else if (diffPos != -1 && curPos > diffPos)
                lineEdit()->setCursorPosition (lineEdit()->text().size() / 2);
            /* Restore the position to the left of dots */
            else
                lineEdit()->setCursorPosition (curPos);
        }
    }
    else if (mPath.isNull())
    {
        /* If we are not in editable mode and no path is
         * stored here - show the translated 'none' string. */
        if (itemText (PathId) != mNoneStr)
        {
            setItemText (PathId, mNoneStr);
            setItemIcon (PathId, QIcon());
            setItemData (PathId, mNoneTip, Qt::ToolTipRole);
            setToolTip (mNoneTip);
        }
    }
    else
    {
        /* Compress text in combobox */
        QStyleOptionComboBox options;
        options.initFrom (this);
        QRect rect = QApplication::style()->subControlRect (
            QStyle::CC_ComboBox, &options, QStyle::SC_ComboBoxEditField);
        setItemText (PathId, shrinkText (rect.width() - iconSize().width()));

        /* Attach corresponding icon */
        setItemIcon (PathId, QFileInfo (mPath).exists() ?
                             vboxGlobal().icon(QFileInfo (mPath)) :
                             defaultIcon());

        /* Set the tooltip */
        setToolTip (fullPath());
        setItemData (PathId, toolTip(), Qt::ToolTipRole);
    }
}

