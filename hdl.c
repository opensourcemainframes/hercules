/* HDL.C        (c) Copyright Jan Jaeger, 2003                       */
/*              Hercules Dynamic Loader                              */

#include "hercules.h"


#if defined(OPTION_DYNAMIC_LOAD)

extern HDLPRE hdl_preload[];             /* Preload list in hdlmain  */

static DLLENT *hdl_dll;                  /* dll chain                */
static LOCK   hdl_lock;                  /* loader lock              */
static DLLENT *hdl_cdll;                 /* current dll (hdl_lock)   */

static HDLDEP *hdl_depend;               /* Version codes in hdlmain */

#endif /*defined(OPTION_DYNAMIC_LOAD)*/


static HDLSHD *hdl_shdlist;              /* Shutdown call list       */


/* hdl_adsc - add shutdown call
 */
void hdl_adsc (void * shdcall, void * shdarg)
{
HDLSHD *newcall;

    newcall = malloc(sizeof(HDLSHD));
    newcall->shdcall = shdcall;
    newcall->shdarg = shdarg;
    newcall->next = hdl_shdlist;
    hdl_shdlist = newcall;
}


/* hdl_rmsc - remove shutdown call
 */
int hdl_rmsc (void *shdcall, void *shdarg)
{
HDLSHD **tmpcall;

    for(tmpcall = &(hdl_shdlist); *tmpcall; tmpcall = &((*tmpcall)->next) )
    {
        if( (*tmpcall)->shdcall == shdcall
          && (*tmpcall)->shdarg == shdarg )
        {
        HDLSHD *frecall;
            frecall = *tmpcall;
            *tmpcall = (*tmpcall)->next;
            free(frecall);
            return 0;
        }
    }
    return -1;
}
    

/* hdl_shut - call all shutdown call entries in LIFO order
 */
void hdl_shut (void)
{
HDLSHD *shdent;

    for(shdent = hdl_shdlist; shdent; shdent = hdl_shdlist)
    {
        (shdent->shdcall) (shdent->shdarg);
        /* Remove shutdown call entry to ensure it is called once */
        hdl_shdlist = shdent->next;
        free(shdent);
    }
}


#if defined(OPTION_DYNAMIC_LOAD)


/* hdl_dvad - register device type
 */
void hdl_dvad (char *devname, DEVHND *devhnd)
{
HDLDEV *newhnd;

    newhnd = malloc(sizeof(HDLDEV));
    newhnd->name = strdup(devname);
    newhnd->hnd = devhnd;
    newhnd->next = hdl_cdll->hndent;
    hdl_cdll->hndent = newhnd;
}


/* hdl_fhnd - find registered device handler
 */
static DEVHND * hdl_fhnd (char *devname)
{
DLLENT *dllent;
HDLDEV *hndent;

    for(dllent = hdl_dll; dllent; dllent = dllent->dllnext)
    {
        for(hndent = dllent->hndent; hndent; hndent = hndent->next)
        {
            if(!strcmp(devname,hndent->name))
            {
                return hndent->hnd;
            }
        }
    }

    return NULL;
}


/* hdl_bdnm - build device module name
 */
static char * hdl_bdnm (char *ltype)
{
char *dtname;
unsigned int n;

    dtname = malloc(strlen(ltype) + sizeof(HDL_HDTP_Q));
    strcpy(dtname,HDL_HDTP_Q);
    strcat(dtname,ltype);

    for(n = 0; n < strlen(dtname); n++)
        if(isupper(dtname[n]))
            dtname[n] = tolower(dtname[n]);

    return dtname;
}


/* hdl_ghnd - obtain device handler
 */
