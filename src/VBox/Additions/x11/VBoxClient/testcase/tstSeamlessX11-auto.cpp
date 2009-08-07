/** @file
 * Automated test of the X11 seamless Additions code.
 */

/*
 * Copyright (C) 2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include <iostream>
#include <stdlib.h> /* exit() */

#include <X11/Xatom.h>
#include <X11/Xmu/WinUtil.h>

#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/semaphore.h>
#include <iprt/stream.h>
#include <iprt/string.h>

#include "../seamless.h"

#undef DefaultRootWindow

/******************************************************
* Mock X11 functions needed by the seamless X11 class *
******************************************************/

int XFree(void *data)
{
    RTMemFree(data);
    return 0;
}

#define TEST_DISPLAY ((Display *)0xffff)
#define TEST_ROOT ((Window)1)

extern "C" Display *XOpenDisplay(const char *display_name);
Display *XOpenDisplay(const char *display_name)
{
    return TEST_DISPLAY;
}

extern "C" int XCloseDisplay(Display *display);
int XCloseDisplay(Display *display)
{
    Assert(display == TEST_DISPLAY);
    return 0;
}

enum
{
    ATOM_PROP = 1,
    ATOM_DESKTOP_PROP
};

extern "C" Atom XInternAtom(Display *display, const char *atom_name,
                            Bool only_if_exists);
Atom XInternAtom(Display *display, const char *atom_name, Bool only_if_exists)
{
    if (!RTStrCmp(atom_name, WM_TYPE_PROP))
        return (Atom) ATOM_PROP;
    if (!RTStrCmp(atom_name, WM_TYPE_DESKTOP_PROP))
        return (Atom) ATOM_DESKTOP_PROP;
    AssertFailed();
    return (Atom)0;
}

/** The window (if any) on which the WM_TYPE_PROP property is set to the
 * WM_TYPE_DESKTOP_PROP atom. */
static Window g_hSmlsDesktopWindow = 0;

extern "C" int XGetWindowProperty(Display *display, Window w, Atom property,
                                  long long_offset, long long_length,
                                  Bool delProp, Atom req_type,
                                  Atom *actual_type_return,
                                  int *actual_format_return,
                                  unsigned long *nitems_return,
                                  unsigned long *bytes_after_return,
                                  unsigned char **prop_return);
int XGetWindowProperty(Display *display, Window w, Atom property,
                       long long_offset, long long_length, Bool delProp,
                       Atom req_type, Atom *actual_type_return,
                       int *actual_format_return,
                       unsigned long *nitems_return,
                       unsigned long *bytes_after_return,
                       unsigned char **prop_return)
{
    Atom atomType = XInternAtom (NULL, WM_TYPE_PROP, true);
    Atom atomTypeDesktop = XInternAtom (NULL, WM_TYPE_DESKTOP_PROP, true);
    /* We only handle things we expect. */
    AssertReturn((req_type == XA_ATOM) || (req_type == AnyPropertyType),
                 0xffff);
    AssertReturn(property == atomType, 0xffff);
    *actual_type_return = XA_ATOM;
    *actual_format_return = sizeof(Atom) * 8;
    *nitems_return = 0;
    *bytes_after_return = sizeof(Atom);
    *prop_return = NULL;
    if ((w != g_hSmlsDesktopWindow) || (g_hSmlsDesktopWindow == 0))
        return Success;
    AssertReturn(long_offset == 0, 0);
    AssertReturn(delProp == false, 0);
    unsigned char *pProp;
    pProp = (unsigned char *)RTMemDup(&atomTypeDesktop,
                                      sizeof(atomTypeDesktop));
    AssertReturn(pProp, 0xffff);
    *nitems_return = 1;
    *prop_return = pProp;
    *bytes_after_return = 0;
    return 0;
}

/** Sets the current set of properties for all mock X11 windows */
static void smlsSetDesktopWindow(Window hWin)
{
    g_hSmlsDesktopWindow = hWin;
}

extern "C" Bool XShapeQueryExtension (Display *dpy, int *event_basep,
                                      int *error_basep);
Bool XShapeQueryExtension (Display *dpy, int *event_basep, int *error_basep)
{
    return true;
}

/* We silently ignore this for now. */
extern "C" int XSelectInput(Display *display, Window w, long event_mask);
int XSelectInput(Display *display, Window w, long event_mask)
{
    return 0;
}

