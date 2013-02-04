/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIGDetailsElements class declaration
 */

/*
 * Copyright (C) 2013 Oracle Corporation
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

/* Qt includes: */
#include <QThread>

/* GUI includes: */
#include "UIGDetailsElement.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"

/* Forward declarations: */
class UIGMachinePreview;

/* Element update thread: */
class UIGDetailsUpdateThread : public QThread
{
    Q_OBJECT;

signals:

    /* Notifier: Prepare stuff: */
    void sigComplete(const UITextTable &text);

public:

    /* Constructor: */
    UIGDetailsUpdateThread(const CMachine &machine);

protected:

    /* Internal API: Machine stuff: */
    const CMachine& machine() const { return m_machine; }

private:

    /* Variables: */
    const CMachine &m_machine;
};

/* Details element interface: */
class UIGDetailsElementInterface : public UIGDetailsElement
{
    Q_OBJECT;

public:

    /* Constructor/destructor: */
    UIGDetailsElementInterface(UIGDetailsSet *pParent, DetailsElementType elementType, bool fOpened);
    ~UIGDetailsElementInterface();

protected:

    /* Helpers: Update stuff: */
    void updateAppearance();
    virtual UIGDetailsUpdateThread* createUpdateThread() = 0;

private slots:

    /* Handler: Update stuff: */
    virtual void sltUpdateAppearanceFinished(const UITextTable &newText);

private:

    /* Helpers: Cleanup stuff: */
    void cleanupThread();

    /* Variables: */
    UIGDetailsUpdateThread *m_pThread;
};


/* Thread 'General': */
class UIGDetailsUpdateThreadGeneral : public UIGDetailsUpdateThread
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsUpdateThreadGeneral(const CMachine &machine);

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
    UIGDetailsElementGeneral(UIGDetailsSet *pParent, bool fOpened);

private:

    /* Helper: Translate stuff: */
    void retranslateUi();

    /* Helper: Update stuff: */
    UIGDetailsUpdateThread* createUpdateThread();
};


/* Element 'Preview': */
class UIGDetailsElementPreview : public UIGDetailsElement
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsElementPreview(UIGDetailsSet *pParent, bool fOpened);

private:

    /* Helper: Translate stuff: */
    void retranslateUi();

    /* Helpers: Layout stuff: */
    int minimumWidthHint() const;
    int minimumHeightHint(bool fClosed) const;
    void updateLayout();

    /* Helper: Update stuff: */
    void updateAppearance();

    /* Variables: */
    UIGMachinePreview *m_pPreview;
};


/* Thread 'System': */
class UIGDetailsUpdateThreadSystem : public UIGDetailsUpdateThread
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsUpdateThreadSystem(const CMachine &machine);

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
    UIGDetailsElementSystem(UIGDetailsSet *pParent, bool fOpened);

private:

    /* Helper: Translate stuff: */
    void retranslateUi();

    /* Helper: Update stuff: */
    UIGDetailsUpdateThread* createUpdateThread();
};


/* Thread 'Display': */
class UIGDetailsUpdateThreadDisplay : public UIGDetailsUpdateThread
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsUpdateThreadDisplay(const CMachine &machine);

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
    UIGDetailsElementDisplay(UIGDetailsSet *pParent, bool fOpened);

private:

    /* Helper: Translate stuff: */
    void retranslateUi();

    /* Helper: Update stuff: */
    UIGDetailsUpdateThread* createUpdateThread();
};


/* Thread 'Storage': */
class UIGDetailsUpdateThreadStorage : public UIGDetailsUpdateThread
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsUpdateThreadStorage(const CMachine &machine);

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
    UIGDetailsElementStorage(UIGDetailsSet *pParent, bool fOpened);

private:

    /* Helper: Translate stuff: */
    void retranslateUi();

    /* Helper: Update stuff: */
    UIGDetailsUpdateThread* createUpdateThread();
};


/* Thread 'Audio': */
class UIGDetailsUpdateThreadAudio : public UIGDetailsUpdateThread
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsUpdateThreadAudio(const CMachine &machine);

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
    UIGDetailsElementAudio(UIGDetailsSet *pParent, bool fOpened);

private:

    /* Helper: Translate stuff: */
    void retranslateUi();

    /* Helper: Update stuff: */
    UIGDetailsUpdateThread* createUpdateThread();
};


/* Thread 'Network': */
class UIGDetailsUpdateThreadNetwork : public UIGDetailsUpdateThread
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsUpdateThreadNetwork(const CMachine &machine);

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
    UIGDetailsElementNetwork(UIGDetailsSet *pParent, bool fOpened);

private:

    /* Helper: Translate stuff: */
    void retranslateUi();

    /* Helper: Update stuff: */
    UIGDetailsUpdateThread* createUpdateThread();
};


/* Thread 'Serial': */
class UIGDetailsUpdateThreadSerial : public UIGDetailsUpdateThread
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsUpdateThreadSerial(const CMachine &machine);

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
    UIGDetailsElementSerial(UIGDetailsSet *pParent, bool fOpened);

private:

    /* Helper: Translate stuff: */
    void retranslateUi();

    /* Helper: Update stuff: */
    UIGDetailsUpdateThread* createUpdateThread();
};


#ifdef VBOX_WITH_PARALLEL_PORTS
/* Thread 'Parallel': */
class UIGDetailsUpdateThreadParallel : public UIGDetailsUpdateThread
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsUpdateThreadParallel(const CMachine &machine);

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
    UIGDetailsElementParallel(UIGDetailsSet *pParent, bool fOpened);

private:

    /* Helper: Translate stuff: */
    void retranslateUi();

    /* Helper: Update stuff: */
    UIGDetailsUpdateThread* createUpdateThread();
};
#endif /* VBOX_WITH_PARALLEL_PORTS */


/* Thread 'USB': */
class UIGDetailsUpdateThreadUSB : public UIGDetailsUpdateThread
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsUpdateThreadUSB(const CMachine &machine);

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
    UIGDetailsElementUSB(UIGDetailsSet *pParent, bool fOpened);

private:

    /* Helper: Translate stuff: */
    void retranslateUi();

    /* Helper: Update stuff: */
    UIGDetailsUpdateThread* createUpdateThread();
};


/* Thread 'SF': */
class UIGDetailsUpdateThreadSF : public UIGDetailsUpdateThread
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsUpdateThreadSF(const CMachine &machine);

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
    UIGDetailsElementSF(UIGDetailsSet *pParent, bool fOpened);

private:

    /* Helper: Translate stuff: */
    void retranslateUi();

    /* Helper: Update stuff: */
    UIGDetailsUpdateThread* createUpdateThread();
};


/* Thread 'Description': */
class UIGDetailsUpdateThreadDescription : public UIGDetailsUpdateThread
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGDetailsUpdateThreadDescription(const CMachine &machine);

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
    UIGDetailsElementDescription(UIGDetailsSet *pParent, bool fOpened);

private:

    /* Helper: Translate stuff: */
    void retranslateUi();

    /* Helper: Update stuff: */
    UIGDetailsUpdateThread* createUpdateThread();
};

#endif /* __UIGDetailsElements_h__ */

