/* HDTEQ.C      (c) Copyright Jan Jaeger, 2003                       */
/*              Hercules Dynamic Loader                              */


#include "hercules.h"


typedef struct _DTEQ {
    char *alias;
    char *name;
} DTEQ;


static DTEQ dteq[] = {
/*
    This table provides aliases for device types, such that various 
    device types may be mapped to a common loadable module.

    The only purpose of this table is to associate the right loadable 
    module with a specific device type, before the device type in 
    question has been registered.  This table will not be searched 
    for registered device types or if the specific loadable module exists.

       device type requested 
       |
       |         base device support
       |         |
       V         V                                                   */

//  { "3390",   "3990" },
//  { "3380",   "3990" },

    { "1052",   "3270" },
    { "3215",   "3270" },
    { "3278",   "3270" },

    { "1442",   "3505" },
    { "2501",   "3505" },

    { "3211",   "1403" },

    { "3410",   "3420" },
    { "3411",   "3420" },
    { "3420",   "3420" },
    { "3480",   "3420" },
    { "3490",   "3420" },
    { "9347",   "3420" },
    { "9348",   "3420" },
    { "8809",   "3420" },

    { NULL,     NULL   } };


static char *hdt_device_type_equates(char *typname)
{
DTEQ *device_type;
char *(*nextcall)(char *);

    for(device_type = dteq; device_type->name; device_type++)
        if(!strcasecmp(device_type->alias, typname))
            return device_type->name;

    if((nextcall = HDL_FINDNXT(hdt_device_type_equates)))
        return nextcall(typname);

    return NULL;
}


HDL_DEPENDENCY_SECTION;
{
     HDL_DEPENDENCY(HERCULES);
}
END_DEPENDENCY_SECTION;


HDL_REGISTER_SECTION;
{
    HDL_REGISTER(hdl_device_type_equates,hdt_device_type_equates);
}
END_REGISTER_SECTION;
