/* $Id$ */
/** @file
 * VBox Qt GUI - UIGInformationElement[Name] classes declaration.
 */

/*
 * Copyright (C) 2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIGInformationElements_h___
#define ___UIGInformationElements_h___

/* GUI includes: */
#include "UIThreadPool.h"
#include "UIGInformationElement.h"

/* Forward declarations: */
class UIGInformationPreview;
class CNetworkAdapter;


/** UITask extension used as update task for the details-element. */
class UIGInformationUpdateTask : public UITask
{
    Q_OBJECT;

public:

    /** Constructs update task taking @a machine as data. */
    UIGInformationUpdateTask(const CMachine &machine);
};

/** UIGInformationElement extension used as a wrapping interface to
  * extend base-class with async functionality performed by the COM worker-threads. */
class UIGInformationElementInterface : public UIGInformationElement
{
    Q_OBJECT;

public:

    /** Constructs details-element interface for passed @a pParent set.
      * @param type    brings the details-element type this element belongs to.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIGInformationElementInterface(UIGInformationSet *pParent, InformationElementType type, bool fOpened);

protected:

    /** Performs translation. */
    virtual void retranslateUi();

    /** Updates appearance. */
    virtual void updateAppearance();

    /** Creates update task. */
    virtual UITask* createUpdateTask() = 0;

private slots:

    /** Handles the signal about update @a pTask is finished. */
    virtual void sltUpdateAppearanceFinished(UITask *pTask);

private:

    /** Holds the instance of the update task. */
    UITask *m_pTask;
};


/** UIGInformationElementInterface extension for the details-element type 'Preview'. */
class UIGInformationElementPreview : public UIGInformationElement
{
    Q_OBJECT;

public:

    /** Constructs details-element interface for passed @a pParent set.
      * @param fOpened brings whether the details-element should be opened. */
    UIGInformationElementPreview(UIGInformationSet *pParent, bool fOpened);

private slots:

    /** Handles preview size-hint changes. */
    void sltPreviewSizeHintChanged();

private:

    /** Performs translation. */
    virtual void retranslateUi();

    /** Returns minimum width hint. */
    int minimumWidthHint() const;
    /** Returns minimum height hint.
      * @param fClosed allows to specify whether the hint should
      *                be calculated for the closed element. */
    int minimumHeightHint(bool fClosed) const;
    /** Updates layout. */
    void updateLayout();

    /** Updates appearance. */
    void updateAppearance();

    /** Holds the instance of VM preview. */
    UIGInformationPreview *m_pPreview;
};


/** UITask extension used as update task for the details-element type 'General'. */
class UIGInformationUpdateTaskGeneral : public UIGInformationUpdateTask
{
    Q_OBJECT;

public:

    /** Constructs update task passing @a machine to the base-class. */
    UIGInformationUpdateTaskGeneral(const CMachine &machine)
        : UIGInformationUpdateTask(machine) {}

private:

    /** Contains update task body. */
    void run();
};

/** UIGInformationElementInterface extension for the details-element type 'General'. */
class UIGInformationElementGeneral : public UIGInformationElementInterface
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIGInformationElementGeneral(UIGInformationSet *pParent, bool fOpened)
        : UIGInformationElementInterface(pParent, InformationElementType_General, fOpened) {}

private:

    /** Creates update task for this element. */
    UITask* createUpdateTask() { return new UIGInformationUpdateTaskGeneral(machine()); }
};


/** UITask extension used as update task for the details-element type 'System'. */
class UIGInformationUpdateTaskSystem : public UIGInformationUpdateTask
{
    Q_OBJECT;

public:

    /** Constructs update task passing @a machine to the base-class. */
    UIGInformationUpdateTaskSystem(const CMachine &machine)
        : UIGInformationUpdateTask(machine) {}

private:

    /** Contains update task body. */
    void run();
};

/** UIGInformationElementInterface extension for the details-element type 'System'. */
class UIGInformationElementSystem : public UIGInformationElementInterface
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIGInformationElementSystem(UIGInformationSet *pParent, bool fOpened)
        : UIGInformationElementInterface(pParent, InformationElementType_System, fOpened) {}

private:

    /** Creates update task for this element. */
    UITask* createUpdateTask() { return new UIGInformationUpdateTaskSystem(machine()); }
};


/** UITask extension used as update task for the details-element type 'Display'. */
class UIGInformationUpdateTaskDisplay : public UIGInformationUpdateTask
{
    Q_OBJECT;

public:

    /** Constructs update task passing @a machine to the base-class. */
    UIGInformationUpdateTaskDisplay(const CMachine &machine)
        : UIGInformationUpdateTask(machine) {}

private:

    /** Contains update task body. */
    void run();
};

