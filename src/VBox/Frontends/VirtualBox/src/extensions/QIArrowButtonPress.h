/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QIArrowButtonPress class declaration.
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

#ifndef ___QIArrowButtonPress_h___
#define ___QIArrowButtonPress_h___

/* GUI includes: */
#include "QIRichToolButton.h"
#include "QIWithRetranslateUI.h"
#include "UILibraryDefs.h"

/** QIRichToolButton extension
  * representing arrow tool-button with text-label,
  * can be used as back/next buttons in various places. */
class SHARED_LIBRARY_STUFF QIArrowButtonPress : public QIWithRetranslateUI<QIRichToolButton>
{
    Q_OBJECT;

public:

    /** Button types. */
    enum ButtonType { ButtonType_Back, ButtonType_Next };

    /** Constructs button passing @a pParent to the base-class.
      * @param  enmButtonType  Brings which type of the button it is. */
    QIArrowButtonPress(ButtonType enmButtonType, QWidget *pParent = 0);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Handles key-press @a pEvent. */
    virtual void keyPressEvent(QKeyEvent *pEvent) /* override */;

private:

    /** Holds the button-type. */
    ButtonType m_enmButtonType;
};

#endif /* !___QIArrowButtonPress_h___ */
