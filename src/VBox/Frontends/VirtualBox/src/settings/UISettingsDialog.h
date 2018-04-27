/* $Id$ */
/** @file
 * VBox Qt GUI - UISettingsDialog class declaration.
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

#ifndef ___UISettingsDialog_h___
#define ___UISettingsDialog_h___

/* Qt includes: */
#include <QPointer>
#include <QVariant>

/* GUI includes: */
#include "QIMainDialog.h"
#include "QIWithRetranslateUI.h"
#include "UISettingsDefs.h"
#include "UISettingsDialog.gen.h"

/* Forward declarations: */
class QEvent;
class QObject;
class QProgressBar;
class QShowEvent;
class QStackedWidget;
class QTimer;
class UIPageValidator;
class UISettingsPage;
class UISettingsSelector;
class UISettingsSerializer;
class UIWarningPane;

/* Using declarations: */
using namespace UISettingsDefs;

/** QIMainDialog aubclass used as
  * base dialog class for both Global & VM settings which encapsulates most of their common functionality. */
class SHARED_LIBRARY_STUFF UISettingsDialog : public QIWithRetranslateUI<QIMainDialog>, public Ui::UISettingsDialog
{
    Q_OBJECT;

public:

    /** Constructs settings dialog passing @a pParent to the base-class. */
    UISettingsDialog(QWidget *pParent);
    /** Destructs settings dialog. */
    virtual ~UISettingsDialog() /* override */;

    /** Performs modal dialog call. */
    void execute();

protected slots:

    /** Hides the modal dialog and sets the result code to Accepted. */
    virtual void accept() /* override */;
    /** Hides the modal dialog and sets the result code to Rejected. */
    virtual void reject() /* override */;

    /** Handles category change to @a cId. */
    virtual void sltCategoryChanged(int cId);

    /** Marks dialog loaded. */
    virtual void sltMarkLoaded();
    /** Marks dialog saved. */
    virtual void sltMarkSaved();

    /** Handles process start. */
    void sltHandleProcessStarted();
    /** Handles process progress change to @a iValue. */
    void sltHandleProcessProgressChange(int iValue);

protected:

    /** Preprocesses any Qt @a pEvent for passed @a pObject. */
    virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;
    /** Handles translation event. */
    virtual void retranslateUi() /* override */;
    /** Handles show @a pEvent. */
    virtual void showEvent(QShowEvent *pEvent) /* override */;

    /** Returns the serialize process instance. */
    UISettingsSerializer *serializeProcess() const { return m_pSerializeProcess; }
    /** Returns whether the serialization is in progress. */
    bool isSerializationInProgress() const { return m_fSerializationIsInProgress; }

    /** Loads the @a data. */
    void loadData(QVariant &data);
    /** Wrapper for the method above.
      * Loads the data from the corresponding source. */
    virtual void loadOwnData() = 0;

    /** Saves the @a data. */
    void saveData(QVariant &data);
    /** Wrapper for the method above.
      * Saves the data to the corresponding source. */
    virtual void saveOwnData() = 0;

    /** Returns configuration access level. */
    ConfigurationAccessLevel configurationAccessLevel() const { return m_enmConfigurationAccessLevel; }
    /** Defines configuration access level. */
    void setConfigurationAccessLevel(ConfigurationAccessLevel enmConfigurationAccessLevel);

    /** Returns the dialog title extension. */
    virtual QString titleExtension() const = 0;
    /** Returns the dialog title. */
    virtual QString title() const = 0;

    /** Adds an item (page).
      * @param  strBigIcon     Brings the big icon.
      * @param  strMediumIcon  Brings the medium icon.
      * @param  strSmallIcon   Brings the small icon.
      * @param  cId            Brings the page ID.
      * @param  strLink        Brings the page link.
      * @param  pSettingsPage  Brings the page reference.
      * @param  iParentId      Brings the page parent ID. */
    void addItem(const QString &strBigIcon, const QString &strMediumIcon, const QString &strSmallIcon,
                 int cId, const QString &strLink,
                 UISettingsPage* pSettingsPage = 0, int iParentId = -1);

    /** Verifies data integrity between certain @a pSettingsPage and other pages. */
    virtual void recorrelate(UISettingsPage *pSettingsPage) { Q_UNUSED(pSettingsPage); }

    /** Validates data correctness using certain @a pValidator. */
    void revalidate(UIPageValidator *pValidator);
    /** Validates data correctness. */
    void revalidate();

    /** Holds the page selector instance. */
    UISettingsSelector *m_pSelector;
    /** Holds the page stack instance. */
    QStackedWidget     *m_pStack;

private slots:

    /** Handles validity change for certain @a pValidator. */
    void sltHandleValidityChange(UIPageValidator *pValidator);

    /** Handles hover enter for warning pane specified by @a pValidator. */
    void sltHandleWarningPaneHovered(UIPageValidator *pValidator);
    /** Handles hover leave for warning pane specified by @a pValidator. */
    void sltHandleWarningPaneUnhovered(UIPageValidator *pValidator);

    /** Updates watch this information depending on whether we have @a fGotFocus. */
    void sltUpdateWhatsThis(bool fGotFocus);
    /** Updates watch this information. */
    void sltUpdateWhatsThisNoFocus() { sltUpdateWhatsThis(false); }

private:

    /** Prepares all. */
    void prepare();

    /** Assigns validater for passed @a pPage. */
    void assignValidator(UISettingsPage *pPage);

    /** Holds whether the dialog is polished. */
    bool  m_fPolished;

    /** Holds configuration access level. */
    ConfigurationAccessLevel  m_enmConfigurationAccessLevel;

    /** Holds the serialize process instance. */
    UISettingsSerializer *m_pSerializeProcess;

    /** Holds whether the serialization is in progress. */
    bool  m_fSerializationIsInProgress;
    /** Holds whether there were no serialization errors. */
    bool  m_fSerializationClean;

    /** Holds the status-bar widget instance. */
    QStackedWidget *m_pStatusBar;
    /** Holds the process-bar widget instance. */
    QProgressBar   *m_pProcessBar;
    /** Holds the warning-pane instance. */
    UIWarningPane  *m_pWarningPane;

    /** Holds whether settings dialog is valid (no errors, can be warnings). */
    bool  m_fValid;
    /** Holds whether settings dialog is silent (no errors and no warnings). */
    bool  m_fSilent;

    /** Holds the warning hint. */
    QString  m_strWarningHint;

    /** Holds the what's this hover timer instance. */
    QTimer            *m_pWhatsThisTimer;
    /** Holds the what's this hover timer instance. */
    QPointer<QWidget>  m_pWhatsThisCandidate;

    /** Holds the map of settings pages. */
    QMap<int, int>  m_pages;

#ifdef VBOX_WS_MAC
    /** Holds the list of settings page sizes for animation purposes. */
    QList<QSize>  m_sizeList;
#endif
};

#endif /* !___UISettingsDialog_h___ */
