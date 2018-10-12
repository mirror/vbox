/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestControlFileTable class implementation.
 */

/*
 * Copyright (C) 2016-2017 Oracle Corporation
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

/* Qt includes: */
# include <QAction>
# include <QComboBox>
# include <QDateTime>
# include <QDir>
# include <QHeaderView>
# include <QItemDelegate>
# include <QGridLayout>
# include <QMenu>
# include <QSortFilterProxyModel>
# include <QTextEdit>
# include <QPushButton>

/* GUI includes: */
# include "QIDialog.h"
# include "QIDialogButtonBox.h"
# include "QILabel.h"
# include "QILineEdit.h"
# include "QIMessageBox.h"
# include "UIErrorString.h"
# include "UIGuestFileTable.h"
# include "UIIconPool.h"
# include "UIGuestControlFileTable.h"
# include "UIGuestControlFileModel.h"
# include "UIToolBar.h"
# include "UIVMInformationDialog.h"

/* COM includes: */
# include "CFsObjInfo.h"
# include "CGuestFsObjInfo.h"
# include "CGuestDirectory.h"
# include "CProgress.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */



/*********************************************************************************************************************************
*   UIGuestControlFileView definition.                                                                                           *
*********************************************************************************************************************************/

/** Using QITableView causes the following problem when I click on the table items
    Qt WARNING: Cannot creat accessible child interface for object:  UIGuestControlFileView.....
    so for now subclass QTableView */
class UIGuestControlFileView : public QTableView
{

    Q_OBJECT;

signals:

    void sigGoUp();
    void sigGoHome();
    void sigRefresh();
    void sigRename();
    void sigCreateNewDirectory();
    void sigDelete();
    void sigCut();
    void sigCopy();
    void sigPaste();
    void sigShowProperties();

    void sigSelectionChanged(const QItemSelection & selected, const QItemSelection & deselected);

public:

    UIGuestControlFileView(QWidget * parent = 0);
    bool hasSelection() const;

protected:

    virtual void selectionChanged(const QItemSelection & selected, const QItemSelection & deselected) /*override */;
    void contextMenuEvent(QContextMenuEvent *pEvent);

private:

    void configure();

};


/*********************************************************************************************************************************
*   UIFileDelegate definition.                                                                                                   *
*********************************************************************************************************************************/
/** A QItemDelegate child class to disable dashed lines drawn around selected cells in QTableViews */
class UIFileDelegate : public QItemDelegate
{

    Q_OBJECT;

protected:
        virtual void drawFocus ( QPainter * /*painter*/, const QStyleOptionViewItem & /*option*/, const QRect & /*rect*/ ) const {}
};


/*********************************************************************************************************************************
*   UIFileStringInputDialog definition.                                                                                          *
*********************************************************************************************************************************/

/** A QIDialog child including a line edit whose text exposed when the dialog is accepted */
class UIStringInputDialog : public QIDialog
{

    Q_OBJECT;

public:

    UIStringInputDialog(QWidget *pParent = 0, Qt::WindowFlags flags = 0);
    QString getString() const;

private:

    QILineEdit      *m_pLineEdit;

};


/*********************************************************************************************************************************
*   UIHostDirectoryDiskUsageComputer implementation.                                                                             *
*********************************************************************************************************************************/

UIDirectoryDiskUsageComputer::UIDirectoryDiskUsageComputer(QObject *parent, QStringList pathList)
    :QThread(parent)
    , m_pathList(pathList)
    , m_fOkToContinue(true)
{
}

void UIDirectoryDiskUsageComputer::run()
{
    for (int i = 0; i < m_pathList.size(); ++i)
        directoryStatisticsRecursive(m_pathList[i], m_resultStatistics);
}

void UIDirectoryDiskUsageComputer::stopRecursion()
{
    m_mutex.lock();
    m_fOkToContinue = false;
    m_mutex.unlock();
}

bool UIDirectoryDiskUsageComputer::isOkToContinue() const
{
    return m_fOkToContinue;
}


/*********************************************************************************************************************************
*   UIPathOperations implementation.                                                                                             *
*********************************************************************************************************************************/

const QChar UIPathOperations::delimiter = QChar('/');
const QChar UIPathOperations::dosDelimiter = QChar('\\');

/* static */ QString UIPathOperations::removeMultipleDelimiters(const QString &path)
{
    QString newPath(path);
    QString doubleDelimiter(2, delimiter);

    while (newPath.contains(doubleDelimiter) && !newPath.isEmpty())
        newPath = newPath.replace(doubleDelimiter, delimiter);
    return newPath;
}

/* static */ QString UIPathOperations::removeTrailingDelimiters(const QString &path)
{
    if (path.isNull() || path.isEmpty())
        return QString();
    QString newPath(path);
    /* Make sure for we dont have any trailing delimiters: */
    while (newPath.length() > 1 && newPath.at(newPath.length() - 1) == UIPathOperations::delimiter)
        newPath.chop(1);
    return newPath;
}

/* static */ QString UIPathOperations::addTrailingDelimiters(const QString &path)
{
    if (path.isNull() || path.isEmpty())
        return QString();
    QString newPath(path);
    while (newPath.length() > 1 && newPath.at(newPath.length() - 1) != UIPathOperations::delimiter)
        newPath += UIPathOperations::delimiter;
    return newPath;
}

/* static */ QString UIPathOperations::addStartDelimiter(const QString &path)
{
    if (path.isEmpty())
        return QString(path);
    QString newPath(path);

    if (doesPathStartWithDriveLetter(newPath))
    {
        if (newPath.at(newPath.length() - 1) != delimiter)
            newPath += delimiter;
        return newPath;
    }
    if (newPath.at(0) != delimiter)
        newPath.insert(0, delimiter);
    return newPath;
}

/* static */ QString UIPathOperations::sanitize(const QString &path)
{
    //return addStartDelimiter(removeTrailingDelimiters(removeMultipleDelimiters(path)));
    QString newPath = addStartDelimiter(removeTrailingDelimiters(removeMultipleDelimiters(path))).replace(dosDelimiter, delimiter);
    return newPath;
}

/* static */ QString UIPathOperations::mergePaths(const QString &path, const QString &baseName)
{
    QString newBase(baseName);
    newBase = newBase.remove(delimiter);

    /* make sure we have one and only one trailing '/': */
    QString newPath(sanitize(path));
    if(newPath.isEmpty())
        newPath = delimiter;
    if(newPath.at(newPath.length() - 1) != delimiter)
        newPath += UIPathOperations::delimiter;
    newPath += newBase;
    return sanitize(newPath);
}

/* static */ QString UIPathOperations::getObjectName(const QString &path)
{
    if (path.length() <= 1)
        return QString(path);

    QString strTemp(sanitize(path));
    if (strTemp.length() < 2)
        return strTemp;
    int lastSlashPosition = strTemp.lastIndexOf(UIPathOperations::delimiter);
    if (lastSlashPosition == -1)
        return QString();
    return strTemp.right(strTemp.length() - lastSlashPosition - 1);
}

