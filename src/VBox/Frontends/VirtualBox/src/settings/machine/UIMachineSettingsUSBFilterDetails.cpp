/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsUSBFilterDetails class implementation.
 */

/*
 * Copyright (C) 2008-2020 Oracle Corporation
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
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>

/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "UIMachineSettingsUSBFilterDetails.h"
#include "UIConverter.h"


UIMachineSettingsUSBFilterDetails::UIMachineSettingsUSBFilterDetails(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI2<QIDialog>(pParent, Qt::Sheet)
    , m_pComboBoxRemote(0)
    , m_pLineEditName(0)
    , m_pLineEditVendorID(0)
    , m_pLineEditProductID(0)
    , m_pLineEditRevision(0)
    , m_pLineEditPort(0)
    , m_pLineEditManufacturer(0)
    , m_pLineEditProduct(0)
    , m_pLineEditSerialNo(0)
    , m_pLabelPort(0)
    , m_pLabelRemote(0)
    , m_pLabelProductID(0)
    , m_pLabelName(0)
    , m_pLabelVendorID(0)
    , m_pLabelRevision(0)
    , m_pLabelManufacturer(0)
    , m_pLabelProduct(0)
    , m_pLabelSerialNo(0)
    , gridLayout(0)
{
    prepareWidgets();

    m_pComboBoxRemote->insertItem (UIMachineSettingsUSB::ModeAny, ""); /* Any */
    m_pComboBoxRemote->insertItem (UIMachineSettingsUSB::ModeOn,  ""); /* Yes */
    m_pComboBoxRemote->insertItem (UIMachineSettingsUSB::ModeOff, ""); /* No */

    m_pLineEditName->setValidator (new QRegExpValidator (QRegExp (".+"), this));
    m_pLineEditVendorID->setValidator (new QRegExpValidator (QRegExp ("[0-9a-fA-F]{0,4}"), this));
    m_pLineEditProductID->setValidator (new QRegExpValidator (QRegExp ("[0-9a-fA-F]{0,4}"), this));
    m_pLineEditRevision->setValidator (new QRegExpValidator (QRegExp ("[0-9a-fA-F]{0,4}"), this));
    m_pLineEditPort->setValidator (new QRegExpValidator (QRegExp ("[0-9]*"), this));

    /* Applying language settings */
    retranslateUi();

    resize (minimumSize());
    setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void UIMachineSettingsUSBFilterDetails::retranslateUi()
{
    setWindowTitle(QApplication::translate("UIMachineSettingsUSBFilterDetails", "USB Filter Details"));
    m_pLabelName->setText(QApplication::translate("UIMachineSettingsUSBFilterDetails", "&Name:"));
    m_pLineEditName->setToolTip(QApplication::translate("UIMachineSettingsUSBFilterDetails", "Holds the filter name."));
    m_pLabelVendorID->setText(QApplication::translate("UIMachineSettingsUSBFilterDetails", "&Vendor ID:"));
    m_pLineEditVendorID->setToolTip(QApplication::translate("UIMachineSettingsUSBFilterDetails", "Holds the vendor ID filter. The "
                                                    "<i>exact match</i> string format is <tt>XXXX</tt> where <tt>X</tt> is a "
                                                            "hexadecimal digit. An empty string will match any value."));
    m_pLabelProductID->setText(QApplication::translate("UIMachineSettingsUSBFilterDetails", "&Product ID:"));
    m_pLineEditProductID->setToolTip(QApplication::translate("UIMachineSettingsUSBFilterDetails", "Holds the product ID filter. The "
                                                     "<i>exact match</i> string format is <tt>XXXX</tt> where <tt>X</tt> is a "
                                                     "hexadecimal digit. An empty string will match any value."));
    m_pLabelRevision->setText(QApplication::translate("UIMachineSettingsUSBFilterDetails", "&Revision:"));
    m_pLineEditRevision->setToolTip(QApplication::translate("UIMachineSettingsUSBFilterDetails", "Holds the revision number filter. The "
                                                    "<i>exact match</i> string format is <tt>IIFF</tt> where <tt>I</tt> is a decimal "
                                                    "digit of the integer part and <tt>F</tt> is a decimal digit of the fractional "
                                                    "part. An empty string will match any value."));
    m_pLabelManufacturer->setText(QApplication::translate("UIMachineSettingsUSBFilterDetails", "&Manufacturer:"));
    m_pLineEditManufacturer->setToolTip(QApplication::translate("UIMachineSettingsUSBFilterDetails", "Holds the manufacturer filter as an "
                                                        "<i>exact match</i> string. An empty string will match any value."));
    m_pLabelProduct->setText(QApplication::translate("UIMachineSettingsUSBFilterDetails", "Pro&duct:"));
    m_pLineEditProduct->setToolTip(QApplication::translate("UIMachineSettingsUSBFilterDetails", "Holds the product name filter as an "
                                                   "<i>exact match</i> string. An empty string will match any value."));
    m_pLabelSerialNo->setText(QApplication::translate("UIMachineSettingsUSBFilterDetails", "&Serial No.:"));
    m_pLineEditSerialNo->setToolTip(QApplication::translate("UIMachineSettingsUSBFilterDetails", "Holds the serial number filter as an "
                                                    "<i>exact match</i> string. An empty string will match any value."));
    m_pLabelPort->setText(QApplication::translate("UIMachineSettingsUSBFilterDetails", "Por&t:"));
    m_pLineEditPort->setToolTip(QApplication::translate("UIMachineSettingsUSBFilterDetails", "Holds the host USB port filter as an "
                                                "<i>exact match</i> string. An empty string will match any value."));
    m_pLabelRemote->setText(QApplication::translate("UIMachineSettingsUSBFilterDetails", "R&emote:"));
    m_pComboBoxRemote->setToolTip(QApplication::translate("UIMachineSettingsUSBFilterDetails", "Holds whether this filter applies to USB "
                                                  "devices attached locally to the host computer (<i>No</i>), to a VRDP client's "
                                                  "computer (<i>Yes</i>), or both (<i>Any</i>)."));

    m_pComboBoxRemote->setItemText (UIMachineSettingsUSB::ModeAny, tr ("Any", "remote"));
    m_pComboBoxRemote->setItemText (UIMachineSettingsUSB::ModeOn,  tr ("Yes", "remote"));
    m_pComboBoxRemote->setItemText (UIMachineSettingsUSB::ModeOff, tr ("No",  "remote"));
}


