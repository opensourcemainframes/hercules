/* INLINE.H	(c) Copyright Jan Jaeger, 2000-2001		     */
/*		Inline function definitions			     */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2001      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2001      */

/* Storage protection override fix		 Jan Jaeger 31/08/00 */
/* ESAME low-address protection 	 v208d Roger Bowler 20/01/01 */
/* ESAME subspace replacement		 v208e Roger Bowler 27/01/01 */

// #define INLINE_STORE_FETCH_ADDR_CHECK

_DAT_C_STATIC U16 ARCH_DEP(translate_asn) (U16 asn, REGS *regs,
					       U32 *asteo, U32 aste[]);
_DAT_C_STATIC int ARCH_DEP(authorize_asn) (U16 ax, U32 aste[],
					      int atemask, REGS *regs);
_DAT_C_STATIC U16 ARCH_DEP(translate_alet) (U32 alet, U16 eax,
	   int acctype, REGS *regs, U32 *asteo, U32 aste[], int *prot);
_DAT_C_STATIC void ARCH_DEP(purge_alb) (REGS *regs);
_DAT_C_STATIC int ARCH_DEP(translate_addr) (VADR vaddr, int arn,
	   REGS *regs, int acctype, RADR *raddr, U16 *xcode, int *priv,
		                                int *prot, int *pstid);
_DAT_C_STATIC void ARCH_DEP(purge_tlb) (REGS *regs);
_DAT_C_STATIC void ARCH_DEP(invalidate_pte) (BYTE ibyte, int r1,
						   int r2, REGS *regs);
_DAT_C_STATIC RADR ARCH_DEP(logical_to_abs) (VADR addr, int arn,
				   REGS *regs, int acctype, BYTE akey);

#if defined(_FEATURE_SIE)
_DAT_C_STATIC RADR s390_logical_to_abs (U32 addr, int arn, REGS *regs,
					       int acctype, BYTE akey);
_DAT_C_STATIC int s390_translate_addr (U32 vaddr, int arn, REGS *regs,
		       int acctype, RADR *raddr, U16 *xcode, int *priv,
		                                int *prot, int *pstid);
#endif /*defined(_FEATURE_SIE)*/

#if defined(_FEATURE_ZSIE)
_DAT_C_STATIC RADR z900_logical_to_abs (U64 addr, int arn, REGS *regs,
					       int acctype, BYTE akey);
_DAT_C_STATIC int z900_translate_addr (U64 vaddr, int arn, REGS *regs,
		       int acctype, RADR *raddr, U16 *xcode, int *priv,
		                                int *prot, int *pstid);
#endif /*defined(_FEATURE_ZSIE)*/

_VSTORE_C_STATIC void ARCH_DEP(vstorec) (void *src, BYTE len,
				       VADR addr, int arn, REGS *regs);
_VSTORE_C_STATIC void ARCH_DEP(vstoreb) (BYTE value, VADR addr,
						  int arn, REGS *regs);
_VSTORE_C_STATIC void ARCH_DEP(vstore2) (U16 value, VADR addr, int arn,
							   REGS *regs);
_VSTORE_C_STATIC void ARCH_DEP(vstore4) (U32 value, VADR addr, int arn,
							   REGS *regs);
_VSTORE_C_STATIC void ARCH_DEP(vstore8) (U64 value, VADR addr, int arn,
							   REGS *regs);
_VSTORE_C_STATIC void ARCH_DEP(vfetchc) (void *dest, BYTE len,
				       VADR addr, int arn, REGS *regs);
_VSTORE_C_STATIC BYTE ARCH_DEP(vfetchb) (VADR addr, int arn,
							   REGS *regs);
_VSTORE_C_STATIC U16 ARCH_DEP(vfetch2) (VADR addr, int arn,
							   REGS *regs);
_VSTORE_C_STATIC U32 ARCH_DEP(vfetch4) (VADR addr, int arn,
							   REGS *regs);
_VSTORE_C_STATIC U64 ARCH_DEP(vfetch8) (VADR addr, int arn,
							   REGS *regs);
_VFETCH_C_STATIC void ARCH_DEP(instfetch) (BYTE *dest, VADR addr,
							   REGS *regs);