/* static */ QString UIPathOperations::getPathExceptObjectName(const QString &path)
{
    if (path.length() <= 1)
        return QString(path);

    QString strTemp(sanitize(path));
    int lastSlashPosition = strTemp.lastIndexOf(UIPathOperations::delimiter);
    if (lastSlashPosition == -1)
        return QString();
    return strTemp.left(lastSlashPosition + 1);
}

/* static */ QString UIPathOperations::constructNewItemPath(const QString &previousPath, const QString &newBaseName)
{
    if (previousPath.length() <= 1)
         return QString(previousPath);
    return sanitize(mergePaths(getPathExceptObjectName(previousPath), newBaseName));
}

/* static */ QStringList UIPathOperations::pathTrail(const QString &path)
{
    QStringList pathList = path.split(UIPathOperations::delimiter, QString::SkipEmptyParts);
    if (!pathList.isEmpty() && doesPathStartWithDriveLetter(pathList[0]))
    {
        pathList[0] = addTrailingDelimiters(pathList[0]);
    }
    return pathList;
}

/* static */ bool UIPathOperations::doesPathStartWithDriveLetter(const QString &path)
{
    if (path.length() < 2)
        return false;
    /* search for ':' with the path: */
    if (!path[0].isLetter())
        return false;
    if (path[1] != ':')
        return false;
    return true;
}


/*********************************************************************************************************************************
*   UIGuestControlFileView implementation.                                                                                       *
*********************************************************************************************************************************/

UIGuestControlFileView::UIGuestControlFileView(QWidget * parent)
    :QTableView(parent)
{
    configure();
}

void UIGuestControlFileView::configure()
{
    setShowGrid(false);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    verticalHeader()->setVisible(false);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    /* Minimize the row height: */
    verticalHeader()->setDefaultSectionSize(verticalHeader()->minimumSectionSize());
    setAlternatingRowColors(true);
}

void UIGuestControlFileView::contextMenuEvent(QContextMenuEvent *pEvent)
{
    bool selectionAvaible = hasSelection();

    QMenu *menu = new QMenu(this);
    if (!menu)
        return;
    /* I know this is ugly: */
    bool isGuestFileTable = qobject_cast<UIGuestFileTable*>(parent());

    QAction *pActionGoUp = menu->addAction(UIVMInformationDialog::tr("Go up"));
    if (pActionGoUp)
    {
        pActionGoUp->setIcon(UIIconPool::iconSet(QString(":/arrow_up_10px_x2.png")));
        connect(pActionGoUp, &QAction::triggered, this, &UIGuestControlFileView::sigGoUp);
    }
    QAction *pActionGoHome = menu->addAction(UIVMInformationDialog::tr("Go home"));
    if (pActionGoHome)
    {
        pActionGoHome->setIcon(UIIconPool::iconSet(QString(":/nw_24px.png")));
        connect(pActionGoHome, &QAction::triggered, this, &UIGuestControlFileView::sigGoHome);
    }

    QAction *pActionRefresh = menu->addAction(UIVMInformationDialog::tr("Refresh"));
    if (pActionRefresh)
    {
        pActionRefresh->setIcon(UIIconPool::iconSet(QString(":/refresh_24.png")));
        connect(pActionRefresh, &QAction::triggered, this, &UIGuestControlFileView::sigRefresh);
    }

    menu->addSeparator();
    QAction *pActionDelete = menu->addAction(UIVMInformationDialog::tr("Delete"));
    if (pActionDelete)
    {
        pActionDelete->setIcon(UIIconPool::iconSet(QString(":/vm_delete_32px.png")));
        pActionDelete->setEnabled(selectionAvaible);
        connect(pActionDelete, &QAction::triggered, this, &UIGuestControlFileView::sigDelete);
    }

    QAction *pActionRename = menu->addAction(UIVMInformationDialog::tr("Rename"));
    if (pActionRename)
    {
        pActionRename->setIcon(UIIconPool::iconSet(QString(":/name_16px_x2.png")));
        pActionRename->setEnabled(selectionAvaible);
        pActionRename->setEnabled(selectionAvaible);
        connect(pActionRename, &QAction::triggered, this, &UIGuestControlFileView::sigRename);
    }

    QAction *pActionCreateNewDirectory = menu->addAction(UIVMInformationDialog::tr("Create New Directory"));
    if (pActionCreateNewDirectory)
    {
        pActionCreateNewDirectory->setIcon(UIIconPool::iconSet(QString(":/sf_add_16px.png")));
        connect(pActionCreateNewDirectory, &QAction::triggered, this, &UIGuestControlFileView::sigCreateNewDirectory);
    }

    QAction *pActionCopy = 0;
    QAction *pActionCut = 0;
    QAction *pActionPaste = 0;

    if (isGuestFileTable)
    {
        pActionCopy = menu->addAction(UIVMInformationDialog::tr("Copy"));
        if (pActionCopy)
        {
            pActionCopy->setIcon(UIIconPool::iconSet(QString(":/fd_copy_32px.png")));
            pActionCopy->setEnabled(selectionAvaible);
            connect(pActionCopy, &QAction::triggered, this, &UIGuestControlFileView::sigCopy);
        }

        pActionCut = menu->addAction(UIVMInformationDialog::tr("Cut"));
        if (pActionCut)
        {
            pActionCut->setIcon(UIIconPool::iconSet(QString(":/fd_move_32px.png")));
            pActionCut->setEnabled(selectionAvaible);
            connect(pActionCut, &QAction::triggered, this, &UIGuestControlFileView::sigCut);
        }

        pActionPaste = menu->addAction(UIVMInformationDialog::tr("Paste"));
        if (pActionPaste)
        {
            pActionPaste->setIcon(UIIconPool::iconSet(QString(":/shared_clipboard_16px.png")));
            connect(pActionPaste, &QAction::triggered, this, &UIGuestControlFileView::sigPaste);
        }
    }

    menu->addSeparator();
    QAction *pActionShowProperties = menu->addAction(UIVMInformationDialog::tr("Properties"));
    if (pActionShowProperties)
    {
        pActionShowProperties->setIcon(UIIconPool::iconSet(QString(":/session_info_32px.png")));
        pActionShowProperties->setEnabled(selectionAvaible);
        connect(pActionShowProperties, &QAction::triggered, this, &UIGuestControlFileView::sigShowProperties);
    }

    menu->exec(pEvent->globalPos());

    if (pActionGoUp)
        disconnect(pActionGoUp, &QAction::triggered, this, &UIGuestControlFileView::sigGoUp);
    if (pActionGoHome)
        disconnect(pActionGoHome, &QAction::triggered, this, &UIGuestControlFileView::sigGoHome);
    if (pActionRefresh)
        disconnect(pActionRefresh, &QAction::triggered, this, &UIGuestControlFileView::sigRefresh);
    if (pActionDelete)
        disconnect(pActionDelete, &QAction::triggered, this, &UIGuestControlFileView::sigDelete);
    if (pActionRename)
        disconnect(pActionRename, &QAction::triggered, this, &UIGuestControlFileView::sigRename);
    if (pActionCreateNewDirectory)
        disconnect(pActionCreateNewDirectory, &QAction::triggered, this, &UIGuestControlFileView::sigCreateNewDirectory);
    if (isGuestFileTable)
    {
        if (pActionCopy)
            disconnect(pActionCopy, &QAction::triggered, this, &UIGuestControlFileView::sigCopy);
        if (pActionCut)
            disconnect(pActionCut, &QAction::triggered, this, &UIGuestControlFileView::sigCut);
        if (pActionPaste)
            disconnect(pActionPaste, &QAction::triggered, this, &UIGuestControlFileView::sigPaste);
    }
    if (pActionShowProperties)
        disconnect(pActionShowProperties, &QAction::triggered, this, &UIGuestControlFileView::sigShowProperties);

    delete menu;
}