void UIMachineSettingsUSBFilterDetails::prepareWidgets()
{
    if (objectName().isEmpty())
        setObjectName(QStringLiteral("UIMachineSettingsUSBFilterDetails"));
    resize(350, 363);
    setMinimumSize(QSize(350, 0));
    gridLayout = new QGridLayout(this);
    gridLayout->setObjectName(QStringLiteral("gridLayout"));
    m_pLabelName = new QLabel();
    m_pLabelName->setObjectName(QStringLiteral("m_pLabelName"));
    m_pLabelName->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    gridLayout->addWidget(m_pLabelName, 0, 0, 1, 1);

    m_pLineEditName = new QLineEdit();
    m_pLineEditName->setObjectName(QStringLiteral("m_pLineEditName"));
    gridLayout->addWidget(m_pLineEditName, 0, 1, 1, 1);

    m_pLabelVendorID = new QLabel();
    m_pLabelVendorID->setObjectName(QStringLiteral("m_pLabelVendorID"));
    m_pLabelVendorID->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    gridLayout->addWidget(m_pLabelVendorID, 1, 0, 1, 1);

    m_pLineEditVendorID = new QLineEdit();
    m_pLineEditVendorID->setObjectName(QStringLiteral("m_pLineEditVendorID"));
    gridLayout->addWidget(m_pLineEditVendorID, 1, 1, 1, 1);

    m_pLabelProductID = new QLabel();
    m_pLabelProductID->setObjectName(QStringLiteral("m_pLabelProductID"));
    m_pLabelProductID->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    gridLayout->addWidget(m_pLabelProductID, 2, 0, 1, 1);

    m_pLineEditProductID = new QLineEdit();
    m_pLineEditProductID->setObjectName(QStringLiteral("m_pLineEditProductID"));
    gridLayout->addWidget(m_pLineEditProductID, 2, 1, 1, 1);

    m_pLabelRevision = new QLabel();
    m_pLabelRevision->setObjectName(QStringLiteral("m_pLabelRevision"));
    m_pLabelRevision->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    gridLayout->addWidget(m_pLabelRevision, 3, 0, 1, 1);

    m_pLineEditRevision = new QLineEdit();
    m_pLineEditRevision->setObjectName(QStringLiteral("m_pLineEditRevision"));
    gridLayout->addWidget(m_pLineEditRevision, 3, 1, 1, 1);

    m_pLabelManufacturer = new QLabel();
    m_pLabelManufacturer->setObjectName(QStringLiteral("m_pLabelManufacturer"));
    m_pLabelManufacturer->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    gridLayout->addWidget(m_pLabelManufacturer, 4, 0, 1, 1);

    m_pLineEditManufacturer = new QLineEdit();
    m_pLineEditManufacturer->setObjectName(QStringLiteral("m_pLineEditManufacturer"));
    gridLayout->addWidget(m_pLineEditManufacturer, 4, 1, 1, 1);

    m_pLabelProduct = new QLabel();
    m_pLabelProduct->setObjectName(QStringLiteral("m_pLabelProduct"));
    m_pLabelProduct->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    gridLayout->addWidget(m_pLabelProduct, 5, 0, 1, 1);

    m_pLineEditProduct = new QLineEdit();
    m_pLineEditProduct->setObjectName(QStringLiteral("m_pLineEditProduct"));
    gridLayout->addWidget(m_pLineEditProduct, 5, 1, 1, 1);

    m_pLabelSerialNo = new QLabel();
    m_pLabelSerialNo->setObjectName(QStringLiteral("m_pLabelSerialNo"));
    m_pLabelSerialNo->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    gridLayout->addWidget(m_pLabelSerialNo, 6, 0, 1, 1);

    m_pLineEditSerialNo = new QLineEdit();
    m_pLineEditSerialNo->setObjectName(QStringLiteral("m_pLineEditSerialNo"));
    gridLayout->addWidget(m_pLineEditSerialNo, 6, 1, 1, 1);

    m_pLabelPort = new QLabel();
    m_pLabelPort->setObjectName(QStringLiteral("m_pLabelPort"));
    m_pLabelPort->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    gridLayout->addWidget(m_pLabelPort, 7, 0, 1, 1);

    m_pLineEditPort = new QLineEdit();
    m_pLineEditPort->setObjectName(QStringLiteral("m_pLineEditPort"));
    gridLayout->addWidget(m_pLineEditPort, 7, 1, 1, 1);

    m_pLabelRemote = new QLabel();
    m_pLabelRemote->setObjectName(QStringLiteral("m_pLabelRemote"));
    m_pLabelRemote->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    gridLayout->addWidget(m_pLabelRemote, 8, 0, 1, 1);

    m_pComboBoxRemote = new QComboBox();
    m_pComboBoxRemote->setObjectName(QStringLiteral("m_pComboBoxRemote"));
    QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    sizePolicy.setHeightForWidth(m_pComboBoxRemote->sizePolicy().hasHeightForWidth());
    m_pComboBoxRemote->setSizePolicy(sizePolicy);
    gridLayout->addWidget(m_pComboBoxRemote, 8, 1, 1, 1);

    QSpacerItem *pSpacerItem = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding);
    gridLayout->addItem(pSpacerItem, 9, 1, 1, 1);

    QIDialogButtonBox *pButtonBox = new QIDialogButtonBox();
    pButtonBox->setObjectName(QStringLiteral("pButtonBox"));
    pButtonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::NoButton|QDialogButtonBox::Ok);
    gridLayout->addWidget(pButtonBox, 10, 0, 1, 2);

    m_pLabelName->setBuddy(m_pLineEditName);
    m_pLabelVendorID->setBuddy(m_pLineEditVendorID);
    m_pLabelProductID->setBuddy(m_pLineEditProductID);
    m_pLabelRevision->setBuddy(m_pLineEditRevision);
    m_pLabelManufacturer->setBuddy(m_pLineEditManufacturer);
    m_pLabelProduct->setBuddy(m_pLineEditProduct);
    m_pLabelSerialNo->setBuddy(m_pLineEditSerialNo);
    m_pLabelPort->setBuddy(m_pLineEditPort);
    m_pLabelRemote->setBuddy(m_pComboBoxRemote);

    QObject::connect(pButtonBox, &QIDialogButtonBox::accepted, this, &UIMachineSettingsUSBFilterDetails::accept);
    QObject::connect(pButtonBox, &QIDialogButtonBox::rejected, this, &UIMachineSettingsUSBFilterDetails::reject);
}
