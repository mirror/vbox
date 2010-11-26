
#ifndef ___UIDesktopServices_p_h___
#define ___UIDesktopServices_p_h___

#include <VBox/VBoxCocoa.h>
#include <iprt/cdefs.h> /* for RT_C_DECLS_BEGIN/RT_C_DECLS_END & stuff */

ADD_COCOA_NATIVE_REF(NSString);

RT_C_DECLS_BEGIN

bool darwinCreateMachineShortcut(NativeNSStringRef pstrSrcFile, NativeNSStringRef pstrDstPath, NativeNSStringRef pstrName, NativeNSStringRef pstrUuid);
bool darwinOpenInFileManager(NativeNSStringRef pstrFile);

RT_C_DECLS_END

#endif /* !___UIDesktopServices_p_h___ */

