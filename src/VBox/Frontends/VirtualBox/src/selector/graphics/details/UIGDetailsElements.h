/* $Id$ */
/** @file
 * VBox Qt GUI - UIGDetailsElements class declaration.
 */

/*
 * Copyright (C) 2012-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIGDetailsElements_h__
#define __UIGDetailsElements_h__

/* GUI includes: */
#include "UIThreadPool.h"
#include "UIGDetailsElement.h"

/* Forward declarations: */
class UIGMachinePreview;
class CNetworkAdapter;


/* Element update task: */
class UIGDetailsUpdateTask : public UITask
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsUpdateTask(const CMachine &machine);
};

/* Details element interface: */
class UIGDetailsElementInterface : public UIGDetailsElement
{
    Q_OBJECT;

public:

    /* Constructor/destructor: */
    UIGDetailsElementInterface(UIGDetailsSet *pParent, DetailsElementType type, bool fOpened);

protected:

    /* Helper: Translate stuff: */
    virtual void retranslateUi();

    /* Helpers: Update stuff: */
    virtual void updateAppearance();
    virtual UITask* createUpdateTask() = 0;

private slots:

    /* Handler: Update stuff: */
    virtual void sltUpdateAppearanceFinished(UITask *pTask);

private:

    /* Variables: */
    UITask *m_pTask;
};


/* Element 'Preview': */
class UIGDetailsElementPreview : public UIGDetailsElement
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsElementPreview(UIGDetailsSet *pParent, bool fOpened);

private slots:

    /** Handles preview size-hint changes. */
    void sltPreviewSizeHintChanged();

private:

    /* Helper: Translate stuff: */
    virtual void retranslateUi();

    /* Helpers: Layout stuff: */
    int minimumWidthHint() const;
    int minimumHeightHint(bool fClosed) const;
    void updateLayout();

    /* Helper: Update stuff: */
    void updateAppearance();

    /* Variables: */
    UIGMachinePreview *m_pPreview;
};


/* Task 'General': */
class UIGDetailsUpdateTaskGeneral : public UIGDetailsUpdateTask
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsUpdateTaskGeneral(const CMachine &machine)
        : UIGDetailsUpdateTask(machine) {}

private:

    /* Helpers: Prepare stuff: */
    void run();
};

/* Element 'General': */
class UIGDetailsElementGeneral : public UIGDetailsElementInterface
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsElementGeneral(UIGDetailsSet *pParent, bool fOpened)
        : UIGDetailsElementInterface(pParent, DetailsElementType_General, fOpened) {}

private:

    /* Helper: Update stuff: */
    UITask* createUpdateTask() { return new UIGDetailsUpdateTaskGeneral(machine()); }
};


/* Task 'System': */
class UIGDetailsUpdateTaskSystem : public UIGDetailsUpdateTask
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsUpdateTaskSystem(const CMachine &machine)
        : UIGDetailsUpdateTask(machine) {}

private:

    /* Helpers: Prepare stuff: */
    void run();
};

/* Element 'System': */
class UIGDetailsElementSystem : public UIGDetailsElementInterface
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsElementSystem(UIGDetailsSet *pParent, bool fOpened)
        : UIGDetailsElementInterface(pParent, DetailsElementType_System, fOpened) {}

private:

    /* Helper: Update stuff: */
    UITask* createUpdateTask() { return new UIGDetailsUpdateTaskSystem(machine()); }
};


/* Task 'Display': */
class UIGDetailsUpdateTaskDisplay : public UIGDetailsUpdateTask
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsUpdateTaskDisplay(const CMachine &machine)
        : UIGDetailsUpdateTask(machine) {}

private:

    /* Helpers: Prepare stuff: */
    void run();
};

/* Element 'Display': */
class UIGDetailsElementDisplay : public UIGDetailsElementInterface
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsElementDisplay(UIGDetailsSet *pParent, bool fOpened)
        : UIGDetailsElementInterface(pParent, DetailsElementType_Display, fOpened) {}

private:

    /* Helper: Update stuff: */
    UITask* createUpdateTask() { return new UIGDetailsUpdateTaskDisplay(machine()); }
};


/* Task 'Storage': */
class UIGDetailsUpdateTaskStorage : public UIGDetailsUpdateTask
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsUpdateTaskStorage(const CMachine &machine)
        : UIGDetailsUpdateTask(machine) {}

private:

    /* Helpers: Prepare stuff: */
    void run();
};

/* Element 'Storage': */
class UIGDetailsElementStorage : public UIGDetailsElementInterface
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsElementStorage(UIGDetailsSet *pParent, bool fOpened)
        : UIGDetailsElementInterface(pParent, DetailsElementType_Storage, fOpened) {}

private:

    /* Helper: Update stuff: */
    UITask* createUpdateTask() { return new UIGDetailsUpdateTaskStorage(machine()); }
};


/* Task 'Audio': */
class UIGDetailsUpdateTaskAudio : public UIGDetailsUpdateTask
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsUpdateTaskAudio(const CMachine &machine)
        : UIGDetailsUpdateTask(machine) {}

private:

    /* Helpers: Prepare stuff: */
    void run();
};

/* Element 'Audio': */
class UIGDetailsElementAudio : public UIGDetailsElementInterface
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsElementAudio(UIGDetailsSet *pParent, bool fOpened)
        : UIGDetailsElementInterface(pParent, DetailsElementType_Audio, fOpened) {}

