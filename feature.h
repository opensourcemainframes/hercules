/* FEATURES.H   (c) Copyright Jan Jaeger, 2000-2003          */
/*      Architecture-dependent macro definitions         */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/*-------------------------------------------------------------------*/
/* S/370, ESA/390 and ESAME features implemented             */
/*-------------------------------------------------------------------*/
#if !defined(FEATCHK_CHECK_DONE)
#include  "featall.h"
#include  "feat370.h"
#include  "feat390.h"
#include  "feat900.h"
#define   FEATCHK_CHECK_ALL
#include  "featchk.h"
#undef    FEATCHK_CHECK_ALL
#define   FEATCHK_CHECK_DONE
#endif /*!defined(FEATCHK_CHECK_DONE)*/

#undef __GEN_ARCH
#if defined(_GEN_ARCH)
 #define __GEN_ARCH _GEN_ARCH
#else
 #define __GEN_ARCH _ARCHMODE1
#endif

#include  "featall.h"
#if   __GEN_ARCH == 370
 #include "feat370.h"
#elif     __GEN_ARCH == 390
 #include "feat390.h"
#elif     __GEN_ARCH == 900
 #include "feat900.h"
#else
 #error Unable to determine Architecture Mode
#endif


#include  "featchk.h"


#undef ARCH_MODE
#undef APPLY_PREFIXING
#undef AMASK
#undef ADDRESS_MAXWRAP
#undef ADDRESS_MAXWRAP_E
#undef REAL_MODE
#undef PER_MODE
#undef ASF_ENABLED
#undef ASTE_AS_DESIGNATOR
#undef ASTE_LT_DESIGNATOR
#undef SAEVENT_BIT
#undef SSEVENT_BIT
#undef SSGROUP_BIT
#undef LSED_UET_HDR
#undef LSED_UET_TLR
#undef LSED_UET_BAKR
#undef LSED_UET_PC
#undef CR12_BRTRACE
#undef CR12_TRACEEA
#undef CHM_GPR2_RESV
#undef DEF_INST
#undef ARCH_DEP
#undef PSA
#undef PSA_SIZE
#undef IA
#undef PX
#undef CR
#undef GR
#undef GR_A
#undef MONCODE
#undef TEA
#undef DXC
#undef ET 
#undef PX_MASK
#undef RSTOLD
#undef RSTNEW
#undef RADR
#undef F_RADR
#undef VADR
#undef F_VADR
#undef GREG
#undef F_GREG
#undef CREG
#undef F_CREG
#undef AREG
#undef F_AREG
#undef STORE_W
#undef FETCH_W
#undef VI
#undef AI
#undef AE
#undef VE
#undef SIEBK
#undef ZPB
#undef TLB_STD
#undef TLB_VADDR
#undef TLB_PTE

#if !defined(NO_ATTR_REGPARM)
#define ATTR_REGPARM(n) __attribute__ ((regparm(n)))
#else
#define ATTR_REGPARM(n) /* nothing */
#endif

#if __GEN_ARCH == 370

#define ARCH_MODE   ARCH_370