bool UIGuestControlFileView::hasSelection() const
{
    QItemSelectionModel *pSelectionModel =  selectionModel();
    if (!pSelectionModel)
        return false;
    return pSelectionModel->hasSelection();
}

void UIGuestControlFileView::selectionChanged(const QItemSelection & selected, const QItemSelection & deselected)
{
    emit sigSelectionChanged(selected, deselected);
    QTableView::selectionChanged(selected, deselected);
}


/*********************************************************************************************************************************
*   UIFileStringInputDialog implementation.                                                                                      *
*********************************************************************************************************************************/

UIStringInputDialog::UIStringInputDialog(QWidget *pParent /* = 0 */, Qt::WindowFlags flags /* = 0 */)
    :QIDialog(pParent, flags)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    m_pLineEdit = new QILineEdit(this);
    layout->addWidget(m_pLineEdit);

    QIDialogButtonBox *pButtonBox =
        new QIDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    layout->addWidget(pButtonBox);
        // {
        //     /* Configure button-box: */
    connect(pButtonBox, &QIDialogButtonBox::accepted, this, &UIStringInputDialog::accept);
    connect(pButtonBox, &QIDialogButtonBox::rejected, this, &UIStringInputDialog::reject);

}

QString UIStringInputDialog::getString() const
{
    if (!m_pLineEdit)
        return QString();
    return m_pLineEdit->text();
}


/*********************************************************************************************************************************
*   UIPropertiesDialog implementation.                                                                                           *
*********************************************************************************************************************************/

UIPropertiesDialog::UIPropertiesDialog(QWidget *pParent, Qt::WindowFlags flags)
    :QIDialog(pParent, flags)
    , m_pMainLayout(new QVBoxLayout)
    , m_pInfoEdit(new QTextEdit)
{
    setLayout(m_pMainLayout);

    if (m_pMainLayout)
        m_pMainLayout->addWidget(m_pInfoEdit);
    if (m_pInfoEdit)
    {
        m_pInfoEdit->setReadOnly(true);
        m_pInfoEdit->setFrameStyle(QFrame::NoFrame);
    }
    QIDialogButtonBox *pButtonBox =
        new QIDialogButtonBox(QDialogButtonBox::Ok, Qt::Horizontal, this);
    m_pMainLayout->addWidget(pButtonBox);
    connect(pButtonBox, &QIDialogButtonBox::accepted, this, &UIStringInputDialog::accept);
}

void UIPropertiesDialog::setPropertyText(const QString &strProperty)
{
    if (!m_pInfoEdit)
        return;
    m_strProperty = strProperty;
    m_pInfoEdit->setHtml(strProperty);
}

void UIPropertiesDialog::addDirectoryStatistics(UIDirectoryStatistics directoryStatistics)
{
    if (!m_pInfoEdit)
        return;
    // QString propertyString = m_pInfoEdit->toHtml();
    // propertyString += "<b>Total Size:</b> " + QString::number(directoryStatistics.m_totalSize) + QString(" bytes");
    // if (directoryStatistics.m_totalSize >= UIGuestControlFileTable::m_iKiloByte)
    //     propertyString += " (" + UIGuestControlFileTable::humanReadableSize(directoryStatistics.m_totalSize) + ")";
    // propertyString += "<br/>";
    // propertyString += "<b>File Count:</b> " + QString::number(directoryStatistics.m_uFileCount);

    // m_pInfoEdit->setHtml(propertyString);

    QString detailsString(m_strProperty);
    detailsString += "<br/>";
    detailsString += "<b>Total Size:</b> " + QString::number(directoryStatistics.m_totalSize) + QString(" bytes");
    if (directoryStatistics.m_totalSize >= UIGuestControlFileTable::m_iKiloByte)
        detailsString += " (" + UIGuestControlFileTable::humanReadableSize(directoryStatistics.m_totalSize) + ")";
    detailsString += "<br/>";

    detailsString += "<b>File Count:</b> " + QString::number(directoryStatistics.m_uFileCount);

    m_pInfoEdit->setHtml(detailsString);

}

/*********************************************************************************************************************************
*   UIDirectoryStatistics implementation.
                                                                                        *
*********************************************************************************************************************************/

UIDirectoryStatistics::UIDirectoryStatistics()
    : m_totalSize(0)
    , m_uFileCount(0)
    , m_uDirectoryCount(0)
    , m_uSymlinkCount(0)
{
}


/*********************************************************************************************************************************
*   UIFileTableItem implementation.                                                                                              *
*********************************************************************************************************************************/

UIFileTableItem::UIFileTableItem(const QList<QVariant> &data,
                                 UIFileTableItem *parent, FileObjectType type)
    : m_itemData(data)
    , m_parentItem(parent)
    , m_bIsOpened(false)
    , m_isTargetADirectory(false)
    , m_type(type)
    , m_isDriveItem(false)
{
}

UIFileTableItem::~UIFileTableItem()
{
    qDeleteAll(m_childItems);
    m_childItems.clear();
}

void UIFileTableItem::appendChild(UIFileTableItem *item)
{
    if (!item)
        return;
    m_childItems.append(item);

    m_childMap.insert(item->name(), item);
}

UIFileTableItem *UIFileTableItem::child(int row) const
{
    return m_childItems.value(row);
}

UIFileTableItem *UIFileTableItem::child(const QString &path) const
{
    if (!m_childMap.contains(path))
        return 0;
    return m_childMap.value(path);
}

int UIFileTableItem::childCount() const
{
    return m_childItems.count();
}

int UIFileTableItem::columnCount() const
{
    return m_itemData.count();
}

QVariant UIFileTableItem::data(int column) const
{
    return m_itemData.value(column);
}

QString UIFileTableItem::name() const
{
    if (m_itemData.isEmpty() || !m_itemData[0].canConvert(QMetaType::QString))
        return QString();
    return m_itemData[0].toString();
}

void UIFileTableItem::setData(const QVariant &data, int index)
{
    if (index >= m_itemData.length())
        return;
    m_itemData[index] = data;
}

UIFileTableItem *UIFileTableItem::parentItem()
{
    return m_parentItem;
}