_VSTORE_C_STATIC void ARCH_DEP(move_chars) (VADR addr1, int arn1,
      BYTE key1, VADR addr2, int arn2, BYTE key2, int len, REGS *regs);
_VSTORE_C_STATIC void ARCH_DEP(validate_operand) (VADR addr, int arn,
				     int len, int acctype, REGS *regs);

#if defined(_FEATURE_SIE)
_VFETCH_C_STATIC void s370_instfetch (BYTE *dest, U32 addr, REGS *regs);
#endif /*defined(_FEATURE_SIE)*/

#if defined(_FEATURE_ZSIE)
_VFETCH_C_STATIC void s390_instfetch (BYTE *dest, U32 addr, REGS *regs);
#endif /*defined(_FEATURE_ZSIE)*/

#if !defined(_INLINE_H)

#define _INLINE_H


static inline int add_logical(U32 *result, U32 op1, U32 op2)
{
    *result = op1 + op2;

    return (*result == 0 ? 0 : 1) | (op1 > *result ? 2 : 0);
}
        

static inline int sub_logical(U32 *result, U32 op1, U32 op2)
{
    *result = op1 - op2;

    return (*result == 0 ? 0 : 1) | (op1 < *result ? 0 : 2);
}
        

static inline int add_signed(U32 *result, U32 op1, U32 op2)
{
    *result = (S32)op1 + (S32)op2;

    return (((S32)op1 < 0 && (S32)op2 < 0 && (S32)*result >= 0)
      || ((S32)op1 >= 0 && (S32)op2 >= 0 && (S32)*result < 0)) ? 3 :
                                              (S32)*result < 0 ? 1 :
                                              (S32)*result > 0 ? 2 : 0;
}
        

static inline int sub_signed(U32 *result, U32 op1, U32 op2)
{
    *result = (S32)op1 - (S32)op2;

    return (((S32)op1 < 0 && (S32)op2 >= 0 && (S32)*result >= 0)
      || ((S32)op1 >= 0 && (S32)op2 < 0 && (S32)*result < 0)) ? 3 :
                                             (S32)*result < 0 ? 1 :
                                             (S32)*result > 0 ? 2 : 0;
}


/*-------------------------------------------------------------------*/
/* Multiply two signed fullwords giving a signed doubleword result   */
/*-------------------------------------------------------------------*/
static inline void mul_signed ( U32 *resulthi, U32 *resultlo,
						     U32 op1, U32 op2 )
{
S64	r;

    r = (S64)(S32)op1 * (S32)op2;
    *resulthi = (U32)((U64)r >> 32);
    *resultlo = (U32)((U64)r & 0xFFFFFFFF);
} /* end function mul_signed */

/*-------------------------------------------------------------------*/
/* Divide a signed doubleword dividend by a signed fullword divisor  */
/* giving a signed fullword remainder and a signed fullword quotient.*/
/* Returns 0 if successful, 1 if divide overflow.		     */
/*-------------------------------------------------------------------*/
static inline int div_signed ( U32 *remainder, U32 *quotient,
			  U32 dividendhi, U32 dividendlo, U32 divisor )
{
U64	dividend;
S64	quot, rem;

    if (divisor == 0) return 1;
    dividend = (U64)dividendhi << 32 | dividendlo;
    quot = (S64)dividend / (S32)divisor;
    rem = (S64)dividend % (S32)divisor;
    if (quot < -2147483648LL || quot > 2147483647LL) return 1;
    *quotient = (U32)quot;
    *remainder = (U32)rem;
    return 0;
} /* end function div_signed */


#endif /*!defined(_INLINE_H)*/


