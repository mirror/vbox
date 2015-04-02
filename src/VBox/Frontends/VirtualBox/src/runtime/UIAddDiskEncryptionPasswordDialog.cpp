/* $Id$ */
/** @file
 * VBox Qt GUI - UIAddDiskEncryptionPasswordDialog class implementation.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
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
# include <QVBoxLayout>
# include <QLabel>
# include <QLineEdit>
# include <QTableView>
# include <QHeaderView>
# include <QPushButton>
# include <QItemEditorFactory>
# include <QAbstractTableModel>
# include <QStandardItemEditorCreator>

/* GUI includes: */
# include "UIMedium.h"
# include "UIIconPool.h"
# include "VBoxGlobal.h"
# include "QIDialogButtonBox.h"
# include "QIWithRetranslateUI.h"
# include "QIStyledItemDelegate.h"
# include "UIAddDiskEncryptionPasswordDialog.h"

/* Other VBox includes: */
# include <iprt/assert.h>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/** UIEncryptionDataTable field indexes. */
enum UIEncryptionDataTableSection
{
    UIEncryptionDataTableSection_Status,
    UIEncryptionDataTableSection_Id,
    UIEncryptionDataTableSection_Password,
    UIEncryptionDataTableSection_Max
};

/** QLineEdit reimplementation used as
  * the embedded password editor for the UIEncryptionDataTable. */
class UIPasswordEditor : public QLineEdit
{
    Q_OBJECT;

    /** Holds the current password of the editor. */
    Q_PROPERTY(QString password READ password WRITE setPassword USER true);

signals:

    /** Notifies listeners about data should be committed. */
    void sigCommitData(QWidget *pThis);

public:

    /** Constructor.
      * @param pParent being passed to the base-class. */
    UIPasswordEditor(QWidget *pParent);

private slots:

    /** Handles @s strPassword changes. */
    void sltPasswordChanged(const QString &strPassword);

private:

    /** Prepare routine. */
    void prepare();

    /** Property: Returns the current password of the editor. */
    QString password() const { return QLineEdit::text(); }
    /** Property: Defines the current @a strPassword of the editor. */
    void setPassword(const QString &strPassword) { QLineEdit::setText(strPassword); }
};

/** QAbstractTableModel reimplementation used as
  * the data representation model for the UIEncryptionDataTable. */
class UIEncryptionDataModel : public QAbstractTableModel
{
    Q_OBJECT;

public:

    /** Constructor.
      * @param pParent          being passed to the base-class,
      * @param encryptedMediums contains the lists of medium ids (values) encrypted with passwords with ids (keys). */
    UIEncryptionDataModel(QObject *pParent, const EncryptedMediumMap &encryptedMediums);

    /** Returns the shallow copy of the encryption password map instance. */
    EncryptionPasswordMap encryptionPasswords() const { return m_encryptionPasswords; }

    /** Returns whether the model is valid. */
    bool isValid() const;

    /** Returns the row count, taking optional @a parent instead of root if necessary. */
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    /** Returns the column count, taking optional @a parent instead of root if necessary. */
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;

    /** Returns the @a index flags. */
    virtual Qt::ItemFlags flags(const QModelIndex &index) const;

    /** Returns the header data for the @a iSection, @a orientation and @a iRole. */
    virtual QVariant headerData(int iSection, Qt::Orientation orientation, int iRole) const;

    /** Returns the @a index data for the @a iRole. */
    virtual QVariant data(const QModelIndex &index, int iRole = Qt::DisplayRole) const;
    /** Defines the @a index data for the @a iRole as @a value. */
    virtual bool setData(const QModelIndex &index, const QVariant &value, int iRole = Qt::EditRole);

private:

    /** Prepare routine. */
    void prepare();

    /** Returns whether passed @a strPassword is valid for medium with passed @a strMediumId. */
    bool isPasswordValid(const QString strMediumId, const QString strPassword);

    /** Holds the encrypted medium map reference. */
    const EncryptedMediumMap &m_encryptedMediums;

