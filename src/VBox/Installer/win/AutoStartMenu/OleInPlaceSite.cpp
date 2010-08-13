#define MUST_BE_IMPLEMENTED(s) ::MessageBox(0, s, "Not Implemented", 0);  return E_NOTIMPL;
#include "OleInPlaceSite.h"

OleInPlaceSite::OleInPlaceSite( class IOleInPlaceFrame* ole_in_place_frame,
                                HWND h) :
ole_client_site_    (0),
ole_in_place_frame_ (ole_in_place_frame),
browser_object_     (0),
hwnd_(h)
{
}

HRESULT  OleInPlaceSite::QueryInterface( REFIID riid, LPVOID FAR* ppvObj)
{
    return ole_client_site_->QueryInterface(riid, ppvObj);
}

void OleInPlaceSite::BrowserObject(IOleObject* o)
{
    browser_object_ = o;
}