/*-------------------------------------------------------------------*/
/* Test for fetch protected storage location.			     */
/*								     */
/* Input:							     */
/*	addr	Logical address of storage location		     */
/*	skey	Storage key with fetch, reference, and change bits   */
/*		and one low-order zero appended 		     */
/*	akey	Access key with 4 low-order zeroes appended	     */
/*	private 1=Location is in a private address space	     */
/*	regs	Pointer to the CPU register context		     */
/* Return value:						     */
/*	1=Fetch protected, 0=Not fetch protected		     */
/*-------------------------------------------------------------------*/
static inline int ARCH_DEP(is_fetch_protected) (VADR addr, BYTE skey,
				    BYTE akey, int private, REGS *regs)
{
    /* [3.4.1] Fetch is allowed if access key is zero, regardless
       of the storage key and fetch protection bit */
    if (akey == 0)
	return 0;

#ifdef FEATURE_FETCH_PROTECTION_OVERRIDE
    /* [3.4.1.2] Fetch protection override allows fetch from first
       2K of non-private address spaces if CR0 bit 6 is set */
    if (addr < 2048
	&& (regs->CR(0) & CR0_FETCH_OVRD)
	&& private == 0)
	return 0;
#endif /*FEATURE_FETCH_PROTECTION_OVERRIDE*/

#ifdef FEATURE_STORAGE_PROTECTION_OVERRIDE
    /* [3.4.1.1] Storage protection override allows access to
       locations with storage key 9, regardless of the access key,
       provided that CR0 bit 7 is set */
    if ((skey & STORKEY_KEY) == 0x90
	&& (regs->CR(0) & CR0_STORE_OVRD))
	return 0;
#endif /*FEATURE_STORAGE_PROTECTION_OVERRIDE*/

    /* [3.4.1] Fetch protection prohibits fetch if storage key fetch
       protect bit is on and access key does not match storage key */
    if ((skey & STORKEY_FETCH)
	&& akey != (skey & STORKEY_KEY))
	return 1;

    /* Return zero if location is not fetch protected */
    return 0;

} /* end function is_fetch_protected */

/*-------------------------------------------------------------------*/
/* Test for low-address protection.				     */
/*								     */
/* Input:							     */
/*	addr	Logical address of storage location		     */
/*	private 1=Location is in a private address space	     */
/*	regs	Pointer to the CPU register context		     */
/* Return value:						     */
/*	1=Low-address protected, 0=Not low-address protected	     */
/*-------------------------------------------------------------------*/
static inline int ARCH_DEP(is_low_address_protected) (VADR addr,
			int private, REGS *regs)
{
#if defined (FEATURE_ESAME)
    /* For ESAME, low-address protection applies to locations
       0-511 (0000-01FF) and 4096-4607 (1000-11FF) */
    if (addr & 0xFFFFFFFFFFFFEE00ULL)
#else /*!defined(FEATURE_ESAME)*/
    /* For S/370 and ESA/390, low-address protection applies
       to locations 0-511 only */
    if (addr > 511)
#endif /*!defined(FEATURE_ESAME)*/
	return 0;

    /* Low-address protection applies only if the low-address
       protection control bit in control register 0 is set */
    if ((regs->CR(0) & CR0_LOW_PROT) == 0)
	return 0;

#if defined(_FEATURE_SIE)
    /* Host low-address protection is not applied to guest
       references to guest storage */
    if (regs->sie_active)
	return 0;
#endif /*defined(_FEATURE_SIE)*/

    /* Low-address protection does not apply to private address
       spaces */
    if (private)
	return 0;

    /* Return one if location is low-address protected */
    return 1;

} /* end function is_low_address_protected */

/*-------------------------------------------------------------------*/
/* Test for store protected storage location.			     */
/*								     */
/* Input:							     */
/*	addr	Logical address of storage location		     */
/*	skey	Storage key with fetch, reference, and change bits   */
/*		and one low-order zero appended 		     */
/*	akey	Access key with 4 low-order zeroes appended	     */
/*	private 1=Location is in a private address space	     */
/*	protect 1=Access list protection or page protection applies  */
/*	regs	Pointer to the CPU register context		     */
/* Return value:						     */
/*	1=Store protected, 0=Not store protected		     */
/*-------------------------------------------------------------------*/
static inline int ARCH_DEP(is_store_protected) (VADR addr, BYTE skey,
		       BYTE akey, int private, int protect, REGS *regs)
{
    /* [3.4.4] Low-address protection prohibits stores into certain
       locations in the prefixed storage area of non-private address
       address spaces, if the low-address control bit in CR0 is set,
       regardless of the access key and storage key */
    if (ARCH_DEP(is_low_address_protected) (addr, private, regs))
	return 1;

    /* Access-list controlled protection prohibits all stores into
       the address space, and page protection prohibits all stores
       into the page, regardless of the access key and storage key */
    if (protect)
	return 1;

    /* [3.4.1] Store is allowed if access key is zero, regardless
       of the storage key */
    if (akey == 0)
	return 0;

#ifdef FEATURE_STORAGE_PROTECTION_OVERRIDE
    /* [3.4.1.1] Storage protection override allows access to
       locations with storage key 9, regardless of the access key,
       provided that CR0 bit 7 is set */
    if ((skey & STORKEY_KEY) == 0x90
	&& (regs->CR(0) & CR0_STORE_OVRD))
	return 0;
#endif /*FEATURE_STORAGE_PROTECTION_OVERRIDE*/

    /* [3.4.1] Store protection prohibits stores if the access
       key does not match the storage key */
    if (akey != (skey & STORKEY_KEY))
	return 1;

    /* Return zero if location is not store protected */
    return 0;

} /* end function is_store_protected */


