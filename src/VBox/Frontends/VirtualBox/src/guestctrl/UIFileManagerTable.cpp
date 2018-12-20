/* $Id$ */
/** @file
 * VBox Qt GUI - UIFileManagerTable class implementation.
 */

/*
 * Copyright (C) 2016-2018 Oracle Corporation
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
# include <QCheckBox>
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
# include "VBoxGlobal.h"
# include "UIActionPool.h"
# include "UIErrorString.h"
# include "UIFileManagerGuestTable.h"
# include "UIIconPool.h"
# include "UIFileManagerTable.h"
# include "UIFileManager.h"
# include "UICustomFileSystemModel.h"
# include "UIToolBar.h"

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

    void sigSelectionChanged(const QItemSelection & selected, const QItemSelection & deselected);

public:

    UIGuestControlFileView(QWidget * parent = 0);
    bool hasSelection() const;

protected:

    virtual void selectionChanged(const QItemSelection & selected, const QItemSelection & deselected) /*override */;

private:

    void configure();
    QWidget *m_pParent;
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
*   UStringInputDialog definition.                                                                                          *
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
*   UIFileDeleteConfirmationDialog definition.                                                                                   *
*********************************************************************************************************************************/

/** A QIDialog child including a line edit whose text exposed when the dialog is accepted */
class UIFileDeleteConfirmationDialog : public QIDialog
{

    Q_OBJECT;

public:

    UIFileDeleteConfirmationDialog(QWidget *pParent = 0, Qt::WindowFlags flags = 0);
    /** Returns whether m_pAskNextTimeCheckBox is checked or not. */
    bool askDeleteConfirmationNextTime() const;

private:

    QCheckBox *m_pAskNextTimeCheckBox;
    QILabel   *m_pQuestionLabel;

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

UIGuestControlFileView::UIGuestControlFileView(QWidget *parent)
    :QTableView(parent)
    , m_pParent(parent)
{
    configure();
}

void UIGuestControlFileView::configure()
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    setShowGrid(false);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    verticalHeader()->setVisible(false);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    /* Minimize the row height: */
    verticalHeader()->setDefaultSectionSize(verticalHeader()->minimumSectionSize());
    setAlternatingRowColors(true);
    installEventFilter(m_pParent);
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
    // if (directoryStatistics.m_totalSize >= UIFileManagerTable::m_iKiloByte)
    //     propertyString += " (" + UIFileManagerTable::humanReadableSize(directoryStatistics.m_totalSize) + ")";
    // propertyString += "<br/>";
    // propertyString += "<b>File Count:</b> " + QString::number(directoryStatistics.m_uFileCount);

    // m_pInfoEdit->setHtml(propertyString);

    QString detailsString(m_strProperty);
    detailsString += "<br/>";
    detailsString += "<b>" + UIFileManager::tr("Total Size") + "</b> " +
        QString::number(directoryStatistics.m_totalSize) + UIFileManager::tr(" bytes");
    if (directoryStatistics.m_totalSize >= UIFileManagerTable::m_iKiloByte)
        detailsString += " (" + UIFileManagerTable::humanReadableSize(directoryStatistics.m_totalSize) + ")";
    detailsString += "<br/>";

    detailsString += "<b>" + UIFileManager::tr("File Count") + ":</b> " +
        QString::number(directoryStatistics.m_uFileCount);

    m_pInfoEdit->setHtml(detailsString);
}


/*********************************************************************************************************************************
*   UIDirectoryStatistics implementation.                                                                                        *
*********************************************************************************************************************************/

UIDirectoryStatistics::UIDirectoryStatistics()
    : m_totalSize(0)
    , m_uFileCount(0)
    , m_uDirectoryCount(0)
    , m_uSymlinkCount(0)
{
}

/*********************************************************************************************************************************
+*   UIFileDeleteConfirmationDialog implementation.                                                                                *
+*********************************************************************************************************************************/

