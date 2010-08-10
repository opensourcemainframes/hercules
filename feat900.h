/* FEAT900.H    (c) Copyright Jan Jaeger, 2000-2009                  */
/*              ESAME feature definitions                            */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*-------------------------------------------------------------------*/
/* This file defines the architectural features which are included   */
/* at compilation time for ESAME (z/Architecture) mode               */
/*-------------------------------------------------------------------*/
 
#if defined(OPTION_900_MODE)
#define _ARCH_900_NAME "z/Arch" /* also: "ESAME" */

/* This file MUST NOT contain #undef statements */
#define FEATURE_4K_STORAGE_KEYS
#define FEATURE_ACCESS_REGISTERS
#define FEATURE_ADDRESS_LIMIT_CHECKING
#define FEATURE_ASN_AND_LX_REUSE
#define FEATURE_BASIC_FP_EXTENSIONS
#define FEATURE_BIMODAL_ADDRESSING
#define FEATURE_BINARY_FLOATING_POINT
#define FEATURE_BRANCH_AND_SET_AUTHORITY
#define FEATURE_BROADCASTED_PURGING
#define FEATURE_CANCEL_IO_FACILITY /* comment if FEATURE_S370_CHANNEL used */
#define FEATURE_CALLED_SPACE_IDENTIFICATION
#define FEATURE_CHANNEL_SUBSYSTEM  /* comment if FEATURE_S370_CHANNEL used */
//#define FEATURE_CHANNEL_SWITCHING  /* comment if FEATURE_CHANNEL_SUBSYSTEM used */
#define FEATURE_CHECKSUM_INSTRUCTION
#define FEATURE_CHSC
#define FEATURE_COMPARE_AND_MOVE_EXTENDED
#define FEATURE_COMPARE_AND_SWAP_AND_STORE                      /*407*/
#define FEATURE_COMPARE_AND_SWAP_AND_STORE_FACILITY_2           /*ISW*/
#define FEATURE_COMPRESSION
#define FEATURE_CONDITIONAL_SSKE                                /*407*/
#define FEATURE_CONFIGURATION_TOPOLOGY_FACILITY                 /*208*/
//#define FEATURE_CPU_MEASUREMENT_COUNTER_FACILITY
//#define FEATURE_CPU_MEASUREMENT_SAMPLING_FACILITY
#define FEATURE_CPU_RECONFIG
#define FEATURE_DAT_ENHANCEMENT
#define FEATURE_DAT_ENHANCEMENT_FACILITY_2                      /*@Z9*/
#define FEATURE_DECIMAL_FLOATING_POINT                          /*DFP*/
#define FEATURE_DISTINCT_OPERANDS_FACILITY                      /*810*/
#define FEATURE_DUAL_ADDRESS_SPACE
#define FEATURE_EMULATE_VM
//#define FEATURE_ENHANCED_DAT_FACILITY                           /*208*/
#define FEATURE_ESAME
#define FEATURE_ETF2_ENHANCEMENT                                /*@Z9*/
#define FEATURE_ETF3_ENHANCEMENT                                /*@Z9*/
#define FEATURE_EXECUTE_EXTENSIONS_FACILITY                     /*208*/
#define FEATURE_EXPANDED_STORAGE
#define FEATURE_EXPEDITED_SIE_SUBSET
#define FEATURE_EXTENDED_DIAG204
#define FEATURE_EXTENDED_IMMEDIATE                              /*@Z9*/
#define FEATURE_EXTENDED_STORAGE_KEYS
#define FEATURE_EXTENDED_TOD_CLOCK
#define FEATURE_EXTENDED_TRANSLATION
#define FEATURE_EXTENDED_TRANSLATION_FACILITY_2
#define FEATURE_EXTENDED_TRANSLATION_FACILITY_3
#define FEATURE_EXTERNAL_INTERRUPT_ASSIST
#define FEATURE_EXTRACT_CPU_TIME                                /*407*/
#define FEATURE_FETCH_PROTECTION_OVERRIDE
//#define FEATURE_FLOATING_POINT_EXTENSION_FACILITY               /*810*/
#define FEATURE_FPS_ENHANCEMENT                                 /*DFP*/
#define FEATURE_FPS_EXTENSIONS
#define FEATURE_GENERAL_INSTRUCTIONS_EXTENSION_FACILITY         /*208*/
#define FEATURE_HERCULES_DIAGCALLS
#define FEATURE_HEXADECIMAL_FLOATING_POINT
#define FEATURE_HFP_EXTENSIONS
#define FEATURE_HFP_MULTIPLY_ADD_SUBTRACT
#define FEATURE_HFP_UNNORMALIZED_EXTENSION                      /*@Z9*/
//#define FEATURE_HIGH_WORD_FACILITY                              /*810*/
#define FEATURE_HYPERVISOR
#define FEATURE_IEEE_EXCEPTION_SIMULATION                       /*407*/
#define FEATURE_IMMEDIATE_AND_RELATIVE
#define FEATURE_INCORRECT_LENGTH_INDICATION_SUPPRESSION
#define FEATURE_INTEGRATED_3270_CONSOLE
//#define FEATURE_INTEGRATED_ASCII_CONSOLE
//#define FEATURE_INTERLOCKED_ACCESS_FACILITY                     /*810*/
#define FEATURE_INTERPRETIVE_EXECUTION
#define FEATURE_IO_ASSIST
#define FEATURE_LINKAGE_STACK
#define FEATURE_LOAD_REVERSED
#define FEATURE_LOAD_STORE_ON_CONDITION_FACILITY                /*810*/
#define FEATURE_LOCK_PAGE
#define FEATURE_LONG_DISPLACEMENT
#define FEATURE_MESSAGE_SECURITY_ASSIST
#define FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1             /*@Z9*/
#define FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
//#define FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3             /*810*/
//#define FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4             /*810*/
#define FEATURE_MIDAW                                           /*@Z9*/
#define FEATURE_MOVE_PAGE_FACILITY_2
#define FEATURE_MOVE_WITH_OPTIONAL_SPECIFICATIONS               /*208*/
#define FEATURE_MPF_INFO
#define FEATURE_MVS_ASSIST
#define FEATURE_PAGE_PROTECTION
#define FEATURE_PARSING_ENHANCEMENT_FACILITY                    /*208*/
#define FEATURE_PERFORM_LOCKED_OPERATION
#define FEATURE_PER
#define FEATURE_PER2
#define FEATURE_PER3                                            /*@Z9*/
//#define FEATURE_PFPO                                            /*407*/
#define FEATURE_POPULATION_COUNT_FACILITY                       /*810*/
#define FEATURE_PRIVATE_SPACE
//#define FEATURE_PROGRAM_DIRECTED_REIPL /*DIAG308 incomplete*/  /*@Z9*/
#define FEATURE_PROTECTION_INTERCEPTION_CONTROL
#define FEATURE_QUEUED_DIRECT_IO
//#define FEATURE_RESTORE_SUBCHANNEL_FACILITY                     /*208*/
#define FEATURE_RESUME_PROGRAM
#define FEATURE_REGION_RELOCATE
//#define FEATURE_S370_CHANNEL  /* comment if FEATURE_CHANNEL_SUBSYSTEM used */
#define FEATURE_SCEDIO
#define FEATURE_SENSE_RUNNING_STATUS                            /*@Z9*/
#define FEATURE_SERVICE_PROCESSOR
#define FEATURE_SET_ADDRESS_SPACE_CONTROL_FAST
//#define FEATURE_SET_PROGRAM_PARAMETER_FACILITY
#define FEATURE_SQUARE_ROOT
#define FEATURE_STORAGE_KEY_ASSIST
#define FEATURE_STORAGE_PROTECTION_OVERRIDE
#define FEATURE_STORE_CLOCK_FAST
#define FEATURE_STORE_FACILITY_LIST
#define FEATURE_STORE_FACILITY_LIST_EXTENDED                    /*@Z9*/
#define FEATURE_STORE_SYSTEM_INFORMATION
#define FEATURE_STRING_INSTRUCTION
#define FEATURE_SUBSPACE_GROUP
#define FEATURE_SUPPRESSION_ON_PROTECTION
#define FEATURE_SYSTEM_CONSOLE
#define FEATURE_TEST_BLOCK
#define FEATURE_TOD_CLOCK_STEERING                              /*@Z9*/
#define FEATURE_TRACING
#define FEATURE_WAITSTATE_ASSIST
#define FEATURE_VM_BLOCKIO

#endif /*defined(OPTION_900_MODE)*/
/* end of FEAT900.H */
