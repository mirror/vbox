/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: VBoxFilePathSelectorWidget class implementation
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

#include "VBoxFilePathSelectorWidget.h"
#include "VBoxGlobal.h"
#include "QILabel.h"

/* Qt includes */
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFileIconProvider>
#include <QLineEdit>
#include <QTimer>
#include <QPushButton>

////////////////////////////////////////////////////////////////////////////////
// VBoxFilePathSelectorWidget

enum
{
    PathId = 0,
    SelectId,
    ResetId
};

/**
 * Returns first position of difference between passed strings.
 */
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

VBoxFilePathSelectorWidget::VBoxFilePathSelectorWidget (QWidget *aParent)
    : QIWithRetranslateUI<QComboBox> (aParent)
    , mIconProvider (new QFileIconProvider())
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
    setItemIcon (SelectId, VBoxGlobal::iconSet (":/select_file_16px.png"));
    setItemIcon (ResetId, VBoxGlobal::iconSet (":/eraser_16px.png"));

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

VBoxFilePathSelectorWidget::~VBoxFilePathSelectorWidget()
{
    delete mIconProvider;
}

void VBoxFilePathSelectorWidget::setMode (Mode aMode)
{
    mMode = aMode;
}

VBoxFilePathSelectorWidget::Mode VBoxFilePathSelectorWidget::mode() const
{
    return mMode;
}

void VBoxFilePathSelectorWidget::setEditable (bool aOn)
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
    }else
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

bool VBoxFilePathSelectorWidget::isEditable() const
{
    return mIsEditable;
}

void VBoxFilePathSelectorWidget::setResetEnabled (bool aEnabled)
{
    if (!aEnabled && count() - 1 == ResetId)
        removeItem (ResetId);
    else if (aEnabled && count() - 1 == ResetId - 1)
    {
        insertItem (ResetId, "");
        setItemIcon (ResetId, VBoxGlobal::iconSet (":/eraser_16px.png"));
    }
    retranslateUi();
}

bool VBoxFilePathSelectorWidget::isResetEnabled() const
{
    return (count() - 1  == ResetId);
}

void VBoxFilePathSelectorWidget::resetModified()
{
    mModified = false;
}

bool VBoxFilePathSelectorWidget::isModified() const
{
    return mModified;
}

void VBoxFilePathSelectorWidget::setFileDialogTitle (const QString& aTitle)
{
    mFileDialogTitle = aTitle;
}

QString VBoxFilePathSelectorWidget::fileDialogTitle() const
{
    return mFileDialogTitle;
}

void VBoxFilePathSelectorWidget::setFileFilters (const QString& aFilters)
{
    mFileFilters = aFilters;
}

QString VBoxFilePathSelectorWidget::fileFilters() const
{
    return mFileFilters;
}

void VBoxFilePathSelectorWidget::setDefaultSaveExt (const QString &aExt)
{
    mDefaultSaveExt = aExt;
}

QString VBoxFilePathSelectorWidget::defaultSaveExt() const
{
    return mDefaultSaveExt;
}

/**
 * Returns @c true if the selected (active) combobox item is a path item.
 *
 * May be used in @c activated() signal handlers to distinguish between
 * non-path items like "Other..." or "Reset" that get temporarily activated
 * when performing the corresponding actiojn and the item that contains a
 * real selected file/folder path.
 */
bool VBoxFilePathSelectorWidget::isPathSelected() const
{
    return (currentIndex() == PathId);
}

QString VBoxFilePathSelectorWidget::path() const
{
    return mPath;
}

void VBoxFilePathSelectorWidget::setPath (const QString &aPath, bool aRefreshText /* = true */)
{
    mPath = aPath.isEmpty() ? QString::null :
            QDir::toNativeSeparators (aPath);
    if (aRefreshText)
        refreshText();
}

void VBoxFilePathSelectorWidget::setHomeDir (const QString &aHomeDir)
{
    mHomeDir = aHomeDir;
}

void VBoxFilePathSelectorWidget::resizeEvent (QResizeEvent *aEvent)
{
    QIWithRetranslateUI<QComboBox>::resizeEvent (aEvent);
    refreshText();
}

void VBoxFilePathSelectorWidget::focusInEvent (QFocusEvent *aEvent)
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

void VBoxFilePathSelectorWidget::focusOutEvent (QFocusEvent *aEvent)
{
    if (isPathSelected())
    {
        mIsEditableMode = false;
        refreshText();
    }
    QIWithRetranslateUI<QComboBox>::focusOutEvent (aEvent);
}

bool VBoxFilePathSelectorWidget::eventFilter (QObject *aObj, QEvent *aEv)
{
    if (mIsMouseAwaited && (aEv->type() == QEvent::MouseButtonPress))
        QTimer::singleShot (0, this, SLOT (refreshText()));

    return QIWithRetranslateUI<QComboBox>::eventFilter (aObj, aEv);
}