/*-------------------------------------------------------------------*/
/* Fetch a doubleword from absolute storage.			     */
/* The caller is assumed to have already checked that the absolute   */
/* address is within the limit of main storage. 		     */
/* All bytes of the word are fetched concurrently as observed by     */
/* other CPUs.  The doubleword is first fetched as an integer, then  */
/* the bytes are reversed into host byte order if necessary.	     */
/*-------------------------------------------------------------------*/
static inline U64 ARCH_DEP(fetch_doubleword_absolute) (RADR addr,
							    REGS *regs)
{
#if defined(INLINE_STORE_FETCH_ADDR_CHECK)
    if(addr > regs->mainsize - 8)
	ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);
#endif /*defined(INLINE_STORE_FETCH_ADDR_CHECK)*/

    SIE_TRANSLATE(&addr, ACCTYPE_READ, regs);

    /* Set the main storage reference bit */
    STORAGE_KEY(addr) |= STORKEY_REF;

    /* Fetch the doubleword from absolute storage */
    return CSWAP64(*((U64*)(sysblk.mainstor + addr)));

} /* end function fetch_doubleword_absolute */


/*-------------------------------------------------------------------*/
/* Fetch a fullword from absolute storage.			     */
/* The caller is assumed to have already checked that the absolute   */
/* address is within the limit of main storage. 		     */
/* All bytes of the word are fetched concurrently as observed by     */
/* other CPUs.	The fullword is first fetched as an integer, then    */
/* the bytes are reversed into host byte order if necessary.	     */
/*-------------------------------------------------------------------*/
static inline U32 ARCH_DEP(fetch_fullword_absolute) (RADR addr,
							    REGS *regs)
{
#if defined(INLINE_STORE_FETCH_ADDR_CHECK)
    if(addr > regs->mainsize - 4)
	ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);
#endif /*defined(INLINE_STORE_FETCH_ADDR_CHECK)*/

    SIE_TRANSLATE(&addr, ACCTYPE_READ, regs);

    /* Set the main storage reference bit */
    STORAGE_KEY(addr) |= STORKEY_REF;

    /* Fetch the fullword from absolute storage */
    return CSWAP32(*((U32*)(sysblk.mainstor + addr)));
} /* end function fetch_fullword_absolute */


/*-------------------------------------------------------------------*/
/* Fetch a halfword from absolute storage.			     */
/* The caller is assumed to have already checked that the absolute   */
/* address is within the limit of main storage. 		     */
/* All bytes of the halfword are fetched concurrently as observed by */
/* other CPUs.	The halfword is first fetched as an integer, then    */
/* the bytes are reversed into host byte order if necessary.	     */
/*-------------------------------------------------------------------*/
static inline U16 ARCH_DEP(fetch_halfword_absolute) (RADR addr,
							    REGS *regs)
{
#if defined(INLINE_STORE_FETCH_ADDR_CHECK)
    if(addr > regs->mainsize - 2)
	ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);
