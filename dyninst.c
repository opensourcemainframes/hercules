/* DYNINST.C    (c) Copyright Jan Jaeger, 2003                       */
/*              Hercules Dynamic Loader                              */


/* This module dynamically loads instructions.  Instruction routine  */
/* names must be registered under the name of s370_opcode_B220 for   */
/* example, where s370 may also be s390 or z900 for ESA/390 or ESAME */
/* mode respectively.  B220 is the opcode, and is depending on the   */
/* instruction 2 3 or 4 digits.                                      */


#include "hercules.h"


#if defined(WIN32)
/* We need to do some special tricks for cygwin here, since cygwin   */
/* does not support backlink and we need to resolve symbols during   */
/* dll initialisation (REGISTER/RESOLVER). Opcode tables are renamed */
/* such that no naming conflicts occur.                              */
 #define opcode_table opcode_table_r
 #define opcode_01xx  opcode_01xx_r
 #define opcode_a5xx  opcode_a5xx_r
 #define opcode_a4xx  opcode_a4xx_r
 #define opcode_a7xx  opcode_a1xx_r
 #define opcode_b2xx  opcode_b2xx_r
 #define opcode_b3xx  opcode_b3xx_r
 #define opcode_b9xx  opcode_b9xx_r
 #define opcode_c0xx  opcode_c0xx_r
 #define opcode_e3xx  opcode_e3xx_r
 #define opcode_e5xx  opcode_e5xx_r
 #define opcode_e6xx  opcode_e6xx_r
 #define opcode_ebxx  opcode_ebxx_r
 #define opcode_ecxx  opcode_ecxx_r
 #define opcode_edxx  opcode_edxx_r
 #define s370_opcode_table s370_opcode_table_r
 #define s390_opcode_table s390_opcode_table_r
 #define z900_opcode_table z900_opcode_table_r
#endif

#include "opcode.h"

#if defined(WIN32)
 #undef opcode_table
 #undef opcode_01xx
 #undef opcode_a5xx
 #undef opcode_a4xx
 #undef opcode_a7xx
 #undef opcode_b2xx
 #undef opcode_b3xx
 #undef opcode_b9xx
 #undef opcode_c0xx
 #undef opcode_e3xx
 #undef opcode_e5xx
 #undef opcode_e6xx
 #undef opcode_ebxx
 #undef opcode_ecxx
 #undef opcode_edxx
 #undef s370_opcode_table
 #undef s390_opcode_table
 #undef z900_opcode_table
#endif

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


static zz_func save_table[256][GEN_MAXARCH];
static zz_func save_01xx[256][GEN_MAXARCH];
#if defined (FEATURE_VECTOR_FACILITY)
static zz_func save_a4xx[256][GEN_MAXARCH];
#endif
static zz_func save_a5xx[16][GEN_MAXARCH];
static zz_func save_a7xx[16][GEN_MAXARCH];
static zz_func save_b2xx[256][GEN_MAXARCH];
static zz_func save_b3xx[256][GEN_MAXARCH];
static zz_func save_b9xx[256][GEN_MAXARCH];
static zz_func save_c0xx[16][GEN_MAXARCH];
static zz_func save_e3xx[256][GEN_MAXARCH];
static zz_func save_e5xx[256][GEN_MAXARCH];
static zz_func save_e6xx[256][GEN_MAXARCH];
static zz_func save_ebxx[256][GEN_MAXARCH];
static zz_func save_ecxx[256][GEN_MAXARCH];
static zz_func save_edxx[256][GEN_MAXARCH];

#if defined(WIN32)
static int opcodes_saved;
static void * opcode_table;
static void * opcode_01xx;
#if defined (FEATURE_VECTOR_FACILITY)
static void * opcode_a4xx;
#endif
static void * opcode_a5xx;
static void * opcode_a7xx;
static void * opcode_b2xx;
static void * opcode_b3xx;
static void * opcode_b9xx;
static void * opcode_c0xx;
static void * opcode_e3xx;
static void * opcode_e5xx;
static void * opcode_e6xx;
static void * opcode_ebxx;
static void * opcode_ecxx;
static void * opcode_edxx;
static void * s370_opcode_table;
static void * s390_opcode_table;
static void * z900_opcode_table;
#endif

static char *prefix[] = {
#if defined(_370)
    "s370_opcode_",
#endif
#if defined(_390)
    "s390_opcode_",
#endif
#if defined(_900)
    "z900_opcode_"
#endif
    };


