/* HTTPSERV.C   (c)Copyright Jan Jaeger, 2002                        */
/*              HTTP Server                                          */

/* This file contains all code required for the HTTP server,         */
/* when the http_server thread is started it will listen on          */
/* the HTTP port specified on the HTTPPORT config statement.         */
/*                                                                   */
/* When a request comes in a http_request thread is started,         */
/* which will handle the request.                                    */
/*                                                                   */
/* When authentification is required (auth parm on the HTTPPORT      */
/* statement) then the user will be authenticated based on the       */
/* userid and password supplied on the HTTPPORT statement, if        */
/* these where not supplied then the userid and password of the      */
/* that hercules is running under will be used. In the latter case   */
/* the root userid/password will also be accepted.                   */
/*                                                                   */
/* If the request is for a /cgi-bin/ path, then the cgibin           */
/* directory in cgibin.c will be searched, for any other request     */
/* the HTTP_ROOT (/usr/local/hercules) will be used as the root      */
/* to find the specified file.                                       */
/*                                                                   */
/* As realpath() is used to verify that the files are from within    */
/* the HTTP_ROOT tree symbolic links that refer to files outside     */
/* the HTTP_ROOT tree are not supported.                             */
/*                                                                   */
/*                                                                   */
/*                                           Jan Jaeger - 28/03/2002 */

#include "hercules.h"
#include "httpmisc.h"


#if defined(OPTION_HTTP_SERVER)

/* External reference to the cgi-bin directory in cgibin.c */
extern CGITAB cgidir[];


static CONTYP mime_types[] = {
    { NULL,    NULL },                            /* No suffix entry */
    { "txt",   "text/plain" },
    { "jcl",   "text/plain" },
    { "gif",   "image/gif"  },
    { "jpg",   "image/jpeg" },
    { "css",   "text/css"   },
    { "html",  "text/html"  },
    { "htm",   "text/html"  },
    { NULL,    NULL } };                     /* Default suffix entry */


int html_include(WEBBLK *webblk, char *filename)
{
    FILE *inclfile;
    char fullname[1024] = HTTP_ROOT;
    char buffer[1024];
    int ret;

    inclfile = fopen(strcat(fullname,filename),"r");

    if (!inclfile)
    {
        logmsg("HHS021E html_include: Cannot open %s: %s\n",
          fullname,strerror(errno));
        fprintf(webblk->hsock,"ERROR: Cannot open %s: %s\n",
          filename,strerror(errno));
        return 0;
    }

    while (!feof(inclfile))
    {
        ret = fread(buffer, 1, sizeof(buffer), inclfile);
        if (ret <= 0) break;
        fwrite(buffer, 1, ret, webblk->hsock);
    }

    fclose(inclfile);
    return 1;
}


void html_header(WEBBLK *webblk)
{
    if (webblk->request_type != REQTYPE_POST)
        fprintf(webblk->hsock,"Expires: 0\n");

    fprintf(webblk->hsock,"Content-type: text/html;\n\n");

    if (!html_include(webblk,HTML_HEADER))
        fprintf(webblk->hsock,"<HTML>\n<HEAD>\n<TITLE>Hercules</TITLE>\n</HEAD>\n<BODY>\n\n");
}


void html_footer(WEBBLK *webblk)
{
    if (!html_include(webblk,HTML_FOOTER))
        fprintf(webblk->hsock,"\n</BODY>\n</HTML>\n");
}


static void http_exit(WEBBLK *webblk)
{
CGIVAR *cgivar;
    if(webblk)
    {
        fclose(webblk->hsock);
        if(webblk->user) free(webblk->user);
        if(webblk->request) free(webblk->request);
        if(webblk->get_arg) free(webblk->get_arg);
        if(webblk->post_arg) free(webblk->post_arg);
        cgivar = webblk->cgivar;
        while(cgivar)
        {
            CGIVAR *tmpvar = cgivar->next;
            free(cgivar->name);
            free(cgivar->value);
            free(cgivar);
            cgivar = tmpvar;
        }
        free(webblk);
    }
    pthread_exit(NULL);
}


