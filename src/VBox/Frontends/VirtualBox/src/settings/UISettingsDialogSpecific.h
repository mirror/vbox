/* $Id$ */
/** @file
 * VBox Qt GUI - UISettingsDialogSpecific class declaration.
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

#ifndef ___UISettingsDialogSpecific_h___
#define ___UISettingsDialogSpecific_h___

/* GUI includes: */
#include "UISettingsDialog.h"

/* COM includes: */
#include "COMEnums.h"
#include "CConsole.h"
#include "CMachine.h"
#include "CSession.h"


/** UISettingsDialog extension encapsulating all the specific functionality of the Global Preferences. */
class SHARED_LIBRARY_STUFF UISettingsDialogGlobal : public UISettingsDialog
{
    Q_OBJECT;

public:

    /** Constructs settings dialog passing @a pParent to the base-class.
      * @param  strCategory  Brings the name of category to be opened.
      * @param  strControl   Brings the name of control to be focused. */
    UISettingsDialogGlobal(QWidget *pParent,
                           const QString &strCategory = QString(),
                           const QString &strControl = QString());

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Loads the data from the corresponding source. */
    virtual void loadOwnData() /* override */;
    /** Saves the data to the corresponding source. */
    virtual void saveOwnData() /* override */;

    /** Returns the dialog title extension. */
    virtual QString titleExtension() const /* override */;
    /** Returns the dialog title. */
    virtual QString title() const /* override */;

private:

    /** Prepares all. */
    void prepare();

    /** Returns whether page with certain @a iPageId is available. */
    bool isPageAvailable(int iPageId) const;

    /** Holds the name of category to be opened. */
    QString  m_strCategory;
    /** Holds the name of control to be focused. */
    QString  m_strControl;
};


/** UISettingsDialog extension encapsulating all the specific functionality of the Machine Settings. */
class SHARED_LIBRARY_STUFF UISettingsDialogMachine : public UISettingsDialog
{
    Q_OBJECT;

public:

    /** Constructs settings dialog passing @a pParent to the base-class.
      * @param  strMachineId  Brings the machine ID.
      * @param  strCategory   Brings the name of category to be opened.
      * @param  strControl    Brings the name of control to be focused. */
    UISettingsDialogMachine(QWidget *pParent, const QString &strMachineId,
                            const QString &strCategory, const QString &strControl);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Loads the data from the corresponding source. */
    virtual void loadOwnData() /* override */;
    /** Saves the data to the corresponding source. */
    virtual void saveOwnData() /* override */;

    /** Returns the dialog title extension. */
    virtual QString titleExtension() const /* override */;
    /** Returns the dialog title. */
    virtual QString title() const /* override */;

    /** Verifies data integrity between certain @a pSettingsPage and other pages. */
    virtual void recorrelate(UISettingsPage *pSettingsPage) /* override */;

protected slots:

    /** Handles category change to @a cId. */
    virtual void sltCategoryChanged(int cId) /* override */;

    /** Marks dialog loaded. */
    virtual void sltMarkLoaded() /* override */;
    /** Marks dialog saved. */
    virtual void sltMarkSaved() /* override */;

private slots:

    /** Handles session state change for machine with certain @a strMachineId to @a enmSessionState. */
    void sltSessionStateChanged(QString strMachineId, KSessionState enmSessionState);
    /** Handles machine state change for machine with certain @a strMachineId to @a enmMachineState. */
    void sltMachineStateChanged(QString strMachineId, KMachineState enmMachineState);
    /** Handles machine data change for machine with certain @a strMachineId. */
    void sltMachineDataChanged(QString strMachineId);

    /** Handles request to allow to reset first run flag. */
    void sltAllowResetFirstRunFlag();
    /** Handles request to reset first run flag. */
    void sltResetFirstRunFlag();

private:

    /** Prepares all. */
    void prepare();

    /** Returns whether page with certain @a iPageId is available. */
    bool isPageAvailable(int iPageId) const;

    /** Returns whether settings were changed. */
    bool isSettingsChanged();

    /** Recalculates configuration access level. */
    void updateConfigurationAccessLevel();

    /** Holds the machine ID. */
    QString  m_strMachineId;
    /** Holds the name of category to be opened. */
    QString  m_strCategory;
    /** Holds the name of control to be focused. */
    QString  m_strControl;

    /** Holds the session state. */
    KSessionState  m_enmSessionState;
    /** Holds the machine state. */
    KMachineState  m_enmMachineState;

    /** Holds the session reference. */
    CSession  m_session;
    /** Holds the machine reference. */
    CMachine  m_machine;
    /** Holds the console reference. */
    CConsole  m_console;

    /** Holds whether we are allowed to reset first run flag. */
    bool  m_fAllowResetFirstRunFlag : 1;
    /** Holds whether we have request to reset first run flag. */
    bool  m_fResetFirstRunFlag : 1;
};


#endif /* !___UISettingsDialogSpecific_h___ */