int UIFileTableItem::row() const
{
    if (m_parentItem)
        return m_parentItem->m_childItems.indexOf(const_cast<UIFileTableItem*>(this));
    return 0;
}

bool UIFileTableItem::isDirectory() const
{
    return m_type == FileObjectType_Directory;
}

bool UIFileTableItem::isSymLink() const
{
    return m_type == FileObjectType_SymLink;
}

bool UIFileTableItem::isFile() const
{
    return m_type == FileObjectType_File;
}

void UIFileTableItem::clearChildren()
{
    qDeleteAll(m_childItems);
    m_childItems.clear();
    m_childMap.clear();
}

bool UIFileTableItem::isOpened() const
{
    return m_bIsOpened;
}

void UIFileTableItem::setIsOpened(bool flag)
{
    m_bIsOpened = flag;
}

const QString  &UIFileTableItem::path() const
{
    return m_strPath;
}

void UIFileTableItem::setPath(const QString &path)
{
    if (path.isNull() || path.isEmpty())
        return;
    m_strPath = path;
    UIPathOperations::removeTrailingDelimiters(m_strPath);
}

bool UIFileTableItem::isUpDirectory() const
{
    if (!isDirectory())
        return false;
    if (data(0) == UIGuestControlFileModel::strUpDirectoryString)
        return true;
    return false;
}

FileObjectType UIFileTableItem::type() const
{
    return m_type;
}

const QString &UIFileTableItem::targetPath() const
{
    return m_strTargetPath;
}

void UIFileTableItem::setTargetPath(const QString &path)
{
    m_strTargetPath = path;
}

bool UIFileTableItem::isTargetADirectory() const
{
    return m_isTargetADirectory;
}

void UIFileTableItem::setIsTargetADirectory(bool flag)
{
    m_isTargetADirectory = flag;
}

void UIFileTableItem::setIsDriveItem(bool flag)
{
    m_isDriveItem = flag;
}

bool UIFileTableItem::isDriveItem() const
{
    return m_isDriveItem;
}


/*********************************************************************************************************************************
*   UIGuestControlFileTable implementation.                                                                                      *
*********************************************************************************************************************************/
const unsigned UIGuestControlFileTable::m_iKiloByte = 1000;
UIGuestControlFileTable::UIGuestControlFileTable(QWidget *pParent /* = 0 */)
    :QIWithRetranslateUI<QWidget>(pParent)
    , m_pRootItem(0)
    , m_pLocationLabel(0)
    , m_pCopy(0)
    , m_pCut(0)
    , m_pPaste(0)
    , m_pPropertiesDialog(0)
    , m_pModel(0)
    , m_pView(0)
    , m_pProxyModel(0)
    , m_pMainLayout(0)
    , m_pLocationComboBox(0)
    , m_pToolBar(0)
    , m_pGoUp(0)
    , m_pGoHome(0)
    , m_pRefresh(0)
    , m_pDelete(0)
    , m_pRename(0)
    , m_pCreateNewDirectory(0)
    , m_pShowProperties(0)
    , m_pSelectAll(0)
    , m_pInvertSelection(0)

{
    prepareObjects();
    prepareActions();
}

UIGuestControlFileTable::~UIGuestControlFileTable()
{
    delete m_pRootItem;
}

void UIGuestControlFileTable::reset()
{
    if (m_pModel)
        m_pModel->beginReset();
    delete m_pRootItem;
    m_pRootItem = 0;
    if (m_pModel)
        m_pModel->endReset();
    if (m_pLocationComboBox)
    {
        disconnect(m_pLocationComboBox, static_cast<void(QComboBox::*)(const QString&)>(&QComboBox::currentIndexChanged),
                   this, &UIGuestControlFileTable::sltLocationComboCurrentChange);
        m_pLocationComboBox->clear();
        connect(m_pLocationComboBox, static_cast<void(QComboBox::*)(const QString&)>(&QComboBox::currentIndexChanged),
                this, &UIGuestControlFileTable::sltLocationComboCurrentChange);
    }
}

void UIGuestControlFileTable::emitLogOutput(const QString& strOutput)
{
    emit sigLogOutput(strOutput);
}

void UIGuestControlFileTable::prepareObjects()
{
    m_pMainLayout = new QGridLayout();
    if (!m_pMainLayout)
        return;
    m_pMainLayout->setSpacing(0);
    m_pMainLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(m_pMainLayout);

    m_pToolBar = new UIToolBar;
    if (m_pToolBar)
    {
        m_pMainLayout->addWidget(m_pToolBar, 0, 0, 1, 5);
    }

    m_pLocationLabel = new QILabel;
    if (m_pLocationLabel)
    {
        m_pMainLayout->addWidget(m_pLocationLabel, 1, 0, 1, 1);
    }

    m_pLocationComboBox = new QComboBox;
    if (m_pLocationComboBox)
    {
        m_pMainLayout->addWidget(m_pLocationComboBox, 1, 1, 1, 4);
        m_pLocationComboBox->setEditable(false);
        connect(m_pLocationComboBox, static_cast<void(QComboBox::*)(const QString&)>(&QComboBox::currentIndexChanged),
                this, &UIGuestControlFileTable::sltLocationComboCurrentChange);
    }


    m_pModel = new UIGuestControlFileModel(this);
    if (!m_pModel)
        return;

    m_pProxyModel = new UIGuestControlFileProxyModel(this);
    if (!m_pProxyModel)
        return;
    m_pProxyModel->setSourceModel(m_pModel);

    m_pView = new UIGuestControlFileView;
    if (m_pView)
    {
        m_pMainLayout->addWidget(m_pView, 2, 0, 5, 5);
        m_pView->setModel(m_pProxyModel);
        //m_pView->setModel(m_pModel);
        m_pView->setItemDelegate(new UIFileDelegate);
        m_pView->setSortingEnabled(true);
        m_pView->sortByColumn(0, Qt::AscendingOrder);

        connect(m_pView, &UIGuestControlFileView::doubleClicked,
                this, &UIGuestControlFileTable::sltItemDoubleClicked);
        connect(m_pView, &UIGuestControlFileView::sigGoUp,
                this, &UIGuestControlFileTable::sltGoUp);
        connect(m_pView, &UIGuestControlFileView::sigGoHome,
                this, &UIGuestControlFileTable::sltGoHome);
        connect(m_pView, &UIGuestControlFileView::sigRefresh,
                this, &UIGuestControlFileTable::sltRefresh);
        connect(m_pView, &UIGuestControlFileView::sigDelete,
                this, &UIGuestControlFileTable::sltDelete);
        connect(m_pView, &UIGuestControlFileView::sigRename,
                this, &UIGuestControlFileTable::sltRename);
        connect(m_pView, &UIGuestControlFileView::sigCreateNewDirectory,
                this, &UIGuestControlFileTable::sltCreateNewDirectory);
        connect(m_pView, &UIGuestControlFileView::sigCopy,
                this, &UIGuestControlFileTable::sltCopy);
        connect(m_pView, &UIGuestControlFileView::sigCut,
                this, &UIGuestControlFileTable::sltCut);
        connect(m_pView, &UIGuestControlFileView::sigPaste,
                this, &UIGuestControlFileTable::sltPaste);
        connect(m_pView, &UIGuestControlFileView::sigShowProperties,
                this, &UIGuestControlFileTable::sltShowProperties);
        connect(m_pView, &UIGuestControlFileView::sigSelectionChanged,
                this, &UIGuestControlFileTable::sltSelectionChanged);

    }
}

