/* $Id$ */
/** @file
 * VBox Qt GUI - UIFileManagerGuestTable class implementation.
 */

/*
 * Copyright (C) 2016-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QDateTime>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QPushButton>
#include <QUuid>

/* GUI includes: */
#include "QILabel.h"
#include "UIActionPool.h"
#include "UIConverter.h"
#include "UICommon.h"
#include "UICustomFileSystemModel.h"
#include "UIErrorString.h"
#include "UIFileManager.h"
#include "UIFileManagerGuestTable.h"
#include "UIMessageCenter.h"
#include "UIPathOperations.h"
#include "UIUserNamePasswordEditor.h"
#include "UIVirtualBoxEventHandler.h"
#include "QILineEdit.h"
#include "QIToolBar.h"

/* COM includes: */
#include "CConsole.h"
#include "CFsObjInfo.h"
#include "CGuestFsObjInfo.h"
#include "CGuestDirectory.h"
#include "CProgress.h"
#include "CGuestSessionStateChangedEvent.h"

#include <iprt/path.h>

/*********************************************************************************************************************************
*   UIGuestSessionCreateWidget definition.                                                                                   *
*********************************************************************************************************************************/
/** A QWidget extension containing text entry fields for password and username and buttons to
 *  start/stop a guest session. */
class UIGuestSessionCreateWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigCreateSession(QString strUserName, QString strPassword);
    void sigCloseSession();

public:

    UIGuestSessionCreateWidget(QWidget *pParent = 0);
    /** Disables certain widget after a guest session has been created. */
    void switchSessionCreateMode();
    /** Makes sure certain widgets are enabled so that a guest session can be created. */
    void switchSessionCloseMode();
    void markForError(bool fMarkForError);

protected:

    void retranslateUi() /* override */;
    void keyPressEvent(QKeyEvent * pEvent) /* override */;
    void showEvent(QShowEvent *pEvent) /* override */;

private slots:

    void sltButtonClick();
    void sltHandleTextChanged(const QString &strText);

private:

    enum ButtonMode
    {
        ButtonMode_Create,
        ButtonMode_Close
    };

    void          prepareWidgets();
    void          updateButton();

    ButtonMode    m_enmButtonMode;
    QILineEdit   *m_pUserNameEdit;
    UIPasswordLineEdit   *m_pPasswordEdit;
    QPushButton  *m_pButton;
    QHBoxLayout  *m_pMainLayout;
    QColor        m_defaultBaseColor;
    QColor        m_errorBaseColor;
    bool          m_fMarkedForError;
};


/*********************************************************************************************************************************
*   UIGuestSessionCreateWidget implementation.                                                                                   *
*********************************************************************************************************************************/