static void opcode_save()    
{
    memcpy(save_table,opcode_table,sizeof(save_table));
    memcpy(save_01xx,opcode_01xx,sizeof(save_01xx));
#if defined (FEATURE_VECTOR_FACILITY)
    memcpy(save_a4xx,opcode_a4xx,sizeof(save_a4xx));
#endif
    memcpy(save_a5xx,opcode_a5xx,sizeof(save_a5xx));
    memcpy(save_a7xx,opcode_a7xx,sizeof(save_a7xx));
    memcpy(save_b2xx,opcode_b2xx,sizeof(save_b2xx));
    memcpy(save_b3xx,opcode_b3xx,sizeof(save_b3xx));
    memcpy(save_b9xx,opcode_b9xx,sizeof(save_b9xx));
    memcpy(save_c0xx,opcode_c0xx,sizeof(save_c0xx));
    memcpy(save_e3xx,opcode_e3xx,sizeof(save_e3xx));
    memcpy(save_e5xx,opcode_e5xx,sizeof(save_e5xx));
    memcpy(save_e6xx,opcode_e6xx,sizeof(save_e6xx));
    memcpy(save_ebxx,opcode_ebxx,sizeof(save_ebxx));
    memcpy(save_ecxx,opcode_ecxx,sizeof(save_ecxx));
    memcpy(save_edxx,opcode_edxx,sizeof(save_edxx));
}


static void opcode_restore()
{
    memcpy(opcode_table,save_table,sizeof(save_table));
    memcpy(opcode_01xx,save_01xx,sizeof(save_01xx));
#if defined (FEATURE_VECTOR_FACILITY)
    memcpy(opcode_a4xx,save_a4xx,sizeof(save_a4xx));
#endif
    memcpy(opcode_a5xx,save_a5xx,sizeof(save_a5xx));
    memcpy(opcode_a7xx,save_a7xx,sizeof(save_a7xx));
    memcpy(opcode_b2xx,save_b2xx,sizeof(save_b2xx));
    memcpy(opcode_b3xx,save_b3xx,sizeof(save_b3xx));
    memcpy(opcode_b9xx,save_b9xx,sizeof(save_b9xx));
    memcpy(opcode_c0xx,save_c0xx,sizeof(save_c0xx));
    memcpy(opcode_e3xx,save_e3xx,sizeof(save_e3xx));
    memcpy(opcode_e5xx,save_e5xx,sizeof(save_e5xx));
    memcpy(opcode_e6xx,save_e6xx,sizeof(save_e6xx));
    memcpy(opcode_ebxx,save_ebxx,sizeof(save_ebxx));
    memcpy(opcode_ecxx,save_ecxx,sizeof(save_ecxx));
    memcpy(opcode_edxx,save_edxx,sizeof(save_edxx));
}


static void assign_extop1(int opcode, int extop, zz_func table[256][GEN_MAXARCH],
                                                 zz_func saved[256][GEN_MAXARCH])
{
int arch;
void *tmp;

    for(arch = 0; arch < GEN_MAXARCH - 2; arch++)
    {
    char name[32];

        sprintf(name,"%s%02X%1X",prefix[arch],opcode,extop);

        if((tmp = HDL_FINDSYM(name)))
            table[extop][arch] = tmp;
        else
            table[extop][arch] = saved[extop][arch];

    }


}


static void assign_extop(int opcode, int extop, zz_func table[256][GEN_MAXARCH],
                                                zz_func saved[256][GEN_MAXARCH])
{
int arch;
void *tmp;

    for(arch = 0; arch < GEN_MAXARCH - 2; arch++)
    {
    char name[32];

        sprintf(name,"%s%02X%02X",prefix[arch],opcode,extop);

        if((tmp = HDL_FINDSYM(name)))
            table[extop][arch] = tmp;
        else
            table[extop][arch] = saved[extop][arch];

    }


}


static void assign_opcode(int opcode, zz_func table[256][GEN_MAXARCH],
                                      zz_func saved[256][GEN_MAXARCH])
{
int arch;
void *tmp;

    for(arch = 0; arch < GEN_MAXARCH -  2; arch++)
    {
    char name[32];

        sprintf(name,"%s%02X",prefix[arch],opcode);

        if((tmp = HDL_FINDSYM(name)))
            table[opcode][arch] = tmp;
        else
            table[opcode][arch] = saved[opcode][arch];

    }


}


static inline void copy_opcode(zz_func to_table[256], zz_func from_table[256][GEN_MAXARCH], int opcode, int arch_mode)
{
    to_table[opcode] = from_table[opcode][arch_mode];
}


HDL_DEPENDENCY_SECTION;
{
     HDL_DEPENDENCY (HERCULES);
     HDL_DEPENDENCY (REGS);
     HDL_DEPENDENCY (DEVBLK);
     HDL_DEPENDENCY (SYSBLK);

} END_DEPENDENCY_SECTION;