void UIGuestControlFileTable::prepareActions()
{
    if (!m_pToolBar)
        return;

    m_pGoUp = new QAction(this);
    if (m_pGoUp)
    {
        connect(m_pGoUp, &QAction::triggered, this, &UIGuestControlFileTable::sltGoUp);
        m_pGoUp->setIcon(UIIconPool::iconSet(QString(":/arrow_up_10px_x2.png")));
        m_pToolBar->addAction(m_pGoUp);
    }

    m_pGoHome = new QAction(this);
    if (m_pGoHome)
    {
        connect(m_pGoHome, &QAction::triggered, this, &UIGuestControlFileTable::sltGoHome);
        m_pGoHome->setIcon(UIIconPool::iconSet(QString(":/nw_24px.png")));
        m_pToolBar->addAction(m_pGoHome);
    }

    m_pRefresh = new QAction(this);
    if (m_pRefresh)
    {
        connect(m_pRefresh, &QAction::triggered, this, &UIGuestControlFileTable::sltRefresh);
        m_pRefresh->setIcon(UIIconPool::iconSet(QString(":/refresh_24.png")));
        m_pToolBar->addAction(m_pRefresh);
    }

    m_pToolBar->addSeparator();

    m_pDelete = new QAction(this);
    if (m_pDelete)
    {
        connect(m_pDelete, &QAction::triggered, this, &UIGuestControlFileTable::sltDelete);
        m_pDelete->setIcon(UIIconPool::iconSet(QString(":/vm_delete_32px.png")));
        m_pToolBar->addAction(m_pDelete);
        m_selectionDependentActions.push_back(m_pDelete);
    }

    m_pRename = new QAction(this);
    if (m_pRename)
    {
        connect(m_pRename, &QAction::triggered, this, &UIGuestControlFileTable::sltRename);
        m_pRename->setIcon(UIIconPool::iconSet(QString(":/name_16px_x2.png")));
        m_pToolBar->addAction(m_pRename);
        m_selectionDependentActions.push_back(m_pRename);
    }

    m_pCreateNewDirectory = new QAction(this);
    if (m_pCreateNewDirectory)
    {
        connect(m_pCreateNewDirectory, &QAction::triggered, this, &UIGuestControlFileTable::sltCreateNewDirectory);
        m_pCreateNewDirectory->setIcon(UIIconPool::iconSet(QString(":/sf_add_16px.png")));
        m_pToolBar->addAction(m_pCreateNewDirectory);
    }

    m_pCopy = new QAction(this);
    if (m_pCopy)
    {
        m_pCopy->setIcon(UIIconPool::iconSet(QString(":/fd_copy_32px.png")));
        m_pToolBar->addAction(m_pCopy);
        connect(m_pCopy, &QAction::triggered, this, &UIGuestControlFileTable::sltCopy);
        m_selectionDependentActions.push_back(m_pCopy);
    }

    m_pCut = new QAction(this);
    if (m_pCut)
    {
        m_pCut->setIcon(UIIconPool::iconSet(QString(":/fd_move_32px.png")));
        m_pToolBar->addAction(m_pCut);
        connect(m_pCut, &QAction::triggered, this, &UIGuestControlFileTable::sltCut);
        m_selectionDependentActions.push_back(m_pCut);
    }

    m_pPaste = new QAction(this);
    if (m_pPaste)
    {
        m_pPaste->setIcon(UIIconPool::iconSet(QString(":/shared_clipboard_16px.png")));
        m_pToolBar->addAction(m_pPaste);
        connect(m_pPaste, &QAction::triggered, this, &UIGuestControlFileTable::sltPaste);
        m_pPaste->setEnabled(false);
    }

    m_pToolBar->addSeparator();

    m_pShowProperties = new QAction(this);
    {
        m_pShowProperties->setIcon(UIIconPool::iconSet(QString(":/session_info_32px.png")));
        m_pToolBar->addAction(m_pShowProperties);
        connect(m_pShowProperties, &QAction::triggered, this, &UIGuestControlFileTable::sltShowProperties);
        m_selectionDependentActions.push_back(m_pShowProperties);
    }

    m_pSelectAll = new QAction(this);
    {
        m_pSelectAll->setIcon(UIIconPool::iconSet(QString(":/session_info_32px.png")));
        m_pToolBar->addAction(m_pSelectAll);
        connect(m_pSelectAll, &QAction::triggered, this, &UIGuestControlFileTable::sltSelectAll);
    }

    m_pInvertSelection = new QAction(this);
    {
        m_pInvertSelection->setIcon(UIIconPool::iconSet(QString(":/session_info_32px.png")));
        m_pToolBar->addAction(m_pInvertSelection);
        connect(m_pInvertSelection, &QAction::triggered, this, &UIGuestControlFileTable::sltInvertSelection);
    }

    disableSelectionDependentActions();
}

void UIGuestControlFileTable::updateCurrentLocationEdit(const QString& strLocation)
{
    if (!m_pLocationComboBox)
        return;
    int itemIndex = m_pLocationComboBox->findText(strLocation,
                                                  Qt::MatchExactly | Qt::MatchCaseSensitive);
    if (itemIndex == -1)
    {
        m_pLocationComboBox->insertItem(m_pLocationComboBox->count(), strLocation);
        itemIndex = m_pLocationComboBox->count() - 1;
    }
    m_pLocationComboBox->setCurrentIndex(itemIndex);
}

void UIGuestControlFileTable::changeLocation(const QModelIndex &index)
{
    if (!index.isValid() || !m_pView)
        return;
    m_pView->setRootIndex(m_pProxyModel->mapFromSource(index));
    m_pView->clearSelection();

    UIFileTableItem *item = static_cast<UIFileTableItem*>(index.internalPointer());
    if (item)
    {
        updateCurrentLocationEdit(item->path());
    }
    /** @todo check if we really need this and if not remove it */
    //m_pModel->signalUpdate();
}

void UIGuestControlFileTable::initializeFileTree()
{
    if (m_pRootItem)
        reset();

    /* Root item: */
    const QString startPath("/");
    QList<QVariant> headData;
    headData << "Name" << "Size" << "Change Time" << "Owner" << "Permissions";
    m_pRootItem = new UIFileTableItem(headData, 0, FileObjectType_Directory);
    QList<QVariant> startDirData;
    startDirData << startPath << 4096 << QDateTime() << "" << "";
    UIFileTableItem* startItem = new UIFileTableItem(startDirData, m_pRootItem, FileObjectType_Directory);

    startItem->setPath(startPath);
    m_pRootItem->appendChild(startItem);
    startItem->setIsOpened(false);
    populateStartDirectory(startItem);

    m_pModel->signalUpdate();
    updateCurrentLocationEdit(startPath);
    m_pView->setRootIndex(m_pProxyModel->mapFromSource(m_pModel->rootIndex()));
}