UIFileDeleteConfirmationDialog::UIFileDeleteConfirmationDialog(QWidget *pParent /* = 0 */, Qt::WindowFlags flags /* = 0 */)
    :QIDialog(pParent, flags)
    , m_pAskNextTimeCheckBox(0)
    , m_pQuestionLabel(0)
{
    QVBoxLayout *pLayout = new QVBoxLayout(this);

    m_pQuestionLabel = new QILabel;
    if (m_pQuestionLabel)
    {
        pLayout->addWidget(m_pQuestionLabel);
        m_pQuestionLabel->setText(UIFileManager::tr("Delete the selected file(s) and/or folder(s)"));
    }

    QIDialogButtonBox *pButtonBox =
        new QIDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    if (pButtonBox)
    {
        pLayout->addWidget(pButtonBox, 0, Qt::AlignCenter);
        connect(pButtonBox, &QIDialogButtonBox::accepted, this, &UIStringInputDialog::accept);
        connect(pButtonBox, &QIDialogButtonBox::rejected, this, &UIStringInputDialog::reject);
    }

    m_pAskNextTimeCheckBox = new QCheckBox;

    if (m_pAskNextTimeCheckBox)
    {
        UIFileManagerOptions *pFileManagerOptions = UIFileManagerOptions::instance();
        if (pFileManagerOptions)
            m_pAskNextTimeCheckBox->setChecked(pFileManagerOptions->bAskDeleteConfirmation);

        pLayout->addWidget(m_pAskNextTimeCheckBox);
        m_pAskNextTimeCheckBox->setText(UIFileManager::tr("Ask for this confirmation next time"));
        m_pAskNextTimeCheckBox->setToolTip(UIFileManager::tr("Delete confirmation can be "
                                                                         "disabled/enabled also from the Options panel."));
    }
}

bool UIFileDeleteConfirmationDialog::askDeleteConfirmationNextTime() const
{
    if (!m_pAskNextTimeCheckBox)
        return true;
    return m_pAskNextTimeCheckBox->isChecked();
}


/*********************************************************************************************************************************
*   UIFileManagerTable implementation.                                                                                      *
*********************************************************************************************************************************/
const unsigned UIFileManagerTable::m_iKiloByte = 1024; /**< Our kilo bytes are a power of two! (bird) */
UIFileManagerTable::UIFileManagerTable(UIActionPool *pActionPool, QWidget *pParent /* = 0 */)
    :QIWithRetranslateUI<QWidget>(pParent)
    , m_eFileOperationType(FileOperationType_None)
    , m_pLocationLabel(0)
    , m_pPropertiesDialog(0)
    , m_pActionPool(pActionPool)
    , m_pToolBar(0)
    , m_pModel(0)
    , m_pView(0)
    , m_pProxyModel(0)
    , m_pMainLayout(0)
    , m_pLocationComboBox(0)
    , m_pWarningLabel(0)
{
    prepareObjects();
}

UIFileManagerTable::~UIFileManagerTable()
{
}

void UIFileManagerTable::reset()
{
    if (m_pModel)
        m_pModel->reset();

    if (m_pLocationComboBox)
    {
        disconnect(m_pLocationComboBox, static_cast<void(QComboBox::*)(const QString&)>(&QComboBox::currentIndexChanged),
                   this, &UIFileManagerTable::sltLocationComboCurrentChange);
        m_pLocationComboBox->clear();
        connect(m_pLocationComboBox, static_cast<void(QComboBox::*)(const QString&)>(&QComboBox::currentIndexChanged),
                this, &UIFileManagerTable::sltLocationComboCurrentChange);
    }
}

void UIFileManagerTable::emitLogOutput(const QString& strOutput, FileManagerLogType eLogType)
{
    emit sigLogOutput(strOutput, eLogType);
}

