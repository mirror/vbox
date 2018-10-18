/* $Id$ */
/** @file
 * VBox Qt GUI - UICloudProfileDetailsWidget class declaration.
 */

/*
 * Copyright (C) 2009-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UICloudProfileDetailsWidget_h___
#define ___UICloudProfileDetailsWidget_h___

/* Qt includes: */
#include <QUuid>
#include <QWidget>

/* GUI includes: */
#include "QIManagerDialog.h"
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QAbstractButton;
class QLabel;
class QLineEdit;
class QTableWidget;
class QTableWidgetItem;
class QIDialogButtonBox;


/** Cloud Provider data structure. */
struct UIDataCloudProvider
{
    /** Constructs data. */
    UIDataCloudProvider()
        : m_strName(QString())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataCloudProvider &other) const
    {
        return true
               && (m_uuid == other.m_uuid)
               && (m_strName == other.m_strName)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataCloudProvider &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataCloudProvider &other) const { return !equal(other); }

    /** Holds the provider ID. */
    QUuid    m_uuid;
    /** Holds the provider name. */
    QString  m_strName;
};

/** Cloud Profile data structure. */
struct UIDataCloudProfile
{
    /** Constructs data. */
    UIDataCloudProfile()
        : m_strName(QString())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataCloudProfile &other) const
    {
        return true
               && (m_strName == other.m_strName)
               && (m_data == other.m_data)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataCloudProfile &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataCloudProfile &other) const { return !equal(other); }

    /** Holds the profile name. */
    QString  m_strName;

    /** Holds the profile data. */
    QMap<QString, QPair<QString, QString> >  m_data;
};


/** Cloud Profile details widget. */
class UICloudProfileDetailsWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about data changed and whether it @a fDiffers. */
    void sigDataChanged(bool fDiffers);

    /** Notifies listeners about data change rejected and should be reseted. */
    void sigDataChangeRejected();
    /** Notifies listeners about data change accepted and should be applied. */
    void sigDataChangeAccepted();

public:

    /** Constructs cloud profile details widget passing @a pParent to the base-class.
      * @param  enmEmbedding  Brings embedding type. */
    UICloudProfileDetailsWidget(EmbedTo enmEmbedding, QWidget *pParent = 0);

    /** Returns the cloud profile data. */
    const UIDataCloudProfile &data() const { return m_newData; }
    /** Defines the cloud profile @a data. */
    void setData(const UIDataCloudProfile &data);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;
    /** Handles editor translation. */
    void retranslateEditor();
    /** Handles buttons translation. */
    void retranslateButtons();

private slots:

    /** @name Change handling stuff.
      * @{ */
        /** Handles name change. */
        void sltNameChanged(const QString &strName);
        /** Handles table change. */
        void sltTableChanged(QTableWidgetItem *pItem);

        /** Handles button-box button click. */
        void sltHandleButtonBoxClick(QAbstractButton *pButton);
    /** @} */

private:

    /** @name Prepare/cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares widgets. */
        void prepareWidgets();
    /** @} */

    /** @name Loading stuff.
      * @{ */
        /** Loads data. */
        void loadData();
    /** @} */

    /** @name Change handling stuff.
      * @{ */
        /** Revalidates changes for passed @a pWidget. */
        void revalidate(QWidget *pWidget = 0);

        /** Retranslates validation for passed @a pWidget. */
        void retranslateValidation(QWidget *pWidget = 0);

        /** Updates table tooltips. */
        void updateTableToolTips();
        /** Adjusts table contents. */
        void adjustTableContents();

        /** Updates button states. */
        void updateButtonStates();
    /** @} */

    /** @name General variables.
      * @{ */
        /** Holds the parent widget embedding type. */
        const EmbedTo  m_enmEmbedding;

        /** Holds the old data copy. */
        UIDataCloudProfile  m_oldData;
        /** Holds the new data copy. */
        UIDataCloudProfile  m_newData;
    /** @} */

    /** @name Widget variables.
      * @{ */
        /** Holds the name label instance. */
        QLabel    *m_pLabelName;
        /** Holds the name editor instance. */
        QLineEdit *m_pEditorName;

        /** Holds the table-widget label instance. */
        QLabel       *m_pLabelTableWidget;
        /** Holds the table-widget instance. */
        QTableWidget *m_pTableWidget;

        /** Holds the server button-box instance. */
        QIDialogButtonBox *m_pButtonBox;
    /** @} */
};


#endif /* !___UICloudProfileDetailsWidget_h___ */