UIGuestSessionCreateWidget::UIGuestSessionCreateWidget(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_enmButtonMode(ButtonMode_Create)
    , m_pUserNameEdit(0)
    , m_pPasswordEdit(0)
    , m_pButton(0)
    , m_pMainLayout(0)
    , m_fMarkedForError(0)
{
    prepareWidgets();
}

void UIGuestSessionCreateWidget::prepareWidgets()
{
    m_pMainLayout = new QHBoxLayout(this);
    if (!m_pMainLayout)
        return;

    m_pMainLayout->setContentsMargins(0, 0, 0, 0);

    m_pUserNameEdit = new QILineEdit;
    if (m_pUserNameEdit)
    {
        m_pMainLayout->addWidget(m_pUserNameEdit, 2);
        m_pUserNameEdit->setPlaceholderText(QApplication::translate("UIFileManager", "User Name"));
        m_defaultBaseColor = m_pUserNameEdit->palette().color(QPalette::Base);
        m_errorBaseColor = QColor(m_defaultBaseColor.red(),
                                  0.5 * m_defaultBaseColor.green(),
                                  0.5 * m_defaultBaseColor.blue());
        connect(m_pUserNameEdit, &QILineEdit::textChanged,
                this, &UIGuestSessionCreateWidget::sltHandleTextChanged);
    }

    m_pPasswordEdit = new UIPasswordLineEdit;
    if (m_pPasswordEdit)
    {
        m_pMainLayout->addWidget(m_pPasswordEdit, 2);
        m_pPasswordEdit->setPlaceholderText(QApplication::translate("UIFileManager", "Password"));
        m_pPasswordEdit->setEchoMode(QLineEdit::Password);
        connect(m_pPasswordEdit, &UIPasswordLineEdit::textChanged,
                this, &UIGuestSessionCreateWidget::sltHandleTextChanged);
    }

    m_pButton = new QPushButton;
    if (m_pButton)
    {
        m_pMainLayout->addWidget(m_pButton);
        connect(m_pButton, &QPushButton::clicked, this, &UIGuestSessionCreateWidget::sltButtonClick);
    }


    m_pMainLayout->insertStretch(-1, 1);
    switchSessionCreateMode();
    retranslateUi();
}

void UIGuestSessionCreateWidget::sltButtonClick()
{
    if (m_enmButtonMode == ButtonMode_Create && m_pUserNameEdit && m_pPasswordEdit)
        emit sigCreateSession(m_pUserNameEdit->text(), m_pPasswordEdit->text());
    else if (m_enmButtonMode == ButtonMode_Close)
        emit sigCloseSession();
}

void UIGuestSessionCreateWidget::sltHandleTextChanged(const QString &strText)
{
    Q_UNUSED(strText);
    markForError(false);
}

void UIGuestSessionCreateWidget::retranslateUi()
{
    if (m_pUserNameEdit)
    {
        m_pUserNameEdit->setToolTip(QApplication::translate("UIFileManager", "User name to authenticate session creation"));
        m_pUserNameEdit->setPlaceholderText(QApplication::translate("UIFileManager", "User Name"));

    }
    if (m_pPasswordEdit)
    {
        m_pPasswordEdit->setToolTip(QApplication::translate("UIFileManager", "Password to authenticate session creation"));
        m_pPasswordEdit->setPlaceholderText(QApplication::translate("UIFileManager", "Password"));
    }

    if (m_pButton)
    {
        if (m_enmButtonMode == ButtonMode_Create)
            m_pButton->setText(QApplication::translate("UIFileManager", "Create Session"));
        else
            m_pButton->setText(QApplication::translate("UIFileManager", "Close Session"));
    }
}

void UIGuestSessionCreateWidget::keyPressEvent(QKeyEvent * pEvent)
{
    /* Emit sigCreateSession upon enter press: */
    if (pEvent->key() == Qt::Key_Enter || pEvent->key() == Qt::Key_Return)
    {
        if ((m_pUserNameEdit && m_pUserNameEdit->hasFocus()) ||
            (m_pPasswordEdit && m_pPasswordEdit->hasFocus()))
            sigCreateSession(m_pUserNameEdit->text(), m_pPasswordEdit->text());
    }
    QWidget::keyPressEvent(pEvent);
}

void UIGuestSessionCreateWidget::showEvent(QShowEvent *pEvent)
{
    QIWithRetranslateUI<QWidget>::showEvent(pEvent);
    if (m_pUserNameEdit)
        m_pUserNameEdit->setFocus();
}

void UIGuestSessionCreateWidget::switchSessionCreateMode()
{
    if (m_pUserNameEdit)
        m_pUserNameEdit->setEnabled(true);
    if (m_pPasswordEdit)
        m_pPasswordEdit->setEnabled(true);
    m_enmButtonMode = ButtonMode_Create;
    retranslateUi();
}

void UIGuestSessionCreateWidget::switchSessionCloseMode()
{
    if (m_pUserNameEdit)
        m_pUserNameEdit->setEnabled(false);
    if (m_pPasswordEdit)
        m_pPasswordEdit->setEnabled(false);
    m_enmButtonMode = ButtonMode_Close;
    retranslateUi();
}

void UIGuestSessionCreateWidget::markForError(bool fMarkForError)
{
    if (m_fMarkedForError == fMarkForError)
        return;
    m_fMarkedForError = fMarkForError;

        if (m_pUserNameEdit)
        {
            QPalette mPalette = m_pUserNameEdit->palette();
            if (m_fMarkedForError)
                mPalette.setColor(QPalette::Base, m_errorBaseColor);
            else
                mPalette.setColor(QPalette::Base, m_defaultBaseColor);
            m_pUserNameEdit->setPalette(mPalette);
        }
        if (m_pPasswordEdit)
        {
            QPalette mPalette = m_pPasswordEdit->palette();
            if (m_fMarkedForError)
                mPalette.setColor(QPalette::Base, m_errorBaseColor);
            else
                mPalette.setColor(QPalette::Base, m_defaultBaseColor);
            m_pPasswordEdit->setPalette(mPalette);
        }
}


/*********************************************************************************************************************************
*   UIGuestDirectoryDiskUsageComputer definition.                                                                                *
*********************************************************************************************************************************/

/** Open directories recursively and sum the disk usage. Don't block the GUI thread while doing this */
class UIGuestDirectoryDiskUsageComputer : public UIDirectoryDiskUsageComputer
{
    Q_OBJECT;

public:

    UIGuestDirectoryDiskUsageComputer(QObject *parent, QStringList strStartPath, const CGuestSession &session);

protected:

    virtual void run() /* override */;
    virtual void directoryStatisticsRecursive(const QString &path, UIDirectoryStatistics &statistics) /* override */;

private:

    CGuestSession m_comGuestSession;
};


/*********************************************************************************************************************************
*   UIGuestDirectoryDiskUsageComputer implementation.                                                                            *
*********************************************************************************************************************************/

UIGuestDirectoryDiskUsageComputer::UIGuestDirectoryDiskUsageComputer(QObject *parent, QStringList pathList, const CGuestSession &session)
    :UIDirectoryDiskUsageComputer(parent, pathList)
    , m_comGuestSession(session)
{
}

void UIGuestDirectoryDiskUsageComputer::run()
{
    /* Initialize COM: */
    COMBase::InitializeCOM(false);
    UIDirectoryDiskUsageComputer::run();
    /* Cleanup COM: */
    COMBase::CleanupCOM();
}

void UIGuestDirectoryDiskUsageComputer::directoryStatisticsRecursive(const QString &path, UIDirectoryStatistics &statistics)
{
    if (m_comGuestSession.isNull())
        return;
    /* Prevent modification of the continue flag while reading: */
    m_mutex.lock();
    /* Check if m_fOkToContinue is set to false. if so just end recursion: */
    if (!isOkToContinue())
    {
        m_mutex.unlock();
        return;
    }
    m_mutex.unlock();

    CGuestFsObjInfo fileInfo = m_comGuestSession.FsObjQueryInfo(path, true);

    if (!m_comGuestSession.isOk())
        return;
    /* if the object is a file or a symlink then read the size and return: */
    if (fileInfo.GetType() == KFsObjType_File)
    {
        statistics.m_totalSize += fileInfo.GetObjectSize();
        ++statistics.m_uFileCount;
        sigResultUpdated(statistics);
        return;
    }
    else if (fileInfo.GetType() == KFsObjType_Symlink)
    {
        statistics.m_totalSize += fileInfo.GetObjectSize();
        ++statistics.m_uSymlinkCount;
        sigResultUpdated(statistics);
        return;
    }

    if (fileInfo.GetType() != KFsObjType_Directory)
        return;
    /* Open the directory to start reading its content: */
    QVector<KDirectoryOpenFlag> flag(1, KDirectoryOpenFlag_None);
    CGuestDirectory directory = m_comGuestSession.DirectoryOpen(path, /*aFilter*/ "", flag);
    if (!m_comGuestSession.isOk())
        return;

    if (directory.isOk())
    {
        CFsObjInfo fsInfo = directory.Read();
        while (fsInfo.isOk())
        {
            if (fsInfo.GetType() == KFsObjType_File)
                statistics.m_uFileCount++;
            else if (fsInfo.GetType() == KFsObjType_Symlink)
                statistics.m_uSymlinkCount++;
            else if(fsInfo.GetType() == KFsObjType_Directory)
            {
                QString dirPath = UIPathOperations::mergePaths(path, fsInfo.GetName());
                directoryStatisticsRecursive(dirPath, statistics);
            }
        }
    }
    sigResultUpdated(statistics);
}

UIFileManagerGuestTable::UIFileManagerGuestTable(UIActionPool *pActionPool, const CMachine &comMachine, QWidget *pParent /*= 0*/)
    :UIFileManagerTable(pActionPool, pParent)
    , m_comMachine(comMachine)
    , m_pGuestSessionPanel(0)
{
    if (!m_comMachine.isNull())
        m_strTableName = m_comMachine.GetName();
    prepareToolbar();
    prepareGuestSessionPanel();
    prepareActionConnections();

    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineStateChange,
            this, &UIFileManagerGuestTable::sltMachineStateChange);
    connect(&uiCommon(), &UICommon::sigAskToCommitData,
            this, &UIFileManagerGuestTable::sltCommitDataSignalReceived);

    if (m_pActionPool && m_pActionPool->action(UIActionIndex_M_FileManager_T_GuestSession))
        m_pActionPool->action(UIActionIndex_M_FileManager_T_GuestSession)->setChecked(true);

    if (!m_comMachine.isNull() && m_comMachine.GetState() == KMachineState_Running)
        openMachineSession();
    setSessionDependentWidgetsEnabled(isSessionPossible());

    retranslateUi();
}