    /** Holds the encryption password map instance. */
    EncryptionPasswordMap m_encryptionPasswords;
    /** Holds the encryption password status map instance. */
    EncryptionPasswordStatusMap m_encryptionPasswordStatus;
};

/** QTableView reimplementation used to
  * allow the UIAddDiskEncryptionPasswordDialog to enter
  * disk encryption passwords for particular password ids. */
class UIEncryptionDataTable : public QTableView
{
    Q_OBJECT;

signals:

    /** Notifies listeners about data change. */
    void sigDataChanged();

public:

    /** Constructor.
      * @param pParent being passed to the base-class. */
    UIEncryptionDataTable(const EncryptedMediumMap &encryptedMediums);

    /** Returns the shallow copy of the encryption password map
      * acquired from the UIEncryptionDataModel instance. */
    EncryptionPasswordMap encryptionPasswords() const;

    /** Returns whether the table is valid. */
    bool isValid() const;

    /** Initiates the editor for the first index available. */
    void editFirstIndex();

private:

    /** Prepare routine. */
    void prepare();

    /** Holds the encrypted medium map reference. */
    const EncryptedMediumMap &m_encryptedMediums;

    /** Holds the encryption-data model instance. */
    UIEncryptionDataModel *m_pModelEncryptionData;
};

UIPasswordEditor::UIPasswordEditor(QWidget *pParent)
    : QLineEdit(pParent)
{
    /* Prepare: */
    prepare();
}

void UIPasswordEditor::sltPasswordChanged(const QString &strPassword)
{
    Q_UNUSED(strPassword);
    /* Commit data to the listener: */
    emit sigCommitData(this);
}

void UIPasswordEditor::prepare()
{
    /* Set echo mode: */
    setEchoMode(QLineEdit::Password);
    /* Listen for the text changes: */
    connect(this, SIGNAL(textChanged(const QString&)),
            this, SLOT(sltPasswordChanged(const QString&)));
}

UIEncryptionDataModel::UIEncryptionDataModel(QObject *pParent, const EncryptedMediumMap &encryptedMediums)
    : QAbstractTableModel(pParent)
    , m_encryptedMediums(encryptedMediums)
{
    /* Prepare: */
    prepare();
}

bool UIEncryptionDataModel::isValid() const
{
    /* Check whether the model contains invalid passwords: */
    foreach (const bool &fValue, m_encryptionPasswordStatus.values())
        if (!fValue)
            return false;
    /* Valid by default: */
    return true;
}

int UIEncryptionDataModel::rowCount(const QModelIndex &parent /* = QModelIndex() */) const
{
    Q_UNUSED(parent);
    return m_encryptionPasswords.size();
}

int UIEncryptionDataModel::columnCount(const QModelIndex &parent /* = QModelIndex() */) const
{
    Q_UNUSED(parent);
    return UIEncryptionDataTableSection_Max;
}

Qt::ItemFlags UIEncryptionDataModel::flags(const QModelIndex &index) const
{
    /* Check index validness: */
    if (!index.isValid())
        return Qt::NoItemFlags;
    /* Depending on column index: */
    switch (index.column())
    {
        case UIEncryptionDataTableSection_Status:   return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        case UIEncryptionDataTableSection_Id:       return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        case UIEncryptionDataTableSection_Password: return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
        default: break;
    }
    /* No flags by default: */
    return Qt::NoItemFlags;
}

QVariant UIEncryptionDataModel::headerData(int iSection, Qt::Orientation orientation, int iRole) const
{
    /* Check argument validness: */
    if (iRole != Qt::DisplayRole || orientation != Qt::Horizontal)
        return QVariant();
    /* Depending on column index: */
    switch (iSection)
    {
        case UIEncryptionDataTableSection_Status:   return tr("Status", "password table field");
        case UIEncryptionDataTableSection_Id:       return tr("ID", "password table field");
        case UIEncryptionDataTableSection_Password: return tr("Password", "password table field");
        default: break;
    }
    /* Null value by default: */
    return QVariant();
}