void UIFileManagerTable::prepareObjects()
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
                this, &UIFileManagerTable::sltLocationComboCurrentChange);
    }


    m_pModel = new UICustomFileSystemModel(this);
    if (!m_pModel)
        return;

    m_pProxyModel = new UICustomFileSystemProxyModel(this);
    if (!m_pProxyModel)
        return;
    m_pProxyModel->setSourceModel(m_pModel);

    m_pView = new UIGuestControlFileView(this);
    if (m_pView)
    {
        m_pMainLayout->addWidget(m_pView, 2, 0, 5, 5);

        QHeaderView *pHorizontalHeader = m_pView->horizontalHeader();
        if (pHorizontalHeader)
            pHorizontalHeader->setHighlightSections(false);

        m_pView->setModel(m_pProxyModel);
        m_pView->setItemDelegate(new UIFileDelegate);
        m_pView->setSortingEnabled(true);
        m_pView->sortByColumn(0, Qt::AscendingOrder);

        connect(m_pView, &UIGuestControlFileView::doubleClicked,
                this, &UIFileManagerTable::sltItemDoubleClicked);
        connect(m_pView, &UIGuestControlFileView::clicked,
                this, &UIFileManagerTable::sltItemClicked);
        connect(m_pView, &UIGuestControlFileView::sigSelectionChanged,
                this, &UIFileManagerTable::sltSelectionChanged);
        connect(m_pView, &UIGuestControlFileView::customContextMenuRequested,
                this, &UIFileManagerTable::sltCreateFileViewContextMenu);

    }
    m_pWarningLabel = new QILabel(this);
    if (m_pWarningLabel)
    {
        m_pMainLayout->addWidget(m_pWarningLabel, 2, 0, 5, 5);
        QFont labelFont = m_pWarningLabel->font();
        float fSizeMultiplier = 2.5;
        if (labelFont.pointSize() != -1)
            labelFont.setPointSize(fSizeMultiplier * labelFont.pointSize());
        else
            labelFont.setPixelSize(fSizeMultiplier * labelFont.pixelSize());
        labelFont.setBold(true);
        m_pWarningLabel->setFont(labelFont);
        m_pWarningLabel->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
        m_pWarningLabel->setWordWrap(true);
    }
    m_pWarningLabel->setVisible(!isEnabled());
    m_pView->setVisible(isEnabled());

    m_pSearchLineEdit = new QILineEdit;
    if (m_pSearchLineEdit)
    {
        m_pMainLayout->addWidget(m_pSearchLineEdit, 8, 0, 1, 5);
        m_pSearchLineEdit->hide();
        m_pSearchLineEdit->setClearButtonEnabled(true);
        connect(m_pSearchLineEdit, &QLineEdit::textChanged,
                this, &UIFileManagerTable::sltSearchTextChanged);
    }
    optionsUpdated();
}

void UIFileManagerTable::updateCurrentLocationEdit(const QString& strLocation)
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

void UIFileManagerTable::changeLocation(const QModelIndex &index)
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

void UIFileManagerTable::initializeFileTree()
{
    if (m_pModel)
        m_pModel->reset();
    if (!rootItem())
        return;

    const QString startPath("/");
    UIFileTableItem* startItem = new UIFileTableItem(createTreeItemData(startPath, 4096, QDateTime(),
                                                                        "" /* owner */, "" /* permissions */),
                                                     rootItem(), KFsObjType_Directory);
    startItem->setPath(startPath);
    rootItem()->appendChild(startItem);
    startItem->setIsOpened(false);
    populateStartDirectory(startItem);

    m_pModel->signalUpdate();
    updateCurrentLocationEdit(startPath);
    m_pView->setRootIndex(m_pProxyModel->mapFromSource(m_pModel->rootIndex()));
}