/* We silently ignore this for now. */
extern "C" void XShapeSelectInput(Display *display, Window w,
                                  unsigned long event_mask);
void XShapeSelectInput(Display *display, Window w, unsigned long event_mask)
{}

extern "C" Window XDefaultRootWindow(Display *display);
Window XDefaultRootWindow(Display *display)
{
    return TEST_ROOT;
}

static unsigned g_cSmlsWindows = 0;
static Window *g_paSmlsWindows = NULL;
static XWindowAttributes *g_paSmlsWinAttribs = NULL;
static const char **g_papszSmlsWinNames = NULL;

extern "C" Status XQueryTree(Display *display, Window w, Window *root_return,
                             Window *parent_return, Window **children_return,
                             unsigned int *nchildren_return);
Status XQueryTree(Display *display, Window w, Window *root_return,
                  Window *parent_return, Window **children_return,
                  unsigned int *nchildren_return)
{
    AssertReturn(w == TEST_ROOT, False);  /* We support nothing else */
    AssertPtrReturn(children_return, False);
    AssertReturn(g_paSmlsWindows, False);
    if (root_return)
        *root_return = TEST_ROOT;
    if (parent_return)
        *parent_return = TEST_ROOT;
    *children_return = (Window *)RTMemDup(g_paSmlsWindows,
                                          g_cSmlsWindows * sizeof(Window));
    if (nchildren_return)
        *nchildren_return = g_cSmlsWindows;
    return (g_cSmlsWindows != 0);
}

extern "C" Window XmuClientWindow(Display *dpy, Window win);
Window XmuClientWindow(Display *dpy, Window win)
{
    return win;
}

extern "C" Status XGetWindowAttributes(Display *display, Window w,
                      XWindowAttributes *window_attributes_return);
Status XGetWindowAttributes(Display *display, Window w,
                            XWindowAttributes *window_attributes_return)
{
    AssertPtrReturn(window_attributes_return, 1);
    for (unsigned i = 0; i < g_cSmlsWindows; ++i)
        if (g_paSmlsWindows[i] == w)
        {
            *window_attributes_return = g_paSmlsWinAttribs[i];
            return 1;
        }
    return 0;
}

extern "C" Status XGetWMNormalHints(Display *display, Window w,
                                    XSizeHints *hints_return,
                                    long *supplied_return);

Status XGetWMNormalHints(Display *display, Window w,
                         XSizeHints *hints_return, long *supplied_return)
{
    return 1;
}

static void smlsSetWindowAttributes(XWindowAttributes *pAttribs,
                                    Window *pWindows, unsigned cAttribs,
                                    const char **paNames)
{
    g_paSmlsWinAttribs = pAttribs;
    g_paSmlsWindows = pWindows;
    g_cSmlsWindows = cAttribs;
    g_papszSmlsWinNames = paNames;
}

static Window g_SmlsShapedWindow = 0;
static int g_cSmlsShapeRectangles = 0;
static XRectangle *g_pSmlsShapeRectangles = NULL;

extern "C" XRectangle *XShapeGetRectangles (Display *dpy, Window window,
                                            int kind, int *count,
                                            int *ordering);
XRectangle *XShapeGetRectangles (Display *dpy, Window window, int kind,
                                 int *count, int *ordering)
{
    if ((window != g_SmlsShapedWindow) || (window == 0))
        return NULL;  /* Probably not correct, but works for us. */
    *count = g_cSmlsShapeRectangles;
    *ordering = 0;
    return (XRectangle *)RTMemDup(g_pSmlsShapeRectangles,
                                    sizeof(XRectangle)
                                  * g_cSmlsShapeRectangles);
}

static void smlsSetShapeRectangles(Window window, int cRects,
                                   XRectangle *pRects)
{
    g_SmlsShapedWindow = window;
    g_cSmlsShapeRectangles = cRects;
    g_pSmlsShapeRectangles = pRects;
}

/* This should not be needed in the bits of the code we test. */
extern "C" int XNextEvent(Display *display, XEvent *event_return);
int XNextEvent(Display *display, XEvent *event_return)
{
    AssertFailedReturn(0);
}

/* This should not be needed in the bits of the code we test. */
extern "C" Status XSendEvent(Display *display, Window w, Bool propagate,
                             long event_mask, XEvent *event_send);