private:

    /* Helper: Update stuff: */
    UITask* createUpdateTask() { return new UIGDetailsUpdateTaskAudio(machine()); }
};


/* Task 'Network': */
class UIGDetailsUpdateTaskNetwork : public UIGDetailsUpdateTask
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsUpdateTaskNetwork(const CMachine &machine)
        : UIGDetailsUpdateTask(machine) {}

private:

    /* Helpers: Prepare stuff: */
    void run();
    static QString summarizeGenericProperties(const CNetworkAdapter &adapter);
};

/* Element 'Network': */
class UIGDetailsElementNetwork : public UIGDetailsElementInterface
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsElementNetwork(UIGDetailsSet *pParent, bool fOpened)
        : UIGDetailsElementInterface(pParent, DetailsElementType_Network, fOpened) {}

private:

    /* Helper: Update stuff: */
    UITask* createUpdateTask() { return new UIGDetailsUpdateTaskNetwork(machine()); }
};


/* Task 'Serial': */
class UIGDetailsUpdateTaskSerial : public UIGDetailsUpdateTask
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsUpdateTaskSerial(const CMachine &machine)
        : UIGDetailsUpdateTask(machine) {}

private:

    /* Helpers: Prepare stuff: */
    void run();
};

/* Element 'Serial': */
class UIGDetailsElementSerial : public UIGDetailsElementInterface
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsElementSerial(UIGDetailsSet *pParent, bool fOpened)
        : UIGDetailsElementInterface(pParent, DetailsElementType_Serial, fOpened) {}

private:

    /* Helper: Update stuff: */
    UITask* createUpdateTask() { return new UIGDetailsUpdateTaskSerial(machine()); }
};


#ifdef VBOX_WITH_PARALLEL_PORTS
/* Task 'Parallel': */
class UIGDetailsUpdateTaskParallel : public UIGDetailsUpdateTask
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsUpdateTaskParallel(const CMachine &machine)
        : UIGDetailsUpdateTask(machine) {}

private:

    /* Helpers: Prepare stuff: */
    void run();
};

/* Element 'Parallel': */
class UIGDetailsElementParallel : public UIGDetailsElementInterface
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsElementParallel(UIGDetailsSet *pParent, bool fOpened)
        : UIGDetailsElementInterface(pParent, DetailsElementType_Parallel, fOpened) {}

private:

    /* Helper: Update stuff: */
    UITask* createUpdateTask() { return new UIGDetailsUpdateTaskParallel(machine()); }
};
#endif /* VBOX_WITH_PARALLEL_PORTS */


/* Task 'USB': */
class UIGDetailsUpdateTaskUSB : public UIGDetailsUpdateTask
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsUpdateTaskUSB(const CMachine &machine)
        : UIGDetailsUpdateTask(machine) {}

private:

    /* Helpers: Prepare stuff: */
    void run();
};

/* Element 'USB': */
class UIGDetailsElementUSB : public UIGDetailsElementInterface
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsElementUSB(UIGDetailsSet *pParent, bool fOpened)
        : UIGDetailsElementInterface(pParent, DetailsElementType_USB, fOpened) {}

private:

    /* Helper: Update stuff: */
    UITask* createUpdateTask() { return new UIGDetailsUpdateTaskUSB(machine()); }
};


/* Task 'SF': */
class UIGDetailsUpdateTaskSF : public UIGDetailsUpdateTask
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsUpdateTaskSF(const CMachine &machine)
        : UIGDetailsUpdateTask(machine) {}

private:

    /* Helpers: Prepare stuff: */
    void run();
};

/* Element 'SF': */
class UIGDetailsElementSF : public UIGDetailsElementInterface
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsElementSF(UIGDetailsSet *pParent, bool fOpened)
        : UIGDetailsElementInterface(pParent, DetailsElementType_SF, fOpened) {}

private:

    /* Helper: Update stuff: */
    UITask* createUpdateTask() { return new UIGDetailsUpdateTaskSF(machine()); }
};


/* Task 'UI': */
class UIGDetailsUpdateTaskUI : public UIGDetailsUpdateTask
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsUpdateTaskUI(const CMachine &machine)
        : UIGDetailsUpdateTask(machine) {}

private:

    /* Helpers: Prepare stuff: */
    void run();
};

/* Element 'UI': */
class UIGDetailsElementUI : public UIGDetailsElementInterface
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsElementUI(UIGDetailsSet *pParent, bool fOpened)
        : UIGDetailsElementInterface(pParent, DetailsElementType_UI, fOpened) {}

private:

    /* Helper: Update stuff: */
    UITask* createUpdateTask() { return new UIGDetailsUpdateTaskUI(machine()); }
};


/* Task 'Description': */
class UIGDetailsUpdateTaskDescription : public UIGDetailsUpdateTask
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsUpdateTaskDescription(const CMachine &machine)
        : UIGDetailsUpdateTask(machine) {}

private:

    /* Helpers: Prepare stuff: */
    void run();
};

/* Element 'Description': */
class UIGDetailsElementDescription : public UIGDetailsElementInterface
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsElementDescription(UIGDetailsSet *pParent, bool fOpened)
        : UIGDetailsElementInterface(pParent, DetailsElementType_Description, fOpened) {}

private:

    /* Helper: Update stuff: */
    UITask* createUpdateTask() { return new UIGDetailsUpdateTaskDescription(machine()); }
};

#endif /* __UIGDetailsElements_h__ */