void VBoxFilePathSelectorWidget::retranslateUi()
{
    /* How do we interpret the "nothing selected" item? */
    if (isResetEnabled())
    {
        mNoneStr = tr ("<reset to default>");
        mNoneTip = tr ("The actual default path value will be displayed after "
                       "accepting the changes and opening this dialog again.");
    }
    else
    {
        mNoneStr = tr ("<not selected>");
        mNoneTip = tr ("Please use the <b>Other...</b> item from the drop-down "
                       "list to select a desired path.");
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
                         tr ("Opens a dialog to select a different folder."),
                         Qt::ToolTipRole);
            setItemData (ResetId,
                         tr ("Resets the folder path to the default value."),
                         Qt::ToolTipRole);
            break;
        case Mode_File_Open:
        case Mode_File_Save:
            setItemData (SelectId,
                         tr ("Opens a dialog to select a different file."),
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

void VBoxFilePathSelectorWidget::onActivated (int aIndex)
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

void VBoxFilePathSelectorWidget::onTextEdited (const QString &aPath)
{
    changePath (aPath, false /* refresh text? */);
}

void VBoxFilePathSelectorWidget::copyToClipboard()
{
    QString text (fullPath());
    /* Copy the current text to the selection and global clipboard. */
    if (QApplication::clipboard()->supportsSelection())
        QApplication::clipboard()->setText (text, QClipboard::Selection);
    QApplication::clipboard()->setText (text, QClipboard::Clipboard);
}

void VBoxFilePathSelectorWidget::changePath (const QString &aPath,
                                             bool aRefreshText /* = true */)
{
    QString oldPath = mPath;
    setPath (aPath, aRefreshText);
    if (!mModified && mPath != oldPath)
        mModified = true;
    emit pathChanged (aPath);
}

void VBoxFilePathSelectorWidget::selectPath()
{
    /* Preparing initial directory. */
    QString initDir = mPath.isNull() ? mHomeDir :
        VBoxGlobal::getFirstExistingDir (mPath);
    if (initDir.isNull())
        initDir = mHomeDir;

    QString path;
    switch (mMode)
    {
        case Mode_File_Open:
            path = VBoxGlobal::getOpenFileName (initDir, mFileFilters, parentWidget(), mFileDialogTitle); break;
        case Mode_File_Save:
            {
                path = VBoxGlobal::getSaveFileName (initDir, mFileFilters, parentWidget(), mFileDialogTitle);
                if (!path.isEmpty() && QFileInfo (path).suffix().isEmpty())
                    path = QString ("%1.%2").arg (path).arg (mDefaultSaveExt);
                break;
            }
        case Mode_Folder:
            path = VBoxGlobal::getExistingDirectory (initDir, parentWidget(), mFileDialogTitle); break;
    }

    if (path.isNull())
        return;

    path.remove (QRegExp ("[\\\\/]$"));
    changePath (path);
}

QIcon VBoxFilePathSelectorWidget::defaultIcon() const
{
    if (mMode == Mode_Folder)
        return mIconProvider->icon (QFileIconProvider::Folder);
    else
        return mIconProvider->icon (QFileIconProvider::File);
}

QString VBoxFilePathSelectorWidget::fullPath (bool aAbsolute /* = true */) const
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

QString VBoxFilePathSelectorWidget::shrinkText (int aWidth) const
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

void VBoxFilePathSelectorWidget::refreshText()
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
            tr ("Please type the desired folder path here.") :
            tr ("Please type the desired file path here."));

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
                             mIconProvider->icon (QFileInfo (mPath)) :
                             defaultIcon());

        /* Set the tooltip */
        setToolTip (fullPath());
        setItemData (PathId, toolTip(), Qt::ToolTipRole);
    }
}

////////////////////////////////////////////////////////////////////////////////
// VBoxEmptyFileSelector

VBoxEmptyFileSelector::VBoxEmptyFileSelector (QWidget *aParent /* = NULL */)
    : QIWithRetranslateUI<QWidget> (aParent)
    , mHomeDir (QDir::current().absolutePath())
      , mIsModified (false)
{
    QHBoxLayout *mainLayout = new QHBoxLayout (this);
    mainLayout->setMargin (0);

    mSelectButton = new QPushButton (this);
    mainLayout->addWidget (mSelectButton);

    mLabel = new QILabel (this);
    mLabel->setWordWrap (true);
    mainLayout->addWidget (mLabel, 2);

    connect (mSelectButton, SIGNAL (clicked()),
             this, SLOT (choose()));

    retranslateUi();
}

void VBoxEmptyFileSelector::setPath (const QString& aPath)
{
    mLabel->setText (QString ("<compact elipsis=\"start\">%1</compact>").arg (aPath));
    mPath = aPath;
}

QString VBoxEmptyFileSelector::path() const
{
    return mPath;
}

void VBoxEmptyFileSelector::setFileDialogTitle (const QString& aTitle)
{
    mFileDialogTitle = aTitle;
}

QString VBoxEmptyFileSelector::fileDialogTitle() const
{
    return mFileDialogTitle;
}

void VBoxEmptyFileSelector::setFileFilters (const QString& aFilters)
{
    mFileFilters = aFilters;
}

QString VBoxEmptyFileSelector::fileFilters() const
{
    return mFileFilters;
}

void VBoxEmptyFileSelector::setHomeDir (const QString& aDir)
{
    mHomeDir = aDir;
}

QString VBoxEmptyFileSelector::homeDir() const
{
    return mHomeDir;
}

void VBoxEmptyFileSelector::retranslateUi()
{
    mSelectButton->setText (tr ("&Choose..."));
}

void VBoxEmptyFileSelector::choose()
{
    QString path = mPath;

    /* Preparing initial directory. */
    QString initDir = path.isNull() ? mHomeDir :
        VBoxGlobal::getFirstExistingDir (path);
    if (initDir.isNull())
        initDir = mHomeDir;

    path = VBoxGlobal::getOpenFileName (initDir, mFileFilters, parentWidget(), mFileDialogTitle);
    if (!path.isEmpty())
    {
        setPath (path);
        mIsModified = true;
        emit pathChanged (path);
    }
}
