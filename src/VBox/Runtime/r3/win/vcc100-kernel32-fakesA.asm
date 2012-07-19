

%include "iprt/asmdefs.mac"

%ifndef RT_ARCH_X86
 %error "This is x86 only code.
%endif


%macro MAKE_IMPORT_ENTRY 2
extern _ %+ %1 %+ @ %+ %2
global __imp__ %+ %1 %+ @ %+ %2
__imp__ %+ %1 %+ @ %+ %2:
    dd _ %+ %1 %+ @ %+ %2

%endmacro


BEGINDATA

MAKE_IMPORT_ENTRY DecodePointer, 4
MAKE_IMPORT_ENTRY EncodePointer, 4
MAKE_IMPORT_ENTRY InitializeCriticalSectionAndSpinCount, 8
MAKE_IMPORT_ENTRY HeapSetInformation, 16
MAKE_IMPORT_ENTRY HeapQueryInformation, 20


