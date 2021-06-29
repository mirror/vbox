/* $Id$ */
/** @file
 * VBox Qt GUI - UINativeWizard class declaration.
 */

/*
 * Copyright (C) 2009-2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_wizards_UINativeWizard_h
#define FEQT_INCLUDED_SRC_wizards_UINativeWizard_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QMap>
#include <QPointer>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UIExtraDataDefs.h"
#include "UILibraryDefs.h"

/* Forward declarations: */
class QLabel;
class QPushButton;
class QStackedWidget;
class UINativeWizardPage;

/** Native wizard buttons. */
enum WizardButtonType
{
    WizardButtonType_Invalid,
    WizardButtonType_Help,
    WizardButtonType_Expert,
    WizardButtonType_Back,
    WizardButtonType_Next,
    WizardButtonType_Cancel,
    WizardButtonType_Max,
};
Q_DECLARE_METATYPE(WizardButtonType);

#ifdef VBOX_WS_MAC
/** QWidget-based QFrame analog with one particular purpose to
  * simulate macOS wizard frame without influencing palette hierarchy. */
class SHARED_LIBRARY_STUFF UIFrame : public QWidget
{
    Q_OBJECT;

public:

    /** Constructs UIFrame passing @a pParent to the base-class. */
    UIFrame(QWidget *pParent);

protected:

    /** Handles paint @a pEvent. */
    virtual void paintEvent(QPaintEvent *pEvent) /* final */;
};
#endif /* VBOX_WS_MAC */

/** QDialog extension with advanced functionality emulating QWizard behavior. */
class SHARED_LIBRARY_STUFF UINativeWizard : public QIWithRetranslateUI<QDialog>
{
    Q_OBJECT;

public slots:

    /** Executes wizard in window modal mode.
      * @note You shouldn't have to override it! */
    virtual int exec() /* final */;

protected:

    /** Constructs wizard passing @a pParent to the base-class.
      * @param  enmType         Brings the wizard type.
      * @param  enmMode         Brings the wizard mode.
      * @param  strHelpHashtag  Brings the wizard help hashtag. */
    UINativeWizard(QWidget *pParent,
                   WizardType enmType,
                   WizardMode enmMode = WizardMode_Auto,
                   const QString &strHelpHashtag = QString());

    /** Returns wizard type. */
    WizardType type() const { return m_enmType; }
    /** Returns wizard mode. */
    WizardMode mode() const { return m_enmMode; }

    /** Returns wizard button of specified @a enmType. */
    QPushButton *wizardButton(const WizardButtonType &enmType) const;
    /** Defines @a strName for wizard button of specified @a enmType. */
    void setWizardButtonName(const WizardButtonType &enmType, const QString &strName);

    /** Defines pixmap @a strName. */
    void setPixmapName(const QString &strName);

    /** Appends wizard @a pPage.
      * @returns assigned page index. */
    int addPage(UINativeWizardPage *pPage);
    /** Populates pages.
      * @note In your subclasses you should add
      *       pages via addPage declared above. */
    virtual void populatePages() = 0;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** Handles current-page change to page with @a iIndex. */
    void sltCurrentIndexChanged(int iIndex = -1);
    /** Handles page validity changes. */
    void sltCompleteChanged();

    /** Toggles between basic and expert modes. */
    void sltExpert();
    /** Switches to previous page. */
    void sltPrevious();
    /** Switches to next page. */
    void sltNext();

private:

    /** Prepares all. */
    void prepare();
    /** Cleanups all. */
    void cleanup();
    /** Inits all. */
    void init();

    /** Performs pages translation. */
    void retranslatePages();

    /** Resizes wizard to golden ratio. */
    void resizeToGoldenRatio();
#ifdef VBOX_WS_MAC
    /** Assigns wizard background. */
    void assignBackground();
#else
    /** Assigns wizard watermark. */
    void assignWatermark();
#endif

    /** Holds the wizard type. */
    WizardType  m_enmType;
    /** Holds the wizard mode. */
    WizardMode  m_enmMode;
    /** Holds the wizard help hashtag. */
    QString     m_strHelpHashtag;
    /** Holds the pixmap name. */
    QString     m_strPixmapName;

    /** Holds the pixmap label instance. */
    QLabel                               *m_pLabelPixmap;
#ifdef VBOX_WS_MAC
    QLabel                               *m_pLabelPageTitle;
#endif
    /** Holds the widget-stack instance. */
    QStackedWidget                       *m_pWidgetStack;
    /** Holds button instance map. */
    QMap<WizardButtonType, QPushButton*>  m_buttons;
};

/** Native wizard interface pointer. */
typedef QPointer<UINativeWizard> UINativeWizardPointer;

#endif /* !FEQT_INCLUDED_SRC_wizards_UINativeWizard_h */
