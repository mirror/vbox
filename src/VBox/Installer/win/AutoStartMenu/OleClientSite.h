
#ifndef __OLE_CLIENT_SIDE__
#define __OLE_CLIENT_SIDE__

#include <windows.h>
#include <ExDisp.h>
#include <mshtml.h>
#include <mshtmhst.h>
#include <oaidl.h>

class OleClientSite : public IOleClientSite
{

    IOleInPlaceSite       *   in_place_             ;
    IDocHostUIHandler     *   doc_host_ui_handler_  ;
    DWebBrowserEvents2    *   web_browser_events_   ;

public:

    OleClientSite (
                  IOleInPlaceSite*    in_place,
                  IDocHostUIHandler*  doc_host_ui_handler,
                  DWebBrowserEvents2* web_browser_events);


    HRESULT STDMETHODCALLTYPE QueryInterface( REFIID riid, void ** ppvObject);

    ULONG STDMETHODCALLTYPE AddRef()
    {
        return(1);
    }

    ULONG STDMETHODCALLTYPE Release()
    {
        return(1);
    }

    HRESULT STDMETHODCALLTYPE SaveObject()
    {
        MUST_BE_IMPLEMENTED("SaveObject")
    }

    HRESULT STDMETHODCALLTYPE GetMoniker( DWORD dwAssign, DWORD dwWhichMoniker, IMoniker ** ppmk)
    {
        MUST_BE_IMPLEMENTED("GetMoniker")
    }

    HRESULT STDMETHODCALLTYPE GetContainer( LPOLECONTAINER FAR* ppContainer)
    {
        // No container in sight....
        *ppContainer = 0;

        return(E_NOINTERFACE);
    }

    HRESULT STDMETHODCALLTYPE ShowObject()
    {
        return(NOERROR);
    }

    HRESULT STDMETHODCALLTYPE OnShowWindow( BOOL fShow)
    {
        MUST_BE_IMPLEMENTED("OnShowWindow")
    }

    HRESULT STDMETHODCALLTYPE RequestNewObjectLayout()
    {
        MUST_BE_IMPLEMENTED("RequestNewObjectLayout")
    }
};

#endif
