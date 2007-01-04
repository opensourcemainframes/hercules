/* VSTORE.H     (c) Copyright Roger Bowler, 1999-2006                */
/*              ESA/390 Virtual Storage Functions                    */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2006      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2006      */

// $Id$

/*-------------------------------------------------------------------*/
/* This module contains various functions which store, fetch, and    */
/* copy values to, from, or between virtual storage locations.       */
/*                                                                   */
/* Functions provided in this module are:                            */
/* vstorec      Store 1 to 256 characters into virtual storage       */
/* vstoreb      Store a single byte into virtual storage             */
/* vstore2      Store a two-byte integer into virtual storage        */
/* vstore4      Store a four-byte integer into virtual storage       */
/* vstore8      Store an eight-byte integer into virtual storage     */
/* vfetchc      Fetch 1 to 256 characters from virtual storage       */
/* vfetchb      Fetch a single byte from virtual storage             */
/* vfetch2      Fetch a two-byte integer from virtual storage        */
/* vfetch4      Fetch a four-byte integer from virtual storage       */
/* vfetch8      Fetch an eight-byte integer from virtual storage     */
/* instfetch    Fetch instruction from virtual storage               */
/* move_chars   Move characters using specified keys and addrspaces  */
/* validate_operand   Validate addressing, protection, translation   */
/*-------------------------------------------------------------------*/
/* And provided by means of macro's address wrapping versions of     */
/* the above:                                                        */
/* wstoreX                                                           */
/* wfetchX                                                           */
/* wmove_chars                                                       */
/* wvalidate_operand                                                 */
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.69  2007/01/04 00:29:17  gsmith
// 03 Jan 2007 vstorex patch to vstore2, vstore4, vstore8
//
// Revision 1.68  2007/01/03 05:53:34  gsmith
// 03 Jan 2007 Sloppy fetch - Greg Smith
//
// Revision 1.67  2006/12/20 04:26:20  gsmith
// 19 Dec 2006 ip_all.pat - performance patch - Greg Smith
//
// Revision 1.66  2006/12/08 09:43:31  jj
// Add CVS message log
//