UIFileManagerGuestTable::~UIFileManagerGuestTable()
{
    cleanAll();
}

void UIFileManagerGuestTable::initFileTable()
{
    if (!m_comGuestSession.isOk() || m_comGuestSession.GetStatus() != KGuestSessionStatus_Started)
        return;
    /* To determine the path separator we need to have a valid guest session: */
    determinePathSeparator();
    initializeFileTree();
}

void UIFileManagerGuestTable::retranslateUi()
{
    if (m_pLocationLabel)
        m_pLocationLabel->setText(UIFileManager::tr("Guest File System:"));

    if (m_pWarningLabel)
    {
        QString strWarningText;
        switch (m_enmCheckMachine)
        {
            case CheckMachine_InvalidMachineReference:
                strWarningText = UIFileManager::tr("Machine reference is invalid.");
                break;
            case CheckMachine_MachineNotRunning:
                strWarningText = UIFileManager::tr("File manager cannot work since it works only with running guests.");
                break;
            case CheckMachine_NoGuestAdditions:
                strWarningText = UIFileManager::tr("File manager cannot work since it needs running guest additions in the guest system.");
                break;
            case CheckMachine_SessionPossible:
            default:
                break;
        }
        m_pWarningLabel->setText(QString("<p>%1</p>").arg(strWarningText));
    }

    UIFileManagerTable::retranslateUi();
}

void UIFileManagerGuestTable::readDirectory(const QString& strPath,
                                     UICustomFileSystemItem *parent, bool isStartDir /*= false*/)
{
    if (!parent)
        return;

    CGuestDirectory directory;
    QVector<KDirectoryOpenFlag> flag;
    flag.push_back(KDirectoryOpenFlag_None);

    directory = m_comGuestSession.DirectoryOpen(UIPathOperations::sanitize(strPath), /*aFilter*/ "", flag);
    if (!m_comGuestSession.isOk())
    {
        emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession), m_strTableName, FileManagerLogType_Error);
        return;
    }

    parent->setIsOpened(true);
    if (directory.isOk())
    {
        CFsObjInfo fsInfo = directory.Read();
        QMap<QString, UICustomFileSystemItem*> fileObjects;

        while (fsInfo.isOk())
        {
            if (fsInfo.GetName() != "." && fsInfo.GetName() != "..")
            {
                QVector<QVariant> data;
                QDateTime changeTime = QDateTime::fromMSecsSinceEpoch(fsInfo.GetChangeTime()/RT_NS_1MS);
                KFsObjType fsObjectType = fileType(fsInfo);
                UICustomFileSystemItem *item = new UICustomFileSystemItem(fsInfo.GetName(), parent, fsObjectType);
                if (!item)
                    continue;
                item->setData(static_cast<qulonglong>(fsInfo.GetObjectSize()), UICustomFileSystemModelColumn_Size);
                item->setData(changeTime, UICustomFileSystemModelColumn_ChangeTime);
                item->setData(fsInfo.GetUserName(), UICustomFileSystemModelColumn_Owner);
                item->setData(permissionString(fsInfo), UICustomFileSystemModelColumn_Permissions);
                item->setPath(UIPathOperations::removeTrailingDelimiters(UIPathOperations::mergePaths(strPath, fsInfo.GetName())));
                item->setIsOpened(false);
                item->setIsHidden(isFileObjectHidden(fsInfo));
                fileObjects.insert(fsInfo.GetName(), item);
                /* @todo. We will need to wait a fully implemented SymlinkRead function
                 * to be able to handle sym links properly: */
                // QString path = UIPathOperations::mergePaths(strPath, fsInfo.GetName());
                // QVector<KSymlinkReadFlag> aFlags;
                // printf("%s %s %s\n", qPrintable(fsInfo.GetName()), qPrintable(path),
                //        qPrintable(m_comGuestSession.SymlinkRead(path, aFlags)));
            }
            fsInfo = directory.Read();
        }
        checkDotDot(fileObjects, parent, isStartDir);
    }
    directory.Close();
}

void UIFileManagerGuestTable::deleteByItem(UICustomFileSystemItem *item)
{
    if (!item)
        return;
    if (item->isUpDirectory())
        return;

    if (item->isDirectory())
    {
        QVector<KDirectoryRemoveRecFlag> aFlags(1, KDirectoryRemoveRecFlag_ContentAndDir);
        m_comGuestSession.DirectoryRemoveRecursive(item->path(), aFlags);
    }
    else
        m_comGuestSession.FsObjRemove(item->path());
    if (!m_comGuestSession.isOk())
    {
        emit sigLogOutput(QString(item->path()).append(" could not be deleted"), m_strTableName, FileManagerLogType_Error);
        emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession), m_strTableName, FileManagerLogType_Error);
    }
}

void UIFileManagerGuestTable::deleteByPath(const QStringList &pathList)
{
    foreach (const QString &strPath, pathList)
    {
        CGuestFsObjInfo fileInfo = m_comGuestSession.FsObjQueryInfo(strPath, true);
        KFsObjType eType = fileType(fileInfo);
        if (eType == KFsObjType_File || eType == KFsObjType_Symlink)
        {
              m_comGuestSession.FsObjRemove(strPath);
        }
        else if (eType == KFsObjType_Directory)
        {
            QVector<KDirectoryRemoveRecFlag> aFlags(1, KDirectoryRemoveRecFlag_ContentAndDir);
            m_comGuestSession.DirectoryRemoveRecursive(strPath, aFlags);
        }
    }
}

void UIFileManagerGuestTable::goToHomeDirectory()
{
    if (m_comGuestSession.isNull())
        return;
    if (!rootItem() || rootItem()->childCount() <= 0)
        return;
    UICustomFileSystemItem *startDirItem = rootItem()->child(0);
    if (!startDirItem)
        return;

    QString userHome = UIPathOperations::sanitize(m_comGuestSession.GetUserHome());
    if (!m_comGuestSession.isOk())
    {
        emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession), m_strTableName, FileManagerLogType_Error);
        return;
    }
    QStringList pathList = userHome.split(UIPathOperations::delimiter, QString::SkipEmptyParts);
    goIntoDirectory(UIPathOperations::pathTrail(userHome));
}

bool UIFileManagerGuestTable::renameItem(UICustomFileSystemItem *item, QString newBaseName)
{

    if (!item || item->isUpDirectory() || newBaseName.isEmpty())
        return false;
    QString newPath = UIPathOperations::removeTrailingDelimiters(UIPathOperations::constructNewItemPath(item->path(), newBaseName));
    QVector<KFsObjRenameFlag> aFlags(1, KFsObjRenameFlag_Replace);

    m_comGuestSession.FsObjRename(item->path(), newPath, aFlags);

    if (!m_comGuestSession.isOk())
    {
        emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession), m_strTableName, FileManagerLogType_Error);
        return false;
    }
    item->setPath(newPath);
    return true;
}