static void http_error(WEBBLK *webblk, char *err, char *header, char *info)
{
    fprintf(webblk->hsock,"HTTP/1.0 %s\n%sConnection: close\n"
                          "Content-Type: text/html\n\n"
                          "<HTML><HEAD><TITLE>%s</TITLE></HEAD>" 
                          "<BODY><H1>%s</H1>%s<P></BODY></HTML>\n\n",
                          err, header, err, err, info);
    http_exit(webblk);
}


static char *http_timestring(time_t t)
{
    static char buf[80];
    struct tm *tm = localtime(&t);
    strftime(buf, sizeof(buf)-1, "%a, %d %b %Y %H:%M:%S %Z", tm);
    return buf;
}


static void http_decode_base64(char *s)
{
    char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int bit_o, byte_o, idx, i, n;
    unsigned char *d = (unsigned char *)s;
    char *p;

    n=i=0;

    while (*s && (p = strchr(b64, *s)))
    {
        idx = (int)(p - b64);
        byte_o = (i*6)/8;
        bit_o = (i*6)%8;
        d[byte_o] &= ~((1<<(8-bit_o))-1);
        if (bit_o < 3)
        {
            d[byte_o] |= (idx << (2-bit_o));
            n = byte_o+1;
        }
        else
        {
            d[byte_o] |= (idx >> (bit_o-2));
            d[byte_o+1] = 0;
            d[byte_o+1] |= (idx << (8-(bit_o-2))) & 0xFF;
            n = byte_o+2;
        }
            s++; i++;
    }
    /* null terminate */
    d[n] = 0;
}


static char *http_unescape(char *buffer)
{
    char *pointer = buffer;

    while ( (pointer = strchr(pointer,'+')) )
        *pointer = ' ';

    pointer = buffer;

    while (pointer && *pointer && (pointer = strchr(pointer,'%')))
    {
        int highnibble = pointer[1];
        int lownibble = pointer[2];

        if (highnibble >= '0' && highnibble <= '9')
            highnibble = highnibble - '0';
        else if (highnibble >= 'A' && highnibble <= 'F')
            highnibble = 10 + highnibble - 'A';
        else if (highnibble >= 'a' && highnibble <= 'f')
            highnibble = 10 + highnibble - 'a';
        else
        {
            pointer++;
            continue;
        }
    
        if (lownibble >= '0' && lownibble <= '9')
            lownibble = lownibble - '0';
        else if (lownibble >= 'A' && lownibble <= 'F')
            lownibble = 10 + lownibble - 'A';
        else if (lownibble >= 'a' && lownibble <= 'f')
            lownibble = 10 + lownibble - 'a';
        else
        {
            pointer++;
            continue;
        }
                        
        *pointer = (highnibble<<4) | lownibble;

        memcpy(pointer+1, pointer+3, strlen(pointer+3)+1);

        pointer++;
    } 

    return buffer;
}


static void http_interpret_variable_string(WEBBLK *webblk, char *qstring, int type)
{
char *name;
char *value;
char *strtok_str;
CGIVAR **cgivar;

    for(cgivar = &(webblk->cgivar); *cgivar != NULL;
        cgivar = &((*cgivar)->next));

    for (name = strtok_r(qstring,"&;",&strtok_str);
         name; 
         name = strtok_r(NULL,"&;",&strtok_str))
    {
        if(!(value = strchr(name,'=')))
            break;
                        
        *value++ = '\0';

        (*cgivar) = malloc(sizeof(CGIVAR));
        (*cgivar)->next = NULL;
        (*cgivar)->name = strdup(http_unescape(name));
        (*cgivar)->value = strdup(http_unescape(value));
        (*cgivar)->type = type;
        cgivar = &((*cgivar)->next);
    }
}


static void http_load_cgi_variables(WEBBLK *webblk)
{
    if(webblk->post_arg)
        http_interpret_variable_string(webblk, webblk->post_arg, REQTYPE_POST);
    if(webblk->get_arg)
        http_interpret_variable_string(webblk, webblk->get_arg, REQTYPE_GET);
}