/** UIGInformationElementInterface extension for the details-element type 'Display'. */
class UIGInformationElementDisplay : public UIGInformationElementInterface
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIGInformationElementDisplay(UIGInformationSet *pParent, bool fOpened)
        : UIGInformationElementInterface(pParent, InformationElementType_Display, fOpened) {}

private:

    /** Creates update task for this element. */
    UITask* createUpdateTask() { return new UIGInformationUpdateTaskDisplay(machine()); }
};


/** UITask extension used as update task for the details-element type 'Storage'. */
class UIGInformationUpdateTaskStorage : public UIGInformationUpdateTask
{
    Q_OBJECT;

public:

    /** Constructs update task passing @a machine to the base-class. */
    UIGInformationUpdateTaskStorage(const CMachine &machine)
        : UIGInformationUpdateTask(machine) {}

private:

    /** Contains update task body. */
    void run();
};

/** UIGInformationElementInterface extension for the details-element type 'Storage'. */
class UIGInformationElementStorage : public UIGInformationElementInterface
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIGInformationElementStorage(UIGInformationSet *pParent, bool fOpened)
        : UIGInformationElementInterface(pParent, InformationElementType_Storage, fOpened) {}

private:

    /** Creates update task for this element. */
    UITask* createUpdateTask() { return new UIGInformationUpdateTaskStorage(machine()); }
};


/** UITask extension used as update task for the details-element type 'Audio'. */
class UIGInformationUpdateTaskAudio : public UIGInformationUpdateTask
{
    Q_OBJECT;

public:

    /** Constructs update task passing @a machine to the base-class. */
    UIGInformationUpdateTaskAudio(const CMachine &machine)
        : UIGInformationUpdateTask(machine) {}

private:

    /** Contains update task body. */
    void run();
};

/** UIGInformationElementInterface extension for the details-element type 'Audio'. */
class UIGInformationElementAudio : public UIGInformationElementInterface
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIGInformationElementAudio(UIGInformationSet *pParent, bool fOpened)
        : UIGInformationElementInterface(pParent, InformationElementType_Audio, fOpened) {}

private:

    /** Creates update task for this element. */
    UITask* createUpdateTask() { return new UIGInformationUpdateTaskAudio(machine()); }
};


/** UITask extension used as update task for the details-element type 'Network'. */
class UIGInformationUpdateTaskNetwork : public UIGInformationUpdateTask
{
    Q_OBJECT;

public:

    /** Constructs update task passing @a machine to the base-class. */
    UIGInformationUpdateTaskNetwork(const CMachine &machine)
        : UIGInformationUpdateTask(machine) {}

private:

    /** Contains update task body. */
    void run();

    /** Summarizes generic properties. */
    static QString summarizeGenericProperties(const CNetworkAdapter &adapter);
};

/** UIGInformationElementInterface extension for the details-element type 'Network'. */
class UIGInformationElementNetwork : public UIGInformationElementInterface
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIGInformationElementNetwork(UIGInformationSet *pParent, bool fOpened)
        : UIGInformationElementInterface(pParent, InformationElementType_Network, fOpened) {}

private:

    /** Creates update task for this element. */
    UITask* createUpdateTask() { return new UIGInformationUpdateTaskNetwork(machine()); }
};


/** UITask extension used as update task for the details-element type 'Serial'. */
class UIGInformationUpdateTaskSerial : public UIGInformationUpdateTask
{
    Q_OBJECT;

public:

    /** Constructs update task passing @a machine to the base-class. */
    UIGInformationUpdateTaskSerial(const CMachine &machine)
        : UIGInformationUpdateTask(machine) {}

private:

    /** Contains update task body. */
    void run();
};

/** UIGInformationElementInterface extension for the details-element type 'Serial'. */
class UIGInformationElementSerial : public UIGInformationElementInterface
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIGInformationElementSerial(UIGInformationSet *pParent, bool fOpened)
        : UIGInformationElementInterface(pParent, InformationElementType_Serial, fOpened) {}

private:

    /** Creates update task for this element. */
    UITask* createUpdateTask() { return new UIGInformationUpdateTaskSerial(machine()); }
};


#ifdef VBOX_WITH_PARALLEL_PORTS
/** UITask extension used as update task for the details-element type 'Parallel'. */
class UIGInformationUpdateTaskParallel : public UIGInformationUpdateTask
{
    Q_OBJECT;

public:

    /** Constructs update task passing @a machine to the base-class. */
    UIGInformationUpdateTaskParallel(const CMachine &machine)
        : UIGInformationUpdateTask(machine) {}

private:

    /** Contains update task body. */
    void run();
};

/** UIGInformationElementInterface extension for the details-element type 'Parallel'. */
class UIGInformationElementParallel : public UIGInformationElementInterface
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIGInformationElementParallel(UIGInformationSet *pParent, bool fOpened)
        : UIGInformationElementInterface(pParent, InformationElementType_Parallel, fOpened) {}

