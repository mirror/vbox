/* $Id$ */
/** @file
 * Private guest drag and drop code, used by GuestDnDTarget +
 * GuestDnDSource.
 */

/*
 * Copyright (C) 2011-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_GUESTDNDPRIVATE
#define ____H_GUESTDNDPRIVATE

#include "VBox/hgcmsvc.h" /* For PVBOXHGCMSVCPARM. */

/* Forward prototype declarations. */
class Guest;
class Progress;

/**
 * Class for handling drag'n drop responses from
 * the guest side.
 */
class GuestDnDResponse
{

public:

    GuestDnDResponse(const ComObjPtr<Guest>& pGuest);

    virtual ~GuestDnDResponse(void);

public:

    int notifyAboutGuestResponse(void);
    int waitForGuestResponse(RTMSINTERVAL msTimeout = 500);

    void setDefAction(uint32_t a) { m_defAction = a; }
    uint32_t defAction(void) const { return m_defAction; }

    void setAllActions(uint32_t a) { m_allActions = a; }
    uint32_t allActions() const { return m_allActions; }

    void setFormat(const Utf8Str &strFormat) { m_strFormat = strFormat; }
    Utf8Str format(void) const { return m_strFormat; }

    void setDropDir(const Utf8Str &strDropDir) { m_strDropDir = strDropDir; }
    Utf8Str dropDir(void) const { return m_strDropDir; }

    int dataAdd(const void *pvData, uint32_t cbData, uint32_t *pcbCurSize);
    int dataSetStatus(size_t cbDataAdd, size_t cbDataTotal = 0);
    void reset(void);
    const void *data(void) { return m_pvData; }
    size_t size(void) const { return m_cbData; }

    int setProgress(unsigned uPercentage, uint32_t uState, int rcOp = VINF_SUCCESS);
    HRESULT resetProgress(const ComObjPtr<Guest>& pParent);
    HRESULT queryProgressTo(IProgress **ppProgress);

    int writeToFile(const char *pszPath, size_t cbPath, void *pvData, size_t cbData, uint32_t fMode);

public:

    Utf8Str errorToString(const ComObjPtr<Guest>& pGuest, int guestRc);

private:
    RTSEMEVENT           m_EventSem;
    uint32_t             m_defAction;
    uint32_t             m_allActions;
    Utf8Str              m_strFormat;

    /** The actual MIME data.*/
    void                *m_pvData;
    /** Size (in bytes) of MIME data. */
    uint32_t             m_cbData;

    size_t               m_cbDataCurrent;
    size_t               m_cbDataTotal;
    /** Dropped files directory on the host. */
    Utf8Str              m_strDropDir;
    /** The handle of the currently opened file being written to
     *  or read from. */
    RTFILE               m_hFile;
    Utf8Str              m_strFile;

    ComObjPtr<Guest>     m_parent;
    ComObjPtr<Progress>  m_progress;
};

/**
 * Private singleton class for the guest's DnD
 * implementation. Can't be instanciated directly, only via
 * the factory pattern.
 */
class GuestDnD
{
public:

    static GuestDnD *createInstance(const ComObjPtr<Guest>& pGuest)
    {
        Assert(NULL == GuestDnD::s_pInstance);
        GuestDnD::s_pInstance = new GuestDnD(pGuest);
        return GuestDnD::s_pInstance;
    }

    static void destroyInstance(void)
    {
        if (GuestDnD::s_pInstance)
        {
            delete GuestDnD::s_pInstance;
            GuestDnD::s_pInstance = NULL;
        }
    }

    static inline GuestDnD *getInstance(void)
    {
        AssertPtr(GuestDnD::s_pInstance);
        return GuestDnD::s_pInstance;
    }

protected:

    GuestDnD(const ComObjPtr<Guest>& pGuest);
    virtual ~GuestDnD(void);

public:

    /** @name Public helper functions.
     * @{ */
    int                        adjustScreenCoordinates(ULONG uScreenId, ULONG *puX, ULONG *puY) const;
    int                        hostCall(uint32_t u32Function, uint32_t cParms, PVBOXHGCMSVCPARM paParms) const;
    GuestDnDResponse          *response(void) { return m_pResponse; }
    std::vector<com::Utf8Str>  supportedFormats(void) const { return m_strSupportedFormats; }
    /** @}  */

public:

    /** @name Static low-level HGCM callback handler.
     * @{ */
    static DECLCALLBACK(int)   notifyDnDDispatcher(void *pvExtension, uint32_t u32Function, void *pvParms, uint32_t cbParms);
    /** @}  */

    /** @name Static helper methods.
     * @{ */
    static com::Utf8Str        toFormatString(const std::vector<com::Utf8Str> &lstSupportedFormats, const std::vector<com::Utf8Str> &lstFormats);
    static void                toFormatVector(const std::vector<com::Utf8Str> &lstSupportedFormats, const com::Utf8Str &strFormats, std::vector<com::Utf8Str> &vecformats);
    static DnDAction_T         toMainAction(uint32_t uAction);
    static void                toMainActions(uint32_t uActions, std::vector<DnDAction_T> &vecActions);
    static uint32_t            toHGCMAction(DnDAction_T enmAction);
    static void                toHGCMActions(DnDAction_T enmDefAction, uint32_t *puDefAction, const std::vector<DnDAction_T> vecAllowedActions, uint32_t *puAllowedActions);
    /** @}  */

protected:

    /** @name Singleton properties.
     * @{ */
    /** List of supported MIME types (formats). */
    std::vector<com::Utf8Str>  m_strSupportedFormats;
    /** Pointer to guest implementation. */
    const ComObjPtr<Guest>     m_pGuest;
    /** The current (last) response from the guest. At the
     *  moment we only support only response a time (ARQ-style). */
    GuestDnDResponse          *m_pResponse;
    /** @}  */

protected:

#ifdef VBOX_WITH_DRAG_AND_DROP_GH
    /** @name Dispatch handlers for the HGCM callbacks.
     * @{ */
    int onGHSendData(GuestDnDResponse *pResp, const void *pvData, size_t cbData, size_t cbTotalSize);
    int onGHSendDir(GuestDnDResponse *pResp, const char *pszPath, size_t cbPath, uint32_t fMode);
    int onGHSendFile(GuestDnDResponse *pResp, const char *pszPath, size_t cbPath, void *pvData, size_t cbData, uint32_t fMode);
    /** @}  */
#endif /* VBOX_WITH_DRAG_AND_DROP_GH */

private:

    /** Staic pointer to singleton instance. */
    static GuestDnD           *s_pInstance;
};

/** Access to the GuestDnD's singleton instance. */
#define GuestDnDInst() GuestDnD::getInstance()

#endif /* ____H_GUESTDNDPRIVATE */