#define DEF_INST(_name) \
void (ATTR_REGPARM(3) s370_ ## _name) (BYTE inst[], int execflag, REGS *regs)

#define ARCH_DEP(_name) \
s370_ ## _name

#define APPLY_PREFIXING(addr,pfx) \
    (((U32)((addr)&0x7FFFF000)==(U32)0)?((addr)&0xFFF)|pfx:\
    ((U32)((addr)&0x7FFFF000)==(U32)pfx)?(addr)&0xFFF:(addr))

#define AMASK   AMASK_L

#define ADDRESS_MAXWRAP(_register_context) \
    (AMASK24)

#define ADDRESS_MAXWRAP_E(_register_context) \
    (AMASK31)

#define REAL_MODE(p) \
    ((p)->ecmode==0 || ((p)->sysmask & PSW_DATMODE)==0)

#if defined(_FEATURE_SIE)
#define PER_MODE(_regs) \
        ( ((_regs)->psw.ecmode && ((_regs)->psw.sysmask & PSW_PERMODE)) \
          | ((_regs)->sie_state && ((_regs)->siebk->m & SIE_M_GPE)) )
#else
#define PER_MODE(_regs) \
        ((_regs)->psw.ecmode && ((_regs)->psw.sysmask & PSW_PERMODE))
#endif

#define ASF_ENABLED(_regs)  0 /* ASF is never enabled for S/370 */

#define ASTE_AS_DESIGNATOR(_aste) \
    ((_aste)[2])

#define ASTE_LT_DESIGNATOR(_aste) \
    ((_aste)[3])

#define SAEVENT_BIT STD_SAEVENT
#define SSEVENT_BIT STD_SSEVENT
#define SSGROUP_BIT STD_GROUP

#define PSA PSA_3XX
#define PSA_SIZE 4096
#define IA  IA_L
#define PX  PX_L
#define CR(_r)  CR_L(_r)
#define GR(_r)  GR_L(_r)
#define GR_A(_r, _regs) ((_regs)->GR_L((_r)))
#define MONCODE MC_L
#define TEA EA_L
#define DXC     tea
#define ET  ET_L
#define PX_MASK 0x7FFFF000
#define RSTOLD  iplccw1
#define RSTNEW  iplpsw
#if !defined(_FEATURE_ZSIE)
#define RADR    U32
#define F_RADR  "%8.8X"
#else
#define RADR    U64
#define F_RADR  "%8.8llX"
#endif
#define VADR    U32
#define F_VADR  "%8.8X"
#define GREG    U32
#define F_GREG  "%8.8X"
#define CREG    U32
#define F_CREG  "%8.8X"
#define AREG    U32
#define F_AREG  "%8.8X"
#define STORE_W STORE_FW
#define FETCH_W FETCH_FW
#define VI  VI_L
#define AI  AI_L
#define AE(_r)  AE_L(_r)
#define VE(_r)  VE_L(_r)
#define SIEBK                   SIE1BK
#define ZPB                     ZPB1
#define TLB_STD   TLB_STD_L
#define TLB_VADDR TLB_VADDR_L
#define TLB_PTE   TLB_PTE_L

#elif __GEN_ARCH == 390

#define ARCH_MODE   ARCH_390

#define DEF_INST(_name) \
void (ATTR_REGPARM(3) s390_ ## _name) (BYTE inst[], int execflag, REGS *regs)

#define ARCH_DEP(_name) \
s390_ ## _name

#define APPLY_PREFIXING(addr,pfx) \
    (((U32)((addr)&0x7FFFF000)==(U32)0)    ?((addr)&0xFFF)|(pfx):\
     ((U32)((addr)&0x7FFFF000)==(U32)(pfx))?((addr)&0xFFF):(addr))

#define AMASK   AMASK_L

#define ADDRESS_MAXWRAP(_register_context) \
    ((_register_context)->psw.AMASK)

#define ADDRESS_MAXWRAP_E(_register_context) \
    ((_register_context)->psw.AMASK)

#define REAL_MODE(p) \
    (((p)->sysmask & PSW_DATMODE)==0)

#if defined(_FEATURE_SIE)
#define PER_MODE(_regs) \
        ( ((_regs)->psw.sysmask & PSW_PERMODE) \
          | ((_regs)->sie_state && ((_regs)->siebk->m & SIE_M_GPE)) )
#else
#define PER_MODE(_regs) \
        ((_regs)->psw.sysmask & PSW_PERMODE)
#endif

#define ASF_ENABLED(_regs)  ((_regs)->CR(0) & CR0_ASF)

#define ASTE_AS_DESIGNATOR(_aste) \
    ((_aste)[2])

#define ASTE_LT_DESIGNATOR(_aste) \
    ((_aste)[3])

#define SAEVENT_BIT STD_SAEVENT
#define SSEVENT_BIT STD_SSEVENT
#define SSGROUP_BIT STD_GROUP

#define LSED_UET_HDR    S_LSED_UET_HDR
#define LSED_UET_TLR    S_LSED_UET_TLR
#define LSED_UET_BAKR   S_LSED_UET_BAKR
#define LSED_UET_PC S_LSED_UET_PC
#define CR12_BRTRACE    S_CR12_BRTRACE
#define CR12_TRACEEA    S_CR12_TRACEEA

#define CHM_GPR2_RESV   S_CHM_GPR2_RESV

#define PSA PSA_3XX
#define PSA_SIZE 4096
#define IA  IA_L
#define PX  PX_L
#define CR(_r)  CR_L(_r)
#define GR(_r)  GR_L(_r)
#define GR_A(_r, _regs) ((_regs)->GR_L((_r)))
#define MONCODE MC_L
#define TEA EA_L
#define DXC     tea
#define ET  ET_L
#define PX_MASK 0x7FFFF000
#define RSTNEW  iplpsw
#define RSTOLD  iplccw1
#if !defined(_FEATURE_ZSIE)
#define RADR    U32
#define F_RADR  "%8.8X"
#else
#define RADR    U64
#define F_RADR  "%8.8llX"
#endif
#define VADR    U32
#define F_VADR  "%8.8X"
#define GREG    U32
#define F_GREG  "%8.8X"
#define CREG    U32
#define F_CREG  "%8.8X"
#define AREG    U32
#define F_AREG  "%8.8X"
#define STORE_W STORE_FW
#define FETCH_W FETCH_FW
#define VI  VI_L
#define AI  AI_L
#define AE(_r)  AE_L(_r)
#define VE(_r)  VE_L(_r)
#define SIEBK                   SIE1BK
#define ZPB                     ZPB1
#define TLB_STD   TLB_STD_L
#define TLB_VADDR TLB_VADDR_L
#define TLB_PTE   TLB_PTE_L

#elif __GEN_ARCH == 900

#define ARCH_MODE   ARCH_900

#define APPLY_PREFIXING(addr,pfx) \
    (((U64)((addr)&0xFFFFFFFFFFFFE000ULL)==(U64)0)?((addr)&0x1FFF)|(pfx):\
    ((U64)((addr)&0xFFFFFFFFFFFFE000ULL)==(U64)(pfx))?(addr)&0x1FFF:(addr))

#define AMASK   AMASK_G

#define ADDRESS_MAXWRAP(_register_context) \
    ((_register_context)->psw.AMASK)

#define ADDRESS_MAXWRAP_E(_register_context) \
    ((_register_context)->psw.AMASK)

#define REAL_MODE(p) \
    (((p)->sysmask & PSW_DATMODE)==0)

#if defined(_FEATURE_SIE)
#define PER_MODE(_regs) \
        ( ((_regs)->psw.sysmask & PSW_PERMODE) \
          | ((_regs)->sie_state && ((_regs)->siebk->m & SIE_M_GPE)) )
#else
#define PER_MODE(_regs) \
        ((_regs)->psw.sysmask & PSW_PERMODE)
#endif

#define ASF_ENABLED(_regs)  1 /* ASF is always enabled for ESAME */

#define ASTE_AS_DESIGNATOR(_aste) \
    (((U64)((_aste)[2])<<32)|(U64)((_aste)[3]))

#define ASTE_LT_DESIGNATOR(_aste) \
    ((_aste)[6])

#define SAEVENT_BIT ASCE_S
#define SSEVENT_BIT ASCE_X
#define SSGROUP_BIT ASCE_G

#define LSED_UET_HDR    Z_LSED_UET_HDR
#define LSED_UET_TLR    Z_LSED_UET_TLR
#define LSED_UET_BAKR   Z_LSED_UET_BAKR
#define LSED_UET_PC Z_LSED_UET_PC
#define CR12_BRTRACE    Z_CR12_BRTRACE
#define CR12_TRACEEA    Z_CR12_TRACEEA

#define CHM_GPR2_RESV   Z_CHM_GPR2_RESV

#define DEF_INST(_name) \
void (ATTR_REGPARM(3) z900_ ## _name) (BYTE inst[], int execflag, REGS *regs)

#define ARCH_DEP(_name) \
z900_ ## _name

#define PSA PSA_900
#define PSA_SIZE 8192
#define IA  IA_G
#define PX  PX_L
#define CR(_r)  CR_G(_r)
#define GR(_r)  GR_G(_r)
#define GR_A(_r, _regs) ((_regs)->psw.amode64 ? (_regs)->GR_G((_r)) : (_regs)->GR_L((_r)))
#define MONCODE MC_G
#define TEA EA_G
#define DXC     dataexc
#define ET  ET_G
#define PX_MASK 0x7FFFE000
#define RSTOLD  rstold
#define RSTNEW  rstnew
#define RADR    U64
#define F_RADR  "%16.16llX"
#define VADR    U64
#define F_VADR  "%16.16llX"
#define GREG    U64
#define F_GREG  "%16.16llX"
#define CREG    U64
#define F_CREG  "%16.16llX"
#define AREG    U32
#define F_AREG  "%8.8X"
#define STORE_W STORE_DW
#define FETCH_W FETCH_DW
#define VI  VI_G
#define AI  AI_G
#define AE(_r)  AE_G(_r)
#define VE(_r)  VE_G(_r)
#define SIEBK                   SIE2BK
#define ZPB                     ZPB2
#define TLB_STD   TLB_STD_G
#define TLB_VADDR TLB_VADDR_G
#define TLB_PTE   TLB_PTE_G

#else

#warning __GEN_ARCH must be 370, 390, 900 or undefined

#endif

#undef PAGEFRAME_PAGESIZE
#undef PAGEFRAME_PAGESHIFT
#undef PAGEFRAME_BYTEMASK
#undef PAGEFRAME_PAGEMASK
#undef MAXADDRESS
#if defined(FEATURE_ESAME)
 #define PAGEFRAME_PAGESIZE 4096
 #define PAGEFRAME_PAGESHIFT    12
 #define PAGEFRAME_BYTEMASK 0x00000FFF
 #define PAGEFRAME_PAGEMASK 0xFFFFFFFFFFFFF000ULL
 #define MAXADDRESS             0xFFFFFFFFFFFFFFFFULL
#elif defined(FEATURE_S390_DAT)
 #define PAGEFRAME_PAGESIZE 4096
 #define PAGEFRAME_PAGESHIFT    12
 #define PAGEFRAME_BYTEMASK 0x00000FFF
 #define PAGEFRAME_PAGEMASK 0x7FFFF000
 #define MAXADDRESS             0x7FFFFFFF
#else /* S/370 */
 #define PAGEFRAME_PAGESIZE 2048
 #define PAGEFRAME_PAGESHIFT    11
 #define PAGEFRAME_BYTEMASK 0x000007FF
 #define PAGEFRAME_PAGEMASK 0x7FFFF800
 #if defined(FEATURE_370E_EXTENDED_ADDRESSING)
  #define MAXADDRESS             0x03FFFFFF
 #else
  #define MAXADDRESS             0x00FFFFFF
 #endif
#endif


#if !defined(_FEATURE_2K_STORAGE_KEYS)
 #define STORAGE_KEY_UNITSIZE 4096
#else
 #define STORAGE_KEY_UNITSIZE 2048
#endif

 #undef STORAGE_KEY
 #undef STORAGE_KEY_PAGESHIFT
 #undef STORAGE_KEY_PAGESIZE
 #undef STORAGE_KEY_PAGEMASK
 #undef STORAGE_KEY_BYTEMASK
#ifdef FEATURE_4K_STORAGE_KEYS
 #if defined(_FEATURE_2K_STORAGE_KEYS)
  #define STORAGE_KEY_PAGESHIFT 11
 #else
  #define STORAGE_KEY_PAGESHIFT 12
 #endif
 #define STORAGE_KEY_PAGESIZE   4096
 #if defined(FEATURE_ESAME)
  #define STORAGE_KEY_PAGEMASK  0xFFFFFFFFFFFFF000ULL
 #else
  #define STORAGE_KEY_PAGEMASK  0x7FFFF000
 #endif
 #define STORAGE_KEY_BYTEMASK   0x00000FFF
#else
 #define STORAGE_KEY_PAGESHIFT  11
 #define STORAGE_KEY_PAGESIZE   2048
 #define STORAGE_KEY_PAGEMASK   0x7FFFF800
 #define STORAGE_KEY_BYTEMASK   0x000007FF
#endif

#define STORAGE_KEY(_addr, _pointer) \
   (_pointer)->storkeys[(_addr)>>STORAGE_KEY_PAGESHIFT]

#if defined(_FEATURE_2K_STORAGE_KEYS)
 #define STORAGE_KEY1(_addr, _pointer) \
    (_pointer)->storkeys[((_addr)>>STORAGE_KEY_PAGESHIFT)&~1]
 #define STORAGE_KEY2(_addr, _pointer) \
    (_pointer)->storkeys[((_addr)>>STORAGE_KEY_PAGESHIFT)|1]
#endif


#define XSTORE_INCREMENT_SIZE   0x00100000
#define XSTORE_PAGESHIFT    12
#define XSTORE_PAGESIZE     4096
#undef   XSTORE_PAGEMASK
#if defined(FEATURE_ESAME) || defined(_FEATURE_ZSIE)
 #define XSTORE_PAGEMASK    0xFFFFFFFFFFFFF000ULL
#else
 #define XSTORE_PAGEMASK    0x7FFFF000
#endif

/* end of FEATURES.H */