DEVHND * hdl_ghnd (char *devtype)
{
DEVHND *hnd;
char *hdtname;
char *ltype;

    if((hnd = hdl_fhnd(devtype)))
        return hnd;

    hdtname = hdl_bdnm(devtype);

    if(hdl_load(hdtname,HDL_LOAD_NOMSG) || !hdl_fhnd(devtype))
    {
        if(hdl_device_type_equates)
        {
            if((ltype = hdl_device_type_equates(devtype)))
            {
                free(hdtname);

                hdtname = hdl_bdnm(ltype);

                hdl_load(hdtname,HDL_LOAD_NOMSG);
            }
        }
    }

    free(hdtname);

    return hdl_fhnd(devtype);
}


/* hdl_list - list all entry points
 */
void hdl_list (int flags)
{
DLLENT *dllent;
MODENT *modent;

    for(dllent = hdl_dll; dllent; dllent = dllent->dllnext)
    {
        logmsg("dll type = %s",(dllent->flags & HDL_LOAD_MAIN) ? "main" : "load");
        logmsg(", name = %s",dllent->name);

        if (dllent->flags & (HDL_LOAD_NOUNLOAD | HDL_LOAD_WAS_FORCED))
        {
            logmsg(", flags = (%s%s%s)"
                ,(dllent->flags & HDL_LOAD_NOUNLOAD)   ? "nounload" : ""
                ,(dllent->flags & HDL_LOAD_NOUNLOAD) &&
                 (dllent->flags & HDL_LOAD_WAS_FORCED) ? ", "       : ""
                ,(dllent->flags & HDL_LOAD_WAS_FORCED) ? "forced"   : ""
                );
        }

        logmsg("\n");

        for(modent = dllent->modent; modent; modent = modent->modnext)
            if((flags & HDL_LIST_ALL) 
              || !((dllent->flags & HDL_LOAD_MAIN) && !modent->fep))
            {
                logmsg(" symbol = %s",modent->name);
//              logmsg(", ep = %p",modent->fep);
                if(modent->fep)
                    logmsg(", loadcount = %d",modent->count);
                else
                    logmsg(", unresolved");
                logmsg(", owner = %s\n",dllent->name);
            }

        if(dllent->hndent)
        {
        HDLDEV *hndent;
            logmsg(" devtype =");
            for(hndent = dllent->hndent; hndent; hndent = hndent->next)
                logmsg(" %s",hndent->name);
            logmsg("\n");
        }
    }
}


/* hdl_dlst - list all dependencies
 */
void hdl_dlst (void)
{
HDLDEP *depent;

    for(depent = hdl_depend;
      depent;
      depent = depent->next)
        logmsg("dependency(%s) version(%s) size(%d)\n",
          depent->name,depent->version,depent->size);
}


/* hdl_dadd - add depency
 */
static int hdl_dadd (char *name, char *version, int size)
{
HDLDEP **newdep;

    for (newdep = &(hdl_depend);
        *newdep;
         newdep = &((*newdep)->next));

    (*newdep) = malloc(sizeof(HDLDEP));
    (*newdep)->next    = NULL;
    (*newdep)->name    = strdup(name);
    (*newdep)->version = strdup(version);
    (*newdep)->size    = size;

    return 0;
}


/* hdl_dchk - depency check
 */
static int hdl_dchk (char *name, char *version, int size)
{
HDLDEP *depent;

    for(depent = hdl_depend;
      depent && strcmp(name,depent->name);
      depent = depent->next);

    if(depent)
    {
        if(strcmp(version,depent->version))
        {
            logmsg(_("HHCHD010I Dependency check failed for %s, version(%s) expected(%s)\n"),
               name,version,depent->version);
            return -1;
        }

        if(size != depent->size)
        {
            logmsg(_("HHCHD011I Dependency check failed for %s, size(%d) expected(%d)\n"),
               name,size,depent->size);
            return -1;
        }
    }
    else
    {
        hdl_dadd(name,version,size);
    }

    return 0;
}


/* hdl_fent - find entry point
 */
