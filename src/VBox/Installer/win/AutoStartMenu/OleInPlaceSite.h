
#ifndef __OLE_IN_PLACE_SITE__
#define __OLE_IN_PLACE_SITE__

#include <windows.h>

class OleInPlaceSite : public IOleInPlaceSite
{
private:
    class IOleClientSite*   ole_client_site_;
    class IOleInPlaceFrame* ole_in_place_frame_;
    class IOleObject          * browser_object_;
    HWND                    hwnd_;

public:
    OleInPlaceSite(class IOleInPlaceFrame* ole_in_place_frame_,
                   HWND h);

    HRESULT STDMETHODCALLTYPE QueryInterface( REFIID riid, LPVOID FAR* ppvObj);

    ULONG STDMETHODCALLTYPE AddRef()
    {
        return(1);
    }

    ULONG STDMETHODCALLTYPE Release()
    {
        return(1);
    }

    HRESULT STDMETHODCALLTYPE GetWindow( HWND FAR* lphwnd)
    {

        *lphwnd = hwnd_;
        return(S_OK);
    }

    HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(BOOL fEnterMode)
    {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE CanInPlaceActivate()
    {
        // In place activate is OK
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnInPlaceActivate()
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnUIActivate()
    {
        return(S_OK);
    }

    HRESULT STDMETHODCALLTYPE GetWindowContext( LPOLEINPLACEFRAME FAR* lplpFrame, LPOLEINPLACEUIWINDOW FAR* lplpDoc, LPRECT lprcPosRect, LPRECT lprcClipRect, LPOLEINPLACEFRAMEINFO lpFrameInfo)
    {
        *lplpFrame = ole_in_place_frame_;

        // We have no OLEINPLACEUIWINDOW
        *lplpDoc = 0;

        // Fill in some other info for the browser
        lpFrameInfo->fMDIApp = FALSE;
        lpFrameInfo->hwndFrame = hwnd_;//((_IOleInPlaceFrameEx *)*lplpFrame)->window;
        lpFrameInfo->haccel = 0;
        lpFrameInfo->cAccelEntries = 0;

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Scroll(SIZE scrollExtent)
    {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE OnUIDeactivate( BOOL fUndoable)
    {
        return(S_OK);
    }

    HRESULT STDMETHODCALLTYPE OnInPlaceDeactivate()
    {
        return(S_OK);
    }

    HRESULT STDMETHODCALLTYPE DiscardUndoState()
    {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE DeactivateAndUndo()
    {
        return E_NOTIMPL;
    }

    // Called when the position of the browser object is changed, such as when we call the IWebBrowser2's put_Width(), // put_Height(), put_Left(), or put_Right().
    HRESULT STDMETHODCALLTYPE OnPosRectChange( LPCRECT lprcPosRect)
    {
        //IOleObject            *browserObject;
        IOleInPlaceObject   *inplace;
        if (!browser_object_->QueryInterface(IID_IOleInPlaceObject, (void**)&inplace))
        {
            inplace->SetObjectRects(lprcPosRect, lprcPosRect);
        }

        return(S_OK);
    }

    void BrowserObject(IOleObject* o);

    void ClientSite(IOleClientSite* o)
    {
        ole_client_site_=o;
    }
};

#endif