#define s370_wstorec(_src, _len, _addr, _arn, _regs) \
        s370_vstorec((_src), (_len), ((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs)) 
#define s370_wstoreb(_value, _addr, _arn, _regs) \
        s370_vstoreb((_value), ((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define s370_wstore2(_value, _addr, _arn, _regs) \
        s370_vstore2((_value), ((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define s370_wstore4(_value, _addr, _arn, _regs) \
        s370_vstore4((_value), ((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define s370_wstore8(_value, _addr, _arn, _regs) \
        s370_vstore8((_value), ((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define s370_wfetchc(_dest, _len, _addr, _arn, _regs) \
        s370_vfetchc((_dest), (_len), ((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs)) 
#define s370_wfetchb(_addr, _arn, _regs) \
        s370_vfetchb(((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define s370_wfetch2(_addr, _arn, _regs) \
        s370_vfetch2(((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define s370_wfetch4(_addr, _arn, _regs) \
        s370_vfetch4(((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define s370_wfetch8(_addr, _arn, _regs) \
        s370_vfetch8(((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define s370_wmove_chars(_addr1, _arn1, _key1, _addr2, _arn2, _key2, _len, _regs) \
        s370_move_chars(((_addr1) & ADDRESS_MAXWRAP((_regs))), (_arn1), (_key1), \
                        ((_addr2) & ADDRESS_MAXWRAP((_regs))), (_arn2), (_key2), (_len), (_regs))
#define s370_wvalidate_operand(_addr, _arn, _len, _acctype, _regs) \
        s370_validate_operand(((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_len), (_acctype), (_regs))

#define s390_wstorec(_src, _len, _addr, _arn, _regs) \
        s390_vstorec((_src), (_len), ((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs)) 
#define s390_wstoreb(_value, _addr, _arn, _regs) \
        s390_vstoreb((_value), ((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define s390_wstore2(_value, _addr, _arn, _regs) \
        s390_vstore2((_value), ((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define s390_wstore4(_value, _addr, _arn, _regs) \
        s390_vstore4((_value), ((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define s390_wstore8(_value, _addr, _arn, _regs) \
        s390_vstore8((_value), ((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define s390_wfetchc(_dest, _len, _addr, _arn, _regs) \
        s390_vfetchc((_dest), (_len), ((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs)) 
#define s390_wfetchb(_addr, _arn, _regs) \
        s390_vfetchb(((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define s390_wfetch2(_addr, _arn, _regs) \
        s390_vfetch2(((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define s390_wfetch4(_addr, _arn, _regs) \
        s390_vfetch4(((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define s390_wfetch8(_addr, _arn, _regs) \
        s390_vfetch8(((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define s390_wmove_chars(_addr1, _arn1, _key1, _addr2, _arn2, _key2, _len, _regs) \
        s390_move_chars(((_addr1) & ADDRESS_MAXWRAP((_regs))), (_arn1), (_key1), \
                        ((_addr2) & ADDRESS_MAXWRAP((_regs))), (_arn2), (_key2), (_len), (_regs))
#define s390_wvalidate_operand(_addr, _arn, _len, _acctype, _regs) \
        s390_validate_operand(((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_len), (_acctype), (_regs))

#define z900_wstorec(_src, _len, _addr, _arn, _regs) \
        z900_vstorec((_src), (_len), ((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs)) 
#define z900_wstoreb(_value, _addr, _arn, _regs) \
        z900_vstoreb((_value), ((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define z900_wstore2(_value, _addr, _arn, _regs) \
        z900_vstore2((_value), ((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define z900_wstore4(_value, _addr, _arn, _regs) \
        z900_vstore4((_value), ((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define z900_wstore8(_value, _addr, _arn, _regs) \
        z900_vstore8((_value), ((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define z900_wfetchc(_dest, _len, _addr, _arn, _regs) \
        z900_vfetchc((_dest), (_len), ((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs)) 
#define z900_wfetchb(_addr, _arn, _regs) \
        z900_vfetchb(((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define z900_wfetch2(_addr, _arn, _regs) \
        z900_vfetch2(((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define z900_wfetch4(_addr, _arn, _regs) \
        z900_vfetch4(((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define z900_wfetch8(_addr, _arn, _regs) \
        z900_vfetch8(((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_regs))
#define z900_wmove_chars(_addr1, _arn1, _key1, _addr2, _arn2, _key2, _len, _regs) \
        z900_move_chars(((_addr1) & ADDRESS_MAXWRAP((_regs))), (_arn1), (_key1), \
                        ((_addr2) & ADDRESS_MAXWRAP((_regs))), (_arn2), (_key2), (_len), (_regs))
#define z900_wvalidate_operand(_addr, _arn, _len, _acctype, _regs) \
        z900_validate_operand(((_addr) & ADDRESS_MAXWRAP((_regs))), (_arn), (_len), (_acctype), (_regs))

/*-------------------------------------------------------------------*/
/*              Operand Length Checking Macros                       */
/*                                                                   */
/* The following macros are used to determine whether an operand     */
/* storage access will cross a 2K page boundary or not.              */
/*                                                                   */
/* The first 'plain' pair of macros (without the 'L') are used for   */
/* 0-based lengths wherein zero = 1 byte is being referenced and 255 */
/* means 256 bytes are being referenced. They are obviously designed */
/* for maximum length values of 0-255 as used w/MVC instructions.    */
/*                                                                   */
/* The second pair of 'L' macros are using for 1-based lengths where */
/* 0 = no bytes are being referenced, 1 = one byte, etc. They are    */
/* designed for 'Large' maximum length values such as occur with the */
/* MVCL instruction for example (where the length can be up to 16MB) */
/*-------------------------------------------------------------------*/

#define NOCROSS2K(_addr,_len) likely( ( (int)((_addr) & 0x7FF)) <= ( 0x7FF - (_len) ) )
#define CROSS2K(_addr,_len) unlikely( ( (int)((_addr) & 0x7FF)) > ( 0x7FF - (_len) ) )

#define NOCROSS2KL(_addr,_len) likely( ( (int)((_addr) & 0x7FF)) <= ( 0x800 - (_len) ) )
#define CROSS2KL(_addr,_len) unlikely( ( (int)((_addr) & 0x7FF)) > ( 0x800 - (_len) ) )

#if !defined(OPTION_NO_INLINE_VSTORE) || defined(_VSTORE_C)

/*-------------------------------------------------------------------*/
/* Store 1 to 256 characters into virtual storage operand            */
/*                                                                   */
/* Input:                                                            */
/*      src     1 to 256 byte input buffer                           */
/*      len     Size of operand minus 1                              */
/*      addr    Logical address of leftmost character of operand     */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/*                                                                   */
/*      A program check may be generated if the logical address      */
/*      range causes an addressing, translation, or protection       */
/*      exception, and in this case no real storage locations are    */
/*      updated, and the function does not return.                   */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC void ARCH_DEP(vstorec) (void *src, BYTE len,
                                        VADR addr, int arn, REGS *regs)
{
BYTE   *main1, *main2;                  /* Mainstor addresses        */
BYTE   *sk;                             /* Storage key addresses     */
int     len2;                           /* Length to end of page     */

    if ( NOCROSS2K(addr,len) )
    {
        memcpy(MADDR(addr, arn, regs, ACCTYPE_WRITE, regs->psw.pkey),
               src, len + 1);
        ITIMER_UPDATE(addr,len,regs);
    }
    else
    {
        len2 = 0x800 - (addr & 0x7FF);
        main1 = MADDR(addr, arn, regs, ACCTYPE_WRITE_SKP,
                      regs->psw.pkey);
        sk = regs->dat.storkey;
        main2 = MADDR((addr + len2) & ADDRESS_MAXWRAP(regs), arn,
                      regs, ACCTYPE_WRITE, regs->psw.pkey);
        *sk |= (STORKEY_REF | STORKEY_CHANGE);
        memcpy (main1, src, len2);
        memcpy (main2, (BYTE*)src + len2, len + 1 - len2);
    }

} /* end function ARCH_DEP(vstorec) */

/*-------------------------------------------------------------------*/
/* Store a single byte into virtual storage operand                  */
/*                                                                   */
/* Input:                                                            */
/*      value   Byte value to be stored                              */
/*      addr    Logical address of operand byte                      */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/*                                                                   */
/*      A program check may be generated if the logical address      */
/*      causes an addressing, translation, or protection             */
/*      exception, and in this case the function does not return.    */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC void ARCH_DEP(vstoreb) (BYTE value, VADR addr,
                                         int arn, REGS *regs)
{
BYTE   *main1;                          /* Mainstor address          */

    main1 = MADDR(addr, arn, regs, ACCTYPE_WRITE, regs->psw.pkey);
    *main1 = value;
    ITIMER_UPDATE(addr,1-1,regs);

} /* end function ARCH_DEP(vstoreb) */

/*-------------------------------------------------------------------*/
/* Store a two-byte integer into virtual storage operand             */
/*                                                                   */
/* Input:                                                            */
/*      value   16-bit integer value to be stored                    */
/*      addr    Logical address of leftmost operand byte             */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/*                                                                   */
/*      A program check may be generated if the logical address      */
/*      causes an addressing, translation, or protection             */
/*      exception, and in this case the function does not return.    */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC void ARCH_DEP(vstore2) (U16 value, VADR addr, int arn,
                                         REGS *regs)
{
BYTE   *mn1, *mn2;                      /* Mainstor addresses        */
BYTE   *sk;                             /* Storage key address       */

    /* Quick out if boundary not crossed */
    if (likely((addr & 0x7FF) <= 0x7FE))
    {
        mn1 = MADDR(addr, arn, regs, ACCTYPE_WRITE, regs->psw.pkey);
        STORE_HW(mn1, value);
        ITIMER_UPDATE(addr,2-1,regs);
        return;
    }

    /* Get mainstor address of the first page */
    mn1 = MADDR(addr, arn, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);
    sk = regs->dat.storkey;

    /* Get mainstor address of the second page */
    addr ++;
    addr &= ADDRESS_MAXWRAP(regs);
    mn2 = MADDR(addr, arn, regs, ACCTYPE_WRITE, regs->psw.pkey);

    /* Now set storage key bits for the first page */
    *sk |= (STORKEY_REF | STORKEY_CHANGE);

    *mn1 = value >> 8;
    *mn2 = value & 0xFF;
} /* end function ARCH_DEP(vstore2) */

/*-------------------------------------------------------------------*/
/* Store a four-byte integer into virtual storage operand            */
/*                                                                   */
/* Input:                                                            */
/*      value   32-bit integer value to be stored                    */
/*      addr    Logical address of leftmost operand byte             */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/*                                                                   */
/*      A program check may be generated if the logical address      */
/*      causes an addressing, translation, or protection             */
/*      exception, and in this case the function does not return.    */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC void ARCH_DEP(vstore4) (U32 value, VADR addr, int arn,
                                         REGS *regs)
{
BYTE   *mn1, *mn2, *stop;               /* Mainstor addresses        */
BYTE   *sk;                             /* Storage key address       */
int     len;                            /* Length to end of page     */
BYTE   *p;                              /* -> copied value           */
U32     temp;                           /* Value in right byte order */

    /* Quick out if boundary not crossed */
    if (likely((addr & 0x7FF) <= 0x7FC))
    {
        mn1 = MADDR(addr, arn, regs, ACCTYPE_WRITE, regs->psw.pkey);
        STORE_FW(mn1, value);
        ITIMER_UPDATE(addr,4-1,regs);
        return;
    }

    /* Get mainstor address of the first page */
    mn1 = MADDR(addr, arn, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);
    sk = regs->dat.storkey;

    /* Get mainstor address of the second page */
    len = 0x800 - (addr & 0x7FF);
    addr += len;
    addr &= ADDRESS_MAXWRAP(regs);
    mn2 = MADDR(addr, arn, regs, ACCTYPE_WRITE, regs->psw.pkey);

    /* Now set storage key bits for the first page */
    *sk |= (STORKEY_REF | STORKEY_CHANGE);

    /* Store value in intermediate area */
    temp = CSWAP32(value);
    p = (BYTE *)&temp;

    /* Copy to end of first page */
    stop = mn1 + len;
    do {
        *mn1++ = *p++;
    } while (mn1 < stop);

    /* Copy to beginning of second page */
    stop = mn2 + (4-len);
    do {
        *mn2++ = *p++;
    } while (mn2 < stop);
}

/*-------------------------------------------------------------------*/
/* Store an eight-byte integer into virtual storage operand          */
/*                                                                   */
/* Input:                                                            */
/*      value   64-bit integer value to be stored                    */
/*      addr    Logical address of leftmost operand byte             */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/*                                                                   */
/*      A program check may be generated if the logical address      */
/*      causes an addressing, translation, or protection             */
/*      exception, and in this case the function does not return.    */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC void ARCH_DEP(vstore8) (U64 value, VADR addr, int arn,
                                         REGS *regs)
{
U64    *mn;                             /* Mainstor address          */
BYTE   *mn1, *mn2, *stop;               /* Mainstor addresses        */
BYTE   *sk;                             /* Storage key address       */
int     len;                            /* Length to end of page     */
BYTE   *p;                              /* -> copied value           */
U64     temp;                           /* Value in right byte order */

    /* Quick out if boundary not crossed */
    if (likely((addr & 0x7FF) <= 0x7F8))
    {
        mn = (U64 *)MADDR(addr, arn, regs, ACCTYPE_WRITE, regs->psw.pkey);
#if defined(OPTION_SINGLE_CPU_DW) && defined(ASSIST_STORE_DW)
        if (regs->cpubit == regs->sysblk->started_mask)
            *mn = CSWAP64(value);
        else
#endif
        STORE_DW(mn, value);
        ITIMER_UPDATE(addr,8-1,regs);
        return;
    }

    /* Get mainstor address of the first page */
    mn1 = MADDR(addr, arn, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);
    sk = regs->dat.storkey;

    /* Get mainstor address of the second page */
    len = 0x800 - (addr & 0x7FF);
    addr += len;
    addr &= ADDRESS_MAXWRAP(regs);
    mn2 = MADDR(addr, arn, regs, ACCTYPE_WRITE, regs->psw.pkey);

    /* Now set storage key bits for the first page */
    *sk |= (STORKEY_REF | STORKEY_CHANGE);

    /* Store value in intermediate area */
    temp = CSWAP64(value);
    p = (BYTE *)&temp;

    /* Copy to end of first page */
    stop = mn1 + len;
    do {
        *mn1++ = *p++;
    } while (mn1 < stop);

    /* Copy to beginning of second page */
    stop = mn2 + (8-len);
    do {
        *mn2++ = *p++;
    } while (mn2 < stop);

} /* end function ARCH_DEP(vstore8) */

/*-------------------------------------------------------------------*/
/* Fetch a 1 to 256 character operand from virtual storage           */
/*                                                                   */
/* Input:                                                            */
/*      len     Size of operand minus 1                              */
/*      addr    Logical address of leftmost character of operand     */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/* Output:                                                           */
/*      dest    1 to 256 byte output buffer                          */
/*                                                                   */
/*      A program check may be generated if the logical address      */
/*      causes an addressing, translation, or fetch protection       */
/*      exception, and in this case the function does not return.    */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC void ARCH_DEP(vfetchc) (void *dest, BYTE len,
                                        VADR addr, int arn, REGS *regs)
{
BYTE   *main1, *main2;                  /* Main storage addresses    */
int     len2;                           /* Length to copy on page    */

    main1 = MADDR(addr,arn,regs,ACCTYPE_READ,regs->psw.pkey);

    if ( NOCROSS2K(addr,len) )
    {
        ITIMER_SYNC(addr,len,regs);
        memcpy (dest, main1, len + 1);
    }
    else
    {
        len2 = 0x800 - (addr & 0x7FF);
        main2 = MADDR ((addr + len2) & ADDRESS_MAXWRAP(regs),
                       arn, regs, ACCTYPE_READ, regs->psw.pkey);
        memcpy (dest, main1, len2);
        memcpy ((BYTE*)dest + len2, main2, len + 1 - len2);
    }

} /* end function ARCH_DEP(vfetchc) */

/*-------------------------------------------------------------------*/
/* Fetch a single byte operand from virtual storage                  */
/*                                                                   */
/* Input:                                                            */
/*      addr    Logical address of operand character                 */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/* Returns:                                                          */
/*      Operand byte                                                 */
/*                                                                   */
/*      A program check may be generated if the logical address      */
/*      causes an addressing, translation, or fetch protection       */
/*      exception, and in this case the function does not return.    */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC BYTE ARCH_DEP(vfetchb) (VADR addr, int arn,
                                         REGS *regs)
{
BYTE   *mn;                           /* Main storage address      */

    ITIMER_SYNC(addr,1-1,regs);
    mn = MADDR (addr, arn, regs, ACCTYPE_READ, regs->psw.pkey);
    return *mn;
} /* end function ARCH_DEP(vfetchb) */

/*-------------------------------------------------------------------*/
/* Fetch a two-byte integer operand from virtual storage             */
/*                                                                   */
/* Input:                                                            */
/*      addr    Logical address of leftmost byte of operand          */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/* Returns:                                                          */
/*      Operand in 16-bit integer format                             */
/*                                                                   */
/*      A program check may be generated if the logical address      */
/*      causes an addressing, translation, or fetch protection       */
/*      exception, and in this case the function does not return.    */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC U16 ARCH_DEP(vfetch2) (VADR addr, int arn, REGS *regs)
{
BYTE  *mn;
BYTE   hw[2];

    mn = MADDR (addr, arn, regs, ACCTYPE_READ, regs->psw.pkey);

    /* Quick out if boundary not crossed */
    if(likely(((VADR_L)addr & 0x7ff) < 0x7ff))
    {
        ITIMER_SYNC(addr,1-1,regs);
        return fetch_hw(mn);
    }

    hw[0] = mn[0];
    addr ++;
    addr &= ADDRESS_MAXWRAP(regs);
    mn = MADDR (addr, arn, regs, ACCTYPE_READ, regs->psw.pkey);
    hw[1] = mn[0];
    return fetch_hw(hw); 
}

/*-------------------------------------------------------------------*/
/* Fetch a four-byte integer operand from virtual storage            */
/*                                                                   */
/* Input:                                                            */
/*      addr    Logical address of leftmost byte of operand          */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/* Returns:                                                          */
/*      Operand in 32-bit integer format                             */
/*                                                                   */
/*      A program check may be generated if the logical address      */
/*      causes an addressing, translation, or fetch protection       */
/*      exception, and in this case the function does not return.    */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC U32 ARCH_DEP(vfetch4) (VADR addr, int arn, REGS *regs)
{
BYTE  *mn;
u_int  len;
U32   *p;
U32    fw[2];

    mn = MADDR (addr, arn, regs, ACCTYPE_READ, regs->psw.pkey);

    /* Quick out if boundary not crossed */
    if(likely(((VADR_L)addr & 0x7ff) <= 0x7fc))
    {
        ITIMER_SYNC(addr,4-1,regs);
        return fetch_fw(mn);
    }

    /* sloppy fetch */
#if defined(OPTION_STRICT_ALIGNMENT)
    memcpy(&fw[0], mn, 4);
#else
    fw[0] = *(U32 *)mn;
#endif
    len = 0x800 - ((VADR_L)addr & 0x7FF);
    addr += len;
    addr &= ADDRESS_MAXWRAP(regs);
    mn = MADDR (addr, arn, regs, ACCTYPE_READ, regs->psw.pkey);
    p = (U32 *)((BYTE *)&fw[0] + len);
#if defined(OPTION_STRICT_ALIGNMENT)
    memcpy(p, mn, 4);
#else
    *p = *(U32 *)mn;
#endif
    return CSWAP32(fw[0]); 
}

/*-------------------------------------------------------------------*/
/* Fetch an eight-byte integer operand from virtual storage          */
/*                                                                   */
/* Input:                                                            */
/*      addr    Logical address of leftmost byte of operand          */
/*      arn     Access register number                               */
/*      regs    CPU register context                                 */
/* Returns:                                                          */
/*      Operand in 64-bit integer format                             */
/*                                                                   */
/*      A program check may be generated if the logical address      */
/*      causes an addressing, translation, or fetch protection       */
/*      exception, and in this case the function does not return.    */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC U64 ARCH_DEP(vfetch8) (VADR addr, int arn, REGS *regs)
{
BYTE  *mn;
u_int  len;
U64   *p;
U64    dw[2];

    mn = MADDR (addr, arn, regs, ACCTYPE_READ, regs->psw.pkey);

    /* Quick out if boundary not crossed */
    if(likely(((VADR_L)addr & 0x7ff) <= 0x7f8))
    {
        ITIMER_SYNC(addr,8-1,regs);
#if defined(OPTION_SINGLE_CPU_DW) && defined(ASSIST_FETCH_DW)
        if (regs->cpubit == regs->sysblk->started_mask)
            return CSWAP64(*(U64 *)mn);
#endif
        return fetch_dw(mn);
    }

    /* sloppy fetch */
#if defined(OPTION_STRICT_ALIGNMENT)
    memcpy(&dw[0], mn, 8);
#else
    dw[0] = *(U64 *)mn;
#endif
    len = 0x800 - ((VADR_L)addr & 0x7FF);
    addr += len;
    addr &= ADDRESS_MAXWRAP(regs);
    mn = MADDR (addr, arn, regs, ACCTYPE_READ, regs->psw.pkey);
    p = (U64 *)((BYTE *)&dw[0] + len);
#if defined(OPTION_STRICT_ALIGNMENT)
    memcpy(p, mn, 8);
#else
    *p = *(U64 *)mn;
#endif
    return CSWAP64(dw[0]); 
}

#endif

#if !defined(OPTION_NO_INLINE_IFETCH) || defined(_VSTORE_C)
/*-------------------------------------------------------------------*/
/* Fetch instruction from halfword-aligned virtual storage location  */
/*                                                                   */
/* Input:                                                            */
/*      regs    Pointer to the CPU register context                  */
/*      exec    If 1 then called by EXecute otherwise called by      */
/*              INSTRUCTION_FETCH                                    */
/*                                                                   */
/* If called by INSTRUCTION_FETCH then                               */
/*      addr    regs->psw.IA                                         */
/*      dest    regs->inst                                           */
/*                                                                   */
/* If called by EXecute then                                         */
/*      addr    regs->ET                                             */
/*      dest    regs->exinst                                         */
/*                                                                   */
/* Output:                                                           */
/*      If successful, a pointer is returned to the instruction. If  */
/*      the instruction crossed a page boundary then the instruction */
/*      is copied either to regs->inst or regs->exinst (depending on */
/*      the exec flag).  Otherwise the pointer points into mainstor. */
/*                                                                   */
/*      If the exec flag is 0 and tracing or PER is not active then  */
/*      the AIA is updated.  This forces interrupts to be checked    */
/*      instfetch to be call for each instruction.  Note that        */
/*      process_trace() is called from here if tracing is active.    */
/*                                                                   */
/*      A program check may be generated if the instruction address  */
/*      is odd, or causes an addressing or translation exception,    */
/*      and in this case the function does not return.  In the       */
/*      latter case, regs->instinvalid is 1 which indicates to       */
/*      program_interrupt that the exception occurred during         */
/*      instruction fetch.                                           */
/*                                                                   */
/*      Because this function is inlined and `exec' is a constant    */
/*      (either 0 or 1) the references to exec are optimized out by  */
/*      the compiler.                                                */
/*-------------------------------------------------------------------*/
_VFETCH_C_STATIC BYTE * ARCH_DEP(instfetch) (REGS *regs, int exec)
{
VADR    addr;                           /* Instruction address       */
BYTE   *ia;                             /* Instruction pointer       */
BYTE   *dest;                           /* Copied instruction        */
int     pagesz;                         /* Effective page size       */
int     offset;                         /* Address offset into page  */
int     len;                            /* Length for page crossing  */

    SET_BEAR_REG(regs, regs->bear_ip);

    addr = exec ? regs->ET
         : likely(regs->aie == NULL) ? regs->psw.IA : PSW_IA(regs,0);

    offset = (int)(addr & PAGEFRAME_BYTEMASK);

    /* Program check if instruction address is odd */
    if ( unlikely(offset & 0x01) )
        ARCH_DEP(program_interrupt)(regs, PGM_SPECIFICATION_EXCEPTION);

    pagesz = unlikely(addr < 0x800) ? 0x800 : PAGEFRAME_PAGESIZE;

#if defined(FEATURE_PER)
    /* Save the address address used to fetch the instruction */
    if( EN_IC_PER(regs) )
    {
#if defined(FEATURE_PER2)
        regs->perc = 0x40    /* ATMID-validity */
                   | (regs->psw.amode64 << 7)
                   | (regs->psw.amode << 5)
                   | (!REAL_MODE(&regs->psw) ? 0x10 : 0)
                   | (SPACE_BIT(&regs->psw) << 3)
                   | (AR_BIT(&regs->psw) << 2);
#else /*!defined(FEATURE_PER2)*/
        regs->perc = 0;
#endif /*!defined(FEATURE_PER2)*/

        if(!exec)
            regs->peradr = addr;

        /* Test for PER instruction-fetching event */
        if( EN_IC_PER_IF(regs)
          && PER_RANGE_CHECK(addr,regs->CR(10),regs->CR(11)) )
        {
            ON_IC_PER_IF(regs);
      #if defined(FEATURE_PER3)
            /* If CR9_IFNUL (PER instruction-fetching nullification) is
               set, take a program check immediately, without executing
               the instruction or updating the PSW instruction address */
            if ( EN_IC_PER_IFNUL(regs) )
            {
                ON_IC_PER_IFNUL(regs);
                regs->psw.IA = addr;
                regs->psw.zeroilc = 1;
                ARCH_DEP(program_interrupt) (regs, PGM_PER_EVENT);
            }
      #endif /*defined(FEATURE_PER3)*/
        }
        /* Quick exit if aia valid */
        if (!exec && !regs->tracing
         && regs->aie && regs->ip < regs->aip + pagesz - 5)
            return regs->ip;
    }
#endif /*defined(FEATURE_PER)*/

    if (!exec) regs->instinvalid = 1;

    /* Get instruction address */
    ia = MADDR (addr, USE_INST_SPACE, regs, ACCTYPE_INSTFETCH, regs->psw.pkey);

    /* If boundary is crossed then copy instruction to destination */
    if ( offset + ILC(ia[0]) > pagesz )
    {
        /* Note - dest is 8 bytes */
        dest = exec ? regs->exinst : regs->inst;
        memcpy (dest, ia, 4);
        len = pagesz - offset;
        offset = 0;
        addr = (addr + len) & ADDRESS_MAXWRAP(regs);
        ia = MADDR(addr, USE_INST_SPACE, regs, ACCTYPE_INSTFETCH, regs->psw.pkey);
        if (!exec) regs->ip = ia - len;
        memcpy(dest + len, ia, 4);
    }
    else
    {
        dest = ia;
        if (!exec) regs->ip = ia;
    }

    if (!exec)
    {
        regs->instinvalid = 0;

        /* Update the AIA */
        regs->AIV = addr & PAGEFRAME_PAGEMASK;
        regs->aip = ia - offset;
        regs->aim = (uintptr_t)regs->aip ^ (uintptr_t)regs->AIV;
        if (likely(!regs->tracing && !regs->permode))
            regs->aie = regs->aip + pagesz - 5;
        else
        {
            regs->aie = (BYTE *)1;
            if (regs->tracing)
                ARCH_DEP(process_trace)(regs);
        }
    }

    return dest;

} /* end function ARCH_DEP(instfetch) */
#endif

#if !defined(OPTION_NO_INLINE_VSTORE) || defined(_VSTORE_C)
/*-------------------------------------------------------------------*/
/* Move characters using specified keys and address spaces           */
/*                                                                   */
/* Input:                                                            */
/*      addr1   Effective address of first operand                   */
/*      arn1    Access register number for first operand,            */
/*              or USE_PRIMARY_SPACE or USE_SECONDARY_SPACE          */
/*      key1    Bits 0-3=first operand access key, 4-7=zeroes        */
/*      addr2   Effective address of second operand                  */
/*      arn2    Access register number for second operand,           */
/*              or USE_PRIMARY_SPACE or USE_SECONDARY_SPACE          */
/*      key2    Bits 0-3=second operand access key, 4-7=zeroes       */
/*      len     Operand length minus 1 (range 0-255)                 */
/*      regs    Pointer to the CPU register context                  */
/*                                                                   */
/*      This function implements the MVC, MVCP, MVCS, MVCK, MVCSK,   */
/*      and MVCDK instructions.  These instructions move up to 256   */
/*      characters using the address space and key specified by      */
/*      the caller for each operand.  Operands are moved byte by     */
/*      byte to ensure correct processing of overlapping operands.   */
/*                                                                   */
/*      The arn parameter for each operand may be an access          */
/*      register number, in which case the operand is in the         */
/*      primary, secondary, or home space, or in the space           */
/*      designated by the specified access register, according to    */
/*      the current PSW addressing mode.                             */
/*                                                                   */
/*      Alternatively the arn parameter may be one of the special    */
/*      values USE_PRIMARY_SPACE or USE_SECONDARY_SPACE in which     */
/*      case the operand is in the specified space regardless of     */
/*      the current PSW addressing mode.                             */
/*                                                                   */
/*      A program check may be generated if either logical address   */
/*      causes an addressing, protection, or translation exception,  */
/*      and in this case the function does not return.               */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC void ARCH_DEP(move_chars) (VADR addr1, int arn1,
       BYTE key1, VADR addr2, int arn2, BYTE key2, int len, REGS *regs)
{
BYTE   *dest1, *dest2;                  /* Destination addresses     */
BYTE   *source1, *source2;              /* Source addresses          */
BYTE   *sk1, *sk2;                      /* Storage key addresses     */
int     len2, len3;                     /* Lengths to copy           */

    ITIMER_SYNC(addr2,len,regs);

    /* Quick out if copying just 1 byte */
    if (unlikely(len == 0))
    {
        source1 = MADDR (addr2, arn2, regs, ACCTYPE_READ, key2);
        dest1 = MADDR (addr1, arn1, regs, ACCTYPE_WRITE, key1);
        *dest1 = *source1;
        ITIMER_UPDATE(addr1,len,regs);
        return;
    }

    /* Translate addresses of leftmost operand bytes */
    source1 = MADDR (addr2, arn2, regs, ACCTYPE_READ, key2);
    dest1 = MADDR (addr1, arn1, regs, ACCTYPE_WRITE, key1);

    /* There are several scenarios (in optimal order):
     * (1) dest boundary and source boundary not crossed
     * (2) dest boundary not crossed and source boundary crossed
     * (3) dest boundary crossed and source boundary not crossed
     * (4) dest boundary and source boundary are crossed
     *     (a) dest and source boundary cross at the same time
     *     (b) dest boundary crossed first
     *     (c) source boundary crossed first
     */

    if ( NOCROSS2K(addr1,len) )
    {
        if ( NOCROSS2K(addr2,len) )
        {
            /* (1) - No boundaries are crossed */
            concpy (dest1, source1, len + 1);
        }
        else
        {
            /* (2) - Second operand crosses a boundary */
            len2 = 0x800 - (addr2 & 0x7FF);
            source2 = MADDR ((addr2 + len2) & ADDRESS_MAXWRAP(regs),
                              arn2, regs, ACCTYPE_READ, key2);
            concpy (dest1, source1, len2);
            concpy (dest1 + len2, source2, len - len2 + 1);
        }
    }
    else
    {
        dest1 = MADDR (addr1, arn1, regs, ACCTYPE_WRITE_SKP, key1);
        sk1 = regs->dat.storkey;
        source1 = MADDR (addr2, arn2, regs, ACCTYPE_READ, key2);

        /* First operand crosses a boundary */
        len2 = 0x800 - (addr1 & 0x7FF);
        dest2 = MADDR ((addr1 + len2) & ADDRESS_MAXWRAP(regs),
                       arn1, regs, ACCTYPE_WRITE_SKP, key1);
        sk2 = regs->dat.storkey;

        if ( NOCROSS2K(addr2,len) )
        {
             /* (3) - First operand crosses a boundary */
             concpy (dest1, source1, len2);
             concpy (dest2, source1 + len2, len - len2 + 1);
        }
        else
        {
            /* (4) - Both operands cross a boundary */
            len3 = 0x800 - (addr2 & 0x7FF);
            source2 = MADDR ((addr2 + len3) & ADDRESS_MAXWRAP(regs),
                             arn2, regs, ACCTYPE_READ, key2);
            if (len2 == len3)
            {
                /* (4a) - Both operands cross at the same time */
                concpy (dest1, source1, len2);
                concpy (dest2, source2, len - len2 + 1);
            }
            else if (len2 < len3)
            {
                /* (4b) - First operand crosses first */
                concpy (dest1, source1, len2);
                concpy (dest2, source1 + len2, len3 - len2);
                concpy (dest2 + len3 - len2, source2, len - len3 + 1);
            }
            else
            {
                /* (4c) - Second operand crosses first */
                concpy (dest1, source1, len3);
                concpy (dest1 + len3, source2, len2 - len3);
                concpy (dest2, source2 + len2 - len3, len - len2 + 1);
            }
        }
    }
    ITIMER_UPDATE(addr1,len,regs);

} /* end function ARCH_DEP(move_chars) */


/*-------------------------------------------------------------------*/
/* Validate operand for addressing, protection, translation          */
/*                                                                   */
/* Input:                                                            */
/*      addr    Effective address of operand                         */
/*      arn     Access register number                               */
/*      len     Operand length minus 1 (range 0-255)                 */
/*      acctype Type of access requested: READ or WRITE              */
/*      regs    Pointer to the CPU register context                  */
/*                                                                   */
/*      The purpose of this function is to allow an instruction      */
/*      operand to be validated for addressing, protection, and      */
/*      translation exceptions, thus allowing the instruction to     */
/*      be nullified or suppressed before any updates occur.         */
/*                                                                   */
/*      A program check is generated if the operand causes an        */
/*      addressing, protection, or translation exception, and        */
/*      in this case the function does not return.                   */
/*-------------------------------------------------------------------*/
_VSTORE_C_STATIC void ARCH_DEP(validate_operand) (VADR addr, int arn,
                                      int len, int acctype, REGS *regs)
{
    /* Translate address of leftmost operand byte */
    MADDR (addr, arn, regs, acctype, regs->psw.pkey);

    /* Translate next page if boundary crossed */
    if ( CROSS2K(addr,len) )
    {
        MADDR ((addr + len) & ADDRESS_MAXWRAP(regs),
               arn, regs, acctype, regs->psw.pkey);
    }
#ifdef FEATURE_INTERVAL_TIMER
    else
        ITIMER_SYNC(addr,len,regs);
#endif /*FEATURE_INTERVAL_TIMER*/
} /* end function ARCH_DEP(validate_operand) */

#endif /*!defined(OPTION_NO_INLINE_VSTORE) || defined(_VSTORE_C)*/