bool UIFileManagerGuestTable::createDirectory(const QString &path, const QString &directoryName)
{
    QString newDirectoryPath = UIPathOperations::mergePaths(path, directoryName);
    QVector<KDirectoryCreateFlag> flags(1, KDirectoryCreateFlag_None);

    m_comGuestSession.DirectoryCreate(newDirectoryPath, 0/*aMode*/, flags);

    if (!m_comGuestSession.isOk())
    {
        emit sigLogOutput(newDirectoryPath.append(" could not be created"), m_strTableName, FileManagerLogType_Error);
        emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession), m_strTableName, FileManagerLogType_Error);
        return false;
    }
    emit sigLogOutput(newDirectoryPath.append(" has been created"), m_strTableName, FileManagerLogType_Info);
    return true;
}

void UIFileManagerGuestTable::copyHostToGuest(const QStringList &hostSourcePathList,
                                              const QString &strDestination /* = QString() */)
{
    if (!checkGuestSession())
        return;
    QVector<QString> sourcePaths = hostSourcePathList.toVector();
    QVector<QString> aFilters;
    QVector<QString> aFlags;
    QString strDestinationPath = strDestination;
    if (strDestinationPath.isEmpty())
        strDestinationPath = currentDirectoryPath();

    if (strDestinationPath.isEmpty())
    {
        emit sigLogOutput("No destination for copy operation", m_strTableName, FileManagerLogType_Error);
        return;
    }
    if (hostSourcePathList.empty())
    {
        emit sigLogOutput("No source for copy operation", m_strTableName, FileManagerLogType_Error);
        return;
    }

    foreach (const QString &strSource, sourcePaths)
    {
        RTFSOBJINFO ObjInfo;
        int vrc = RTPathQueryInfo(strSource.toStdString().c_str(), &ObjInfo, RTFSOBJATTRADD_NOTHING);
        if (RT_SUCCESS(vrc))
        {
            /* If the source is an directory, make sure to add the appropriate flag to make copying work
             * into existing directories on the guest. This otherwise would fail (default). */
            if (RTFS_IS_DIRECTORY(ObjInfo.Attr.fMode))
                aFlags.append("CopyIntoExisting");
            else /* Make sure to keep the vector in sync with the number of source items by adding an empty entry. */
                aFlags.append("");
        }
        else
            emit sigLogOutput(QString("Querying information for host item \"%s\" failed with %Rrc").arg(strSource.toStdString().c_str(), vrc),
                              m_strTableName, FileManagerLogType_Error);
    }

    CProgress progress = m_comGuestSession.CopyToGuest(sourcePaths, aFilters, aFlags, strDestinationPath);
    if (!checkGuestSession())
        return;
    emit sigNewFileOperation(progress);
}

QUuid UIFileManagerGuestTable::machineId()
{
    if (m_comMachine.isNull())
        return QUuid();
    return m_comMachine.GetId();
}

bool UIFileManagerGuestTable::isGuestSessionRunning() const
{
    if (m_comGuestSession.isNull())
        return false;
    if (m_comGuestSession.GetStatus() == KGuestSessionStatus_Starting ||
        m_comGuestSession.GetStatus() == KGuestSessionStatus_Started)
        return true;
    return false;
}

void UIFileManagerGuestTable::copyGuestToHost(const QString& hostDestinationPath)
{
    if (!checkGuestSession())
        return;
    QVector<QString> sourcePaths = selectedItemPathList().toVector();
    QVector<QString> aFilters;
    QVector<QString> aFlags;

    if (hostDestinationPath.isEmpty())
    {
        emit sigLogOutput("No destination for copy operation", m_strTableName, FileManagerLogType_Error);
        return;
    }
    if (sourcePaths.empty())
    {
        emit sigLogOutput("No source for copy operation", m_strTableName, FileManagerLogType_Error);
        return;
    }

    foreach (const QString &strSource, sourcePaths)
    {
        /** @todo Cache this info and use the item directly, which has this info already? */

        /* If the source is an directory, make sure to add the appropriate flag to make copying work
         * into existing directories on the guest. This otherwise would fail (default). */
        CGuestFsObjInfo fileInfo = m_comGuestSession.FsObjQueryInfo(strSource, true);
        if (!m_comGuestSession.isOk())
        {
            emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession), m_strTableName, FileManagerLogType_Error);
            return;
        }

        KFsObjType eType = fileType(fileInfo);
        if (eType == KFsObjType_Directory)
            aFlags.append("CopyIntoExisting");
        else /* Make sure to keep the vector in sync with the number of source items by adding an empty entry. */
            aFlags.append("");
    }

    CProgress progress = m_comGuestSession.CopyFromGuest(sourcePaths, aFilters, aFlags, hostDestinationPath);
    if (!checkGuestSession())
        return;
    emit sigNewFileOperation(progress);
}

KFsObjType UIFileManagerGuestTable::fileType(const CFsObjInfo &fsInfo)
{
    if (fsInfo.isNull() || !fsInfo.isOk())
        return KFsObjType_Unknown;
    if (fsInfo.GetType() == KFsObjType_Directory)
         return KFsObjType_Directory;
    else if (fsInfo.GetType() == KFsObjType_File)
        return KFsObjType_File;
    else if (fsInfo.GetType() == KFsObjType_Symlink)
        return KFsObjType_Symlink;

    return KFsObjType_Unknown;
}

KFsObjType UIFileManagerGuestTable::fileType(const CGuestFsObjInfo &fsInfo)
{
    if (fsInfo.isNull() || !fsInfo.isOk())
        return KFsObjType_Unknown;
    if (fsInfo.GetType() == KFsObjType_Directory)
         return KFsObjType_Directory;
    else if (fsInfo.GetType() == KFsObjType_File)
        return KFsObjType_File;
    else if (fsInfo.GetType() == KFsObjType_Symlink)
        return KFsObjType_Symlink;

    return KFsObjType_Unknown;
}