#if 0
static void http_dump_cgi_variables(WEBBLK *webblk)
{
    CGIVAR *cv;
    for(cv = webblk->cgivar; cv != NULL; cv = cv->next)
        logmsg("HHS020D cgi_var_dump: pointer(%p) name(%s) value(%s) type(%d)\n",
          cv, cv->name, cv->value, cv->type);
}
#endif


char *cgi_variable(WEBBLK *webblk, char *name)
{
    CGIVAR *cv;
    for(cv = webblk->cgivar; cv != NULL; cv = cv->next)
        if(!strcmp(name,cv->name))
            return cv->value;
    return NULL;
}


char *cgi_username(WEBBLK *webblk)
{
        return webblk->user;
}


char *cgi_baseurl(WEBBLK *webblk)
{
        return webblk->baseurl;
}


static void http_verify_path(WEBBLK *webblk, char *path)
{
    char resolved_base[1024];
    char resolved_path[1024];
    int i;

    realpath(HTTP_ROOT,resolved_base); strcat(resolved_base,"/");
    realpath(path,resolved_path);

    for (i=0;path[i];i++)
        if (!isalnum((int)path[i]) && !strchr("/.-_", path[i]))
            http_error(webblk, "404 File Not Found","",
                               "Illegal character in filename");

    if(strncmp(resolved_base,resolved_path,strlen(resolved_base)))
        http_error(webblk, "404 File Not Found","",
                           "Invalid pathname");

}


static int http_authenticate(WEBBLK *webblk, char *type, char *userpass)
{
    char *pointer ,*user, *passwd;
    struct passwd *pass = NULL;

    if (!strcasecmp(type,"Basic"))
    {
        if(userpass)
        {
            http_decode_base64(userpass);

            /* the format is now user:password */
            if ((pointer = strchr(userpass,':')))
            {
                *pointer = 0;
                user = userpass;
                passwd = pointer+1;
    
                /* Hardcoded userid and password in configuration file */
                if(sysblk.httpuser && sysblk.httppass)
                {
                    if(!strcmp(user,sysblk.httpuser)
                      && !strcmp(passwd,sysblk.httppass))
                    {
                        webblk->user = strdup(user);
                        return TRUE;
                    }
                }
#if !defined(WIN32)
                else
                {
                    /* unix userid and password check, the userid
                       must be the same as that hercules is
                       currently running under */
// ZZ INCOMPLETE
// ZZ No password check is being performed yet...
                    if((pass = getpwnam(user))
                      &&
                       (pass->pw_uid == 0 
                          || pass->pw_uid == getuid()))
                    { 
                        webblk->user = strdup(user);
                        return TRUE;
                    }
                }
#endif /*!defined(WIN32)*/
            }
        }
    }

    webblk->user = NULL;

    return FALSE;
}


static void http_download(WEBBLK *webblk, char *filename)
{
    char buffer[1024];
    int fd, length;
    char *filetype;
    char fullname[1024] = HTTP_ROOT;
    struct stat st;
    CONTYP *mime_type = mime_types;

    strcat(fullname,filename);

    http_verify_path(webblk,fullname);

    if(stat(fullname,&st)) 
        http_error(webblk, "404 File Not Found","",
                           strerror(errno));

    if(!S_ISREG(st.st_mode))
        http_error(webblk, "404 File Not Found","",
                           "The requested file is not a regular file");

    fd = open(fullname,O_RDONLY,0);
    if (fd == -1)
        http_error(webblk, "404 File Not Found","",
                           strerror(errno));

    fprintf(webblk->hsock,"HTTP/1.0 200 OK\n");
    if ((filetype = strrchr(filename,'.')))
        for(mime_type++;mime_type->suffix
          && strcasecmp(mime_type->suffix,filetype + 1);
          mime_type++);
    if(mime_type->type)
        fprintf(webblk->hsock,"Content-Type: %s\n", mime_type->type);

    fprintf(webblk->hsock,"Expires: %s\n", http_timestring(time(NULL)+HTML_STATIC_EXPIRY_TIME));

    fprintf(webblk->hsock,"Content-Length: %d\n\n", (int)st.st_size);
    while ((length = read(fd, buffer, sizeof(buffer))) > 0)
            fwrite(buffer, 1, length, webblk->hsock);
    close(fd);
    http_exit(webblk);
}


