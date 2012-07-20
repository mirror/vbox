/** @file
 * Definition of GuestDnD
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_GUESTDND
#define ____H_GUESTDND

/* Forward declaration of the d-pointer. */
class GuestDnDPrivate;

class GuestDnD
{
public:
    GuestDnD(const ComObjPtr<Guest>& pGuest);
    ~GuestDnD();

    /* Host -> Guest */
    HRESULT dragHGEnter(ULONG uScreenId, ULONG uX, ULONG uY, DragAndDropAction_T defaultAction, ComSafeArrayIn(DragAndDropAction_T, allowedActions), ComSafeArrayIn(IN_BSTR, formats), DragAndDropAction_T *pResultAction);
    HRESULT dragHGMove(ULONG uScreenId, ULONG uX, ULONG uY, DragAndDropAction_T defaultAction, ComSafeArrayIn(DragAndDropAction_T, allowedActions), ComSafeArrayIn(IN_BSTR, formats), DragAndDropAction_T *pResultAction);
    HRESULT dragHGLeave(ULONG uScreenId);
    HRESULT dragHGDrop(ULONG uScreenId, ULONG uX, ULONG uY, DragAndDropAction_T defaultAction, ComSafeArrayIn(DragAndDropAction_T, allowedActions), ComSafeArrayIn(IN_BSTR, formats), BSTR *pstrFormat, DragAndDropAction_T *pResultAction);
    HRESULT dragHGPutData(ULONG uScreenId, IN_BSTR wstrFormat, ComSafeArrayIn(BYTE, data), IProgress **ppProgress);

    /* Guest -> Host */
    HRESULT dragGHPending(ULONG uScreenId, ComSafeArrayOut(BSTR, formats), ComSafeArrayOut(DragAndDropAction_T, allowedActions), DragAndDropAction_T *pDefaultAction);
    HRESULT dragGHDropped(IN_BSTR bstrFormat, DragAndDropAction_T action, IProgress **ppProgress);
    HRESULT dragGHGetData(ComSafeArrayOut(BYTE, data));

    /* Guest response */
    static DECLCALLBACK(int) notifyGuestDragAndDropEvent(void *pvExtension, uint32_t u32Function, void *pvParms, uint32_t cbParms);

private:

    /* d-pointer */
    GuestDnDPrivate *d_ptr;

    friend struct GuestDnDPrivate;
};

#endif /* ____H_GUESTDND */