QString UIFileManagerGuestTable::fsObjectPropertyString()
{
    QStringList selectedObjects = selectedItemPathList();
    if (selectedObjects.isEmpty())
        return QString();
    if (selectedObjects.size() == 1)
    {
        if (selectedObjects.at(0).isNull())
            return QString();

        CGuestFsObjInfo fileInfo = m_comGuestSession.FsObjQueryInfo(selectedObjects.at(0), false /*aFollowSymlinks*/);
        if (!m_comGuestSession.isOk())
        {
            emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession), m_strTableName, FileManagerLogType_Error);
            return QString();
        }

        QStringList propertyStringList;

        /* Name: */
        propertyStringList << UIFileManager::tr("<b>Name:</b> %1<br/>").arg(UIPathOperations::getObjectName(fileInfo.GetName()));

        /* Size: */
        LONG64 size = fileInfo.GetObjectSize();
        propertyStringList << UIFileManager::tr("<b>Size:</b> %1 bytes").arg(QString::number(size));
        if (size >= UIFileManagerTable::m_iKiloByte)
            propertyStringList << QString(" (%1)<br/>").arg(humanReadableSize(size));
        else
            propertyStringList << QString("<br/>");

        /* Allocated size: */
        size = fileInfo.GetAllocatedSize();
        propertyStringList << UIFileManager::tr("<b>Allocated:</b> %1 bytes").arg(QString::number(size));
        if (size >= UIFileManagerTable::m_iKiloByte)
            propertyStringList << QString(" (%1)<br/>").arg(humanReadableSize(size));
        else
            propertyStringList << QString("<br/>");

        /* Type: */
        QString str;
        KFsObjType const enmType = fileInfo.GetType();
        switch (enmType)
        {
            case KFsObjType_Directory:  str = UIFileManager::tr("directory"); break;
            case KFsObjType_File:       str = UIFileManager::tr("file"); break;
            case KFsObjType_Symlink:    str = UIFileManager::tr("symbolic link"); break;
            case KFsObjType_DevChar:    str = UIFileManager::tr("character device"); break;
            case KFsObjType_DevBlock:   str = UIFileManager::tr("block device"); break;
            case KFsObjType_Fifo:       str = UIFileManager::tr("fifo"); break;
            case KFsObjType_Socket:     str = UIFileManager::tr("socket"); break;
            case KFsObjType_WhiteOut:   str = UIFileManager::tr("whiteout"); break;
            case KFsObjType_Unknown:    str = UIFileManager::tr("unknown"); break;
            default:                    str = UIFileManager::tr("illegal-value"); break;
        }
        propertyStringList <<  UIFileManager::tr("<b>Type:</b> %1<br/>").arg(str);

        /* INode number, device, link count: */
        propertyStringList << UIFileManager::tr("<b>INode:</b> %1<br/>").arg(fileInfo.GetNodeId());
        propertyStringList << UIFileManager::tr("<b>Device:</b> %1<br/>").arg(fileInfo.GetNodeIdDevice());  /** @todo hex */
        propertyStringList << UIFileManager::tr("<b>Hardlinks:</b> %1<br/>").arg(fileInfo.GetHardLinks());

        /* Attributes: */
        str = fileInfo.GetFileAttributes();
        if (!str.isEmpty())
        {
            int offSpace = str.indexOf(' ');
            if (offSpace < 0)
                offSpace = str.length();
            propertyStringList << UIFileManager::tr("<b>Mode:</b> %1<br/>").arg(str.left(offSpace));
            propertyStringList << UIFileManager::tr("<b>Attributes:</b> %1<br/>").arg(str.mid(offSpace + 1).trimmed());
        }

        /* Character/block device ID: */
        ULONG uDeviceNo = fileInfo.GetDeviceNumber();
        if (uDeviceNo != 0 || enmType == KFsObjType_DevChar || enmType == KFsObjType_DevBlock)
            propertyStringList << UIFileManager::tr("<b>Device ID:</b> %1<br/>").arg(uDeviceNo); /** @todo hex */

        /* Owner: */
        propertyStringList << UIFileManager::tr("<b>Owner:</b> %1 (%2)<br/>").
            arg(fileInfo.GetUserName()).arg(fileInfo.GetUID());
        propertyStringList << UIFileManager::tr("<b>Group:</b> %1 (%2)<br/>").
            arg(fileInfo.GetGroupName()).arg(fileInfo.GetGID());

        /* Timestamps: */
        propertyStringList << UIFileManager::tr("<b>Birth:</b> %1<br/>").
            arg(QDateTime::fromMSecsSinceEpoch(fileInfo.GetBirthTime() / RT_NS_1MS).toString());
        propertyStringList << UIFileManager::tr("<b>Change:</b> %1<br/>").
            arg(QDateTime::fromMSecsSinceEpoch(fileInfo.GetChangeTime() / RT_NS_1MS).toString());
        propertyStringList << UIFileManager::tr("<b>Modified:</b> %1<br/>").
            arg(QDateTime::fromMSecsSinceEpoch(fileInfo.GetModificationTime() / RT_NS_1MS).toString());
        propertyStringList << UIFileManager::tr("<b>Access:</b> %1<br/>").
            arg(QDateTime::fromMSecsSinceEpoch(fileInfo.GetAccessTime() / RT_NS_1MS).toString());

        /* Join the list elements into a single string seperated by empty string: */
        return propertyStringList.join(QString());
    }

    int fileCount = 0;
    int directoryCount = 0;
    ULONG64 totalSize = 0;

    for(int i = 0; i < selectedObjects.size(); ++i)
    {
        CGuestFsObjInfo fileInfo = m_comGuestSession.FsObjQueryInfo(selectedObjects.at(0), true);
        if (!m_comGuestSession.isOk())
        {
            emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession), m_strTableName, FileManagerLogType_Error);
            continue;
        }

        KFsObjType type = fileType(fileInfo);

        if (type == KFsObjType_File)
            ++fileCount;
        if (type == KFsObjType_Directory)
            ++directoryCount;
        totalSize += fileInfo.GetObjectSize();
    }
    QStringList propertyStringList;
    propertyStringList << UIFileManager::tr("<b>Selected:</b> %1 files and %2 directories<br/>").
        arg(QString::number(fileCount)).arg(QString::number(directoryCount));
    propertyStringList << UIFileManager::tr("<b>Size (non-recursive):</b> %1 bytes").arg(QString::number(totalSize));
    if (totalSize >= m_iKiloByte)
        propertyStringList << QString(" (%1)").arg(humanReadableSize(totalSize));

    return propertyStringList.join(QString());;
}

void UIFileManagerGuestTable::showProperties()
{
    if (m_comGuestSession.isNull())
        return;
    QString fsPropertyString = fsObjectPropertyString();
    if (fsPropertyString.isEmpty())
        return;

    m_pPropertiesDialog = new UIPropertiesDialog(this);
    if (!m_pPropertiesDialog)
        return;

    QStringList selectedObjects = selectedItemPathList();
    if (selectedObjects.size() == 0)
        return;

    m_pPropertiesDialog->setWindowTitle(UIFileManager::tr("Properties"));
    m_pPropertiesDialog->setPropertyText(fsPropertyString);
    m_pPropertiesDialog->execute();

    delete m_pPropertiesDialog;
    m_pPropertiesDialog = 0;
}