QVariant UIEncryptionDataModel::data(const QModelIndex &index, int iRole /* = Qt::DisplayRole */) const
{
    /* Check index validness: */
    if (!index.isValid())
        return QVariant();
    /* Depending on role: */
    switch (iRole)
    {
        case Qt::DecorationRole:
        {
            /* Depending on column index: */
            switch (index.column())
            {
                case UIEncryptionDataTableSection_Status:
                    return m_encryptionPasswordStatus.value(m_encryptionPasswordStatus.keys().at(index.row())) ?
                           UIIconPool::iconSet(":/status_check_16px.png") : UIIconPool::iconSet(":/status_error_16px.png");
                default:
                    return QVariant();
            }
            break;
        }
        case Qt::DisplayRole:
        {
            /* Depending on column index: */
            switch (index.column())
            {
                case UIEncryptionDataTableSection_Id:
                    return m_encryptionPasswords.keys().at(index.row());
                case UIEncryptionDataTableSection_Password:
                    return QString().fill('*', m_encryptionPasswords.value(m_encryptionPasswords.keys().at(index.row())).size());
                default:
                    return QVariant();
            }
            break;
        }
        case Qt::EditRole:
        {
            /* Depending on column index: */
            switch (index.column())
            {
                case UIEncryptionDataTableSection_Password:
                    return m_encryptionPasswords.value(m_encryptionPasswords.keys().at(index.row()));
                default:
                    return QVariant();
            }
            break;
        }
        case Qt::ToolTipRole:
        {
            /* We are generating tool-tip here and not in retranslateUi() because of the tricky plural form handling,
             * but be quiet, it's safe enough because the tool-tip being re-acquired every time on mouse-hovering. */
            const QStringList encryptedMediums = m_encryptedMediums.values(m_encryptionPasswords.keys().at(index.row()));
            return tr("<nobr>Used by the following %n hard drive(s):</nobr><br>%1",
                      "This text is never used with n == 0. "
                      "Feel free to drop the %n where possible, "
                      "we only included it because of problems with Qt Linguist "
                      "(but the user can see how many hard drives are in the tool-tip "
                      "and doesn't need to be told).",
                      encryptedMediums.size())
                      .arg(encryptedMediums.join("<br>"));
        }
        default:
            break;
    }
    /* Null value by default: */
    return QVariant();
}

bool UIEncryptionDataModel::setData(const QModelIndex &index, const QVariant &value, int iRole /* = Qt::EditRole */)
{
    /* Check index validness: */
    if (!index.isValid())
        return false;
    /* Check argument validness: */
    if (iRole != Qt::EditRole)
        return false;
    /* Depending on column index: */
    switch (index.column())
    {
        case UIEncryptionDataTableSection_Password:
        {
            /* Update password: */
            const int iRow = index.row();
            const QString strPassword = value.toString();
            const QString strKey = m_encryptionPasswords.keys().at(iRow);
            m_encryptionPasswords[strKey] = strPassword;
            /* Update password status: */
            const QString strMediumId = m_encryptedMediums.values(strKey).first();
            const bool fPasswordStatus = isPasswordValid(strMediumId, strPassword);
            m_encryptionPasswordStatus[strKey] = fPasswordStatus;
            /* Initiate explicit password status update: */
            const QModelIndex statusIndex = createIndex(iRow, UIEncryptionDataTableSection_Status);
            emit dataChanged(statusIndex, statusIndex);
            break;
        }
        default:
            break;
    }
    /* Nothing to set by default: */
    return false;
}

void UIEncryptionDataModel::prepare()
{
    /* Populate the map of passwords and statuses. */
    foreach (const QString &strPasswordId, m_encryptedMediums.keys())
    {
        m_encryptionPasswords.insert(strPasswordId, QString());
        m_encryptionPasswordStatus.insert(strPasswordId, false);
    }
}

