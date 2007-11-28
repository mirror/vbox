/** @file
 * Linux seamless guest additions unit tests.
 */

/*
 * Copyright (C) 2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/** Uncomment this to get logging which shows the flow of data received and the
    Xlib calls executed. */
// #define LOG_OUTPUT

#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

#include <iprt/err.h>
#include <iprt/types.h>

#include <iostream>
#include <memory>
#include <vector>

/* We want access to the elements of struct Display */
#define XLIB_ILLEGAL_ACCESS

#include "../seamless-guest.h"
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>

/*****************************************************************************
* Defines and macros                                                         *
*****************************************************************************/

#define TEST_MAX_WINDOWS 7

#define TEST_ROOT ((Window) 1)
#define TEST_DESKTOP_WINDOW ((Window) 2)

#define TEST_ROOT_ATOM ((Atom) 1)

/*****************************************************************************
* Event sequence functions                                                   *
*****************************************************************************/

static Display gsDisplay = { 0 };
static Screen gScreens[2] = { { 0, &gsDisplay, TEST_ROOT } };

/** X11 events to be sent to the testcase. */
/* Move window 3. */
static XConfigureEvent testConfig1 =
  { ConfigureNotify, 0, 0, &gsDisplay, TEST_DESKTOP_WINDOW, 3, 50, 50, 100, 100, 0, 0, 0 };
/* Map window 6. */
static XMapEvent testMap1 =
  { MapNotify, 0, 0, &gsDisplay, TEST_DESKTOP_WINDOW, 6, 0 };
/* Reshape window 4. */
static XShapeEvent testShape1 =
  { ShapeNotify, 0, 0, &gsDisplay, 4, 0, 0, 100, 100, 0, true };
static XRectangle testShape1Rects[3] =
  { { 20, 20, 40, 50 }, { 60, 40, 30, 30 }, { 0, 0, 0, 0} };
/* Unmap window 3. */
static XMapEvent testUnmap1 =
  { UnmapNotify, 0, 0, &gsDisplay, TEST_DESKTOP_WINDOW, 3, 0 };
/* Unmap our desktop window. */
static XMapEvent testUnmap2 =
  { UnmapNotify, 0, 0, &gsDisplay, TEST_ROOT, TEST_DESKTOP_WINDOW, 0 };
/* And map it again. */
static XMapEvent testMap2 =
  { MapNotify, 0, 0, &gsDisplay, TEST_ROOT, TEST_DESKTOP_WINDOW, 0 };
/* Remove the desktop window (what should happen here?) */
static XPropertyEvent testProperty1 =
  { PropertyNotify, 0, 0, &gsDisplay, TEST_ROOT, TEST_ROOT_ATOM, 0, PropertyNewValue };
static Window testProperty1Windows[1] = { 0 };
/* And put the desktop window back again. */
static XPropertyEvent testProperty2 =
  { PropertyNotify, 0, 0, &gsDisplay, TEST_ROOT, TEST_ROOT_ATOM, 0, PropertyNewValue };
static Window testProperty2Windows[2] = { TEST_DESKTOP_WINDOW, 0 };

static struct { XEvent *ev; XRectangle *rects; Window *desktop; } eventList[] =
  {
    { (XEvent *) &testConfig1, 0, 0 },
    { (XEvent *) &testMap1, 0, 0 },
    { (XEvent *) &testShape1, testShape1Rects, 0 },
    { (XEvent *) &testUnmap1, 0, 0 },
    { (XEvent *) &testUnmap2, 0, 0 },
    { (XEvent *) &testMap2, 0, 0 },
    { (XEvent *) &testProperty1, 0, testProperty1Windows },
    { (XEvent *) &testProperty2, 0, testProperty2Windows },
    {}
  };
static unsigned iEvent = 0;

static Window gDesktopWindowInitial[2] = { TEST_DESKTOP_WINDOW, 0 };
static Window *gDesktopWindows = gDesktopWindowInitial;

/** Does this window currently exist?  Indexed by window ID. */
static bool aWindowList[TEST_MAX_WINDOWS] = { false, true, true, true, true, true, false };