void * hdl_fent (char *name)
{
DLLENT *dllent;
MODENT *modent;
void *fep;

    /* Find entry point and increase loadcount */
    for(dllent = hdl_dll; dllent; dllent = dllent->dllnext)
    {
        for(modent = dllent->modent; modent; modent = modent->modnext)
        {
            if(!strcmp(name,modent->name))
            {
                modent->count++;
                return modent->fep;
            }
        }
    }

    /* If not found then lookup as regular symbol */
    for(dllent = hdl_dll; dllent; dllent = dllent->dllnext)
    {
        if((fep = dlsym(dllent->dll,name)))
        {
            if(!(modent = malloc(sizeof(MODENT))))
            {
                logmsg(_("HHCHD001E registration malloc failed for %s\n"),
                  name);
                return NULL;
            }

            modent->fep = fep;
            modent->name = strdup(name);
            modent->count = 1;

            /* Insert current entry as first in chain */
            modent->modnext = dllent->modent;
            dllent->modent = modent;

            return fep;
        }
    }

    /* No entry point found */
    return NULL;
}


/* hdl_nent - find next entry point in chain
 */
void * hdl_nent (void *fep)
{
DLLENT *dllent;
MODENT *modent = NULL;
char   *name;

    for(dllent = hdl_dll; dllent; dllent = dllent->dllnext)
    {
        for(modent = dllent->modent; modent; modent = modent->modnext)
        {
            if(modent->fep == fep)
                break;
        }

        if(modent && modent->fep == fep)
            break;
    }

    if(!modent)
        return NULL;

    name = modent->name;

    if(!(modent = modent->modnext))
    {
        if((dllent = dllent->dllnext))
            modent = dllent->modent;
        else
            return NULL;
    }

    /* Find entry point */
    for(; dllent; dllent = dllent->dllnext, modent = dllent->modent)
    {
        for(; modent; modent = modent->modnext)
        {
            if(!strcmp(name,modent->name))
            {
                return modent->fep;
            }
        }
    }

    return NULL;
}


/* hdl_regi - register entry point
 */
static void hdl_regi (char *name, void *fep)
{
MODENT *modent;

    modent = malloc(sizeof(MODENT));

    modent->fep = fep;
    modent->name = strdup(name);
    modent->count = 0;

    modent->modnext = hdl_cdll->modent;
    hdl_cdll->modent = modent;

}


/* hdl_term - hercules termination
 */
static void hdl_term (void *unused __attribute__ ((unused)) )
{
DLLENT *dllent;

    /* Call all final routines, in reverse load order */
    for(dllent = hdl_dll; dllent; dllent = dllent->dllnext)
    {
        if(dllent->hdlfini)
            (dllent->hdlfini)();
    }
}


/* hdl_main - initialize hercules dynamic loader
 */
void hdl_main (void)
{
HDLPRE *preload;

    initialize_lock(&hdl_lock);

    dlinit();

    if(!(hdl_cdll = hdl_dll = malloc(sizeof(DLLENT))))
    {
        fprintf(stderr, _("HHCHD002E cannot allocate memory for DLL descriptor: %s\n"),
          strerror(errno));
        exit(1);
    }

    hdl_cdll->name = strdup("*Hercules");

    if(!(hdl_cdll->dll = dlopen(NULL, RTLD_NOW )))
    {
        fprintf(stderr, _("HHCHD003E unable to open hercules as DLL: %s\n"),
          dlerror());
        exit(1);
    }

    hdl_cdll->flags = HDL_LOAD_MAIN | HDL_LOAD_NOUNLOAD;

    if(!(hdl_cdll->hdldepc = dlsym(hdl_cdll->dll,HDL_DEPC_Q)))
    {
        fprintf(stderr, _("HHCHD012E No depency section in %s: %s\n"),
          hdl_cdll->name, dlerror());
        exit(1);
    }

    hdl_cdll->hdlinit = dlsym(hdl_cdll->dll,HDL_INIT_Q);

    hdl_cdll->hdlreso = dlsym(hdl_cdll->dll,HDL_RESO_Q);

    hdl_cdll->hdlddev = dlsym(hdl_cdll->dll,HDL_DDEV_Q);

    hdl_cdll->hdlfini = dlsym(hdl_cdll->dll,HDL_FINI_Q);

    /* No modules or device types registered yet */
    hdl_cdll->modent = NULL;
    hdl_cdll->hndent = NULL;

    /* No dll's loaded yet */
    hdl_cdll->dllnext = NULL;

    obtain_lock(&hdl_lock);

    if(hdl_cdll->hdldepc)
        (hdl_cdll->hdldepc)(&hdl_dadd);

    if(hdl_cdll->hdlinit)
        (hdl_cdll->hdlinit)(&hdl_regi);

    if(hdl_cdll->hdlreso)
        (hdl_cdll->hdlreso)(&hdl_fent);

    if(hdl_cdll->hdlddev)
        (hdl_cdll->hdlddev)(&hdl_dvad);

    release_lock(&hdl_lock);

    /* Register termination exit */
    hdl_adsc(hdl_term, NULL);

    for(preload = hdl_preload; preload->name; preload++)
        hdl_load(preload->name, preload->flag);
}