void UIFileManagerTable::populateStartDirectory(UIFileTableItem *startItem)
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
            UIFileTableItem* driveItem = new UIFileTableItem(createTreeItemData(m_driveLetterList[i], 4096,
                                                                                QDateTime(), QString(), QString()),
                                                             startItem, KFsObjType_Directory);
            driveItem->setPath(m_driveLetterList[i]);
            startItem->appendChild(driveItem);
            driveItem->setIsOpened(false);
            driveItem->setIsDriveItem(true);
            startItem->setIsOpened(true);
        }
    }
}

void UIFileManagerTable::insertItemsToTree(QMap<QString,UIFileTableItem*> &map,
                                                UIFileTableItem *parent, bool isDirectoryMap, bool isStartDir)
{
    if (parent)

    /* Make sure we have an item representing up directory, and make sure it is not there for the start dir: */
    if (isDirectoryMap)
    {
        if (!map.contains(UICustomFileSystemModel::strUpDirectoryString)  && !isStartDir)
        {
            QVector<QVariant> data;
            UIFileTableItem *item = new UIFileTableItem(createTreeItemData(UICustomFileSystemModel::strUpDirectoryString, 4096,
                                                                           QDateTime(), QString(), QString())
                                                        , parent, KFsObjType_Directory);
            item->setIsOpened(false);
            map.insert(UICustomFileSystemModel::strUpDirectoryString, item);
        }
        else if (map.contains(UICustomFileSystemModel::strUpDirectoryString)  && isStartDir)
        {
            map.remove(UICustomFileSystemModel::strUpDirectoryString);
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

void UIFileManagerTable::sltItemDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid() ||  !m_pModel || !m_pView)
        return;
    QModelIndex nIndex = m_pProxyModel ? m_pProxyModel->mapToSource(index) : index;
    goIntoDirectory(nIndex);
}

void UIFileManagerTable::sltItemClicked(const QModelIndex &index)
{
    Q_UNUSED(index);
    disableSelectionSearch();
}

void UIFileManagerTable::sltGoUp()
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

void UIFileManagerTable::sltGoHome()
{
    goToHomeDirectory();
}

void UIFileManagerTable::sltRefresh()
{
    refresh();
}

void UIFileManagerTable::goIntoDirectory(const QModelIndex &itemIndex)
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

    if (item->isDirectory() || item->isSymLinkToADirectory())
    {
        if (!item->isOpened())
            readDirectory(item->path(),item);
        changeLocation(index);
    }
}

void UIFileManagerTable::goIntoDirectory(const QStringList &pathTrail)
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

void UIFileManagerTable::goIntoDirectory(UIFileTableItem *item)
{
    if (!item || !m_pModel)
        return;
    goIntoDirectory(m_pModel->index(item));
}

UIFileTableItem* UIFileManagerTable::indexData(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;
    return static_cast<UIFileTableItem*>(index.internalPointer());
}

void UIFileManagerTable::refresh()
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
    setSelectionDependentActionsEnabled(m_pView->hasSelection());
}

void UIFileManagerTable::relist()
{
    if (!m_pProxyModel)
        return;
    m_pProxyModel->invalidate();
}