void UIGuestControlFileTable::populateStartDirectory(UIFileTableItem *startItem)
{
    determineDriveLetters();
    if (m_driveLetterList.isEmpty())
    {
        /* Read the root directory and get the list: */
        readDirectory(startItem->path(), startItem, true);
    }
    else
    {
        for (int i = 0; i < m_driveLetterList.size(); ++i)
        {
            QList<QVariant> data;

            data << m_driveLetterList[i] << 4096 << QDateTime() << "";
            UIFileTableItem* driveItem = new UIFileTableItem(data, startItem, FileObjectType_Directory);
            driveItem->setPath(m_driveLetterList[i]);
            startItem->appendChild(driveItem);
            driveItem->setIsOpened(false);
            driveItem->setIsDriveItem(true);
            startItem->setIsOpened(true);
        }
    }
}

void UIGuestControlFileTable::insertItemsToTree(QMap<QString,UIFileTableItem*> &map,
                                                UIFileTableItem *parent, bool isDirectoryMap, bool isStartDir)
{
    if (parent)

    /* Make sure we have an item representing up directory, and make sure it is not there for the start dir: */
    if (isDirectoryMap)
    {
        if (!map.contains(UIGuestControlFileModel::strUpDirectoryString)  && !isStartDir)
        {
            QList<QVariant> data;
            data << UIGuestControlFileModel::strUpDirectoryString << 4096 << "";
            UIFileTableItem *item = new UIFileTableItem(data, parent, FileObjectType_Directory);
            item->setIsOpened(false);
            map.insert(UIGuestControlFileModel::strUpDirectoryString, item);
        }
        else if (map.contains(UIGuestControlFileModel::strUpDirectoryString)  && isStartDir)
        {
            map.remove(UIGuestControlFileModel::strUpDirectoryString);
        }
    }
    for (QMap<QString,UIFileTableItem*>::const_iterator iterator = map.begin();
        iterator != map.end(); ++iterator)
    {
        if (iterator.key() == "." || iterator.key().isEmpty())
            continue;
        parent->appendChild(iterator.value());
    }
}

void UIGuestControlFileTable::sltItemDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid() ||  !m_pModel || !m_pView)
        return;
    QModelIndex nIndex = m_pProxyModel ? m_pProxyModel->mapToSource(index) : index;
    goIntoDirectory(nIndex);
}

void UIGuestControlFileTable::sltGoUp()
{
    if (!m_pView || !m_pModel)
        return;
    QModelIndex currentRoot = currentRootIndex();

    if (!currentRoot.isValid())
        return;
    if (currentRoot != m_pModel->rootIndex())
    {
        QModelIndex parentIndex = currentRoot.parent();
        if (parentIndex.isValid())
        {
            changeLocation(currentRoot.parent());
            m_pView->selectRow(currentRoot.row());
        }
    }
}

void UIGuestControlFileTable::sltGoHome()
{
    goToHomeDirectory();
}

void UIGuestControlFileTable::sltRefresh()
{
    refresh();
}

void UIGuestControlFileTable::goIntoDirectory(const QModelIndex &itemIndex)
{
    if (!m_pModel)
        return;

    /* Make sure the colum is 0: */
    QModelIndex index = m_pModel->index(itemIndex.row(), 0, itemIndex.parent());
    if (!index.isValid())
        return;

    UIFileTableItem *item = static_cast<UIFileTableItem*>(index.internalPointer());
    if (!item)
        return;

    /* check if we need to go up: */
    if (item->isUpDirectory())
    {
        QModelIndex parentIndex = m_pModel->parent(m_pModel->parent(index));
        if (parentIndex.isValid())
            changeLocation(parentIndex);
        return;
    }

    if (!item->isDirectory())
        return;
    if (!item->isOpened())
       readDirectory(item->path(),item);
    changeLocation(index);
}

void UIGuestControlFileTable::goIntoDirectory(const QStringList &pathTrail)
{
    UIFileTableItem *parent = getStartDirectoryItem();

    for(int i = 0; i < pathTrail.size(); ++i)
    {
        if (!parent)
            return;
        /* Make sure parent is already opened: */
        if (!parent->isOpened())
            readDirectory(parent->path(), parent, parent == getStartDirectoryItem());
        /* search the current path item among the parent's children: */
        UIFileTableItem *item = parent->child(pathTrail.at(i));
        if (!item)
            return;
        parent = item;
    }
    if (!parent)
        return;
    if (!parent->isOpened())
        readDirectory(parent->path(), parent, parent == getStartDirectoryItem());
    goIntoDirectory(parent);
}

void UIGuestControlFileTable::goIntoDirectory(UIFileTableItem *item)
{
    if (!item || !m_pModel)
        return;
    goIntoDirectory(m_pModel->index(item));
}

UIFileTableItem* UIGuestControlFileTable::indexData(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;
    return static_cast<UIFileTableItem*>(index.internalPointer());
}

void UIGuestControlFileTable::refresh()
{
    if (!m_pView || !m_pModel)
        return;
    QModelIndex currentIndex = currentRootIndex();

    UIFileTableItem *treeItem = indexData(currentIndex);
    if (!treeItem)
        return;
    bool isRootDir = (m_pModel->rootIndex() == currentIndex);
    m_pModel->beginReset();
    /* For now we clear the whole subtree (that isrecursively) which is an overkill: */
    treeItem->clearChildren();
    if (isRootDir)
        populateStartDirectory(treeItem);
    else
        readDirectory(treeItem->path(), treeItem, isRootDir);
    m_pModel->endReset();
    m_pView->setRootIndex(m_pProxyModel->mapFromSource(currentIndex));
}

void UIGuestControlFileTable::sltDelete()
{
    if (!m_pView || !m_pModel)
        return;
    QItemSelectionModel *selectionModel =  m_pView->selectionModel();
    if (!selectionModel)
        return;

    QModelIndexList selectedItemIndices = selectionModel->selectedRows();
    for(int i = 0; i < selectedItemIndices.size(); ++i)
    {
        QModelIndex index =
            m_pProxyModel ? m_pProxyModel->mapToSource(selectedItemIndices.at(i)) : selectedItemIndices.at(i);
        deleteByIndex(index);
    }
    /** @todo dont refresh here, just delete the rows and update the table view: */
    refresh();
}

void UIGuestControlFileTable::sltRename()
{
    if (!m_pView)
        return;
    QItemSelectionModel *selectionModel =  m_pView->selectionModel();
    if (!selectionModel)
        return;

    QModelIndexList selectedItemIndices = selectionModel->selectedRows();
    if (selectedItemIndices.size() == 0)
        return;
    QModelIndex modelIndex =
        m_pProxyModel ? m_pProxyModel->mapToSource(selectedItemIndices.at(0)) : selectedItemIndices.at(0);
    UIFileTableItem *item = indexData(modelIndex);
    if (!item || item->isUpDirectory())
        return;
    m_pView->edit(selectedItemIndices.at(0));
}

