/* VSTORE.H     (c) Copyright Roger Bowler, 1999-2004                */
/*              ESA/390 Dynamic Address Translation                  */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2004      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2004      */

/*-------------------------------------------------------------------*/
/* This module implements the DAT, ALET, and ASN translation         */
/* functions of the ESA/390 architecture, described in the manual    */
/* SA22-7201-04 ESA/390 Principles of Operation.  The numbers in     */
/* square brackets in the comments refer to sections in the manual.  */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      S/370 DAT support by Jay Maynard (as described in            */
/*      GA22-7000 System/370 Principles of Operation)                */
/*      Clear remainder of ASTE when ASF=0 - Jan Jaeger              */
/*      S/370 DAT support when running under SIE - Jan Jaeger        */
/*-------------------------------------------------------------------*/

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
RADR    abs, abs2;                      /* Absolute addresses        */
BYTE    len2;                           /* Length to end of page     */

    abs = LOGICAL_TO_ABS (addr, arn, regs, ACCTYPE_WRITE, regs->psw.pkey);
    if ( likely((addr & 0x7FF) <= 0x7FF - len) )
    {
        MEMCPY (regs->mainstor + abs, src, len + 1);
    }
    else
    {
        len2 = 0x800 - (addr & 0x7FF);
        abs2 = LOGICAL_TO_ABS((addr + len2) & ADDRESS_MAXWRAP(regs),
                             arn, regs, ACCTYPE_WRITE, regs->psw.pkey);
        MEMCPY (regs->mainstor + abs, src, len2);
        MEMCPY (regs->mainstor + abs2, src + len2, len + 1 - len2);
        STORAGE_KEY(abs, regs) |= (STORKEY_REF | STORKEY_CHANGE);
        STORAGE_KEY(abs2, regs) |= (STORKEY_REF | STORKEY_CHANGE);
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
    addr = LOGICAL_TO_ABS (addr, arn, regs, ACCTYPE_WRITE,
                                regs->psw.pkey);
    regs->mainstor[addr] = value;
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
_VSTORE_C_STATIC void ARCH_DEP(vstore2_full) (U16 value, VADR addr,
                                              int arn, REGS *regs)
{
RADR    abs, abs2;                      /* Absolute addresses        */

    abs = LOGICAL_TO_ABS (addr, arn, regs, ACCTYPE_WRITE, regs->psw.pkey);

    /* Check if store crosses boundary or not */
    if ((addr & 0x7FF) < 0x7FF)
    {
        STORE_HW (regs->mainstor + abs, value);
    }
    else
    {
        abs2 = LOGICAL_TO_ABS ((addr + 1) & ADDRESS_MAXWRAP(regs),
                       arn, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);
        regs->mainstor[abs] = value >> 8;
        regs->mainstor[abs2] = value & 0xFF;
        STORAGE_KEY(abs, regs) |= (STORKEY_REF | STORKEY_CHANGE);
        STORAGE_KEY(abs2, regs) |= (STORKEY_REF | STORKEY_CHANGE);
    }

} /* end function ARCH_DEP(vstore2) */

/* vstore2 accelerator - Simple case only (better inline candidate) */
_VSTORE_C_STATIC void ARCH_DEP(vstore2) (U16 value, VADR addr, int arn,
                                                            REGS *regs)
{
    /* Most common case : Aligned & not crossing page boundary */
    if(likely(!(addr & 0x01)) || ((addr & 0x7ff) <= 0x7fe))
    {
        register RADR abs;
        abs=LOGICAL_TO_ABS(addr,arn,regs,ACCTYPE_WRITE,regs->psw.pkey);
        STORE_HW(regs->mainstor + abs, value);
        return;
    }
    ARCH_DEP(vstore2_full)(value,addr,arn,regs);
}

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
_VSTORE_C_STATIC void ARCH_DEP(vstore4_full) (U32 value, VADR addr,
                                              int arn, REGS *regs)
{
RADR    abs, abs2;                      /* Absolute addresses        */
int     len;                            /* Length to end of page     */
BYTE    temp[4];                        /* Copied value              */ 

    abs = LOGICAL_TO_ABS (addr, arn, regs, ACCTYPE_WRITE, regs->psw.pkey);

    /* Check if store crosses boundary or not */
    if ((addr & 0x7FF) <= 0x7FC)
    {
        STORE_FW(regs->mainstor + abs, value);
    }
    else
    {
        len = 0x800 - (addr & 0x7FF);
        abs2 = LOGICAL_TO_ABS ((addr + len) & ADDRESS_MAXWRAP(regs),
                        arn, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);
        STORE_FW(temp, value);
        switch (len) {
        case 1: memcpy (regs->mainstor + abs,  temp,  1);
                memcpy (regs->mainstor + abs2, temp + 1, 3);
                break;
        case 2: memcpy (regs->mainstor + abs,  temp,  2);
                memcpy (regs->mainstor + abs2, temp + 2, 2);
                break;
        case 3: memcpy (regs->mainstor + abs,  temp,  3);
                memcpy (regs->mainstor + abs2, temp + 3, 1);
                break;
        }
        STORAGE_KEY(abs, regs) |= (STORKEY_REF | STORKEY_CHANGE);
        STORAGE_KEY(abs2, regs) |= (STORKEY_REF | STORKEY_CHANGE);
    }

} /* end function ARCH_DEP(vstore4_full) */

/* vstore4 accelerator - Simple case only (better inline candidate) */
_VSTORE_C_STATIC void ARCH_DEP(vstore4) (U32 value, VADR addr, int arn,
                                                            REGS *regs)
{
    /* Most common case : Aligned & not crossing page boundary */
    if(likely(!(addr & 0x03)) || ((addr & 0x7ff) <= 0x7fc))
    {
        register RADR abs;
        abs=LOGICAL_TO_ABS(addr,arn,regs,ACCTYPE_WRITE,regs->psw.pkey);
        STORE_FW(regs->mainstor + abs, value);
        return;
    }
    ARCH_DEP(vstore4_full)(value,addr,arn,regs);
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
_VSTORE_C_STATIC void ARCH_DEP(vstore8_full) (U64 value, VADR addr, int arn,
                                                            REGS *regs)
{
RADR    abs, abs2;                      /* Absolute addresses        */
int     len;                            /* Length to end of page     */
BYTE    temp[8];                        /* Copied value              */ 

    abs = LOGICAL_TO_ABS (addr, arn, regs, ACCTYPE_WRITE, regs->psw.pkey);

    /* Check if store crosses boundary or not */
    if ((addr & 0x7FF) <= 0x7F8)
    {
        STORE_DW(regs->mainstor + abs, value);
    }
    else
    {
        len = 0x800 - (addr & 0x7FF);
        abs2 = LOGICAL_TO_ABS ((addr + len) & ADDRESS_MAXWRAP(regs),
                        arn, regs, ACCTYPE_WRITE_SKP, regs->psw.pkey);
        STORE_DW(temp, value);
        switch (len) {
        case 1: memcpy (regs->mainstor + abs,  temp,  1);
                memcpy (regs->mainstor + abs2, temp + 1, 7);
                break;
        case 2: memcpy (regs->mainstor + abs,  temp,  2);
                memcpy (regs->mainstor + abs2, temp + 2, 6);
                break;
        case 3: memcpy (regs->mainstor + abs,  temp,  3);
                memcpy (regs->mainstor + abs2, temp + 3, 5);
                break;
        case 4: memcpy (regs->mainstor + abs,  temp,  4);
                memcpy (regs->mainstor + abs2, temp + 4, 4);
                break;
        case 5: memcpy (regs->mainstor + abs,  temp,  5);
                memcpy (regs->mainstor + abs2, temp + 5, 3);
                break;
        case 6: memcpy (regs->mainstor + abs,  temp,  6);
                memcpy (regs->mainstor + abs2, temp + 6, 2);
                break;
        case 7: memcpy (regs->mainstor + abs,  temp,  7);
                memcpy (regs->mainstor + abs2, temp + 7, 1);
                break;
        }
        STORAGE_KEY(abs, regs) |= (STORKEY_REF | STORKEY_CHANGE);
        STORAGE_KEY(abs2, regs) |= (STORKEY_REF | STORKEY_CHANGE);
    }

} /* end function ARCH_DEP(vstore8) */
_VSTORE_C_STATIC void ARCH_DEP(vstore8) (U64 value, VADR addr, int arn,
                                                            REGS *regs)
{
    /* Most common case : Aligned & not crossing page boundary */
    if(likely(!(addr & 0x07)) || ((addr & 0x7ff) <= 0x7f8))
    {
        register RADR abs;
        abs=LOGICAL_TO_ABS(addr,arn,regs,ACCTYPE_WRITE,regs->psw.pkey);
        STORE_DW(regs->mainstor + abs, value);
        return;
    }
    ARCH_DEP(vstore8_full)(value,addr,arn,regs);
}

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
BYTE   *main;                           /* Main storage addresses    */
int     len2;                           /* Length to copy on page    */

    main = LOGICAL_TO_ABS(addr,arn,regs,ACCTYPE_READ,regs->psw.pkey)
         + regs->mainstor;

    if ( likely((addr & 0x7FF) <= 0x7FF - len) )
    {
#ifdef FEATURE_INTERVAL_TIMER
        if (unlikely(addr == 80))
        {
            obtain_lock( &sysblk.todlock );
            update_TOD_clock ();
        }
#endif /*FEATURE_INTERVAL_TIMER*/

        MEMCPY (dest, main, len + 1);

#ifdef FEATURE_INTERVAL_TIMER
        if (unlikely(addr == 80))
            release_lock( &sysblk.todlock );
#endif /*FEATURE_INTERVAL_TIMER*/
    }
    else
    {
        /* Possibly crossing page or FPO boundary */
        len2 = 0x800 - (addr & 0x7FF);
        MEMCPY (dest, main, len2);
        main = LOGICAL_TO_ABS((addr + len2) & ADDRESS_MAXWRAP(regs),
                               arn, regs, ACCTYPE_READ, regs->psw.pkey)
             + regs->mainstor;
        MEMCPY (dest + len2, main, len + 1 - len2);
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
    addr = LOGICAL_TO_ABS (addr, arn, regs, ACCTYPE_READ,
                                regs->psw.pkey);
    return regs->mainstor[addr];
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
_VSTORE_C_STATIC U16 ARCH_DEP(vfetch2_full) (VADR addr, int arn,
                                             REGS *regs)
{
RADR    abs, abs2;                      /* Absolute addresses        */

    /* Get absolute address of first byte of operand */
    abs = LOGICAL_TO_ABS (addr, arn, regs, ACCTYPE_READ, regs->psw.pkey);

    /* Fetch 2 bytes when operand does not cross a boundary */
    if( (abs & 0x7FF) < 0x7FF)
    {
        return fetch_hw (regs->mainstor + abs);
    }

    abs2 = LOGICAL_TO_ABS ((addr + 1) & ADDRESS_MAXWRAP(regs),
                             arn, regs, ACCTYPE_READ, regs->psw.pkey);
    return (regs->mainstor[abs] << 8) | regs->mainstor[abs2];

} /* end function ARCH_DEP(vfetch2) */

_VSTORE_C_STATIC U16 ARCH_DEP(vfetch2) (VADR addr, int arn, REGS *regs)
{
    if(likely(!(addr & 0x01)) || ((addr & 0x7ff) !=0x7ff ))
    {
        register RADR abs;
        abs=LOGICAL_TO_ABS(addr,arn,regs,ACCTYPE_READ,regs->psw.pkey);
        return(fetch_hw(regs->mainstor + abs));
    }
    return(ARCH_DEP(vfetch2_full)(addr,arn,regs));
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
_VSTORE_C_STATIC U32 ARCH_DEP(vfetch4_full) (VADR addr, int arn,
                                             REGS *regs)
{
RADR    abs, abs2;                      /* Absolute addresses        */
int     len;                            /* Length to end of page     */
BYTE    temp[4];                        /* Copy destination          */

    /* Get absolute address of first byte of operand */
    abs = LOGICAL_TO_ABS (addr, arn, regs, ACCTYPE_READ, regs->psw.pkey);

    /* Fetch 4 bytes when operand does not cross a boundary */
    if ((addr & 0x7FF) <= 0x7FC)
    {
#ifdef FEATURE_INTERVAL_TIMER
        if (unlikely(addr == 80))
        {
            obtain_lock( &sysblk.todlock );
            update_TOD_clock ();
            release_lock( &sysblk.todlock );
          }
#endif /*FEATURE_INTERVAL_TIMER*/
        return fetch_fw(regs->mainstor + abs);
    }

    len = 0x800 - (addr & 0x7FF);
    abs2 = LOGICAL_TO_ABS ((addr + len) & ADDRESS_MAXWRAP(regs),
                           arn, regs, ACCTYPE_READ, regs->psw.pkey);
    switch (len) {
    case 1: memcpy (temp,     regs->mainstor + abs,  1);
            memcpy (temp + 1, regs->mainstor + abs2, 3);
            break;
    case 2: memcpy (temp,     regs->mainstor + abs,  2);
            memcpy (temp + 2, regs->mainstor + abs2, 2);
            break;
    case 3: memcpy (temp,     regs->mainstor + abs,  3);
            memcpy (temp + 3, regs->mainstor + abs2, 1);
            break;
    }
    return fetch_fw(temp);

} /* end function ARCH_DEP(vfetch4_full) */

_VSTORE_C_STATIC U32 ARCH_DEP(vfetch4) (VADR addr, int arn, REGS *regs)
{
    if ( (likely(!(addr & 0x03)) || ((addr & 0x7ff) <= 0x7fc ))
#if defined(FEATURE_INTERVAL_TIMER)
     && addr != 80
#endif
       )
    {
        register RADR abs;
        abs=LOGICAL_TO_ABS(addr,arn,regs,ACCTYPE_READ,regs->psw.pkey);
        return fetch_fw(regs->mainstor + abs);
    }
    return(ARCH_DEP(vfetch4_full)(addr,arn,regs));
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
_VSTORE_C_STATIC U64 ARCH_DEP(vfetch8_full) (VADR addr, int arn,
                                             REGS *regs)
{
RADR    abs, abs2;                      /* Absolute addresses        */
int     len;                            /* Length to end of page     */
BYTE    temp[8];                        /* Copy destination          */

    /* Get absolute address of first byte of operand */
    abs = LOGICAL_TO_ABS (addr, arn, regs, ACCTYPE_READ, regs->psw.pkey);

    /* Fetch 4 bytes when operand does not cross a boundary */
    if ((addr & 0x7FF) <= 0x7F8)
    {
#ifdef FEATURE_INTERVAL_TIMER
        if (unlikely(addr == 80))
        {
            obtain_lock( &sysblk.todlock );
            update_TOD_clock ();
            release_lock( &sysblk.todlock );
          }
#endif /*FEATURE_INTERVAL_TIMER*/
        return fetch_dw(regs->mainstor + abs);
    }

    len = 0x800 - (addr & 0x7FF);
    abs2 = LOGICAL_TO_ABS ((addr + len) & ADDRESS_MAXWRAP(regs),
                           arn, regs, ACCTYPE_READ, regs->psw.pkey);
    switch (len) {
    case 1: memcpy (temp,     regs->mainstor + abs,  1);
            memcpy (temp + 1, regs->mainstor + abs2, 7);
            break;
    case 2: memcpy (temp,     regs->mainstor + abs,  2);
            memcpy (temp + 2, regs->mainstor + abs2, 6);
            break;
    case 3: memcpy (temp,     regs->mainstor + abs,  3);
            memcpy (temp + 3, regs->mainstor + abs2, 5);
            break;
    case 4: memcpy (temp,     regs->mainstor + abs,  4);
            memcpy (temp + 4, regs->mainstor + abs2, 4);
            break;
    case 5: memcpy (temp,     regs->mainstor + abs,  5);
            memcpy (temp + 5, regs->mainstor + abs2, 3);
            break;
    case 6: memcpy (temp,     regs->mainstor + abs,  6);
            memcpy (temp + 6, regs->mainstor + abs2, 2);
            break;
    case 7: memcpy (temp,     regs->mainstor + abs,  7);
            memcpy (temp + 7, regs->mainstor + abs2, 1);
            break;
    }
    return fetch_dw(temp);

} /* end function ARCH_DEP(vfetch8) */

_VSTORE_C_STATIC U64 ARCH_DEP(vfetch8) (VADR addr, int arn, REGS *regs)
{
    if(likely(!(addr & 0x07)) || ((addr & 0x7ff) <= 0x7f8 ))
    {
        register RADR abs;
        abs = LOGICAL_TO_ABS (addr, arn, regs, ACCTYPE_READ, regs->psw.pkey);
        return fetch_dw(regs->mainstor + abs);
    }
    return ARCH_DEP(vfetch8_full)(addr,arn,regs);
}
#endif

#if !defined(OPTION_NO_INLINE_IFETCH) || defined(_VSTORE_C)
/*-------------------------------------------------------------------*/
/* Fetch instruction from halfword-aligned virtual storage location  */
/*                                                                   */
/* Input:                                                            */
/*      dest    Pointer to 6-byte area to receive instruction bytes  */
/*      addr    Logical address of leftmost instruction halfword     */
/*      regs    Pointer to the CPU register context                  */
/*                                                                   */
/* Output:                                                           */
/*      If successful, from one to three instruction halfwords will  */
/*      be fetched from main storage and stored into the 6-byte area */
/*      pointed to by dest.                                          */
/*                                                                   */
/*      A program check may be generated if the instruction address  */
/*      is odd, or causes an addressing or translation exception,    */
/*      and in this case the function does not return.               */
/*-------------------------------------------------------------------*/
_VFETCH_C_STATIC BYTE * ARCH_DEP(instfetch) (BYTE *dest, VADR addr,
                                                            REGS *regs)
{
RADR    abs;                            /* Absolute storage address  */
BYTE   *ia;                             /* Instruction address       */
int     ilc, len = 0;                   /* Lengths for page crossing */
VADR    mask;                           /* Mask for page crossing    */

    /* Program check if instruction address is odd */
    if ( unlikely(addr & 0x01) )
        ARCH_DEP(program_interrupt) (regs, PGM_SPECIFICATION_EXCEPTION);

#if defined(FEATURE_PER)
    /* Save the address address used to fetch the instruction */
    if( unlikely(EN_IC_PER(regs)) )
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

        /* For EXecute instvalid will be true */
        if(!regs->instvalid)
            regs->peradr = addr;

        if( EN_IC_PER_IF(regs)
          && PER_RANGE_CHECK(addr,regs->CR(10),regs->CR(11)) )
            ON_IC_PER_IF(regs);
    }
#endif /*defined(FEATURE_PER)*/

    /* Get instruction address */
    abs = LOGICAL_TO_ABS (addr, 0, regs, ACCTYPE_INSTFETCH, regs->psw.pkey);
    ia = abs + regs->mainstor;

    /* If page is crossed then copy instruction to the destination */
    ilc = ILC(ia[0]);
    mask = addr < PSA_SIZE ? 0x7FF : PAGEFRAME_BYTEMASK;
    if ( unlikely((addr & mask) > mask + 1 - ilc) )
    {
     // NOTE: `dest' must be at least 8 bytes
        len = (mask + 1) - (addr & mask);
        memcpy (dest, ia, 4);
        addr += len;
        addr &= ADDRESS_MAXWRAP(regs);
        abs = LOGICAL_TO_ABS (addr, 0, regs, ACCTYPE_INSTFETCH, regs->psw.pkey);
        ia = abs + regs->mainstor;
        memcpy(dest + len, ia, 4);
    }

    /* Update the AIA */
    if (!regs->instvalid
#if defined(FEATURE_PER)
      && !EN_IC_PER(regs)
#endif /*defined(FEATURE_PER)*/
       )
    {
        regs->VI = addr & PAGEFRAME_PAGEMASK;
        regs->VIE = (addr | mask) - 5;
        abs &= PAGEFRAME_PAGEMASK;
        regs->AI = abs;
        regs->mi = NEW_IADDR(regs, addr, abs + regs->mainstor);
    }

    regs->instvalid = 1;
    return len ? dest : ia;

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
RADR    abs1, abs2;                     /* Destination abs addresses */
BYTE   *source1, *source2;              /* Source addresses          */
int     len2, len3;                     /* Lengths to copy           */
int     i;                              /* Loop counter              */

    /* Translate addresses of leftmost operand bytes */
    abs1 = LOGICAL_TO_ABS (addr1, arn1, regs, ACCTYPE_WRITE_SKP, key1);
    dest1 = abs1 + regs->mainstor;
    source1 = LOGICAL_TO_ABS (addr2, arn2, regs, ACCTYPE_READ, key2)
            + regs->mainstor;

    /* Quick out if copying just 1 byte */
    if (unlikely(len == 0))
    {
        *dest1 = *source1;
        STORAGE_KEY(abs1, regs) |= (STORKEY_REF | STORKEY_CHANGE);
        return;
    }

    /* There are several scenarios (in optimal order):
     * (1) dest boundary and source boundary not crossed
     *     (a) no operand overlap
     *     (b) operand overlap
     * (2) dest boundary not crossed and source boundary crossed
     * (3) dest boundary crossed and source boundary not crossed
     * (4) dest boundary and source boundary are crossed
     *     (a) dest and source boundary cross at the same time
     *     (b) dest boundary crossed first
     *     (c) source boundary crossed first
     * NOTE: for scenarios (2), (3) and (4) we don't check for overlap
     *     but just perform the copy we would for overlap.  Testing
     *     shows that less than 4% of the move_chars calls uses one of
     *     these scenarios.  Over 95% of the calls are scenario (1a).
     */

#ifdef FEATURE_INTERVAL_TIMER
    if (addr2 == 80)
    {
        obtain_lock (&sysblk.todlock);
        /* If a program check occurs during address translation
         * (when a boundary is crossed), then program_interrupt
         * must release the todlock for us.
         */
        regs->todlock = 1;
        update_TOD_clock ();
    }
#endif /* FEATURE_INTERVAL_TIMER */

    if ( (addr1 & 0x7FF) <= 0x7FF - len )
    {
        if ( (addr2 & 0x7FF) <= 0x7FF - len )
        {
             /* (1) - No boundaries are crossed */
             if ( (dest1 < source1 && dest1 + len < source1)
               || (source1 < dest1 && source1 + len < dest1) )
             {
                 /* (1a) - No overlap */
		 /* Single out memcpys that can be translated */
		 /* into 1 or 2 GCC insns                     */
		 switch(len+1)
		 {
			case 2:	/* MOVHI (2) */
				 memcpy(dest1,source1,2);
				 break;
			case 3:	/* MOVHI + MOVQI (2+1) */
				 memcpy(dest1,source1,3);
				 break;
			case 4: /* MOVSI (4) */
				 memcpy(dest1,source1,4);
				 break;
			case 5: /* MOVSI +MOVQI (4+1) */
				 memcpy(dest1,source1,5);
				 break;
			case 6: /* MOVSI + MOVHI (4+2) */
				 memcpy(dest1,source1,6);
				 break;
		        /* 7 : Would be MOVSI+MOVHI+MOVQI (4+2+1) */
			case 8:	/* MOVDI 8) */
				 memcpy(dest1,source1,8);
				 break;
			case 9:	/* MOVDI+MOVQI (8+1) */
				 memcpy(dest1,source1,9);
				 break;
			case 10: /* MOVDI+MOVHI (8+2) */
				 memcpy(dest1,source1,10);
				 break;
		        /* 11 : Would be MOVDI+MOVHI+MOVQI (8+2+1) */
			case 12: /* MOVDI+MOVSI (8+4) */
				 memcpy(dest1,source1,12);
				 break;
		        /* 13, 14, 15 are 3 insns */
			case 16: /* MOVDI+MOVDI (8+8) */
				 memcpy(dest1,source1,16);
				 break;
			default:
                 		MEMCPY (dest1, source1, len + 1);
				break;
		 }
             }
             else
             {
                 /* (1b) - Overlap */
                 for ( i = 0; i <= len; i++) *dest1++ = *source1++;
             }
        }
        else
        {
             /* (2) - Second operand crosses a boundary */
             len2 = 0x800 - (addr2 & 0x7FF);
             source2 = LOGICAL_TO_ABS ((addr2 + len2) & ADDRESS_MAXWRAP(regs),
                                    arn2, regs, ACCTYPE_READ, key2)
                     + regs->mainstor;

             for ( i = 0; i < len2; i++) *dest1++ = *source1++;
             len2 = len - len2;
             for ( i = 0; i <= len2; i++) *dest1++ = *source2++;
        }
        STORAGE_KEY(abs1, regs) |= (STORKEY_REF | STORKEY_CHANGE);
    }
    else
    {
        /* First operand crosses a boundary */
        len2 = 0x800 - (addr1 & 0x7FF);
        abs2 = LOGICAL_TO_ABS ((addr1 + len2) & ADDRESS_MAXWRAP(regs),
                            arn1, regs, ACCTYPE_WRITE_SKP, key1);
        dest2 = abs2 + regs->mainstor;

        if ( (addr2 & 0x7FF) <= 0x7FF - len )
        {
             /* (3) - First operand crosses a boundary */
             for ( i = 0; i < len2; i++) *dest1++ = *source1++;
             len2 = len - len2;
             for ( i = 0; i <= len2; i++) *dest2++ = *source1++;
        }
        else
        {
            /* (4) - Both operands cross a boundary */
            len3 = 0x800 - (addr2 & 0x7FF);
            source2 = LOGICAL_TO_ABS ((addr2 + len3) & ADDRESS_MAXWRAP(regs),
                                    arn2, regs, ACCTYPE_READ, key2)
                    + regs->mainstor;
            if (len2 == len3)
            {
                /* (4a) - Both operands cross at the same time */
                for ( i = 0; i < len2; i++) *dest1++ = *source1++;
                len2 = len - len2;
                for ( i = 0; i <= len2; i++) *dest2++ = *source2++;
            }
            else if (len2 < len3)
            {
                /* (4b) - First operand crosses first */
                for ( i = 0; i < len2; i++) *dest1++ = *source1++;
                len2 = len3 - len2;
                for ( i = 0; i < len2; i++) *dest2++ = *source1++;
                len2 = len - len3;
                for ( i = 0; i <= len2; i++) *dest2++ = *source2++;
            }
            else
            {
                /* (4c) - Second operand crosses first */
                for ( i = 0; i < len3; i++) *dest1++ = *source1++;
                len3 = len2 - len3;
                for ( i = 0; i < len3; i++) *dest1++ = *source2++;
                len3 = len - len2;
                for ( i = 0; i <= len3; i++) *dest2++ = *source2++;
            }
        }
        STORAGE_KEY(abs1, regs) |= (STORKEY_REF | STORKEY_CHANGE);
        STORAGE_KEY(abs2, regs) |= (STORKEY_REF | STORKEY_CHANGE);
    }

#ifdef FEATURE_INTERVAL_TIMER
    if (addr2 == 80)
    {
        regs->todlock = 0;
        release_lock (&sysblk.todlock);
    }
#endif /* FEATURE_INTERVAL_TIMER */

} /* end function ARCH_DEP(move_chars) */


/*-------------------------------------------------------------------*/
/* Validate operand                                                  */
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
    LOGICAL_TO_ABS (addr, arn, regs, acctype, regs->psw.pkey);

    /* Translate next page if boundary crossed */
    if ( (addr & 0x7FF) > 0x7FF - len)
    {
        LOGICAL_TO_ABS ((addr + len) & ADDRESS_MAXWRAP(regs),
                         arn, regs, acctype, regs->psw.pkey);
    }
} /* end function ARCH_DEP(validate_operand) */

#endif /*!defined(OPTION_NO_INLINE_VSTORE) || defined(_VSTORE_C)*/
