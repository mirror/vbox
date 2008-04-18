/** @file
 *
 * VirtualBox console event handling
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#ifndef ____H_CONSOLEEVENTS
#define ____H_CONSOLEEVENTS

#include <iprt/semaphore.h>

template<class C> class ConsoleEventBuffer
{
public:
   /**
    * Constructor
    *
    * @param size FIFO size in elements.
    */
    ConsoleEventBuffer(size_t size) :
        sz(size), buf(new C[size]), curg(0), curp(0), full(false)
    {
        RTSemMutexCreate(&mutex);
    }

    /**
     * Destructor
     *
     */
    virtual ~ConsoleEventBuffer()
    {
        RTSemMutexDestroy(mutex);
        delete buf;
    }

    /**
     *  Opens the buffer for extraction of elements. Must be called before #get().
     */
    void open()
    {
        lock();
    }

    /**
     *  Closes the buffer previously opened by #open(). Must be called after #get().
     */
    void close()
    {
        unlock();
    }

    /**
     *  Gets the element from the buffer. Requires #open() before and #close()
     *  after. Returns the first element and removes it from the buffer. If
     *  the buffer is empty, returns an empty element (constructed with the
     *  default constructor).
     */
    C get()
    {
        C c;
        if (full || curg != curp)
        {
            c = buf[curg];
            ++curg %= sz;
            full = false;
        }
        return c;
    }

    /**
     *  Puts the element to the end of the buffer. #open()/#close() must not
     *  be used. Returns 1 if successful, or 0 if the buffer is full, or 2
     *  if the element is invalid.
     */
    size_t put(C c)
    {
        if (!c.isValid())
            return 2; // invalid element
        lock();
        size_t i = 0;
        if (!full)
        {
            buf[curp] = c;
            ++curp %= sz;
            i++;
            full = curg == curp;
        }
        unlock();
        return i;
    }

    /**
     *  Puts the number of elements to the buffer. #open()/#close() must not
     *  be used. Returns the count of elements placed. If it is less than
     *  the count passed as an argument then the buffer is full. If it is
     *  greater (special case) then the invalid element is encountered and
     *  its index is return value munis count minus 1.
     */
    size_t put(C *codes, size_t count)
    {
        lock();
        size_t i = 0;
        while (i < count && !full)
        {
            if (!codes[i].isValid())
            {
                i += count + 1; // invalid code
                break;
            }
            buf[curp] = codes[i++];
            ++curp %= sz;
            full = curg == curp;
        }
        unlock();
        return i;
    }

private:
    /**
     * Acquire the local mutex
     */
    void lock()
    {
        RTSemMutexRequest(mutex, RT_INDEFINITE_WAIT);
    }
    /**
     *  Release the local mutex
     */
    void unlock()
    {
        RTSemMutexRelease(mutex);
    }

private:
    size_t sz;
    C *buf;
    size_t curg, curp;
    bool full;
    RTSEMMUTEX mutex;
};

#endif // ____H_CONSOLEEVENTS
