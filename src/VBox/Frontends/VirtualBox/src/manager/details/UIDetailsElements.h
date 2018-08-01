/* $Id$ */
/** @file
 * VBox Qt GUI - UIDetailsElement[Name] classes declaration.
 */

/*
 * Copyright (C) 2012-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIDetailsElements_h___
#define ___UIDetailsElements_h___

/* GUI includes: */
#include "UIThreadPool.h"
#include "UIDetailsElement.h"

/* Forward declarations: */
class UIMachinePreview;
class CNetworkAdapter;


/** UITask extension used as update task for the details-element. */
class UIDetailsUpdateTask : public UITask
{
    Q_OBJECT;

public:

    /** Constructs update task taking @a machine as data. */
    UIDetailsUpdateTask(const CMachine &machine);
};

/** UIDetailsElement extension used as a wrapping interface to
  * extend base-class with async functionality performed by the COM worker-threads. */
class UIDetailsElementInterface : public UIDetailsElement
{
    Q_OBJECT;

public:

    /** Constructs details-element interface for passed @a pParent set.
      * @param type    brings the details-element type this element belongs to.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIDetailsElementInterface(UIDetailsSet *pParent, DetailsElementType type, bool fOpened);

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


/** UIDetailsElementInterface extension for the details-element type 'Preview'. */
class UIDetailsElementPreview : public UIDetailsElement
{
    Q_OBJECT;

public:

    /** Constructs details-element interface for passed @a pParent set.
      * @param fOpened brings whether the details-element should be opened. */
    UIDetailsElementPreview(UIDetailsSet *pParent, bool fOpened);

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
    UIMachinePreview *m_pPreview;
};


/** UITask extension used as update task for the details-element type 'General'. */
class UIDetailsUpdateTaskGeneral : public UIDetailsUpdateTask
{
    Q_OBJECT;

public:

    /** Constructs update task passing @a machine to the base-class. */
    UIDetailsUpdateTaskGeneral(const CMachine &machine)
        : UIDetailsUpdateTask(machine) {}

private:

    /** Contains update task body. */
    void run();
};

/** UIDetailsElementInterface extension for the details-element type 'General'. */
class UIDetailsElementGeneral : public UIDetailsElementInterface
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIDetailsElementGeneral(UIDetailsSet *pParent, bool fOpened)
        : UIDetailsElementInterface(pParent, DetailsElementType_General, fOpened) {}

private:

    /** Creates update task for this element. */
    UITask* createUpdateTask() { return new UIDetailsUpdateTaskGeneral(machine()); }
};


/** UITask extension used as update task for the details-element type 'System'. */
class UIDetailsUpdateTaskSystem : public UIDetailsUpdateTask
{
    Q_OBJECT;

public:

    /** Constructs update task passing @a machine to the base-class. */
    UIDetailsUpdateTaskSystem(const CMachine &machine)
        : UIDetailsUpdateTask(machine) {}

private:

    /** Contains update task body. */
    void run();
};

/** UIDetailsElementInterface extension for the details-element type 'System'. */
class UIDetailsElementSystem : public UIDetailsElementInterface
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIDetailsElementSystem(UIDetailsSet *pParent, bool fOpened)
        : UIDetailsElementInterface(pParent, DetailsElementType_System, fOpened) {}

private:

    /** Creates update task for this element. */
    UITask* createUpdateTask() { return new UIDetailsUpdateTaskSystem(machine()); }
};


/** UITask extension used as update task for the details-element type 'Display'. */
class UIDetailsUpdateTaskDisplay : public UIDetailsUpdateTask
{
    Q_OBJECT;

public:

    /** Constructs update task passing @a machine to the base-class. */
    UIDetailsUpdateTaskDisplay(const CMachine &machine)
        : UIDetailsUpdateTask(machine) {}

private:

    /** Contains update task body. */
    void run();
};

