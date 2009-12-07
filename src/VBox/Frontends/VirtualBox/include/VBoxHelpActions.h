/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxHelpActions class declaration
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef __VBoxHelpActions_h__
#define __VBoxHelpActions_h__

/* Qt includes */
#include <QObject>

class QMenu;
class QAction;

/**
 * Help Menu action container.
 *
 * Contains actions for all help menu items and methods to insert them to a
 * QMenu and to perform NLS string translation.
 *
 * Instances of this class are to be created as members of QWidget classes that
 * need a Help menu. The containing class usually passes itself as an argument
 * to the #setup() method and then calls #addTo() to add actions to its Help
 * menu. The #retranslateUi() method is called when it is necessary to
 * re-translate all action NLS according to the current language.
 */
struct VBoxHelpActions
{
    VBoxHelpActions()
        : contentsAction (NULL), webAction (NULL)
        , resetMessagesAction (NULL), registerAction (NULL)
        , updateAction (NULL), aboutAction (NULL)
        {}

    void setup (QObject *aParent);
    void addTo (QMenu *aMenu);
    void retranslateUi();

    QAction *contentsAction;
    QAction *webAction;
    QAction *resetMessagesAction;
    QAction *registerAction;
    QAction *updateAction;
    QAction *aboutAction;
};

#endif /* __VBoxHelpActions_h__ */