/** Window attributes of windows created during the test */
static XWindowAttributes aWindowAttrib[TEST_MAX_WINDOWS] =
  {
    { 0 },
    { 0, 0, 640, 480, 0, 24, 0, TEST_ROOT, InputOutput, 0, 0, 0, 0, 0, 0, 0, 0, IsUnviewable },
    { 0, 0, 640, 480, 0, 24, 0, TEST_ROOT, InputOutput, 0, 0, 0, 0, 0, 0, 0, 0, IsViewable },
    { 10, 10, 100, 100, 0, 24, 0, TEST_ROOT, InputOutput, 0, 0, 0, 0, 0, 0, 0, 0, IsViewable },
    { 50, 50, 200, 200, 0, 24, 0, TEST_ROOT, InputOutput, 0, 0, 0, 0, 0, 0, 0, 0, IsViewable },
    { 90, 90, 120, 50, 0, 24, 0, TEST_ROOT, InputOutput, 0, 0, 0, 0, 0, 0, 0, 0, IsViewable },
    { 150, 150, 90, 100, 0, 24, 0, TEST_ROOT, InputOutput, 0, 0, 0, 0, 0, 0, 0, 0, IsViewable }
  };

static XRectangle rectsDesktop[] = { { 0, 0, 640, 480 }, { 0, 0, 0, 0 } };
static XRectangle rects1[] = { { 0, 0, 100, 100 }, { 0, 0, 0, 0 } };
static XRectangle rects2[] = { { 10, 20, 100, 100 }, { 0, 0, 0, 0 } };
static XRectangle rects3[] = { { 0, 0, 100, 50 }, { 0, 0, 0, 0 } };
static XRectangle rects4[] = { { 50, 50, 10, 10 }, { 0, 0, 0, 0 } };

/** Visible rectangles for the child windows */
static XRectangle *aRects[TEST_MAX_WINDOWS] =
  { 0, 0, rectsDesktop, rects1, rects2, rects3, rects4 };

static RTRECT output1[] =
  {
    { 10, 10, 110, 110 },
    { 60, 70, 160, 170 },
    { 90, 90, 190, 140 },
    { 0, 0, 0, 0 }
  };

static RTRECT output2[] =
  {
    { 50, 50, 150, 150 },
    { 60, 70, 160, 170 },
    { 90, 90, 190, 140 },
    { 0, 0, 0, 0 }
  };

static RTRECT output3[] =
  {
    { 50, 50, 150, 150 },
    { 60, 70, 160, 170 },
    { 90, 90, 190, 140 },
    { 200, 200, 210, 210 },
    { 0, 0, 0, 0 }
  };

static RTRECT output4[] =
  {
    { 50, 50, 150, 150 },
    { 70, 70, 110, 120 },
    { 110, 90, 140, 120 },
    { 90, 90, 190, 140 },
    { 200, 200, 210, 210 },
    { 0, 0, 0, 0 }
  };

static RTRECT output5[] =
  {
    { 70, 70, 110, 120 },
    { 110, 90, 140, 120 },
    { 90, 90, 190, 140 },
    { 200, 200, 210, 210 },
    { 0, 0, 0, 0 }
  };

static RTRECT output6[] =
  {
    { 0, 0, 0, 0 }
  };

static RTRECT output7[] =
  {
    { 0, 0, 640, 480 },
    { 0, 0, 0, 0 }
  };

static RTRECT *testOutput[] =
  { output1, output2, output3, output4, output5, output6, output5, output7, output5 };

static unsigned iOutput = 0;

/*****************************************************************************
* Event sequence functions                                                   *
*****************************************************************************/

static bool eventsLeft(void)
{
#ifdef LOG_OUTPUT
    std::cout << __PRETTY_FUNCTION__ << std::endl;
#endif
    return (0 != eventList[iEvent].ev);
}

/*****************************************************************************
* X11 function emulation                                                     *
*****************************************************************************/

/** Return a static display object. */
Display *XOpenDisplay(_Xconst char *)
{
#ifdef LOG_OUTPUT
    std::cout << __PRETTY_FUNCTION__ << std::endl;
#endif
    gsDisplay.nscreens = 1;
    gsDisplay.screens = gScreens;
    return &gsDisplay;
}

/** Empty stub. */
int XCloseDisplay(Display *pDisplay) { return true; }

/** No need to free data in a short testcase. */
int XFree(void *data) { return true; }

/** Empty stub. */
Status XSendEvent(Display *, Window, Bool, long, XEvent *) { return true; }

/** Return the rectangle information from our array. */
XRectangle *XShapeGetRectangles (Display *, Window w, int kind, int *priCount,
                                 int *priOrdering)
{
    int i;

#ifdef LOG_OUTPUT
    std::cout << __PRETTY_FUNCTION__ << std::endl;
#endif
    CPPUNIT_ASSERT((w > TEST_ROOT) && (w < TEST_MAX_WINDOWS));
    CPPUNIT_ASSERT_EQUAL(kind, ShapeClip);
    /* We have allocated rectangle information to all windows which need to be queried! */
    CPPUNIT_ASSERT(0 != aRects[w]);
    for (
        i = 0;
            (0 != aRects[w][i].x)
        || (0 != aRects[w][i].y)
        || (0 != aRects[w][i].width)
        || (0 != aRects[w][i].height);
        ++i
        );
    *priCount = i;
    *priOrdering = 0;
    return aRects[w];
}