void UIFileManagerGuestTable::determineDriveLetters()
{
    if (m_comGuestSession.isNull())
        return;
    KPathStyle pathStyle = m_comGuestSession.GetPathStyle();
    if (pathStyle != KPathStyle_DOS)
        return;

    /** @todo Currently API lacks a way to query windows drive letters.
     *  so we enumarate them by using CGuestSession::DirectoryExists() */
    m_driveLetterList.clear();
    for (int i = 'A'; i <= 'Z'; ++i)
    {
        QString path((char)i);
        path += ":/";
        bool exists = m_comGuestSession.DirectoryExists(path, false /* aFollowSymlinks */);
        if (exists)
            m_driveLetterList.push_back(path);
    }
}

void UIFileManagerGuestTable::determinePathSeparator()
{
    if (m_comGuestSession.isNull())
        return;
    KPathStyle pathStyle = m_comGuestSession.GetPathStyle();
    if (pathStyle == KPathStyle_DOS)
        setPathSeparator(UIPathOperations::dosDelimiter);
}

void UIFileManagerGuestTable::prepareToolbar()
{
    if (m_pToolBar && m_pActionPool)
    {
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_GoUp));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_GoHome));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Refresh));
        m_pToolBar->addSeparator();
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Delete));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Rename));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_CreateNewDirectory));

        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Copy));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Cut));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Paste));
        m_pToolBar->addSeparator();
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_SelectAll));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_InvertSelection));
        m_pToolBar->addSeparator();
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_ShowProperties));
        m_selectionDependentActions.insert(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Delete));
        m_selectionDependentActions.insert(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Rename));
        m_selectionDependentActions.insert(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Copy));
        m_selectionDependentActions.insert(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Cut));
        m_selectionDependentActions.insert(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_ShowProperties));

        /* Hide these actions for now until we have a suitable guest-to-guest copy function: */
        m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Copy)->setVisible(false);
        m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Cut)->setVisible(false);
        m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Paste)->setVisible(false);

        m_pToolBar->addSeparator();
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_FileManager_T_GuestSession));
    }

    setSelectionDependentActionsEnabled(false);
    setPasteActionEnabled(false);
}

void UIFileManagerGuestTable::createFileViewContextMenu(const QWidget *pWidget, const QPoint &point)
{
    if (!pWidget)
        return;

    QMenu menu;
    menu.addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_GoUp));

    menu.addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_GoHome));
    menu.addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Refresh));
    menu.addSeparator();
    menu.addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Delete));
    menu.addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Rename));
    menu.addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_CreateNewDirectory));
    menu.addSeparator();
    menu.addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Copy));
    menu.addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Cut));
    menu.addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Paste));
    menu.addSeparator();
    menu.addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_SelectAll));
    menu.addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_InvertSelection));
    menu.addSeparator();
    menu.addAction(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_ShowProperties));
    menu.exec(pWidget->mapToGlobal(point));
}

void UIFileManagerGuestTable::setPasteActionEnabled(bool fEnabled)
{
    m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Paste)->setEnabled(fEnabled);
}

void UIFileManagerGuestTable::pasteCutCopiedObjects()
{
}

void UIFileManagerGuestTable::prepareActionConnections()
{
    if (m_pActionPool->action(UIActionIndex_M_FileManager_T_GuestSession))
        connect(m_pActionPool->action(UIActionIndex_M_FileManager_T_GuestSession), &QAction::toggled,
                this, &UIFileManagerGuestTable::sltGuestSessionPanelToggled);

    connect(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_GoUp), &QAction::triggered,
            this, &UIFileManagerTable::sltGoUp);
    connect(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_GoHome), &QAction::triggered,
            this, &UIFileManagerTable::sltGoHome);
    connect(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Refresh), &QAction::triggered,
            this, &UIFileManagerTable::sltRefresh);
    connect(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Delete), &QAction::triggered,
            this, &UIFileManagerTable::sltDelete);
    connect(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Rename), &QAction::triggered,
            this, &UIFileManagerTable::sltRename);
    connect(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Copy), &QAction::triggered,
            this, &UIFileManagerTable::sltCopy);
    connect(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Cut), &QAction::triggered,
            this, &UIFileManagerTable::sltCut);
    connect(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_Paste), &QAction::triggered,
            this, &UIFileManagerTable::sltPaste);
    connect(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_SelectAll), &QAction::triggered,
            this, &UIFileManagerTable::sltSelectAll);
    connect(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_InvertSelection), &QAction::triggered,
            this, &UIFileManagerTable::sltInvertSelection);
    connect(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_ShowProperties), &QAction::triggered,
            this, &UIFileManagerTable::sltShowProperties);
    connect(m_pActionPool->action(UIActionIndex_M_FileManager_S_Guest_CreateNewDirectory), &QAction::triggered,
            this, &UIFileManagerTable::sltCreateNewDirectory);
}

void UIFileManagerGuestTable::prepareGuestSessionPanel()
{
    if (m_pMainLayout)
    {
        m_pGuestSessionPanel = new UIGuestSessionCreateWidget;
        if (m_pGuestSessionPanel)
        {
            m_pMainLayout->addWidget(m_pGuestSessionPanel, m_pMainLayout->rowCount(), 0, 1, m_pMainLayout->columnCount());
            m_pGuestSessionPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
            //sltHandleGuestSessionPanelShown();
            connect(m_pGuestSessionPanel, &UIGuestSessionCreateWidget::sigCreateSession,
                    this, &UIFileManagerGuestTable::sltCreateGuestSession);
            connect(m_pGuestSessionPanel, &UIGuestSessionCreateWidget::sigCloseSession,
                    this, &UIFileManagerGuestTable::sltHandleCloseSessionRequest);
            // connect(m_pGuestSessionPanel, &UIFileManagerGuestSessionPanel::sigHidePanel,
            //         this, &UIFileManagerGuestTable::sltHandleGuestSessionPanelHidden);
            // connect(m_pGuestSessionPanel, &UIFileManagerGuestSessionPanel::sigShowPanel,
            //         this, &UIFileManagerGuestTable::sltHandleGuestSessionPanelShown);
        }
    }
}

bool UIFileManagerGuestTable::checkGuestSession()
{
    if (!m_comGuestSession.isOk())
    {
        emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession), m_strTableName, FileManagerLogType_Error);
        return false;
    }
    return true;
}

QString UIFileManagerGuestTable::permissionString(const CFsObjInfo &fsInfo)
{
    /* Attributes: */
    QString strAttributes = fsInfo.GetFileAttributes();

    if (strAttributes.isEmpty())
        return strAttributes;

    int offSpace = strAttributes.indexOf(' ');
    if (offSpace < 0)
        offSpace = strAttributes.length();
    return strAttributes.left(offSpace);
}

