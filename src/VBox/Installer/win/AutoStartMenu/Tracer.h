/*
   Tracer.h

   Copyright (C) 2002-2004 René Nyffenegger

   This source code is provided 'as-is', without any express or implied
   warranty. In no event will the author be held liable for any damages
   arising from the use of this software.

   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:

   1. The origin of this source code must not be misrepresented; you must not
      claim that you wrote the original source code. If you use this source code
      in a product, an acknowledgment in the product documentation would be
      appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
      misrepresented as being the original source code.

   3. This notice may not be removed or altered from any source distribution.

   René Nyffenegger rene.nyffenegger@adp-gmbh.ch
*/

#ifndef __TRACER_H__
#define __TRACER_H__

#ifdef DO_TRACE

    #include <string>
    #include <sstream>
    #include <windows.h>

    #define StartTrace(x)    TraceFunc_::StartTrace_(x)
    #define Trace(x)         dummy_____for_trace_func______.Trace_(x)
    #define Trace2(x,y)      dummy_____for_trace_func______.Trace_(x,y)
    #define TraceFunc(x)     TraceFunc_ dummy_____for_trace_func______(x)
    #define TraceFunc2(x,y)  TraceFunc_ dummy_____for_trace_func______(x,y)

class TraceFunc_
{
    std::string func_name_;
public:
    TraceFunc_(std::string const&);
    TraceFunc_(std::string const&, std::string const&);
    ~TraceFunc_();

    static void StartTrace_(std::string const& file_name);

    template <typename T>
    void Trace_(T const& t)
    {
        DWORD d;
        std::string indent_s;
        std::stringstream s;

        s << t;

        for (int i=0; i< indent; i++) indent_s += " ";

        ::WriteFile(trace_file_,indent_s.c_str(), indent_s.size(), &d, 0);
        ::WriteFile(trace_file_, s.str().c_str(), s.str().size() ,&d, 0);
        ::WriteFile(trace_file_,"\x0a",1,&d,0);
    }

    template <class T, class U>
    void Trace_(T const& t, U const& u)
    {
        DWORD d;
        std::string indent_s;
        std::stringstream s;

        s << t;
        s << u;

        for (int i=0; i< indent; i++) indent_s += " ";

        ::WriteFile(trace_file_,indent_s.c_str(), indent_s.size(), &d, 0);
        ::WriteFile(trace_file_, s.str().c_str(), s.str().size() ,&d, 0);
        ::WriteFile(trace_file_,"\x0a",1,&d,0);
    }

    static int    indent;
    static HANDLE trace_file_;
};

#else

    #define StartTrace(x)
    #define Trace(x)
    #define Trace2(x,y)
    #define TraceFunc(x)
    #define TraceFunc2(x,y)


#endif  // DO_TRACE

#endif  // __TRACER_H__