static void *http_request(FILE *hsock)
{
    WEBBLK *webblk;
    int authok = FALSE;
    char line[1024];
    char *url = NULL;
    char *pointer;
    char *strtok_str;
    CGITAB *cgient;

    if(!(webblk = malloc(sizeof(WEBBLK))))
        http_exit(webblk);

    memset(webblk,0,sizeof(WEBBLK));
    webblk->hsock = hsock;

    while (fgets(line, sizeof(line), webblk->hsock))
    {
        if (*line == '\r' || *line == '\n')
            break;

        if((pointer = strtok_r(line," \t\r\n",&strtok_str)))
        {
            if(!strcasecmp(pointer,"GET"))
            {
                if((pointer = strtok_r(NULL," \t\r\n",&strtok_str)))
                {
                    webblk->request_type = REQTYPE_GET;
                    url = strdup(pointer);
                }
            }
            else
            if(!strcasecmp(pointer,"POST"))
            {
                if((pointer = strtok_r(NULL," \t\r\n",&strtok_str)))
                {
                    webblk->request_type = REQTYPE_POST;
                    url = strdup(pointer);
                }
            }
            else
            if(!strcasecmp(pointer,"PUT"))
            {
                http_error(webblk,"400 Bad Request", "",
                                  "This server does not accept PUT requests");
            }
            else
            if(!strcasecmp(pointer,"Authorization:"))
            {
                if((pointer = strtok_r(NULL," \t\r\n",&strtok_str)))
                    authok = http_authenticate(webblk,pointer,
                                  strtok_r(NULL," \t\r\n",&strtok_str));
            }
            else
            if(!strcasecmp(pointer,"Content-Length:"))
            {
                if((pointer = strtok_r(NULL," \t\r\n",&strtok_str)))
                    webblk->content_length = atoi(pointer);
            }
        }
    }
    webblk->request = url;

    if(webblk->request_type == REQTYPE_POST
      && webblk->content_length != 0)
    {
        if((pointer = webblk->post_arg = malloc(webblk->content_length + 1)))
        {
        int i;
            for(i = 0; i < webblk->content_length; i++)
            {
                *pointer = fgetc(webblk->hsock);
                if(*pointer != '\n' && *pointer != '\r')
                    pointer++;
            } 
            *pointer = '\0';
        }
    }

    if (sysblk.httpauth && !authok)
        http_error(webblk, "401 Authorization Required", 
                           "WWW-Authenticate: Basic realm=\"HERCULES\"\n",
                           "You must be authenticated to use this service");

    if (!url)
        http_error(webblk,"400 Bad Request", "",
                          "You must specify a GET or POST request");

    /* anything following a ? in the URL is part of the get arguments */
    if ((pointer=strchr(url,'?'))) {
        webblk->get_arg = strdup(pointer+1);
        *pointer = 0;
    }

    while(url[0] == '/' && url[1] == '/')
        url++;

    webblk->baseurl = url;

    if(!strcasecmp("/",url))
        url = HTTP_WELCOME;

    if(strncasecmp("/cgi-bin/",url,9))
        http_download(webblk,url);
    else
        url += 9;

    while(*url == '/')
        url++;

    http_load_cgi_variables(webblk);

#if 0
    http_dump_cgi_variables(webblk);
#endif

    for(cgient = cgidir; cgient->path != NULL; cgient++)
    {
        if(!strcmp(cgient->path, url))
        {
            fprintf(webblk->hsock,"HTTP/1.0 200 OK\nConnection: close\n");
            fprintf(webblk->hsock,"Date: %s\n", http_timestring(time(NULL)));

            (cgient->cgibin) (webblk);
            http_exit(webblk);
        }
    }

    http_error(webblk, "404 File Not Found","",
                       "The requested file was not found");

    return NULL;
}