#endif /*defined(INLINE_STORE_FETCH_ADDR_CHECK)*/

    SIE_TRANSLATE(&addr, ACCTYPE_READ, regs);

    /* Set the main storage reference bit */
    STORAGE_KEY(addr) |= STORKEY_REF;

    /* Fetch the fullword from absolute storage */
    return CSWAP16(*((U16*)(sysblk.mainstor + addr)));

} /* end function fetch_fullword_absolute */


/*-------------------------------------------------------------------*/
/* Store doubleword into absolute storage.			     */
/* All bytes of the word are stored concurrently as observed by      */
/* other CPUs.	The bytes of the word are reversed if necessary      */
/* and the word is then stored as an integer in absolute storage.    */
/*-------------------------------------------------------------------*/
static inline void ARCH_DEP(store_doubleword_absolute) (U64 value,
						  RADR addr, REGS *regs)
{
#if defined(INLINE_STORE_FETCH_ADDR_CHECK)
    if(addr > regs->mainsize - 8)
	ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);
#endif /*defined(INLINE_STORE_FETCH_ADDR_CHECK)*/

    SIE_TRANSLATE(&addr, ACCTYPE_WRITE, regs);

    /* Set the main storage reference and change bits */
    STORAGE_KEY(addr) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Store the doubleword into absolute storage */
    *((U64*)(sysblk.mainstor + addr)) = CSWAP64(value);

} /* end function store_doubleword_absolute */


/*-------------------------------------------------------------------*/
/* Store a fullword into absolute storage.			     */
/* All bytes of the word are stored concurrently as observed by      */
/* other CPUs.	The bytes of the word are reversed if necessary      */
/* and the word is then stored as an integer in absolute storage.    */
/*-------------------------------------------------------------------*/
static inline void ARCH_DEP(store_fullword_absolute) (U32 value,
						  RADR addr, REGS *regs)
{
#if defined(INLINE_STORE_FETCH_ADDR_CHECK)
    if(addr > regs->mainsize - 4)
	ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);
#endif /*defined(INLINE_STORE_FETCH_ADDR_CHECK)*/

    SIE_TRANSLATE(&addr, ACCTYPE_WRITE, regs);

    /* Set the main storage reference and change bits */
    STORAGE_KEY(addr) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Store the fullword into absolute storage */
    *((U32*)(sysblk.mainstor + addr)) = CSWAP32(value);

} /* end function store_fullword_absolute */