bool UIEncryptionDataModel::isPasswordValid(const QString strMediumId, const QString strPassword)
{
    /* Look for the medium with passed ID: */
    const UIMedium uimedium = vboxGlobal().medium(strMediumId);
    if (!uimedium.isNull())
    {
        /* Check wrapped medium for validity: */
        const CMedium medium = uimedium.medium();
        if (!medium.isNull())
        {
            /* Check whether the password is suitable for that medium. */
            medium.CheckEncryptionPassword(strPassword);
            return medium.isOk();
        }
    }
    /* False by default: */
    return false;
}

UIEncryptionDataTable::UIEncryptionDataTable(const EncryptedMediumMap &encryptedMediums)
    : m_encryptedMediums(encryptedMediums)
    , m_pModelEncryptionData(0)
{
    /* Prepare: */
    prepare();
}

EncryptionPasswordMap UIEncryptionDataTable::encryptionPasswords() const
{
    AssertPtrReturn(m_pModelEncryptionData, EncryptionPasswordMap());
    return m_pModelEncryptionData->encryptionPasswords();
}

bool UIEncryptionDataTable::isValid() const
{
    AssertPtrReturn(m_pModelEncryptionData, false);
    return m_pModelEncryptionData->isValid();
}

void UIEncryptionDataTable::editFirstIndex()
{
    AssertPtrReturnVoid(m_pModelEncryptionData);
    /* Compose the password field index of the first available table record: */
    const QModelIndex index = m_pModelEncryptionData->index(0, UIEncryptionDataTableSection_Password);
    /* Navigate table to the corresponding index: */
    setCurrentIndex(index);
    /* Compose the fake mouse-event which will trigger the embedded editor: */
    QMouseEvent event(QEvent::MouseButtonPress, QPoint(), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    /* Initiate the embedded editor for the corresponding index: */
    edit(index, QAbstractItemView::SelectedClicked, &event);
}

void UIEncryptionDataTable::prepare()
{
    /* Create encryption-data model: */
    m_pModelEncryptionData = new UIEncryptionDataModel(this, m_encryptedMediums);
    AssertPtrReturnVoid(m_pModelEncryptionData);
    {
        /* Assign configured model to table: */
        setModel(m_pModelEncryptionData);
        /* Configure encryption-data model: */
        connect(m_pModelEncryptionData, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)),
                this, SIGNAL(sigDataChanged()));
    }

    /* Create item delegate: */
    QIStyledItemDelegate *pStyledItemDelegate = new QIStyledItemDelegate(this);
    AssertPtrReturnVoid(pStyledItemDelegate);
    {
        /* Create item editor factory: */
        QItemEditorFactory *pNewItemEditorFactory = new QItemEditorFactory;
        AssertPtrReturnVoid(pNewItemEditorFactory);
        {
            /* Create item editor creator: */
            QStandardItemEditorCreator<UIPasswordEditor> *pQStringItemEditorCreator = new QStandardItemEditorCreator<UIPasswordEditor>();
            AssertPtrReturnVoid(pQStringItemEditorCreator);
            {
                /* Register UIPasswordEditor as the QString editor: */
                pNewItemEditorFactory->registerEditor(QVariant::String, pQStringItemEditorCreator);
            }
            /* Assign configured item editor factory to table delegate: */
            pStyledItemDelegate->setItemEditorFactory(pNewItemEditorFactory);
        }
        /* Assign configured item delegate to table: */
        delete itemDelegate();
        setItemDelegate(pStyledItemDelegate);
    }

    /* Configure table: */
    setTabKeyNavigation(false);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::CurrentChanged | QAbstractItemView::SelectedClicked);

    /* Configure headers: */
    verticalHeader()->hide();
    verticalHeader()->setDefaultSectionSize((int)(verticalHeader()->minimumSectionSize() * 1.33));
    horizontalHeader()->setStretchLastSection(false);
    horizontalHeader()->setResizeMode(UIEncryptionDataTableSection_Status, QHeaderView::ResizeToContents);
    horizontalHeader()->setResizeMode(UIEncryptionDataTableSection_Id, QHeaderView::Interactive);
    horizontalHeader()->setResizeMode(UIEncryptionDataTableSection_Password, QHeaderView::Stretch);
}