void *http_server (void *arg)
{
int                     rc;             /* Return code               */
int                     lsock;          /* Socket for listening      */
int                     csock;          /* Socket for conversation   */
FILE                   *hsock;          /* Socket for conversation   */
struct sockaddr_in      server;         /* Server address structure  */
fd_set                  selset;         /* Read bit map for select   */
int                     optval;         /* Argument for setsockopt   */
TID                     httptid;        /* Negotiation thread id     */

    /* Display thread started message on control panel */
    logmsg ("HHS019I HTTP connection thread started: "
            "tid="TIDPAT", pid=%d\n",
            thread_id(), getpid());

    /* Obtain a socket */
    lsock = socket (AF_INET, SOCK_STREAM, 0);

    if (lsock < 0)
    {
        logmsg("HHS018E socket: %s\n", strerror(errno));
        return NULL;
    }

    /* Allow previous instance of socket to be reused */
    optval = 1;
    setsockopt (lsock, SOL_SOCKET, SO_REUSEADDR,
                &optval, sizeof(optval));

    /* Prepare the sockaddr structure for the bind */
    memset (&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = sysblk.httpport;
    server.sin_port = htons(server.sin_port);

    /* Attempt to bind the socket to the port */
    while (1)
    {
        rc = bind (lsock, (struct sockaddr *)&server, sizeof(server));

        if (rc == 0 || errno != EADDRINUSE) break;

        logmsg ("HHS017I Waiting for port %u to become free\n",
                sysblk.httpport);
        sleep(10);
    } /* end while */

    if (rc != 0)
    {
        logmsg("HHS016E bind: %s\n", strerror(errno));
        return NULL;
    }

    /* Put the socket into listening state */
    rc = listen (lsock, 32);

    if (rc < 0)
    {
        logmsg("HHS015E listen: %s\n", strerror(errno));
        return NULL;
    }

    logmsg("HHS014I Waiting for HTTP requests on port %u\n",
            sysblk.httpport);

    /* Handle connection requests and attention interrupts */
    while (TRUE) {

        /* Initialize the select parameters */
        FD_ZERO (&selset);
        FD_SET (lsock, &selset);

        /* Wait for a file descriptor to become ready */
#if defined(WIN32)
	{
	    struct timeval tv={0,500000};	/* half a second */
	    rc = select ( lsock+1, &selset, NULL, NULL, &tv );
	}
#else /*!defined(WIN32)*/
        rc = select ( lsock+1, &selset, NULL, NULL, NULL );
#endif /*!defined(WIN32)*/

        if (rc == 0) continue;

        if (rc < 0 )
        {
            if (errno == EINTR) continue;
            logmsg("HHS013E select: %s\n", strerror(errno));
            break;
        }

        /* If a client connection request has arrived then accept it */
        if (FD_ISSET(lsock, &selset))
        {
            /* Accept a connection and create conversation socket */
            csock = accept (lsock, NULL, NULL);

            if (csock < 0)
            {
                logmsg("HHS012E accept: %s\n", strerror(errno));
                continue;
            }

            if(!(hsock = fdopen(csock,"r+")))
            {
                logmsg("HHS011E fdopen: %s\n",strerror(errno));
                close(csock);
                continue;
            }

            /* Create a thread to complete the client connection */
            if ( create_thread (&httptid, &sysblk.detattr,
                                http_request, hsock) )
            {
                logmsg("HHS010E http_request create_thread: %s\n",
                        strerror(errno));
                fclose (hsock);
                close (csock);
            }

        } /* end if(lsock) */

    } /* end while */

    /* Close the listening socket */
    close (lsock);

    return NULL;

} /* end function http_server */

#endif /*defined(OPTION_HTTP_SERVER)*/