void UIFileManagerTable::sltDelete()
{
    if (!checkIfDeleteOK())
        return;

    if (!m_pView || !m_pModel)
        return;

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

void UIFileManagerTable::sltRename()
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

void UIFileManagerTable::sltCreateNewDirectory()
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

void UIFileManagerTable::sltCopy()
{
    m_copyCutBuffer = selectedItemPathList();
    m_eFileOperationType = FileOperationType_Copy;
    setPasteActionEnabled(true);
}

void UIFileManagerTable::sltCut()
{
    m_copyCutBuffer = selectedItemPathList();
    m_eFileOperationType = FileOperationType_Cut;
    setPasteActionEnabled(true);
}

void UIFileManagerTable::sltPaste()
{
    m_copyCutBuffer.clear();

    m_eFileOperationType = FileOperationType_None;
    setPasteActionEnabled(false);
}

void UIFileManagerTable::sltShowProperties()
{
    showProperties();
}

void UIFileManagerTable::sltSelectionChanged(const QItemSelection & selected, const QItemSelection & deselected)
{
    Q_UNUSED(selected);
    Q_UNUSED(deselected);
    setSelectionDependentActionsEnabled(m_pView->hasSelection());
}

void UIFileManagerTable::sltLocationComboCurrentChange(const QString &strLocation)
{
    QString comboLocation(UIPathOperations::sanitize(strLocation));
    if (comboLocation == currentDirectoryPath())
        return;
    goIntoDirectory(UIPathOperations::pathTrail(comboLocation));
}

void UIFileManagerTable::sltSelectAll()
{
    if (!m_pModel || !m_pView)
        return;
    m_pView->selectAll();
    deSelectUpDirectoryItem();
}

void UIFileManagerTable::sltInvertSelection()
{
    setSelectionForAll(QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
    deSelectUpDirectoryItem();
}

void UIFileManagerTable::sltSearchTextChanged(const QString &strText)
{
    performSelectionSearch(strText);
}

void UIFileManagerTable::sltCreateFileViewContextMenu(const QPoint &point)
{
    QWidget *pSender = qobject_cast<QWidget*>(sender());
    if (!pSender)
        return;
    createFileViewContextMenu(pSender, point);
}

void UIFileManagerTable::deSelectUpDirectoryItem()
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

void UIFileManagerTable::setSelectionForAll(QItemSelectionModel::SelectionFlags flags)
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

void UIFileManagerTable::setSelection(const QModelIndex &indexInProxyModel)
{
    if (!m_pView)
        return;
    QItemSelectionModel *selectionModel =  m_pView->selectionModel();
    if (!selectionModel)
        return;
    selectionModel->select(indexInProxyModel, QItemSelectionModel::Current | QItemSelectionModel::Rows | QItemSelectionModel::Select);
    m_pView->scrollTo(indexInProxyModel, QAbstractItemView::EnsureVisible);
}

void UIFileManagerTable::deleteByIndex(const QModelIndex &itemIndex)
{
    UIFileTableItem *treeItem = indexData(itemIndex);
    if (!treeItem)
        return;
    deleteByItem(treeItem);
}

void UIFileManagerTable::retranslateUi()
{
    UIFileTableItem *pRootItem = rootItem();
    if (pRootItem)
    {
        pRootItem->setData(UIFileManager::tr("Name"), UICustomFileSystemModelColumn_Name);
        pRootItem->setData(UIFileManager::tr("Size"), UICustomFileSystemModelColumn_Size);
        pRootItem->setData(UIFileManager::tr("Change Time"), UICustomFileSystemModelColumn_ChangeTime);
        pRootItem->setData(UIFileManager::tr("Owner"), UICustomFileSystemModelColumn_Owner);
        pRootItem->setData(UIFileManager::tr("Permissions"), UICustomFileSystemModelColumn_Permissions);
    }
    if (m_pWarningLabel)
        m_pWarningLabel->setText(UIFileManager::tr("No Guest Session"));
}

bool UIFileManagerTable::eventFilter(QObject *pObject, QEvent *pEvent) /* override */
{
    Q_UNUSED(pObject);
    if (pEvent->type() == QEvent::KeyPress)
    {
        QKeyEvent *pKeyEvent = dynamic_cast<QKeyEvent*>(pEvent);
        if (pKeyEvent)
        {
            if (pKeyEvent->key() == Qt::Key_Enter || pKeyEvent->key() == Qt::Key_Return)
            {
                if (m_pView && m_pModel)
                {
                    /* Get the selected item. If there are 0 or more than 1 selection do nothing: */
                    QItemSelectionModel *selectionModel =  m_pView->selectionModel();
                    if (selectionModel)
                    {
                        QModelIndexList selectedItemIndices = selectionModel->selectedRows();
                        if (selectedItemIndices.size() == 1 && m_pModel)
                            goIntoDirectory( m_pProxyModel->mapToSource(selectedItemIndices.at(0)));
                    }
                }
                return true;
            }
            else if (pKeyEvent->key() == Qt::Key_Delete)
            {
                sltDelete();
                return true;
            }
            else if (pKeyEvent->key() == Qt::Key_Backspace)
            {
                sltGoUp();
                return true;
            }
            else if (pKeyEvent->text().length() == 1 &&
                     (pKeyEvent->text().at(0).isDigit() ||
                      pKeyEvent->text().at(0).isLetter()))
            {
                if (m_pSearchLineEdit)
                {
                    m_pSearchLineEdit->show();
                    QString strText = m_pSearchLineEdit->text();
                    strText.append(pKeyEvent->text());
                    m_pSearchLineEdit->setText(strText);
                }
            }
            else if (pKeyEvent->key() == Qt::Key_Tab)
            {
                return true;
            }
        }/* if (pKeyEvent) */
    }/* if (pEvent->type() == QEvent::KeyPress) */
    else if (pEvent->type() == QEvent::FocusOut)
    {
        disableSelectionSearch();
    }
    /* Dont hold up the @pEvent but rather send it to the target @pObject: */
    return false;
}

UIFileTableItem *UIFileManagerTable::getStartDirectoryItem()
{
    UIFileTableItem* pRootItem = rootItem();
    if (!pRootItem)
        return 0;
    if (pRootItem->childCount() <= 0)
        return 0;
    return pRootItem->child(0);
}


QString UIFileManagerTable::getNewDirectoryName()
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

QString UIFileManagerTable::currentDirectoryPath() const
{
    if (!m_pView)
        return QString();
    QModelIndex currentRoot = currentRootIndex();
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

QStringList UIFileManagerTable::selectedItemPathList()
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

CGuestFsObjInfo UIFileManagerTable::guestFsObjectInfo(const QString& path, CGuestSession &comGuestSession) const
{
    if (comGuestSession.isNull())
        return CGuestFsObjInfo();
    CGuestFsObjInfo comFsObjInfo = comGuestSession.FsObjQueryInfo(path, true /*aFollowSymlinks*/);
    if (!comFsObjInfo.isOk())
        return CGuestFsObjInfo();
    return comFsObjInfo;
}

void UIFileManagerTable::setSelectionDependentActionsEnabled(bool fIsEnabled)
{
    foreach (QAction *pAction, m_selectionDependentActions)
    {
        pAction->setEnabled(fIsEnabled);
    }
}

QVector<QVariant> UIFileManagerTable::createTreeItemData(const QString &strName, ULONG64 size, const QDateTime &changeTime,
                                                            const QString &strOwner, const QString &strPermissions)
{
    QVector<QVariant> data;
    data.resize(UICustomFileSystemModelColumn_Max);
    data[UICustomFileSystemModelColumn_Name]        = strName;
    data[UICustomFileSystemModelColumn_Size]        = (qulonglong)size;
    data[UICustomFileSystemModelColumn_ChangeTime]  = changeTime;
    data[UICustomFileSystemModelColumn_Owner]       = strOwner;
    data[UICustomFileSystemModelColumn_Permissions] = strPermissions;
    return data;
}

UIFileTableItem* UIFileManagerTable::rootItem()
{
    if (!m_pModel)
        return 0;
    return m_pModel->rootItem();
}

bool UIFileManagerTable::event(QEvent *pEvent)
{
    if (pEvent->type() == QEvent::EnabledChange)
    {
        m_pWarningLabel->setVisible(!isEnabled());
        m_pView->setVisible(isEnabled());
        retranslateUi();
    }
    return QIWithRetranslateUI<QWidget>::event(pEvent);
}

QString UIFileManagerTable::fileTypeString(KFsObjType type)
{
    QString strType = UIFileManager::tr("Unknown");
    switch (type)
    {
        case KFsObjType_File:
            strType = UIFileManager::tr("File");
            break;
        case KFsObjType_Directory:
            strType = UIFileManager::tr("Directory");
            break;
        case KFsObjType_Symlink:
            strType = UIFileManager::tr("Symbolic Link");
            break;
        case KFsObjType_Unknown:
        default:
            strType = UIFileManager::tr("Unknown");
            break;
    }
    return strType;
}

/* static */ QString UIFileManagerTable::humanReadableSize(ULONG64 size)
{
    return vboxGlobal().formatSize(size);
}

void UIFileManagerTable::optionsUpdated()
{
    UIFileManagerOptions *pOptions = UIFileManagerOptions::instance();
    if (pOptions)
    {
        if (m_pProxyModel)
            m_pProxyModel->setListDirectoriesOnTop(pOptions->bListDirectoriesOnTop);
        if (m_pModel)
            m_pModel->setShowHumanReadableSizes(pOptions->bShowHumanReadableSizes);
    }
    relist();
}


void UIFileManagerTable::sltReceiveDirectoryStatistics(UIDirectoryStatistics statistics)
{
    if (!m_pPropertiesDialog)
        return;
    m_pPropertiesDialog->addDirectoryStatistics(statistics);
}

QModelIndex UIFileManagerTable::currentRootIndex() const
{
    if (!m_pView)
        return QModelIndex();
    if (!m_pProxyModel)
        return m_pView->rootIndex();
    return m_pProxyModel->mapToSource(m_pView->rootIndex());
}

void UIFileManagerTable::performSelectionSearch(const QString &strSearchText)
{
    if (!m_pProxyModel | !m_pView || strSearchText.isEmpty())
        return;

    int rowCount = m_pProxyModel->rowCount(m_pView->rootIndex());
    UIFileTableItem *pFoundItem = 0;
    QModelIndex index;
    for (int i = 0; i < rowCount && !pFoundItem; ++i)
    {
        index = m_pProxyModel->index(i, 0, m_pView->rootIndex());
        if (!index.isValid())
            continue;
        pFoundItem = static_cast<UIFileTableItem*>(m_pProxyModel->mapToSource(index).internalPointer());
        if (!pFoundItem)
            continue;
        const QString &strName = pFoundItem->name();
        if (!strName.startsWith(m_pSearchLineEdit->text(), Qt::CaseInsensitive))
            pFoundItem = 0;
    }
    if (pFoundItem)
    {
        /* Deselect anything that is already selected: */
        m_pView->clearSelection();
        setSelection(index);
    }
}

void UIFileManagerTable::disableSelectionSearch()
{
    if (!m_pSearchLineEdit)
        return;
    m_pSearchLineEdit->blockSignals(true);
    m_pSearchLineEdit->clear();
    m_pSearchLineEdit->hide();
    m_pSearchLineEdit->blockSignals(false);
}

bool UIFileManagerTable::checkIfDeleteOK()
{
    UIFileManagerOptions *pFileManagerOptions = UIFileManagerOptions::instance();
    if (!pFileManagerOptions)
        return true;
    if (!pFileManagerOptions->bAskDeleteConfirmation)
        return true;
    UIFileDeleteConfirmationDialog *pDialog =
        new UIFileDeleteConfirmationDialog(this);

    bool fContinueWithDelete = (pDialog->execute() == QDialog::Accepted);
    bool bAskNextTime = pDialog->askDeleteConfirmationNextTime();

    /* Update the file manager options only if it is necessary: */
    if (pFileManagerOptions->bAskDeleteConfirmation != bAskNextTime)
    {
        pFileManagerOptions->bAskDeleteConfirmation = bAskNextTime;
        /* Notify file manager options panel so that the check box there is updated: */
        emit sigDeleteConfirmationOptionChanged();
    }

    delete pDialog;

    return fContinueWithDelete;

}

#include "UIFileManagerTable.moc"