Status XSendEvent(Display *display, Window w, Bool propagate,
                  long event_mask, XEvent *event_send)
{
    AssertFailedReturn(0);
}

/* This should not be needed in the bits of the code we test. */
extern "C" int XFlush(Display *display);
int XFlush(Display *display)
{
    AssertFailedReturn(0);
}

/*****************************
* The actual tests to be run *
*****************************/

/** The name of the unit test */
static const char *g_pszTestName = NULL;

/*** Test fixture data and data structures ***/

/** A structure describing a test fixture to be run through.  Each fixture
 * describes the state of the windows visible (and unmapped) on the X server
 * before and after a particular event is delivered, and the expected
 * on-screen positions of all interesting visible windows at the end of the
 * fixture as reported by the code (currently in the order it is likely to
 * report them in, @todo sort this).  We expect that the set of visible
 * windows will be the same whether we start the code before the event and
 * handle it or start the code after the event.
 *
 * If it is ever needed I could write a small tool to record a fixture on
 * a live guest, but I will put that off as long as I can.
 */
struct SMLSFIXTURE
{
    /** The number of windows visible before the event */
    unsigned cWindowsBefore;
    /** An array of Window IDs for the visible and unmapped windows before
     * the event */
    Window *pahWindowsBefore;
    /** The window attributes matching the windows in @a paWindowsBefore */
    XWindowAttributes *paAttribsBefore;
    /** The window names matching the windows in @a paWindowsBefore */
    const char **papszNamesBefore;
    /** The shaped window before the event - we allow at most one of these.
     * Zero for none. */
    Window hShapeWindowBefore;
    /** The number of rectangles in the shaped window before the event. */
    int cShapeRectsBefore;
    /** The rectangles in the shaped window before the event */
    XRectangle *paShapeRectsBefore;
    /** The number of windows visible after the event */
    unsigned cWindowsAfter;
    /** An array of Window IDs for the visible and unmapped windows after
     * the event */
    Window *pahWindowsAfter;
    /** The window attributes matching the windows in @a paWindowsAfter */
    XWindowAttributes *paAttribsAfter;
    /** The window names matching the windows in @a paWindowsAfter */
    const char **papszNamesAfter;
    /** The shaped window after the event - we allow at most one of these.
     * Zero for none. */
    Window hShapeWindowAfter;
    /** The number of rectangles in the shaped window after the event. */
    int cShapeRectsAfter;
    /** The rectangles in the shaped window after the event */
    XRectangle *paShapeRectsAfter;
    /** The event to delivered */
    int x11EventType;
    /** The windows for which the event in @enmEvent is delivered */
    Window hEventWindow;
    /** The number of windows expected to be reported at the end of the
     * fixture */
    unsigned cReportedRects;
    /** The onscreen positions of those windows. */
    RTRECT *paReportedRects;
};

/*** Test fixture to test the code against X11 configure (move) events ***/

static Window g_ahWin1[] = { 20 };
static XWindowAttributes g_aAttrib1Before[] =
{ { 100, 200, 200, 300, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    IsViewable }
};
static XRectangle g_aRectangle1[] =
{
    { 0, 0, 50, 50 },
    { 50, 50, 150, 250 }
};
static XWindowAttributes g_aAttrib1After[] =
{ { 200, 300, 200, 300, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    IsViewable }
};
static const char *g_apszNames1[] = { "Test Window" };

AssertCompile(RT_ELEMENTS(g_ahWin1) == RT_ELEMENTS(g_aAttrib1Before));
AssertCompile(RT_ELEMENTS(g_ahWin1) == RT_ELEMENTS(g_aAttrib1After));
AssertCompile(RT_ELEMENTS(g_ahWin1) == RT_ELEMENTS(g_apszNames1));

static RTRECT g_aRects1[] =
{
    { 200, 300, 250, 350 },
    { 250, 350, 400, 600 }
};

static SMLSFIXTURE g_testMove =
{
    RT_ELEMENTS(g_ahWin1),
    g_ahWin1,
    g_aAttrib1Before,
    g_apszNames1,
    20,
    RT_ELEMENTS(g_aRectangle1),
    g_aRectangle1,
    RT_ELEMENTS(g_ahWin1),
    g_ahWin1,
    g_aAttrib1After,
    g_apszNames1,
    20,
    RT_ELEMENTS(g_aRectangle1),
    g_aRectangle1,
    ConfigureNotify,
    20,
    RT_ELEMENTS(g_aRects1),
    g_aRects1
};

