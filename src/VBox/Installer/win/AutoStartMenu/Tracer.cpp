/*
   Tracer.cpp

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
#include "Tracer.h"

#ifdef DO_TRACE

    #include <windows.h>

HANDLE TraceFunc_::trace_file_=0;

int TraceFunc_::indent = -1;

void TraceFunc_::StartTrace_(std::string const& filename)
{
    trace_file_ = ::CreateFile(filename.c_str(), GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
}

TraceFunc_::TraceFunc_(std::string const& func_name)
{
    func_name_ = func_name;

    indent++;
    Trace_( (std::string("Entered ") + func_name));
    indent++;
}

TraceFunc_::TraceFunc_(std::string const& func_name, std::string const& something)
{
    func_name_ = func_name;

    indent++;
    Trace_( (std::string("Entered ") + func_name + " [" + something + "]"));
    indent++;
}

TraceFunc_::~TraceFunc_()
{
    indent --;
    Trace_( (std::string("Leaving ") + func_name_));
    indent --;
}


#endif // DO_TRACE
