
#ifndef __OLE_IN_PLACE_FRAME__
#define __OLE_IN_PLACE_FRAME__

#include "Tracer.h"

class OleInPlaceFrame : public IOleInPlaceFrame
{
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID FAR* ppvObj)
    {
        return E_NOTIMPL;
    }

    ULONG STDMETHODCALLTYPE AddRef()
    {
        return 1;
    }

    ULONG STDMETHODCALLTYPE Release()
    {
        return 1;
    }

    HRESULT STDMETHODCALLTYPE GetWindow( HWND FAR* lphwnd)
    {
//      *lphwnd = todo_hwnd;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE ContextSensitiveHelp( BOOL fEnterMode)
    {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetBorder( LPRECT lprectBorder)
    {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE RequestBorderSpace( LPCBORDERWIDTHS pborderwidths)
    {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE SetBorderSpace( LPCBORDERWIDTHS pborderwidths)
    {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE SetActiveObject( IOleInPlaceActiveObject *pActiveObject, LPCOLESTR pszObjName)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE InsertMenus( HMENU hmenuShared, LPOLEMENUGROUPWIDTHS lpMenuWidths)
    {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE SetMenu( HMENU hmenuShared, HOLEMENU holemenu, HWND hwndActiveObject)
    {
        return(S_OK);
    }

    HRESULT STDMETHODCALLTYPE RemoveMenus( HMENU hmenuShared)
    {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE SetStatusText( LPCOLESTR pszStatusText)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE EnableModeless( BOOL fEnable)
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE TranslateAccelerator( LPMSG lpmsg, WORD wID)
    {
        TraceFunc("OleInPlaceFrame::TranslateAccelerator");
        return E_NOTIMPL;
    }
};

#endif