/* hdl_load - load a dll
 */
int hdl_load (char *name,int flags)
{
DLLENT *dllent, *tmpdll;
MODENT *modent;
char *modname;

    modname = (modname = strrchr(name,'/')) ? modname+1 : name;

    for(dllent = hdl_dll; dllent; dllent = dllent->dllnext)
    {
        if(strfilenamecmp(modname,dllent->name) == 0)
        {
            logmsg(_("HHCHD005E %s already loaded\n"),dllent->name);
            return -1;
        }
    }

    if(!(dllent = malloc(sizeof(DLLENT))))
    {
        logmsg(_("HHCHD006S cannot allocate memory for DLL descriptor: %s\n"),
          strerror(errno));
        return -1;
    }

    dllent->name = strdup(modname);

    if(!(dllent->dll = dlopen(name, RTLD_NOW)))
    {
        if(!(flags & HDL_LOAD_NOMSG))
            logmsg(_("HHCHD007E unable to open DLL %s: %s\n"),
              name,dlerror());
        free(dllent);
        return -1;
    }

    dllent->flags = (flags & (~HDL_LOAD_WAS_FORCED));

    if(!(dllent->hdldepc = dlsym(dllent->dll,HDL_DEPC_Q)))
    {
        logmsg(_("HHCHD013E No depency section in %s: %s\n"),
          dllent->name, dlerror());
        dlclose(dllent->dll);
        free(dllent);
        return -1;
    }

    for(tmpdll = hdl_dll; tmpdll; tmpdll = tmpdll->dllnext)
    {
        if(tmpdll->hdldepc == dllent->hdldepc)
        {
            logmsg(_("HHCHD016E DLL %s is duplicate of %s\n"),
              dllent->name, tmpdll->name);
            dlclose(dllent->dll);
            free(dllent);
            return -1;
        }
    }


    dllent->hdlinit = dlsym(dllent->dll,HDL_INIT_Q);

    dllent->hdlreso = dlsym(dllent->dll,HDL_RESO_Q);

    dllent->hdlddev = dlsym(dllent->dll,HDL_DDEV_Q);

    dllent->hdlfini = dlsym(dllent->dll,HDL_FINI_Q);

    /* No modules or device types registered yet */
    dllent->modent = NULL;
    dllent->hndent = NULL;

    obtain_lock(&hdl_lock);

    if(dllent->hdldepc)
    {
        if((dllent->hdldepc)(&hdl_dchk))
        {
            logmsg(_("HHCHD014E Dependency check failed for module %s\n"),
              dllent->name);
            if(!(flags & HDL_LOAD_FORCE))
            {
                dlclose(dllent->dll);
                free(dllent);
                release_lock(&hdl_lock);
                return -1;
            }
            dllent->flags |= HDL_LOAD_WAS_FORCED;
        }
    }

    hdl_cdll = dllent;

    /* Call initializer */
    if(hdl_cdll->hdlinit)
        (dllent->hdlinit)(&hdl_regi);

    /* Insert current entry as first in chain */
    dllent->dllnext = hdl_dll;
    hdl_dll = dllent;

    /* Reset the loadcounts */
    for(dllent = hdl_dll; dllent; dllent = dllent->dllnext)
        for(modent = dllent->modent; modent; modent = modent->modnext)
            modent->count = 0;

    /* Call all resolvers */
    for(dllent = hdl_dll; dllent; dllent = dllent->dllnext)
    {
        if(dllent->hdlreso)
            (dllent->hdlreso)(&hdl_fent);
    }

    /* register any device types */
    if(hdl_cdll->hdlddev)
        (hdl_cdll->hdlddev)(&hdl_dvad);

    hdl_cdll = NULL;

    release_lock(&hdl_lock);

    return 0;
}


