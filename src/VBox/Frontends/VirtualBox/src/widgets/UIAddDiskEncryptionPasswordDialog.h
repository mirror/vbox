/* $Id$ */
/** @file
 * VBox Qt GUI - UIAddDiskEncryptionPasswordDialog class declaration.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIAddDiskEncryptionPasswordDialog_h___
#define ___UIAddDiskEncryptionPasswordDialog_h___

/* Qt includes: */
#include <QDialog>
#include <QMap>
#include <QMultiMap>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UILibraryDefs.h"

/* Forward declarations: */
class QLabel;
class QIDialogButtonBox;
class UIEncryptionDataTable;

/* Type definitions: */
typedef QMultiMap<QString, QUuid> EncryptedMediumMap;
typedef QMap<QString, QString> EncryptionPasswordMap;
typedef QMap<QString, bool> EncryptionPasswordStatusMap;

/** QDialog subclass used to
  * allow the user to enter disk encryption passwords for particular password IDs. */
class SHARED_LIBRARY_STUFF UIAddDiskEncryptionPasswordDialog : public QIWithRetranslateUI<QDialog>
{
    Q_OBJECT;

public:

    /** Constructs dialog passing @a pParent to the base-class.
      * @param  strMachineName    Brings the name of the machine we show this dialog for.
      * @param  encryptedMedia  Brings the lists of medium ids (values) encrypted with passwords with ids (keys). */
    UIAddDiskEncryptionPasswordDialog(QWidget *pParent, const QString &strMachineName, const EncryptedMediumMap &encryptedMedia);

    /** Returns the shallow copy of the encryption password map
      * acquired from the UIEncryptionDataTable instance. */
    EncryptionPasswordMap encryptionPasswords() const;

protected:

    /** Translation routine. */
    virtual void retranslateUi() /* override */;

private slots:

    /** Handles editor's Enter/Return key triggering. */
    void sltEditorEnterKeyTriggered() { accept(); }

    /** Performs passwords validation.
      * If all passwords are valid,
      * this slot calls to base-class. */
    void accept();

private:

    /** Prepares all. */
    void prepare();

    /** Returns whether passed @a strPassword is valid for medium with passed @a uMediumId. */
    static bool isPasswordValid(const QUuid &uMediumId, const QString strPassword);

    /** Holds the name of the machine we show this dialog for. */
    const QString  m_strMachineName;

    /** Holds the encrypted medium map reference. */
    const EncryptedMediumMap &m_encryptedMedia;

    /** Holds the description label instance. */
    QLabel                *m_pLabelDescription;
    /** Holds the encryption-data table instance. */
    UIEncryptionDataTable *m_pTableEncryptionData;
    /** Holds the button-box instance. */
    QIDialogButtonBox     *m_pButtonBox;
};

#endif /* !___UIAddDiskEncryptionPasswordDialog_h___ */
