/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestControlWidget class declaration.
 */

/*
 * Copyright (C) 2016-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIGuestControlWidget_h___
#define ___UIGuestControlWidget_h___

/* Qt includes: */
#include <QWidget>

/* COM includes: */
#include "COMEnums.h"
#include "CGuest.h"
#include "CEventListener.h"

/* GUI includes: */
#include "QIManagerDialog.h"
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QITabWidget;
class UIActionPool;
class UIGuestControlFileManager;
class UIGuestProcessControlWidget;

class UIGuestControlWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /** Constructs the Guet Control Widget by passing @a pParent to QWidget base-class constructor.
      * @param  enmEmbedding  Brings the type of widget embedding.
      * @param  pActionPool   Brings the action-pool reference.
      * @param  fShowToolbar  Brings whether we should create/show toolbar.
      * @param  comMachine    Brings the machine for which VM Log-Viewer is requested. */
    UIGuestControlWidget(EmbedTo enmEmbedding, UIActionPool *pActionPool,
                         const CGuest &comGuest,
                         bool fShowToolbar = true, QWidget *pParent = 0);
    /** Destructs the VM Log-Viewer. */
    ~UIGuestControlWidget();

protected:

    virtual void retranslateUi() /* override */;

private:

    enum TabIndex
    {
        TabIndex_FileManager,
        TabIndex_ProcessControl,
        TabIndex_Max
    };

    void prepare();
        /** Holds the widget's embedding type. */
    const EmbedTo m_enmEmbedding;
    UIActionPool *m_pActionPool;
    CGuest       m_comGuest;

    QITabWidget                  *m_pTabWidget;
    UIGuestProcessControlWidget *m_pProcessControlWidget;
    UIGuestControlFileManager   *m_pFileManager;
};

#endif /* !___UIGuestControlWidget_h___ */