/** UIDetailsElementInterface extension for the details-element type 'Display'. */
class UIDetailsElementDisplay : public UIDetailsElementInterface
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIDetailsElementDisplay(UIDetailsSet *pParent, bool fOpened)
        : UIDetailsElementInterface(pParent, DetailsElementType_Display, fOpened) {}

private:

    /** Creates update task for this element. */
    UITask* createUpdateTask() { return new UIDetailsUpdateTaskDisplay(machine()); }
};


/** UITask extension used as update task for the details-element type 'Storage'. */
class UIDetailsUpdateTaskStorage : public UIDetailsUpdateTask
{
    Q_OBJECT;

public:

    /** Constructs update task passing @a machine to the base-class. */
    UIDetailsUpdateTaskStorage(const CMachine &machine)
        : UIDetailsUpdateTask(machine) {}

private:

    /** Contains update task body. */
    void run();
};

/** UIDetailsElementInterface extension for the details-element type 'Storage'. */
class UIDetailsElementStorage : public UIDetailsElementInterface
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIDetailsElementStorage(UIDetailsSet *pParent, bool fOpened)
        : UIDetailsElementInterface(pParent, DetailsElementType_Storage, fOpened) {}

private:

    /** Creates update task for this element. */
    UITask* createUpdateTask() { return new UIDetailsUpdateTaskStorage(machine()); }
};


/** UITask extension used as update task for the details-element type 'Audio'. */
class UIDetailsUpdateTaskAudio : public UIDetailsUpdateTask
{
    Q_OBJECT;

public:

    /** Constructs update task passing @a machine to the base-class. */
    UIDetailsUpdateTaskAudio(const CMachine &machine)
        : UIDetailsUpdateTask(machine) {}

private:

    /** Contains update task body. */
    void run();
};

/** UIDetailsElementInterface extension for the details-element type 'Audio'. */
class UIDetailsElementAudio : public UIDetailsElementInterface
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIDetailsElementAudio(UIDetailsSet *pParent, bool fOpened)
        : UIDetailsElementInterface(pParent, DetailsElementType_Audio, fOpened) {}

private:

    /** Creates update task for this element. */
    UITask* createUpdateTask() { return new UIDetailsUpdateTaskAudio(machine()); }
};


/** UITask extension used as update task for the details-element type 'Network'. */
class UIDetailsUpdateTaskNetwork : public UIDetailsUpdateTask
{
    Q_OBJECT;

public:

    /** Constructs update task passing @a machine to the base-class. */
    UIDetailsUpdateTaskNetwork(const CMachine &machine)
        : UIDetailsUpdateTask(machine) {}

private:

    /** Contains update task body. */
    void run();

    /** Summarizes generic properties. */
    static QString summarizeGenericProperties(const CNetworkAdapter &adapter);
};

/** UIDetailsElementInterface extension for the details-element type 'Network'. */
class UIDetailsElementNetwork : public UIDetailsElementInterface
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIDetailsElementNetwork(UIDetailsSet *pParent, bool fOpened)
        : UIDetailsElementInterface(pParent, DetailsElementType_Network, fOpened) {}

private:

    /** Creates update task for this element. */
    UITask* createUpdateTask() { return new UIDetailsUpdateTaskNetwork(machine()); }
};


/** UITask extension used as update task for the details-element type 'Serial'. */
class UIDetailsUpdateTaskSerial : public UIDetailsUpdateTask
{
    Q_OBJECT;

public:

    /** Constructs update task passing @a machine to the base-class. */
    UIDetailsUpdateTaskSerial(const CMachine &machine)
        : UIDetailsUpdateTask(machine) {}

private:

    /** Contains update task body. */
    void run();
};

/** UIDetailsElementInterface extension for the details-element type 'Serial'. */
class UIDetailsElementSerial : public UIDetailsElementInterface
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIDetailsElementSerial(UIDetailsSet *pParent, bool fOpened)
        : UIDetailsElementInterface(pParent, DetailsElementType_Serial, fOpened) {}