/*** Test fixture to test the code against X11 configure (resize) events ***/

static XWindowAttributes g_aAttrib2Before[] =
{ { 100, 200, 200, 300, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    IsViewable }
};
static XRectangle g_aRectangle2Before[] =
{
    { 0, 0, 50, 50 },
    { 50, 50, 100, 100 }
};

AssertCompile(RT_ELEMENTS(g_ahWin1) == RT_ELEMENTS(g_aAttrib2Before));

static SMLSFIXTURE g_testResize =
{
    RT_ELEMENTS(g_ahWin1),
    g_ahWin1,
    g_aAttrib2Before,
    g_apszNames1,
    20,
    RT_ELEMENTS(g_aRectangle2Before),
    g_aRectangle2Before,
    RT_ELEMENTS(g_ahWin1),
    g_ahWin1,
    g_aAttrib1After,
    g_apszNames1,
    20,
    RT_ELEMENTS(g_aRectangle1),
    g_aRectangle1,
    ConfigureNotify,
    20,
    RT_ELEMENTS(g_aRects1),
    g_aRects1
};

/*** Test fixture to test the code against X11 map events ***/

static XWindowAttributes g_aAttrib3Before[] =
{ { 200, 300, 200, 300, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    IsUnmapped }
};

AssertCompile(RT_ELEMENTS(g_ahWin1) == RT_ELEMENTS(g_aAttrib3Before));

static SMLSFIXTURE g_testMap =
{
    RT_ELEMENTS(g_ahWin1),
    g_ahWin1,
    g_aAttrib3Before,
    g_apszNames1,
    20,
    RT_ELEMENTS(g_aRectangle1),
    g_aRectangle1,
    RT_ELEMENTS(g_ahWin1),
    g_ahWin1,
    g_aAttrib1After,
    g_apszNames1,
    20,
    RT_ELEMENTS(g_aRectangle1),
    g_aRectangle1,
    MapNotify,
    20,
    RT_ELEMENTS(g_aRects1),
    g_aRects1
};

/*** Test fixture to test the code against X11 unmap events ***/

static XWindowAttributes g_aAttrib4After[] =
{ { 100, 200, 300, 400, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    IsUnmapped }
};

AssertCompile(RT_ELEMENTS(g_ahWin1) == RT_ELEMENTS(g_aAttrib4After));

static SMLSFIXTURE g_testUnmap =
{
    RT_ELEMENTS(g_ahWin1),
    g_ahWin1,
    g_aAttrib1Before,
    g_apszNames1,
    20,
    RT_ELEMENTS(g_aRectangle1),
    g_aRectangle1,
    RT_ELEMENTS(g_ahWin1),
    g_ahWin1,
    g_aAttrib4After,
    g_apszNames1,
    20,
    RT_ELEMENTS(g_aRectangle1),
    g_aRectangle1,
    UnmapNotify,
    20,
    0,
    NULL
};

/*** Test fixture to test the code against X11 shape events ***/

static XRectangle g_aRectangle5Before[] =
{
    { 0, 0, 200, 200 }
};

static SMLSFIXTURE g_testShape =
{
    RT_ELEMENTS(g_ahWin1),
    g_ahWin1,
    g_aAttrib1After,
    g_apszNames1,
    20,
    RT_ELEMENTS(g_aRectangle5Before),
    g_aRectangle5Before,
    RT_ELEMENTS(g_ahWin1),
    g_ahWin1,
    g_aAttrib1After,
    g_apszNames1,
    20,
    RT_ELEMENTS(g_aRectangle1),
    g_aRectangle1,
    VBoxShapeNotify,
    20,
    RT_ELEMENTS(g_aRects1),
    g_aRects1
};

/*** And the test code proper ***/

/** Compare two RTRECT structures */
static bool smlsCompRect(RTRECT *pFirst, RTRECT *pSecond)
{
    return (   (pFirst->xLeft == pSecond->xLeft)
            && (pFirst->yTop == pSecond->yTop)
            && (pFirst->xRight == pSecond->xRight)
            && (pFirst->yBottom == pSecond->yBottom));
}