/** Just check inputs. */
int XSelectInput(Display *, Window w, long mask)
{
#ifdef LOG_OUTPUT
    std::cout << __PRETTY_FUNCTION__ << std::endl;
#endif
    CPPUNIT_ASSERT_EQUAL(mask, mask & (  StructureNotifyMask
                                       | SubstructureNotifyMask
                                       | PropertyChangeMask
                                      )
                        );
    CPPUNIT_ASSERT((TEST_ROOT == w) || (TEST_DESKTOP_WINDOW == w));
    return true;
}

/** Empty stub. */
void XShapeSelectInput (Display *, Window, unsigned long) {}

/** Return our hard-coded window attributes. */
Status XGetWindowAttributes(Display *, Window w, XWindowAttributes *pAttrib)
{
#ifdef LOG_OUTPUT
    std::cout << __PRETTY_FUNCTION__ << std::endl;
#endif
    CPPUNIT_ASSERT(w < TEST_MAX_WINDOWS);
    CPPUNIT_ASSERT(aWindowList[w]);
    CPPUNIT_ASSERT(pAttrib);
    *pAttrib = aWindowAttrib[w];
    return true;
}

/** We only have one atom - _NET_VIRTUAL_ROOTS - equal to 1. */
Atom XInternAtom(Display *display, _Xconst char *name, Bool)
{
#ifdef LOG_OUTPUT
    std::cout << __PRETTY_FUNCTION__ << std::endl;
#endif
    CPPUNIT_ASSERT_EQUAL(std::string(name), std::string("_NET_VIRTUAL_ROOTS"));
    return TEST_ROOT_ATOM;
}

/** The only property currently available is _NET_VIRTUAL_ROOTS_ on the root window. */
int XGetWindowProperty(Display *, Window w, Atom atom, long, long, Bool, Atom type,
                       Atom *, int *, unsigned long *nItems, unsigned long *,
                       unsigned char **pProp)
{
    int i;

#ifdef LOG_OUTPUT
    std::cout << __PRETTY_FUNCTION__ << std::endl;
#endif
    CPPUNIT_ASSERT_EQUAL(w, TEST_ROOT);
    CPPUNIT_ASSERT_EQUAL(atom, TEST_ROOT_ATOM);
    CPPUNIT_ASSERT_EQUAL(type, XA_CARDINAL);
    for (i = 0; gDesktopWindows[i] != 0; ++i);
    *nItems = i;
    *pProp = (unsigned char *) gDesktopWindows;
    return Success;
}

/** Works for the desktop window, asserts for all others. */
Status XQueryTree(Display *, Window w, Window *prRoot, Window *prParent,
                  Window **pprChildren, unsigned int *prnChildren)
{
    int cChildren = 0;
    static Window ahChildren[TEST_MAX_WINDOWS];
    Window hParent = 0;

#ifdef LOG_OUTPUT
    std::cout << __PRETTY_FUNCTION__ << std::endl;
#endif
    /* We only have information available for the desktop window. */
    CPPUNIT_ASSERT((TEST_ROOT == w) || (TEST_DESKTOP_WINDOW == w));
    if (TEST_ROOT == w)
    {
        ahChildren[0] = TEST_DESKTOP_WINDOW;
        cChildren = 1;
    }
    else /* TEST_DESKTOP_WINDOW */
    {
        /* 0 is unused, 1 is root, 2 is desktop. */
        for (int i = 3; i < TEST_MAX_WINDOWS; ++i)
        {
            if (aWindowList[i])
            {
                ahChildren[cChildren] = i;
                ++cChildren;
            }
        }
        hParent = TEST_ROOT;
    }
    *prRoot = TEST_ROOT;
    *prParent = hParent;
    *pprChildren = ahChildren;
    *prnChildren = cChildren;
    return true;
}