bool UIFileManagerGuestTable::isFileObjectHidden(const CFsObjInfo &fsInfo)
{
    QString strAttributes = fsInfo.GetFileAttributes();

    if (strAttributes.isEmpty())
        return false;

    int offSpace = strAttributes.indexOf(' ');
    if (offSpace < 0)
        offSpace = strAttributes.length();
    QString strRight(strAttributes.mid(offSpace + 1).trimmed());

    if (strRight.indexOf('H', Qt::CaseSensitive) == -1)
        return false;
    return true;
}

void UIFileManagerGuestTable::sltGuestSessionPanelToggled(bool fChecked)
{
    if (m_pGuestSessionPanel)
        m_pGuestSessionPanel->setVisible(fChecked);
}

// void UIFileManagerGuestTable::sltHandleGuestSessionPanelHidden()
// {
//     if (m_pActionPool && m_pActionPool->action(UIActionIndex_M_FileManager_T_GuestSession))
//         m_pActionPool->action(UIActionIndex_M_FileManager_T_GuestSession)->setChecked(false);
// }

// void UIFileManagerGuestTable::sltHandleGuestSessionPanelShown()
// {
//     if (m_pActionPool && m_pActionPool->action(UIActionIndex_M_FileManager_T_GuestSession))
//         m_pActionPool->action(UIActionIndex_M_FileManager_T_GuestSession)->setChecked(true);
// }

void UIFileManagerGuestTable::sltMachineStateChange(const QUuid &uMachineId, const KMachineState enmMachineState)
{
    if (uMachineId.isNull() || m_comMachine.isNull() || uMachineId != m_comMachine.GetId())
        return;

    if (enmMachineState == KMachineState_Running)
        openMachineSession();
    else if (enmMachineState == KMachineState_Paused)
        return;
    else
        cleanAll();

    setSessionDependentWidgetsEnabled(isSessionPossible());
    retranslateUi();
}

bool UIFileManagerGuestTable::closeMachineSession()
{
    if (!m_comGuest.isNull())
        m_comGuest.detach();

    if (!m_comConsole.isNull())
        m_comConsole.detach();

    if (!m_comSession.isNull())
    {
        m_comSession.UnlockMachine();
        m_comSession.detach();
    }
    return true;
}

bool UIFileManagerGuestTable::openMachineSession()
{
    if (m_comMachine.isNull())
    {
        emit sigLogOutput("Invalid machine reference", m_strTableName, FileManagerLogType_Error);
        return false;
    }
    m_comSession = uiCommon().openSession(m_comMachine.GetId(), KLockType_Shared);
    if (m_comSession.isNull())
    {
        emit sigLogOutput("Could not open machine session", m_strTableName, FileManagerLogType_Error);
        return false;
    }

    m_comConsole = m_comSession.GetConsole();
    if (m_comConsole.isNull())
    {
        emit sigLogOutput("Machine console is invalid", m_strTableName, FileManagerLogType_Error);
        return false;
    }

    m_comGuest = m_comConsole.GetGuest();
    if (m_comGuest.isNull())
    {
        emit sigLogOutput("Guest reference is invalid", m_strTableName, FileManagerLogType_Error);
        return false;
    }

    /* Prepare guest listener for guest session related events: */
    {
        QVector<KVBoxEventType> eventTypes;
        eventTypes << KVBoxEventType_OnGuestSessionRegistered;
        prepareListener(m_pQtGuestListener, m_comGuestListener, m_comGuest.GetEventSource(), eventTypes);
        connect(m_pQtGuestListener->getWrapped(), &UIMainEventListener::sigGuestSessionUnregistered,
                this, &UIFileManagerGuestTable::sltGuestSessionUnregistered);
        connect(m_pQtGuestListener->getWrapped(), &UIMainEventListener::sigGuestSessionRegistered,
                this, &UIFileManagerGuestTable::sltGuestSessionRegistered);
    }

    /* Prepare console listener for guest additions state change events: */
    {
        QVector<KVBoxEventType> eventTypes;
        eventTypes << KVBoxEventType_OnAdditionsStateChanged;
        prepareListener(m_pQtConsoleListener, m_comConsoleListener, m_comConsole.GetEventSource(), eventTypes);
        connect(m_pQtConsoleListener->getWrapped(), &UIMainEventListener::sigAdditionsChange,
                this, &UIFileManagerGuestTable::sltAdditionsStateChange);
    }
    emit sigLogOutput("Shared machine session opened", m_strTableName, FileManagerLogType_Info);
    return true;
}

bool UIFileManagerGuestTable::isGuestAdditionsAvailable()
{
    if (m_comGuest.isNull())
        return false;
    return m_comGuest.GetAdditionsStatus(m_comGuest.GetAdditionsRunLevel());
}

void UIFileManagerGuestTable::cleanupGuestListener()
{
    if (!m_pQtGuestListener.isNull())
    {
        m_pQtGuestListener->getWrapped()->disconnect();
        if (!m_comGuest.isNull())
            cleanupListener(m_pQtGuestListener, m_comGuestListener, m_comGuest.GetEventSource());
    }
}

void UIFileManagerGuestTable::cleanupGuestSessionListener()
{
    if (!m_pQtSessionListener.isNull())
    {
        m_pQtSessionListener->getWrapped()->disconnect();
        if (!m_comGuestSession.isNull())
            cleanupListener(m_pQtSessionListener, m_comSessionListener, m_comGuestSession.GetEventSource());
    }
}

void UIFileManagerGuestTable::cleanupConsoleListener()
{
    if (!m_pQtConsoleListener.isNull())
    {
        m_pQtConsoleListener->getWrapped()->disconnect();
        if (!m_comConsole.isNull())
            cleanupListener(m_pQtConsoleListener, m_comConsoleListener, m_comConsole.GetEventSource());
    }
}

void UIFileManagerGuestTable::postGuestSessionCreated()
{
    if (m_pGuestSessionPanel)
        m_pGuestSessionPanel->switchSessionCloseMode();
}

void UIFileManagerGuestTable::postGuestSessionClosed()
{
    if (m_pGuestSessionPanel)
        m_pGuestSessionPanel->switchSessionCreateMode();
}

void UIFileManagerGuestTable::prepareListener(ComObjPtr<UIMainEventListenerImpl> &QtListener,
                                              CEventListener &comEventListener,
                                              CEventSource comEventSource, QVector<KVBoxEventType>& eventTypes)
{
    if (!comEventSource.isOk())
        return;
    /* Create event listener instance: */
    QtListener.createObject();
    QtListener->init(new UIMainEventListener, this);
    comEventListener = CEventListener(QtListener);

    /* Register event listener for CProgress event source: */
    comEventSource.RegisterListener(comEventListener, eventTypes, FALSE /* active? */);

    /* Register event sources in their listeners as well: */
    QtListener->getWrapped()->registerSource(comEventSource, comEventListener);
}