HDL_REGISTER_SECTION;
{

#if defined(WIN32)
    opcodes_saved = 0;
#else
    opcode_save();
#endif

} END_REGISTER_SECTION;


HDL_RESOLVER_SECTION;
{
int opcode, extop;

#if defined(WIN32)
    if(!opcodes_saved)
    {
        HDL_RESOLVE(opcode_table);
        HDL_RESOLVE(opcode_01xx);
#if defined(FEATURE_VECTOR_FACILITY)
        HDL_RESOLVE(opcode_a4xx);
#endif
        HDL_RESOLVE(opcode_a5xx);
        HDL_RESOLVE(opcode_a7xx);
        HDL_RESOLVE(opcode_b2xx);
        HDL_RESOLVE(opcode_b3xx);
        HDL_RESOLVE(opcode_b9xx);
        HDL_RESOLVE(opcode_c0xx);
        HDL_RESOLVE(opcode_e3xx);
        HDL_RESOLVE(opcode_e5xx);
        HDL_RESOLVE(opcode_e6xx);
        HDL_RESOLVE(opcode_ebxx);
        HDL_RESOLVE(opcode_ecxx);
        HDL_RESOLVE(opcode_edxx);
        HDL_RESOLVE(s370_opcode_table);
        HDL_RESOLVE(s390_opcode_table);
        HDL_RESOLVE(z900_opcode_table);

        opcode_save();

        opcodes_saved = 1;
    }
#endif 

    for(opcode = 0; opcode < 256; opcode++)
    {
        switch(opcode)
        {
            case 0x01:
                for(extop = 0; extop < 256; extop++)
                    assign_extop(opcode, extop, opcode_01xx, save_01xx);
                break;

#if defined (FEATURE_VECTOR_FACILITY)
            case 0xA4:
                for(extop = 0; extop < 256; extop++)
                    assign_extop(opcode, extop, v_opcode_a4xx, save_a4xx);
                break;
#endif

            case 0xA5:
                for(extop = 0; extop < 16; extop++)
                    assign_extop1(opcode, extop, opcode_a5xx, save_a5xx);
                break;

            case 0xA7:
                for(extop = 0; extop < 16; extop++)
                    assign_extop1(opcode, extop, opcode_a7xx, save_a7xx);
                break;

            case 0xB2:
                for(extop = 0; extop < 256; extop++)
                    assign_extop(opcode, extop, opcode_b2xx, save_b2xx);
                break;

            case 0xB3:
                for(extop = 0; extop < 256; extop++)
                    assign_extop(opcode, extop, opcode_b3xx, save_b3xx);
                break;

            case 0xB9:
                for(extop = 0; extop < 256; extop++)
                    assign_extop(opcode, extop, opcode_b9xx, save_b9xx);
                break;

            case 0xC0:
                for(extop = 0; extop < 16; extop++)
                    assign_extop1(opcode, extop, opcode_c0xx, save_c0xx);
                break;

            case 0xE3:
                for(extop = 0; extop < 256; extop++)
                    assign_extop(opcode, extop, opcode_e3xx, save_e3xx);
                break;

            case 0xE5:
                for(extop = 0; extop < 256; extop++)
                    assign_extop(opcode, extop, opcode_e5xx, save_e5xx);
                break;

            case 0xE6:
                for(extop = 0; extop < 256; extop++)
                    assign_extop(opcode, extop, opcode_e6xx, save_e6xx);
                break;

            case 0xEB:
                for(extop = 0; extop < 256; extop++)
                    assign_extop(opcode, extop, opcode_ebxx, save_ebxx);
                break;

            case 0xEC:
                for(extop = 0; extop < 256; extop++)
                    assign_extop(opcode, extop, opcode_ecxx, save_ecxx);
                break;

            case 0xED:
                for(extop = 0; extop < 256; extop++)
                    assign_extop(opcode, extop, opcode_edxx, save_edxx);
                break;

            default:
                assign_opcode(opcode, opcode_table, save_table);
        }

    }

    /* Copy opcodes to performance shadow table */
    for (opcode = 0; opcode < 256; opcode++)
    {
#if defined(_370)
        copy_opcode(s370_opcode_table,opcode_table,opcode,ARCH_370);
#endif
#if defined(_390)
        copy_opcode(s390_opcode_table,opcode_table,opcode,ARCH_390);
#endif
#if defined(_900)
        copy_opcode(z900_opcode_table,opcode_table,opcode,ARCH_900);
#endif
    }

} END_RESOLVER_SECTION;


HDL_FINAL_SECTION;
{

    opcode_restore();

} END_FINAL_SECTION;


#endif /*!defined(_GEN_ARCH)*/

