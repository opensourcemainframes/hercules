/* HDL.H        (c) Copyright Jan Jaeger, 2003                       */
/*              Hercules Dynamic Loader                              */


#if !defined(_HDL_H)
#define _HDL_H

#if defined(HDL_USE_LIBTOOL)
 #include <ltdl.h>
 #define dlopen(_name, _flags)   lt_dlopen(_name)
 #define dlsym(_handle, _symbol) lt_dlsym(_handle, _symbol)
 #define dlclose(_handle)        lt_dlclose(_handle)
 #define dlerror                 lt_dlerror
#else
 #include <dlfcn.h>
#endif

int hdl_load(char *);                 		/* load dll          */
int hdl_dele(char *);                           /* unload dll        */
void hdl_list();                          /* list all loaded modules */

void hdl_main();  

void * hdl_nent(char *, void*);
void * hdl_fent(char *);

/* Cygwin back-link support */
#if defined(WIN32)
static void * (*hdl_fent_l)(char *) __attribute__ ((unused)) ;
#endif

#define HDL_RESO hdl_reso
#define HDL_INIT hdl_init
#define HDL_FINI hdl_fini

#define QSTRING(_string) STRINGMAC(_string)

#define HDL_RESO_Q QSTRING(HDL_RESO)
#define HDL_INIT_Q QSTRING(HDL_INIT)
#define HDL_FINI_Q QSTRING(HDL_FINI)

/* Cygwin back-link support */
#if !defined(WIN32)
#define HDL_RESOLVER_SECTION                            \
void HDL_RESO(void *(*hdl_reso_fent)(char *) __attribute__ ((unused)) ) \
{
#else /*defined(WIN32)*/
#define HDL_RESOLVER_SECTION                            \
void HDL_RESO(void *(*hdl_reso_fent)(char *) __attribute__ ((unused)) ) \
{                                                       \
hdl_fent_l = hdl_reso_fent;
#endif /*defined(WIN32)*/

#define HDL_RESOLVE(_name)                              \
    (_name) = (hdl_reso_fent)(STRINGMAC(_name))

#define END_RESOLVER_SECTION                            \
}


#define HDL_REGISTER_SECTION                            \
void HDL_INIT(int (*hdl_init_regi)(char *, void *) __attribute__ ((unused)) ) \
{

#define HDL_REGISTER(_name, _ep)                        \
    (hdl_init_regi)(STRINGMAC(_name),&(_ep))

#define END_REGISTER_SECTION                            \
}

#if !defined(WIN32)
#define HDL_FINDSYM(_name)                              \
    hdl_fent( (_name) )
#else /*defined(WIN32)*/
#define HDL_FINDSYM(_name)                              \
    hdl_fent_l( (_name) )
#endif /*defined(WIN32)*/

#define HDL_FINDNXT(_name, _ep)                         \
    hdl_nent( STRINGMAC(_name), &(_ep) )

#define HDL_FINAL_SECTION                               \
void HDL_FINI()                                         \
{

#define END_FINAL_SECTION                               \
}


struct _MODENT; 
struct _DLLENT;


typedef struct _MODENT {
    void (*fep)();                           /* Function entry point */
    char *name;                                     /* Function name */
    int  count;
    struct _DLLENT *dllent;
    struct _MODENT *modnext;
} MODENT;


typedef struct _DLLENT {
    char *name;
    void *dll;
    int type;
#define DLL_TYPE_MAIN 0
#define DLL_TYPE_LOAD 1
    int (*hdlreso)(void *);                  /* hdl_reso             */
    int (*hdlinit)(void *);                  /* hdl_init             */
    int (*hdlfini)();                        /* hdl_fini             */
    struct _MODENT *modent;
    struct _DLLENT *dllnext;
} DLLENT;

#endif