void UIFileManagerGuestTable::cleanupListener(ComObjPtr<UIMainEventListenerImpl> &QtListener,
                                              CEventListener &comEventListener,
                                              CEventSource comEventSource)
{
    if (!comEventSource.isOk())
        return;
    /* Unregister everything: */
    QtListener->getWrapped()->unregisterSources();

    /* Make sure VBoxSVC is available: */
    if (!uiCommon().isVBoxSVCAvailable())
        return;

    /* Unregister event listener for CProgress event source: */
    comEventSource.UnregisterListener(comEventListener);
}

void UIFileManagerGuestTable::sltGuestSessionUnregistered(CGuestSession guestSession)
{
    if (guestSession.isNull())
        return;
    if (guestSession == m_comGuestSession && !m_comGuestSession.isNull())
    {
        m_comGuestSession.detach();
        postGuestSessionClosed();
        emit sigLogOutput("Guest session unregistered", m_strTableName, FileManagerLogType_Info);
    }
}

void UIFileManagerGuestTable::sltGuestSessionRegistered(CGuestSession guestSession)
{
    if (guestSession == m_comGuestSession && !m_comGuestSession.isNull())
        emit sigLogOutput("Guest session registered", m_strTableName, FileManagerLogType_Info);
}

void UIFileManagerGuestTable::sltGuestSessionStateChanged(const CGuestSessionStateChangedEvent &cEvent)
{
    if (cEvent.isOk())
    {
        CVirtualBoxErrorInfo cErrorInfo = cEvent.GetError();
        if (cErrorInfo.isOk() && !cErrorInfo.GetText().contains("success", Qt::CaseInsensitive))
            emit sigLogOutput(cErrorInfo.GetText(), m_strTableName, FileManagerLogType_Error);
    }
    if (m_comGuestSession.isOk())
    {
        if (m_comGuestSession.GetStatus() == KGuestSessionStatus_Started)
        {
            initFileTable();
            postGuestSessionCreated();
        }
        emit sigLogOutput(QString("%1: %2").arg("Guest session status has changed").arg(gpConverter->toString(m_comGuestSession.GetStatus())),
                  m_strTableName, FileManagerLogType_Info);
    }
    else
        emit sigLogOutput("Guest session is not valid", m_strTableName, FileManagerLogType_Error);
}

void UIFileManagerGuestTable::sltCreateGuestSession(QString strUserName, QString strPassword)
{
    if (strUserName.isEmpty())
    {
        emit sigLogOutput("No user name is given", m_strTableName, FileManagerLogType_Error);
        if (m_pGuestSessionPanel)
            m_pGuestSessionPanel->markForError(true);
        return;
    }
    if (m_pGuestSessionPanel)
        m_pGuestSessionPanel->markForError(!openGuestSession(strUserName, strPassword));
}

bool UIFileManagerGuestTable::isSessionPossible()
{
    if (m_comMachine.isNull())
    {
        m_enmCheckMachine = CheckMachine_InvalidMachineReference;
        return false;
    }
    if (m_comMachine.GetState() != KMachineState_Running)
    {
        m_enmCheckMachine = CheckMachine_MachineNotRunning;
        return false;
    }
    if (!isGuestAdditionsAvailable())
    {
        m_enmCheckMachine = CheckMachine_NoGuestAdditions;
        return false;
    }
    m_enmCheckMachine = CheckMachine_SessionPossible;
    return true;
}

void UIFileManagerGuestTable::sltHandleCloseSessionRequest()
{
    cleanupGuestSessionListener();

    closeGuestSession();
}

void UIFileManagerGuestTable::sltCommitDataSignalReceived()
{
    cleanAll();
    if (!m_comMachine.isNull())
        m_comMachine.detach();
}

void UIFileManagerGuestTable::sltAdditionsStateChange()
{
    setSessionDependentWidgetsEnabled(isSessionPossible());

}

void UIFileManagerGuestTable::setSessionDependentWidgetsEnabled(bool pEnabled)
{
    UIFileManagerTable::setSessionDependentWidgetsEnabled(pEnabled);
    if (m_pGuestSessionPanel)
        m_pGuestSessionPanel->setEnabled(pEnabled);
}

bool UIFileManagerGuestTable::openGuestSession(const QString &strUserName, const QString &strPassword)
{
    if (m_comGuest.isNull())
    {
        emit sigLogOutput("Guest reference is invalid", m_strTableName, FileManagerLogType_Error);
        return false;
    }

    if (!isGuestAdditionsAvailable())
    {
        emit sigLogOutput("Could not find Guest Additions", m_strTableName, FileManagerLogType_Error);
        postGuestSessionClosed();
        if (m_pGuestSessionPanel)
            m_pGuestSessionPanel->markForError(true);
        return false;
    }

    m_comGuestSession = m_comGuest.CreateSession(strUserName, strPassword,
                                                 QString() /* Domain */, "File Manager Session");
    if (m_comGuestSession.isNull())
    {
        emit sigLogOutput("Could not create guest session", m_strTableName, FileManagerLogType_Error);
        return false;
    }

    if (!m_comGuestSession.isOk())
    {
        emit sigLogOutput(UIErrorString::formatErrorInfo(m_comGuestSession), m_strTableName, FileManagerLogType_Error);
        return false;
    }

    QVector<KVBoxEventType> eventTypes(QVector<KVBoxEventType>() << KVBoxEventType_OnGuestSessionStateChanged);
    prepareListener(m_pQtSessionListener, m_comSessionListener, m_comGuestSession.GetEventSource(), eventTypes);
    qRegisterMetaType<CGuestSessionStateChangedEvent>();
    connect(m_pQtSessionListener->getWrapped(), &UIMainEventListener::sigGuestSessionStatedChanged,
            this, &UIFileManagerGuestTable::sltGuestSessionStateChanged);

    return true;
}

void UIFileManagerGuestTable::closeGuestSession()
{
    if (!m_comGuestSession.isNull())
    {
        m_comGuestSession.Close();
        m_comGuestSession.detach();
        emit sigLogOutput("Guest session is closed", m_strTableName, FileManagerLogType_Info);
    }
    reset();
    postGuestSessionClosed();
}

void UIFileManagerGuestTable::cleanAll()
{
    cleanupConsoleListener();
    cleanupGuestListener();
    cleanupGuestSessionListener();

    closeGuestSession();
    closeMachineSession();
}


#include "UIFileManagerGuestTable.moc"
