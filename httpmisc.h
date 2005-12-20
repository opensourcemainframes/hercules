/* HTTPMISC.C   (c)Copyright Jan Jaeger, 2002-2005                   */
/*              HTTP Server                                          */

#if !defined(_HTTPMISC_H)
#define _HTTPMISC_H


#if !defined(PKGDATADIR)
 #if !defined(_MSVC_)
  #define HTTP_ROOT   "/usr/local/share/hercules/"
 #else
  #define HTTP_ROOT   "%ProgramFiles%\\Hercules\\html\\"
 #endif
#else
 #define HTTP_ROOT   PKGDATADIR "/"
#endif

#if !defined(_MSVC_)
 #define HTTP_PS "/"
#else
 #define HTTP_PS "\\"
#endif

#define HTTP_WELCOME "hercules.html"

#define HTML_HEADER  "include/header.htmlpart"
#define HTML_FOOTER  "include/footer.htmlpart"


#define HTML_STATIC_EXPIRY_TIME (60*60*24*7)

#define HTTP_PATH_LENGTH 1024

typedef struct _CGIVAR {
    struct _CGIVAR *next;
    char *name;
    char *value;
    int  type;
#define VARTYPE_NONE   0
#define VARTYPE_GET    1
#define VARTYPE_POST   2
#define VARTYPE_PUT    4
#define VARTYPE_COOKIE 8
} CGIVAR;


#define cgi_variable(_webblk, _varname) \
        http_variable((_webblk), (_varname), (VARTYPE_GET|VARTYPE_POST))


#define cgi_cookie(_webblk, _varname) \
        http_variable((_webblk), (_varname), (VARTYPE_COOKIE))


#define cgi_username(_webblk) \
        ((_webblk)->user)


#define cgi_baseurl(_webblk) \
        ((_webblk)->baseurl)


typedef struct _MIMETAB {
    char *suffix;
    char *type;
} MIMETAB;


typedef struct _WEBBLK {
#define HDL_VERS_WEBBLK "2.17"
#define HDL_SIZE_WEBBLK sizeof(WEBBLK)
    int sock;
    int request_type;
#define REQTYPE_NONE   0
#define REQTYPE_GET    1
#define REQTYPE_POST   2
#define REQTYPE_PUT    4
    char *request;
    char *baseurl;
    char *user;
    CGIVAR *cgivar;
} WEBBLK;


typedef void (*zz_cgibin) (WEBBLK *webblk);


typedef struct _CGITAB {
    char   *path;
        zz_cgibin cgibin;
} CGITAB;


void html_header(WEBBLK *webblk);
void html_footer(WEBBLK *webblk);
int html_include(WEBBLK *webblk, char *filename);

char *http_variable(WEBBLK *webblk, char *name, int type);
void *http_server (void *arg);

#endif /*!defined(_HTTPMISC_H)*/