/*-------------------------------------------------------------------*/
/* Perform subspace replacement 				     */
/*								     */
/* Input:							     */
/*	std	Original segment table designation (STD) or ASCE     */
/*	asteo	ASTE origin obtained by ASN translation 	     */
/*	xcode	Pointer to field to receive exception code, or NULL  */
/*	regs	Pointer to the CPU register context		     */
/* Output:							     */
/*	xcode	Exception code or zero (if xcode is not NULL)	     */
/* Return value:						     */
/*	On successful completion, the exception code field (if not   */
/*	NULL) is set to zero, and the function return value is the   */
/*	STD resulting from subspace replacement, or is the original  */
/*	STD if subspace replacement is not applicable.		     */
/* Operation:							     */
/*	If the ASF control is enabled, and the STD or ASCE is a      */
/*	member of a subspace-group (bit 22 is one), and the	     */
/*	dispatchable unit is subspace active (DUCT word 1 bit 0 is   */
/*	one), and the ASTE obtained by ASN translation is the ASTE   */
/*	for the base space of the dispatchable unit, then the STD    */
/*	or ASCE is replaced (except for the event control bits) by   */
/*	the STD or ASCE from the ASTE for the subspace in which the  */
/*	dispatchable unit last had control; otherwise the STD or     */
/*	ASCE remains unchanged. 				     */
/* Error conditions:						     */
/*	If an ASTE validity exception or ASTE sequence exception     */
/*	occurs, and the xcode parameter is a non-NULL pointer,	     */
/*	then the exception code is returned in the xcode field	     */
/*	and the function return value is zero.			     */
/*	For all other error conditions a program check is generated  */
/*	and the function does not return.			     */
/*-------------------------------------------------------------------*/
static inline RADR ARCH_DEP(subspace_replace) (RADR std, U32 asteo,
						U16 *xcode, REGS *regs)
{
U32	ducto;				/* DUCT origin		     */
U32	duct0;				/* DUCT word 0		     */
U32	duct1;				/* DUCT word 1		     */
U32	duct3;				/* DUCT word 3		     */
U32	ssasteo;			/* Subspace ASTE origin      */
U32	ssaste[16];			/* Subspace ASTE	     */

    /* Clear the exception code field, if provided */
    if (xcode != NULL) *xcode = 0;

    /* Return the original STD unchanged if the address-space function
       control (CR0 bit 15) is zero, or if the subspace-group control
       (bit 22 of the STD) is zero */
    if (!ASF_ENABLED(regs)
	|| (std & SSGROUP_BIT) == 0)
	return std;

    /* Load the DUCT origin address */
    ducto = regs->CR(2) & CR2_DUCTO;
    ducto = APPLY_PREFIXING (ducto, regs->PX);

    /* Program check if DUCT origin address is invalid */
    if (ducto >= regs->mainsize)
	ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

    /* Fetch DUCT words 0, 1, and 3 from absolute storage
       (note: the DUCT cannot cross a page boundary) */
    duct0 = ARCH_DEP(fetch_fullword_absolute) (ducto, regs);
    duct1 = ARCH_DEP(fetch_fullword_absolute) (ducto+4, regs);
    duct3 = ARCH_DEP(fetch_fullword_absolute) (ducto+12, regs);

    /* Return the original STD unchanged if the dispatchable unit is
       not subspace active or if the ASTE obtained by ASN translation
       is not the same as the base ASTE for the dispatchable unit */
    if ((duct1 & DUCT1_SA) == 0
	|| asteo != (duct0 & DUCT0_BASTEO))
	return std;

    /* Load the subspace ASTE origin from the DUCT */
    ssasteo = duct1 & DUCT1_SSASTEO;
    ssasteo = APPLY_PREFIXING (ssasteo, regs->PX);

    /* Program check if ASTE origin address is invalid */
    if (ssasteo >= regs->mainsize)
	ARCH_DEP(program_interrupt) (regs, PGM_ADDRESSING_EXCEPTION);

    /* Fetch subspace ASTE words 0, 2, 3, and 5 from absolute
       storage (note: the ASTE cannot cross a page boundary) */
    ssaste[0] = ARCH_DEP(fetch_fullword_absolute) (ssasteo, regs);
    ssaste[2] = ARCH_DEP(fetch_fullword_absolute) (ssasteo+8, regs);
#if defined(FEATURE_ESAME)
    ssaste[3] = ARCH_DEP(fetch_fullword_absolute) (ssasteo+12, regs);
#endif /*defined(FEATURE_ESAME)*/
    ssaste[5] = ARCH_DEP(fetch_fullword_absolute) (ssasteo+20, regs);

    /* ASTE validity exception if subspace ASTE invalid bit is one */
    if (ssaste[0] & ASTE0_INVALID)
    {
        regs->excarid = 0;
	if (xcode == NULL)
	    ARCH_DEP(program_interrupt) (regs, PGM_ASTE_VALIDITY_EXCEPTION);
	else
	    *xcode = PGM_ASTE_VALIDITY_EXCEPTION;
	return 0;
    }

    /* ASTE sequence exception if the subspace ASTE sequence
       number does not match the sequence number in the DUCT */
    if ((ssaste[5] & ASTE5_ASTESN) != (duct3 & DUCT3_SSASTESN))
    {
        regs->excarid = 0;
	if (xcode == NULL)
	    ARCH_DEP(program_interrupt) (regs, PGM_ASTE_SEQUENCE_EXCEPTION);
	else
	    *xcode = PGM_ASTE_SEQUENCE_EXCEPTION;
	return 0;
    }

    /* Replace the STD or ASCE with the subspace ASTE STD or ASCE,
       except for the space switch event bit and the storage
       alteration event bit, which remain unchanged */
    std &= (SSEVENT_BIT | SAEVENT_BIT);
    std |= (ASTE_AS_DESIGNATOR(ssaste)
		& ~((RADR)(SSEVENT_BIT | SAEVENT_BIT)));

    /* Return the STD resulting from subspace replacement */
    return std;

} /* end function subspace_replace */


#include "dat.h"

#include "vstore.h"

/* end of INLINE.H */