UIAddDiskEncryptionPasswordDialog::UIAddDiskEncryptionPasswordDialog(QWidget *pParent,
                                                                     const QString &strMachineName,
                                                                     const EncryptedMediumMap &encryptedMediums)
    : QIWithRetranslateUI<QDialog>(pParent)
    , m_strMachineName(strMachineName)
    , m_encryptedMediums(encryptedMediums)
    , m_pLabelDescription(0)
    , m_pTableEncryptionData(0)
    , m_pButtonBox(0)
{
    /* Prepare: */
    prepare();
    /* Translate: */
    retranslateUi();
    /* Validate: */
    revalidate();
}

EncryptionPasswordMap UIAddDiskEncryptionPasswordDialog::encryptionPasswords() const
{
    AssertPtrReturn(m_pTableEncryptionData, EncryptionPasswordMap());
    return m_pTableEncryptionData->encryptionPasswords();
}

void UIAddDiskEncryptionPasswordDialog::prepare()
{
    /* Configure self: */
    setWindowModality(Qt::WindowModal);

    /* Create main-layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    AssertPtrReturnVoid(pMainLayout);
    {
        /* Create input-layout: */
        QVBoxLayout *pInputLayout = new QVBoxLayout;
        AssertPtrReturnVoid(pInputLayout);
        {
            /* Create description label: */
            m_pLabelDescription = new QLabel;
            AssertPtrReturnVoid(m_pLabelDescription);
            {
                /* Add label into layout: */
                pInputLayout->addWidget(m_pLabelDescription);
            }
            /* Create encryption-data table: */
            m_pTableEncryptionData = new UIEncryptionDataTable(m_encryptedMediums);
            AssertPtrReturnVoid(m_pTableEncryptionData);
            {
                /* Configure encryption-data table: */
                connect(m_pTableEncryptionData, SIGNAL(sigDataChanged()),
                        this, SLOT(sltDataChanged()));
                m_pTableEncryptionData->setFocus();
                m_pTableEncryptionData->editFirstIndex();
                /* Add label into layout: */
                pInputLayout->addWidget(m_pTableEncryptionData);
            }
            /* Add layout into parent: */
            pMainLayout->addLayout(pInputLayout);
        }
        /* Create button-box: */
        m_pButtonBox = new QIDialogButtonBox;
        AssertPtrReturnVoid(m_pButtonBox);
        {
            /* Configure button-box: */
            m_pButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
            connect(m_pButtonBox, SIGNAL(accepted()), this, SLOT(accept()));
            connect(m_pButtonBox, SIGNAL(rejected()), this, SLOT(reject()));
            /* Add button-box into layout: */
            pMainLayout->addWidget(m_pButtonBox);
        }
    }
}

void UIAddDiskEncryptionPasswordDialog::retranslateUi()
{
    /* Translate the dialog title: */
    setWindowTitle(tr("%1 - Disk Encryption").arg(m_strMachineName));

    /* Translate the description label: */
    AssertPtrReturnVoid(m_pLabelDescription);
    m_pLabelDescription->setText(tr("This virtual machine is password protected. "
                                    "Please enter the %n encryption password(s) below.",
                                    "This text is never used with n == 0. "
                                    "Feel free to drop the %n where possible, "
                                    "we only included it because of problems with Qt Linguist "
                                    "(but the user can see how many passwords are in the list "
                                    "and doesn't need to be told).",
                                    m_encryptedMediums.size()));
}

void UIAddDiskEncryptionPasswordDialog::revalidate()
{
    /* Validate: */
    AssertPtrReturnVoid(m_pButtonBox);
    m_pButtonBox->button(QDialogButtonBox::Ok)->setEnabled(m_pTableEncryptionData->isValid());
}

#include "UIAddDiskEncryptionPasswordDialog.moc"

