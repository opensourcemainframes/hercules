/* DYNINST.C    (c) Copyright Jan Jaeger, 2003-2010                  */
/*              Hercules Dynamic Loader                              */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/* This module dynamically loads instructions.  Instruction routine  */
/* names must be registered under the name of s370_opcode_B220 for   */
/* example, where s370 may also be s390 or z900 for ESA/390 or ESAME */
/* mode respectively.  B220 is the opcode, and is depending on the   */
/* instruction 2 3 or 4 digits.                                      */

#include "hstdinc.h"
#include "hercules.h"

#if defined(OPTION_DYNAMIC_LOAD)

#include "opcode.h"
#include "inline.h"

#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "dyninst.c"
#endif 

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "dyninst.c"
#endif 

typedef struct
{
  BYTE opcode1;
  BYTE opcode2;
  int arch;
  zz_func newinst;
  zz_func oldinst;
}
DYNINST;

#define MAXDYNINST 100
static DYNINST dyninst[MAXDYNINST];
static int dyninst_index = 0;

static char *prefix[] = {
#if defined(_370)
  "s370_dyninst_opcode_",
#endif
#if defined(_390)
  "s390_dyninst_opcode_",
#endif
#if defined(_900)
  "z900_dyninst_opcode_",
#endif
  NULL
};

static void init_dyninst()
{
  int i;

  for(i = 0; i < MAXDYNINST; i++)
  {
    dyninst[i].opcode1 = 0;
    dyninst[i].opcode2 = 0;
    dyninst[i].arch = -1;
    dyninst[i].newinst = NULL;
    dyninst[i].oldinst = NULL;
  }
}

static void destroy_dyninst()
{
  int i;

  /* Wait if opcode_replace pointer not present */
  while(!sysblk.replace_opcode)
    usleep(100);

  for(i = 0; i < dyninst_index; i++)
    sysblk.replace_opcode(dyninst[i].arch, dyninst[i].oldinst, dyninst[i].opcode1, dyninst[i].opcode2);
}

/* Libtool static name colision resolution */
/* note : lt_dlopen will look for symbol & modulename_LTX_symbol */
#if !defined(HDL_BUILD_SHARED) && defined(HDL_USE_LIBTOOL)
#define hdl_ddev dyninst_LTX_hdl_ddev
#define hdl_depc dyninst_LTX_hdl_depc
#define hdl_reso dyninst_LTX_hdl_reso
#define hdl_init dyninst_LTX_hdl_init
#define hdl_fini dyninst_LTX_hdl_fini
#endif


HDL_DEPENDENCY_SECTION;
{
  HDL_DEPENDENCY (HERCULES);
  HDL_DEPENDENCY (REGS);
  HDL_DEPENDENCY (DEVBLK);
  HDL_DEPENDENCY (SYSBLK);
} 
END_DEPENDENCY_SECTION


HDL_REGISTER_SECTION;
{
  init_dyninst();
}
END_REGISTER_SECTION


HDL_RESOLVER_SECTION;
{
  int arch;
  char name[64];
  zz_func newinst;
  zz_func oldinst;
  int opcode1;
  int opcode2;


  /* Wait if opcode_replace pointer not present */
  while(!sysblk.replace_opcode)
    usleep(100);

  for(arch = 0; arch < GEN_ARCHCOUNT; arch++)
  {
    for(opcode1 = 0; opcode1 < 0x100 && dyninst_index < MAXDYNINST; opcode1++)
    {
      snprintf(name, sizeof(name), "%s%02X", prefix[arch], opcode1);
      newinst = HDL_FINDSYM(name);
      if(newinst)
      {
        oldinst = sysblk.replace_opcode(arch, newinst, opcode1, -1);
        if(oldinst)
        {
          dyninst[dyninst_index].opcode1 = opcode1;
          dyninst[dyninst_index].opcode2 = -1;
          dyninst[dyninst_index].arch = arch;
          dyninst[dyninst_index].newinst = newinst;
          dyninst[dyninst_index].oldinst = oldinst;
          dyninst_index++;
        }
      }
      for(opcode2 = 0; opcode2 < 0x100 && dyninst_index < MAXDYNINST; opcode2++)
      {
        snprintf(name, sizeof(name), "%s%02X%02X", prefix[arch], opcode1, opcode2);
        newinst = HDL_FINDSYM(name);
        if(newinst)
        {
          oldinst = sysblk.replace_opcode(arch, newinst, opcode1, opcode2);
          if(oldinst)
          {
            dyninst[dyninst_index].opcode1 = opcode1;
            dyninst[dyninst_index].opcode2 = opcode2;
            dyninst[dyninst_index].arch = arch;
            dyninst[dyninst_index].newinst = newinst;
            dyninst[dyninst_index].oldinst = oldinst;
            dyninst_index++;
          }
        }
      }
    }
  }
}
END_RESOLVER_SECTION

HDL_FINAL_SECTION;
{
  destroy_dyninst();
}
END_FINAL_SECTION


#endif /*!defined(_GEN_ARCH)*/

#endif /*defined(OPTION_DYNAMIC_LOAD)*/