void UIGuestControlFileTable::sltCreateNewDirectory()
{
    if (!m_pModel || !m_pView)
        return;
    QModelIndex currentIndex = currentRootIndex();
    if (!currentIndex.isValid())
        return;
    UIFileTableItem *item = static_cast<UIFileTableItem*>(currentIndex.internalPointer());
    if (!item)
        return;

    QString newDirectoryName = getNewDirectoryName();
    if (newDirectoryName.isEmpty())
        return;

    if (createDirectory(item->path(), newDirectoryName))
    {
        /** @todo instead of refreshing here (an overkill) just add the
           rows and update the tabel view: */
        sltRefresh();
    }
}

void UIGuestControlFileTable::sltCopy()
{

    m_copyCutBuffer = selectedItemPathList();
    if (!m_copyCutBuffer.isEmpty())
        m_pPaste->setEnabled(true);
    else
        m_pPaste->setEnabled(false);
}

void UIGuestControlFileTable::sltCut()
{
    m_copyCutBuffer = selectedItemPathList();
    if (!m_copyCutBuffer.isEmpty())
        m_pPaste->setEnabled(true);
    else
        m_pPaste->setEnabled(false);
}

void UIGuestControlFileTable::sltPaste()
{
    // paste them
    m_copyCutBuffer.clear();
    m_pPaste->setEnabled(false);
}

void UIGuestControlFileTable::sltShowProperties()
{
    showProperties();
}

void UIGuestControlFileTable::sltSelectionChanged(const QItemSelection & selected, const QItemSelection & deselected)
{
    Q_UNUSED(selected);
    Q_UNUSED(deselected);
    if (m_pView->hasSelection())
        enableSelectionDependentActions();
    else
        disableSelectionDependentActions();
}

void UIGuestControlFileTable::sltLocationComboCurrentChange(const QString &strLocation)
{
    QString comboLocation(UIPathOperations::sanitize(strLocation));
    if (comboLocation == currentDirectoryPath())
        return;
    goIntoDirectory(UIPathOperations::pathTrail(comboLocation));
}

void UIGuestControlFileTable::sltSelectAll()
{
    if (!m_pModel || !m_pView)
        return;
    m_pView->selectAll();
    deSelectUpDirectoryItem();
}