private:

    /** Creates update task for this element. */
    UITask* createUpdateTask() { return new UIGInformationUpdateTaskParallel(machine()); }
};
#endif /* VBOX_WITH_PARALLEL_PORTS */


/** UITask extension used as update task for the details-element type 'USB'. */
class UIGInformationUpdateTaskUSB : public UIGInformationUpdateTask
{
    Q_OBJECT;

public:

    /** Constructs update task passing @a machine to the base-class. */
    UIGInformationUpdateTaskUSB(const CMachine &machine)
        : UIGInformationUpdateTask(machine) {}

private:

    /** Contains update task body. */
    void run();
};

/** UIGInformationElementInterface extension for the details-element type 'USB'. */
class UIGInformationElementUSB : public UIGInformationElementInterface
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIGInformationElementUSB(UIGInformationSet *pParent, bool fOpened)
        : UIGInformationElementInterface(pParent, InformationElementType_USB, fOpened) {}

private:

    /** Creates update task for this element. */
    UITask* createUpdateTask() { return new UIGInformationUpdateTaskUSB(machine()); }
};


/** UITask extension used as update task for the details-element type 'SF'. */
class UIGInformationUpdateTaskSF : public UIGInformationUpdateTask
{
    Q_OBJECT;

public:

    /** Constructs update task passing @a machine to the base-class. */
    UIGInformationUpdateTaskSF(const CMachine &machine)
        : UIGInformationUpdateTask(machine) {}

private:

    /** Contains update task body. */
    void run();
};

/** UIGInformationElementInterface extension for the details-element type 'SF'. */
class UIGInformationElementSF : public UIGInformationElementInterface
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIGInformationElementSF(UIGInformationSet *pParent, bool fOpened)
        : UIGInformationElementInterface(pParent, InformationElementType_SF, fOpened) {}

private:

    /** Creates update task for this element. */
    UITask* createUpdateTask() { return new UIGInformationUpdateTaskSF(machine()); }
};


/** UITask extension used as update task for the details-element type 'UI'. */
class UIGInformationUpdateTaskUI : public UIGInformationUpdateTask
{
    Q_OBJECT;

public:

    /** Constructs update task passing @a machine to the base-class. */
    UIGInformationUpdateTaskUI(const CMachine &machine)
        : UIGInformationUpdateTask(machine) {}

private:

    /** Contains update task body. */
    void run();
};

/** UIGInformationElementInterface extension for the details-element type 'UI'. */
class UIGInformationElementUI : public UIGInformationElementInterface
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIGInformationElementUI(UIGInformationSet *pParent, bool fOpened)
        : UIGInformationElementInterface(pParent, InformationElementType_UI, fOpened) {}

private:

    /** Creates update task for this element. */
    UITask* createUpdateTask() { return new UIGInformationUpdateTaskUI(machine()); }
};


/** UITask extension used as update task for the details-element type 'Description'. */
class UIGInformationUpdateTaskDescription : public UIGInformationUpdateTask
{
    Q_OBJECT;

public:

    /** Constructs update task passing @a machine to the base-class. */
    UIGInformationUpdateTaskDescription(const CMachine &machine)
        : UIGInformationUpdateTask(machine) {}

private:

    /** Contains update task body. */
    void run();
};

/** UIGInformationElementInterface extension for the details-element type 'Description'. */
class UIGInformationElementDescription : public UIGInformationElementInterface
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIGInformationElementDescription(UIGInformationSet *pParent, bool fOpened)
        : UIGInformationElementInterface(pParent, InformationElementType_Description, fOpened) {}

private:

    /** Creates update task for this element. */
    UITask* createUpdateTask() { return new UIGInformationUpdateTaskDescription(machine()); }
};

/** UITask extension used as update task for the details-element type 'Description'. */
class UIGInformationUpdateTaskRuntimeAttributes : public UIGInformationUpdateTask
{
    Q_OBJECT;

public:

    /** Constructs update task passing @a machine to the base-class. */
    UIGInformationUpdateTaskRuntimeAttributes(const CMachine &machine)
        : UIGInformationUpdateTask(machine) {}

private:

    /** Contains update task body. */
    void run();
};

/** UIGInformationElementInterface extension for the details-element type 'Description'. */
class UIGInformationElementRuntimeAttributes : public UIGInformationElementInterface
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIGInformationElementRuntimeAttributes(UIGInformationSet *pParent, bool fOpened)
        : UIGInformationElementInterface(pParent, InformationElementType_RuntimeAttributes, fOpened) {}

private:

    /** Creates update task for this element. */
    UITask* createUpdateTask() { return new UIGInformationUpdateTaskRuntimeAttributes(machine()); }
};

#endif /* !___UIGInformationElements_h___ */