/* hdl_dele - unload a dll
 */
int hdl_dele (char *name)
{
DLLENT **dllent, *tmpdll;
MODENT *modent, *tmpmod;
DEVBLK *dev;
HDLDEV *hnd;
char *modname;

    modname = (modname = strrchr(name,'/')) ? modname+1 : name;

    obtain_lock(&hdl_lock);

    for(dllent = &(hdl_dll); *dllent; dllent = &((*dllent)->dllnext))
    {
        if(strfilenamecmp(modname,(*dllent)->name) == 0)
        {
            if((*dllent)->flags & (HDL_LOAD_MAIN | HDL_LOAD_NOUNLOAD))
            {
                logmsg(_("HHCHD015E Unloading of %s not allowed\n"),(*dllent)->name);
                release_lock(&hdl_lock);
                return -1;
            }

            for(dev = sysblk.firstdev; dev; dev = dev->nextdev)
                if(dev->pmcw.flag5 & PMCW5_V)
                    for(hnd = (*dllent)->hndent; hnd; hnd = hnd->next)
                        if(hnd->hnd == dev->hnd)
                        {
                            logmsg(_("HHCHD008E Device %4.4X bound to %s\n"),dev->devnum,(*dllent)->name);
                            release_lock(&hdl_lock);
                            return -1;
                        }

            /* Call dll close routine */
            if((*dllent)->hdlfini)
            {
            int rc;
                
                if((rc = ((*dllent)->hdlfini)()))
                {
                    logmsg(_("HHCHD017E Unload of %s rejected by final section\n"),(*dllent)->name);
                    release_lock(&hdl_lock);
                    return rc;
                }
            }

            modent = (*dllent)->modent;
            while(modent)
            {
                tmpmod = modent;
                
                /* remove current entry from chain */
                modent = modent->modnext;

                /* free module resources */
                free(tmpmod->name);
                free(tmpmod);
            }

            tmpdll = *dllent;

            /* remove current entry from chain */
            *dllent = (*dllent)->dllnext;

            for(hnd = tmpdll->hndent; hnd;)
            {
            HDLDEV *nexthnd;
                free(hnd->name);
                nexthnd = hnd->next;
                free(hnd);
                hnd = nexthnd;
            }

//          dlclose(tmpdll->dll);

            /* free dll resources */
            free(tmpdll->name);
            free(tmpdll);

            /* Reset the loadcounts */
            for(tmpdll = hdl_dll; tmpdll; tmpdll = tmpdll->dllnext)
                for(tmpmod = tmpdll->modent; tmpmod; tmpmod = tmpmod->modnext)
                    tmpmod->count = 0;

            /* Call all resolvers */
            for(tmpdll = hdl_dll; tmpdll; tmpdll = tmpdll->dllnext)
            {
                if(tmpdll->hdlreso)
                    (tmpdll->hdlreso)(&hdl_fent);
            }

            release_lock(&hdl_lock);

            return 0;
        }
        
    }

    release_lock(&hdl_lock);

    logmsg(_("HHCHD009E %s not found\n"),modname);

    return -1;
}

#endif /*defined(OPTION_DYNAMIC_LOAD)*/