static void smlsPrintDiffRects(RTRECT *pExp, RTRECT *pGot)
{
    RTPrintf("    Expected: %d, %d, %d, %d.  Got: %d, %d, %d, %d\n",
             pExp->xLeft, pExp->yTop, pExp->xRight, pExp->yBottom,
             pGot->xLeft, pGot->yTop, pGot->xRight, pGot->yBottom);
}

/** Run through a test fixture */
static unsigned smlsDoFixture(SMLSFIXTURE *pFixture, const char *pszDesc)
{
    VBoxGuestSeamlessX11 subject;
    unsigned cErrs = 0;

    subject.init(NULL);
    smlsSetWindowAttributes(pFixture->paAttribsBefore,
                            pFixture->pahWindowsBefore,
                            pFixture->cWindowsBefore,
                            pFixture->papszNamesBefore);
    smlsSetShapeRectangles(pFixture->hShapeWindowBefore,
                           pFixture->cShapeRectsBefore,
                           pFixture->paShapeRectsBefore);
    subject.start();
    smlsSetWindowAttributes(pFixture->paAttribsAfter,
                            pFixture->pahWindowsAfter,
                            pFixture->cWindowsAfter,
                            pFixture->papszNamesAfter);
    smlsSetShapeRectangles(pFixture->hShapeWindowAfter,
                           pFixture->cShapeRectsAfter,
                           pFixture->paShapeRectsAfter);
    switch(pFixture->x11EventType)
    {
        case ConfigureNotify:
            subject.doConfigureEvent(pFixture->hEventWindow);
            break;
        case MapNotify:
            subject.doMapEvent(pFixture->hEventWindow);
            break;
        case UnmapNotify:
            subject.doUnmapEvent(pFixture->hEventWindow);
            break;
        case VBoxShapeNotify:
            subject.doShapeEvent(pFixture->hEventWindow);
            break;
        default:
            break;
    }
    std::auto_ptr<std::vector<RTRECT> > rects = subject.getRects();
    if (rects->size() != pFixture->cReportedRects)
    {
        RTPrintf("%s: fixture: %s.  Wrong number of rectangles reported after processing event (expected %u, got %u).\n",
                 g_pszTestName, pszDesc, pFixture->cReportedRects,
                 (*rects).size());
        ++cErrs;
    }
    else
        for (unsigned i = 0; i < rects->size(); ++i)
            if (!smlsCompRect(&(*rects)[i], &pFixture->paReportedRects[i]))
            {
                RTPrintf("%s: fixture: %s.  Rectangle %u wrong after processing event.\n",
                         g_pszTestName, pszDesc, i);
                smlsPrintDiffRects(&pFixture->paReportedRects[i],
                                   &(*rects)[i]);
                ++cErrs;
                break;
            }
    subject.stop();
    subject.start();
    if (rects->size() != pFixture->cReportedRects)
    {
        RTPrintf("%s: fixture: %s.  Wrong number of rectangles reported without processing event (expected %u, got %u).\n",
                 g_pszTestName, pszDesc, pFixture->cReportedRects,
                 (*rects).size());
        ++cErrs;
    }
    else
        for (unsigned i = 0; i < rects->size(); ++i)
            if (!smlsCompRect(&(*rects)[i], &pFixture->paReportedRects[i]))
            {
                RTPrintf("%s: fixture: %s.  Rectangle %u wrong without processing event.\n",
                         g_pszTestName, pszDesc, i);
                smlsPrintDiffRects(&pFixture->paReportedRects[i],
                                   &(*rects)[i]);
                ++cErrs;
                break;
            }
    return cErrs;
}

int main( int argc, char **argv)
{
    RTR3Init();
    unsigned cErrs = 0;
    g_pszTestName = RTPathFilename(argv[0]);

    RTPrintf("%s: TESTING\n", g_pszTestName);
    cErrs += smlsDoFixture(&g_testMove,
                           "ConfigureNotify event (window moved)");
    // Currently not working
    cErrs += smlsDoFixture(&g_testResize,
                           "ConfigureNotify event (window resized)");
    cErrs += smlsDoFixture(&g_testMap, "MapNotify event");
    cErrs += smlsDoFixture(&g_testUnmap, "UnmapNotify event");
    cErrs += smlsDoFixture(&g_testShape, "ShapeNotify event");
    if (cErrs > 0)
        RTPrintf("%u errors\n", cErrs);
    return cErrs == 0 ? 0 : 1;
}
