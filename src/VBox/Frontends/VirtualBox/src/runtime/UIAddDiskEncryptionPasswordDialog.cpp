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
# include <QItemEditorFactory>
# include <QAbstractTableModel>
# include <QStandardItemEditorCreator>

/* GUI includes: */
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
    EncryptionPasswordsMap encryptionPasswords() const { return m_encryptionPasswords; }

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

    /** Holds the encrypted medium map reference. */
    const EncryptedMediumMap &m_encryptedMediums;

    /** Holds the encryption password map instance. */
    EncryptionPasswordsMap m_encryptionPasswords;
};

/** QTableView reimplementation used to
  * allow the UIAddDiskEncryptionPasswordDialog to enter
  * disk encryption passwords for particular password ids. */
class UIEncryptionDataTable : public QTableView
{
    Q_OBJECT;

public:

    /** Constructor.
      * @param pParent being passed to the base-class. */
    UIEncryptionDataTable(const EncryptedMediumMap &encryptedMediums);

    /** Returns the shallow copy of the encryption password map
      * acquired from the UIEncryptionDataModel instance. */
    EncryptionPasswordsMap encryptionPasswords() const;

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
    /* Set alignment: */
    setAlignment(Qt::AlignCenter);
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
        case UIEncryptionDataTableSection_Id:       return tr("Password ID");
        case UIEncryptionDataTableSection_Password: return tr("Password");
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
            return tr("Used by the following %n hard-drive(s):<br>%1",
                      "This text is never used with n == 0. "
                      "Feel free to drop the %n where possible, "
                      "we only included it because of problems with Qt Linguist "
                      "(but the user can see how many hard drives are in the tool-tip "
                      "and doesn't need to be told).",
                      encryptedMediums.size())
                      .arg(encryptedMediums.join("<br>"));
        }
        case Qt::TextAlignmentRole:
        {
            /* Depending on column index: */
            switch (index.column())
            {
                case UIEncryptionDataTableSection_Password:
                    return Qt::AlignCenter;
                default: return QVariant();
            }
            break;
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
        case UIEncryptionDataTableSection_Password: m_encryptionPasswords[m_encryptionPasswords.keys().at(index.row())] = value.toString(); break;
        default: break;
    }
    /* Nothing to set by default: */
    return false;
}

void UIEncryptionDataModel::prepare()
{
    /* Populate the map of passwords. */
    foreach (const QString &strPasswordId, m_encryptedMediums.keys())
        m_encryptionPasswords.insert(strPasswordId, QString());
}

UIEncryptionDataTable::UIEncryptionDataTable(const EncryptedMediumMap &encryptedMediums)
    : m_encryptedMediums(encryptedMediums)
    , m_pModelEncryptionData(0)
{
    /* Prepare: */
    prepare();
}

EncryptionPasswordsMap UIEncryptionDataTable::encryptionPasswords() const
{
    AssertPtrReturn(m_pModelEncryptionData, EncryptionPasswordsMap());
    return m_pModelEncryptionData->encryptionPasswords();
}

void UIEncryptionDataTable::prepare()
{
    /* Create encryption-data model: */
    m_pModelEncryptionData = new UIEncryptionDataModel(this, m_encryptedMediums);
    AssertPtrReturnVoid(m_pModelEncryptionData);
    {
        /* Assign configured model to table: */
        setModel(m_pModelEncryptionData);
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
{
    /* Prepare: */
    prepare();
    /* Retranslate: */
    retranslateUi();
}

EncryptionPasswordsMap UIAddDiskEncryptionPasswordDialog::encryptionPasswords() const
{
    AssertPtrReturn(m_pTableEncryptionData, EncryptionPasswordsMap());
    return m_pTableEncryptionData->encryptionPasswords();
}

void UIAddDiskEncryptionPasswordDialog::prepare()
{
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
                /* Add label into layout: */
                pInputLayout->addWidget(m_pTableEncryptionData);
            }
            /* Add layout into parent: */
            pMainLayout->addLayout(pInputLayout);
        }
        /* Create button-box: */
        QIDialogButtonBox *pButtonBox = new QIDialogButtonBox;
        AssertPtrReturnVoid(pButtonBox);
        {
            /* Configure button-box: */
            pButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
            connect(pButtonBox, SIGNAL(accepted()), this, SLOT(accept()));
            connect(pButtonBox, SIGNAL(rejected()), this, SLOT(reject()));
            /* Add button-box into layout: */
            pMainLayout->addWidget(pButtonBox);
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

#include "UIAddDiskEncryptionPasswordDialog.moc"