/** Process and send out the next X11 event from our queue as long as it is not empty. */
int XNextEvent(Display *, XEvent *pEvent)
{
#ifdef LOG_OUTPUT
    std::cout << __PRETTY_FUNCTION__ << std::endl;
#endif
    if (0 != eventList[iEvent].ev)
    {
        XEvent *ev = eventList[iEvent].ev;
        XConfigureEvent *cf = &ev->xconfigure;
        XMapEvent *map = &ev->xmap;
        XUnmapEvent *unmap = &ev->xunmap;
        CPPUNIT_ASSERT(   (ev->xany.window < TEST_MAX_WINDOWS)
                       && (aWindowList[ev->xany.window]));
        switch (ev->type)
        {
        case ConfigureNotify:
            CPPUNIT_ASSERT((cf->window < TEST_MAX_WINDOWS) && (aWindowList[cf->window]));
            aWindowAttrib[cf->window].x = cf->x;
            aWindowAttrib[cf->window].y = cf->y;
            aWindowAttrib[cf->window].width = cf->width;
            aWindowAttrib[cf->window].height = cf->height;
            break;
        case MapNotify:
            CPPUNIT_ASSERT((map->window < TEST_MAX_WINDOWS) && (map->window > TEST_ROOT));
            aWindowList[map->window] = true;
            break;
        case PropertyNotify:
            gDesktopWindows = eventList[iEvent].desktop;
            break;
        case ShapeNotify:
            aRects[ev->xany.window] = eventList[iEvent].rects;
            break;
        case UnmapNotify:
            CPPUNIT_ASSERT((unmap->window < TEST_MAX_WINDOWS) && (unmap->window > TEST_ROOT));
            aWindowList[unmap->window] = false;
            break;
        default:
            /* So far we don't actually change the property in a PropertyNotify event. */
            CPPUNIT_FAIL("Unhandled event");
            break;
        }
        *pEvent = *eventList[iEvent].ev;
        ++iEvent;
    }
    else
    {
        XEvent ev = { };
        *pEvent = ev;
    }
    return true;
}

/** Return "supported". */
Bool XShapeQueryExtension (Display *, int *, int *) { return true; }

/*****************************************************************************
* Test classes                                                               *
*****************************************************************************/

/**
 * Observer class for the guest test fixture
 */
class VBoxSeamlessX11TestObserver : public VBoxGuestSeamlessObserver
{
private:
    VBoxGuestSeamlessX11 *mX11;
public:
    VBoxSeamlessX11TestObserver(VBoxGuestSeamlessX11 *pX11) : mX11(pX11) {}

    /** This is the notification function that is called by the class when new rectangle
        data is available.  We compare the data sent with what we were expecting. */
    virtual void notify(void)
    {
#ifdef LOG_OUTPUT
        std::cout << __PRETTY_FUNCTION__ << std::endl;
#endif
        std::auto_ptr<std::vector<RTRECT> > apRects = mX11->getRects();
        std::vector<RTRECT>::iterator it;
        int i = 0;
        for (it = apRects.get()->begin(); it < apRects.get()->end(); ++it, ++i)
        {
#ifdef LOG_OUTPUT
            std::cout << "Rectangle " << it->xLeft << ", " << it->yTop << ", "
                      << it->xRight << ", " << it->yBottom << std::endl;
#endif
            CPPUNIT_ASSERT_EQUAL(it->xLeft, testOutput[iOutput][i].xLeft);
            CPPUNIT_ASSERT_EQUAL(it->yTop, testOutput[iOutput][i].yTop);
            CPPUNIT_ASSERT_EQUAL(it->xRight, testOutput[iOutput][i].xRight);
            CPPUNIT_ASSERT_EQUAL(it->yBottom, testOutput[iOutput][i].yBottom);
        }
        ++iOutput;
    }
};


/**
 * Test fixture for guest proxy class
 */
class VBoxSeamlessX11Test : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE( VBoxSeamlessX11Test );

    CPPUNIT_TEST( testX11 );

    CPPUNIT_TEST_SUITE_END();

private:

    VBoxGuestSeamlessX11 *mX11;
    VBoxSeamlessX11TestObserver *mObserver;
public:
    void setUp()
    {
        mX11 = new VBoxGuestSeamlessX11;
        mObserver = new VBoxSeamlessX11TestObserver(mX11);
    }

    void tearDown()
    {
        delete mObserver;
        delete mX11;
    }

    void testX11()
    {
        CPPUNIT_ASSERT(RT_SUCCESS(mX11->init(mObserver)));
        CPPUNIT_ASSERT(RT_SUCCESS(mX11->start()));
        while (eventsLeft())
        {
            mX11->nextEvent();
        }
        /* Make sure we get notification from the last event. */
        mX11->nextEvent();
        mX11->stop();
        mX11->uninit();
    }
};

// Create text test runner and run all tests.
int main( int argc, char **argv)
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( VBoxSeamlessX11Test::suite() );
    return (runner.run() ? 0 : 1);
}
