/* FEAT370.H    (c) Copyright Jan Jaeger, 2000-2009                  */
/*              S/370 feature definitions                            */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*-------------------------------------------------------------------*/
/* This file defines the architectural features which are included   */
/* at compilation time for S/370 mode                                */
/*-------------------------------------------------------------------*/

#if defined(OPTION_370_MODE)
#define _ARCH_370_NAME "S/370"

/* This file MUST NOT contain #undef statements */
#define FEATURE_2K_STORAGE_KEYS
#define FEATURE_BASIC_STORAGE_KEYS
#define FEATURE_EXTENDED_STORAGE_KEYS
#define FEATURE_BCMODE
#define FEATURE_DUAL_ADDRESS_SPACE
#define FEATURE_EMULATE_VM
#define FEATURE_HERCULES_DIAGCALLS
#define FEATURE_HEXADECIMAL_FLOATING_POINT
#define FEATURE_PER
#define FEATURE_INTERVAL_TIMER
#define FEATURE_SEGMENT_PROTECTION
#define FEATURE_S370_CHANNEL
#define FEATURE_CHANNEL_SWITCHING
#define FEATURE_S370E_EXTENDED_ADDRESSING
#define FEATURE_TEST_BLOCK
#define FEATURE_ECPSVM
#define FEATURE_VM_BLOCKIO

/* The instructions for the following features will be made */
/* available unconditionally if OPTION_370_EXTENSION is NOT */
/* defined. If OPTION_370_EXTENSION is defined, then the    */
/* instructions pertaining to the following features        */
/* will only be made available if they are made available   */
/* at runtime                                               */

/* The following ESA/390 features can be retrofitted to S/370 and
   may be activated if desired by uncommenting the appropriate
   define statements below and performing a complete rebuild */
#define FEATURE_BASIC_FP_EXTENSIONS
#define FEATURE_BINARY_FLOATING_POINT
#define FEATURE_CHECKSUM_INSTRUCTION
#define FEATURE_COMPARE_AND_MOVE_EXTENDED
#define FEATURE_COMPRESSION
#define FEATURE_EXTENDED_TRANSLATION
#define FEATURE_EXTENDED_TRANSLATION_FACILITY_2
#define FEATURE_HFP_EXTENSIONS
#define FEATURE_HFP_MULTIPLY_ADD_SUBTRACT
#define FEATURE_HFP_UNNORMALIZED_EXTENSION
#define FEATURE_IMMEDIATE_AND_RELATIVE
#define FEATURE_SQUARE_ROOT
#define FEATURE_STRING_INSTRUCTION

/* The following ESAME features can be retrofitted to S/370 and
   may be activated if desired by uncommenting the appropriate
   define statements below and performing a complete rebuild */
#define FEATURE_ESAME_N3_ESA390
#define FEATURE_ETF2_ENHANCEMENT
#define FEATURE_ETF3_ENHANCEMENT
#define FEATURE_EXECUTE_EXTENSIONS_FACILITY
#define FEATURE_EXTENDED_IMMEDIATE
#define FEATURE_EXTENDED_TRANSLATION_FACILITY_3
#define FEATURE_GENERAL_INSTRUCTIONS_EXTENSION_FACILITY
#define FEATURE_LONG_DISPLACEMENT
#define FEATURE_MESSAGE_SECURITY_ASSIST
#define FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
#define FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
#define FEATURE_PARSING_ENHANCEMENT_FACILITY

/* End of features which instructions are */
/* controlled by OPTION_370_EXTENSION     */

#endif /*defined(OPTION_370_MODE)*/
/* end of FEAT370.H */
