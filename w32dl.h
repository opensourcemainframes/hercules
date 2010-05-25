/* W32DL.H      (c) Copyright Jan Jaeger, 2004-2009                  */
/*                        dlopen compat                              */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

#ifndef _W32_DL_H
#define _W32_DL_H

#ifdef _WIN32

#define RTLD_NOW 0

#define dlopen(_name, _flags) \
        (void*) ((_name) ? LoadLibrary((_name)) : GetModuleHandle( NULL ) )
#define dlsym(_handle, _symbol) \
        (void*)GetProcAddress((HMODULE)(_handle), (_symbol))
#define dlclose(_handle) \
        FreeLibrary((HMODULE)(_handle))
#define dlerror() \
        ("(unknown)")
 
#endif /* _WIN32 */

#endif /* _W32_DL_H */