private:

    /** Creates update task for this element. */
    UITask* createUpdateTask() { return new UIDetailsUpdateTaskSerial(machine()); }
};


/** UITask extension used as update task for the details-element type 'USB'. */
class UIDetailsUpdateTaskUSB : public UIDetailsUpdateTask
{
    Q_OBJECT;

public:

    /** Constructs update task passing @a machine to the base-class. */
    UIDetailsUpdateTaskUSB(const CMachine &machine)
        : UIDetailsUpdateTask(machine) {}

private:

    /** Contains update task body. */
    void run();
};

/** UIDetailsElementInterface extension for the details-element type 'USB'. */
class UIDetailsElementUSB : public UIDetailsElementInterface
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIDetailsElementUSB(UIDetailsSet *pParent, bool fOpened)
        : UIDetailsElementInterface(pParent, DetailsElementType_USB, fOpened) {}

private:

    /** Creates update task for this element. */
    UITask* createUpdateTask() { return new UIDetailsUpdateTaskUSB(machine()); }
};


/** UITask extension used as update task for the details-element type 'SF'. */
class UIDetailsUpdateTaskSF : public UIDetailsUpdateTask
{
    Q_OBJECT;

public:

    /** Constructs update task passing @a machine to the base-class. */
    UIDetailsUpdateTaskSF(const CMachine &machine)
        : UIDetailsUpdateTask(machine) {}

private:

    /** Contains update task body. */
    void run();
};

/** UIDetailsElementInterface extension for the details-element type 'SF'. */
class UIDetailsElementSF : public UIDetailsElementInterface
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIDetailsElementSF(UIDetailsSet *pParent, bool fOpened)
        : UIDetailsElementInterface(pParent, DetailsElementType_SF, fOpened) {}

private:

    /** Creates update task for this element. */
    UITask* createUpdateTask() { return new UIDetailsUpdateTaskSF(machine()); }
};


/** UITask extension used as update task for the details-element type 'UI'. */
class UIDetailsUpdateTaskUI : public UIDetailsUpdateTask
{
    Q_OBJECT;

public:

    /** Constructs update task passing @a machine to the base-class. */
    UIDetailsUpdateTaskUI(const CMachine &machine)
        : UIDetailsUpdateTask(machine) {}

private:

    /** Contains update task body. */
    void run();
};

/** UIDetailsElementInterface extension for the details-element type 'UI'. */
class UIDetailsElementUI : public UIDetailsElementInterface
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIDetailsElementUI(UIDetailsSet *pParent, bool fOpened)
        : UIDetailsElementInterface(pParent, DetailsElementType_UI, fOpened) {}

private:

    /** Creates update task for this element. */
    UITask* createUpdateTask() { return new UIDetailsUpdateTaskUI(machine()); }
};


/** UITask extension used as update task for the details-element type 'Description'. */
class UIDetailsUpdateTaskDescription : public UIDetailsUpdateTask
{
    Q_OBJECT;

public:

    /** Constructs update task passing @a machine to the base-class. */
    UIDetailsUpdateTaskDescription(const CMachine &machine)
        : UIDetailsUpdateTask(machine) {}

private:

    /** Contains update task body. */
    void run();
};

/** UIDetailsElementInterface extension for the details-element type 'Description'. */
class UIDetailsElementDescription : public UIDetailsElementInterface
{
    Q_OBJECT;

public:

    /** Constructs details-element object for passed @a pParent set.
      * @param fOpened brings whether the details-element should be visually opened. */
    UIDetailsElementDescription(UIDetailsSet *pParent, bool fOpened)
        : UIDetailsElementInterface(pParent, DetailsElementType_Description, fOpened) {}

private:

    /** Creates update task for this element. */
    UITask* createUpdateTask() { return new UIDetailsUpdateTaskDescription(machine()); }
};

#endif /* !___UIDetailsElements_h___ */

