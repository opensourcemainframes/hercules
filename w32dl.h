/* W32DL.H      (c) Copyright Jan Jaeger, 2004-2005                  */
/*              dlopen compat                                        */

#ifndef _W32_DL_H
#define _W32_DL_H

#ifdef _WIN32

#define RTLD_NOW 0

#if 1
#define dlopen(_name, _flags) \
        (void*) ((_name) ? LoadLibrary((_name)) : GetModuleHandle( NULL ) )
#else
#define dlopen(_name, _flags) \
        (void*) ((_name) ? LoadLibrary((_name)) : LoadLibrary("HERCULES.DLL") )
#endif
#define dlsym(_handle, _symbol) \
        (void*)GetProcAddress((HMODULE)(_handle), (_symbol))
#define dlclose(_handle) \
        FreeLibrary((HMODULE)(_handle))
#define dlerror() \
        ("(unknown)")
 
#endif /* _WIN32 */

#endif /* _W32_DL_H */