void UIGuestControlFileTable::sltInvertSelection()
{
    setSelectionForAll(QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
    deSelectUpDirectoryItem();
}

void UIGuestControlFileTable::deSelectUpDirectoryItem()
{
    if (!m_pView)
        return;
    QItemSelectionModel *pSelectionModel = m_pView->selectionModel();
    if (!pSelectionModel)
        return;
    QModelIndex currentRoot = currentRootIndex();
    if (!currentRoot.isValid())
        return;

    /* Make sure that "up directory item" (if exists) is deselected: */
    for (int i = 0; i < m_pModel->rowCount(currentRoot); ++i)
    {
        QModelIndex index = m_pModel->index(i, 0, currentRoot);
        if (!index.isValid())
            continue;

        UIFileTableItem *item = static_cast<UIFileTableItem*>(index.internalPointer());
        if (item && item->isUpDirectory())
        {
            QModelIndex indexToDeselect = m_pProxyModel ? m_pProxyModel->mapFromSource(index) : index;
            pSelectionModel->select(indexToDeselect, QItemSelectionModel::Deselect | QItemSelectionModel::Rows);
        }
    }
}

void UIGuestControlFileTable::setSelectionForAll(QItemSelectionModel::SelectionFlags flags)
{
    if (!m_pView)
        return;
    QItemSelectionModel *pSelectionModel = m_pView->selectionModel();
    if (!pSelectionModel)
        return;
    QModelIndex currentRoot = currentRootIndex();
    if (!currentRoot.isValid())
        return;

    for (int i = 0; i < m_pModel->rowCount(currentRoot); ++i)
    {
        QModelIndex index = m_pModel->index(i, 0, currentRoot);
        if (!index.isValid())
            continue;
        QModelIndex indexToSelect = m_pProxyModel ? m_pProxyModel->mapFromSource(index) : index;
        pSelectionModel->select(indexToSelect, flags);
    }
}

void UIGuestControlFileTable::deleteByIndex(const QModelIndex &itemIndex)
{
    UIFileTableItem *treeItem = indexData(itemIndex);
    if (!treeItem)
        return;
    deleteByItem(treeItem);
}

void UIGuestControlFileTable::retranslateUi()
{
    if (m_pGoUp)
    {
        m_pGoUp->setText(UIVMInformationDialog::tr("Move one level up"));
        m_pGoUp->setToolTip(UIVMInformationDialog::tr("Move one level up"));
        m_pGoUp->setStatusTip(UIVMInformationDialog::tr("Move one level up"));
    }

    if (m_pGoHome)
    {
        m_pGoHome->setText(UIVMInformationDialog::tr("Go to home directory"));
        m_pGoHome->setToolTip(UIVMInformationDialog::tr("Go to home directory"));
        m_pGoHome->setStatusTip(UIVMInformationDialog::tr("Go to home directory"));
    }

    if (m_pRename)
    {
        m_pRename->setText(UIVMInformationDialog::tr("Rename the selected item"));
        m_pRename->setToolTip(UIVMInformationDialog::tr("Rename the selected item"));
        m_pRename->setStatusTip(UIVMInformationDialog::tr("Rename the selected item"));
    }

    if (m_pRefresh)
    {
        m_pRefresh->setText(UIVMInformationDialog::tr("Refresh"));
        m_pRefresh->setToolTip(UIVMInformationDialog::tr("Refresh the current directory"));
        m_pRefresh->setStatusTip(UIVMInformationDialog::tr("Refresh the current directory"));
    }
    if (m_pDelete)
    {
        m_pDelete->setText(UIVMInformationDialog::tr("Delete"));
        m_pDelete->setToolTip(UIVMInformationDialog::tr("Delete the selected item(s)"));
        m_pDelete->setStatusTip(UIVMInformationDialog::tr("Delete the selected item(s)"));
    }

    if (m_pCreateNewDirectory)
    {
        m_pCreateNewDirectory->setText(UIVMInformationDialog::tr("Create new directory"));
        m_pCreateNewDirectory->setToolTip(UIVMInformationDialog::tr("Create new directory"));
        m_pCreateNewDirectory->setStatusTip(UIVMInformationDialog::tr("Create new directory"));
    }

    if (m_pCopy)
    {
        m_pCopy->setText(UIVMInformationDialog::tr("Copy the selected item"));
        m_pCopy->setToolTip(UIVMInformationDialog::tr("Copy the selected item(s)"));
        m_pCopy->setStatusTip(UIVMInformationDialog::tr("Copy the selected item(s)"));

    }

    if (m_pCut)
    {
        m_pCut->setText(UIVMInformationDialog::tr("Cut the selected item(s)"));
        m_pCut->setToolTip(UIVMInformationDialog::tr("Cut the selected item(s)"));
        m_pCut->setStatusTip(UIVMInformationDialog::tr("Cut the selected item(s)"));

    }

    if ( m_pPaste)
    {
        m_pPaste->setText(UIVMInformationDialog::tr("Paste the copied item(s)"));
        m_pPaste->setToolTip(UIVMInformationDialog::tr("Paste the copied item(s)"));
        m_pPaste->setStatusTip(UIVMInformationDialog::tr("Paste the copied item(s)"));
    }

    if (m_pShowProperties)
    {
        m_pShowProperties->setText(UIVMInformationDialog::tr("Show the properties of the selected item(s)"));
        m_pShowProperties->setToolTip(UIVMInformationDialog::tr("Show the properties of the selected item(s)"));
        m_pShowProperties->setStatusTip(UIVMInformationDialog::tr("Show the properties of the selected item(s)"));
    }

    if (m_pSelectAll)
    {
        m_pSelectAll->setText(UIVMInformationDialog::tr("Select All"));
        m_pSelectAll->setToolTip(UIVMInformationDialog::tr("Select All"));
        m_pSelectAll->setStatusTip(UIVMInformationDialog::tr("Select All"));
    }

    if (m_pInvertSelection)
    {
        m_pInvertSelection->setText(UIVMInformationDialog::tr("Invert Selection"));
        m_pInvertSelection->setToolTip(UIVMInformationDialog::tr("Invert Selection"));
        m_pInvertSelection->setStatusTip(UIVMInformationDialog::tr("Invert Selection"));
    }
}


void UIGuestControlFileTable::keyPressEvent(QKeyEvent * pEvent)
{
    /* Browse into directory with enter: */
    if (pEvent->key() == Qt::Key_Enter || pEvent->key() == Qt::Key_Return)
    {
        if (m_pView && m_pModel)
        {
            /* Get the selected item. If there are 0 or more than 1 selection do nothing: */
            QItemSelectionModel *selectionModel =  m_pView->selectionModel();
            if (selectionModel)
            {
                QModelIndexList selectedItemIndices = selectionModel->selectedRows();
                if (selectedItemIndices.size() == 1)
                    goIntoDirectory(selectedItemIndices.at(0));
            }
        }
    }
    else if (pEvent->key() == Qt::Key_Delete)
    {
        sltDelete();
    }
    else if (pEvent->key() == Qt::Key_Backspace)
    {
        sltGoUp();
    }

    QWidget::keyPressEvent(pEvent);
}

UIFileTableItem *UIGuestControlFileTable::getStartDirectoryItem()
{
    if (!m_pRootItem)
        return 0;
    if (m_pRootItem->childCount() <= 0)
        return 0;
    return m_pRootItem->child(0);
}


QString UIGuestControlFileTable::getNewDirectoryName()
{
    UIStringInputDialog *dialog = new UIStringInputDialog();
    if (dialog->execute())
    {
        QString strDialog = dialog->getString();
        delete dialog;
        return strDialog;
    }
    delete dialog;
    return QString();
}

QString UIGuestControlFileTable::currentDirectoryPath() const
{
    if (!m_pView)
        return QString();
    QModelIndex currentRoot = m_pView->rootIndex();
    if (!currentRoot.isValid())
        return QString();
    UIFileTableItem *item = static_cast<UIFileTableItem*>(currentRoot.internalPointer());
    if (!item)
        return QString();
    /* be paranoid: */
    if (!item->isDirectory())
        return QString();
    return item->path();
}

QStringList UIGuestControlFileTable::selectedItemPathList()
{
    QItemSelectionModel *selectionModel =  m_pView->selectionModel();
    if (!selectionModel)
        return QStringList();

    QStringList pathList;
    QModelIndexList selectedItemIndices = selectionModel->selectedRows();
    for(int i = 0; i < selectedItemIndices.size(); ++i)
    {
        QModelIndex index =
            m_pProxyModel ? m_pProxyModel->mapToSource(selectedItemIndices.at(i)) : selectedItemIndices.at(i);
        UIFileTableItem *item = static_cast<UIFileTableItem*>(index.internalPointer());
        if (!item)
            continue;
        pathList.push_back(item->path());
    }
    return pathList;
}

CGuestFsObjInfo UIGuestControlFileTable::guestFsObjectInfo(const QString& path, CGuestSession &comGuestSession) const
{
    if (comGuestSession.isNull())
        return CGuestFsObjInfo();
    CGuestFsObjInfo comFsObjInfo = comGuestSession.FsObjQueryInfo(path, true /*aFollowSymlinks*/);
    if (!comFsObjInfo.isOk())
        return CGuestFsObjInfo();
    return comFsObjInfo;
}

void UIGuestControlFileTable::enableSelectionDependentActions()
{
    for (int i = 0; i < m_selectionDependentActions.size(); ++i)
    {
        if (m_selectionDependentActions[i])
            m_selectionDependentActions[i]->setEnabled(true);
    }
}

void UIGuestControlFileTable::disableSelectionDependentActions()
{
    /* Disable all the action that operate on some selection: */
    for (int i = 0; i < m_selectionDependentActions.size(); ++i)
    {
        if (m_selectionDependentActions[i])
            m_selectionDependentActions[i]->setEnabled(false);
    }
}

QString UIGuestControlFileTable::fileTypeString(FileObjectType type)
{
    QString strType("Unknown");
    switch(type)
    {
        case FileObjectType_File:
            strType = "File";
            break;
        case FileObjectType_Directory:
            strType = "Directory";
            break;
        case FileObjectType_SymLink:
            strType = "Symbolic Link";
            break;
        case FileObjectType_Other:
            strType = "Other";
            break;

        case FileObjectType_Unknown:
        default:
            break;
    }
    return strType;
}

/* static */ QString UIGuestControlFileTable::humanReadableSize(ULONG64 size)
{
    int i = 0;
    double dSize = size;
    const char* units[] = {" B", " kB", " MB", " GB", " TB", " PB", " EB", " ZB", " YB"};
    while (size > m_iKiloByte) {
        size /= m_iKiloByte;
        dSize /= (double) m_iKiloByte;
        i++;
    }
    if (i > 8)
        return QString();

    QString strResult(QString::number(dSize, 'f', 2));
    strResult += units[i];
    return strResult;
}

void UIGuestControlFileTable::sltReceiveDirectoryStatistics(UIDirectoryStatistics statistics)
{
    if (!m_pPropertiesDialog)
        return;
    m_pPropertiesDialog->addDirectoryStatistics(statistics);
}

QModelIndex UIGuestControlFileTable::currentRootIndex() const
{
    if (!m_pView)
        return QModelIndex();
    if (!m_pProxyModel)
        return m_pView->rootIndex();
    return m_pProxyModel->mapToSource(m_pView->rootIndex());
}

#include "UIGuestControlFileTable.moc"
