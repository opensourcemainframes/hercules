/* FEATALL.H    (c) Copyright Jan Jaeger, 2000-2002                  */
/*              Architecture-dependent macro definitions             */

/*-------------------------------------------------------------------*/
/* Default features                                                  */
/*   All existing features MUST be #undef-ed here.                   */
/*-------------------------------------------------------------------*/
#define MAX_CPU_ENGINES               2 /* Maximum number of engines */
#undef  SMP_SERIALIZATION               /* Serialize storage for SMP */
#define OPTION_370_MODE                 /* Generate S/370 support    */
#define OPTION_390_MODE                 /* Generate ESA/390 support  */
#define OPTION_900_MODE                 /* Generate ESAME support    */
#define OPTION_LPP_RESTRICT             /* Disable Licensed Software */
#define VECTOR_SECTION_SIZE         128 /* Vector section size       */
#define VECTOR_PARTIAL_SUM_NUMBER     1 /* Vector partial sum number */
#define CKD_MAXFILES                  4 /* Max files per CKD volume  */
#define OPTION_MIPS_COUNTING            /* Display MIPS on ctl panel */
#define OPTION_TODCLOCK_DRAG_FACTOR     /* Enable toddrag feature    */
#define PANEL_REFRESH_RATE              /* Enable panrate feature    */
#define PANEL_REFRESH_RATE_FAST      50 /* Fast refresh rate         */
#define PANEL_REFRESH_RATE_SLOW     500 /* Slow refresh rate         */
#define MAX_DEVICE_THREAD_IDLE_SECS 300 /* 5 Minute thread timeout   */
#define OPTION_AIA_BUFFER               /* Instruction addr cache    */
#define OPTION_AEA_BUFFER               /* Effective addr cache      */
#undef  OPTION_NO_INLINE_DAT            /* Performance option        */
#define OPTION_NO_INLINE_LOGICAL        /* Performance option        */
#undef  OPTION_NO_INLINE_VSTORE         /* Performance option        */
#define OPTION_NO_INLINE_IFETCH         /* Performance option        */
#define OPTION_CPU_UNROLL               /* Performance option        */
#define OPTION_FAST_MOVECHAR            /* Performance option        */
#define OPTION_FAST_MOVELONG            /* Performance option        */
#define OPTION_FAST_INSTFETCH           /* Performance option        */
#define OPTION_FAST_LOGICAL             /* Performance option        */
#define OPTION_FAST_PREFIX              /* Performance option        */
#define OPTION_REDUCED_INVAL            /* Performance option        */
#define OPTION_FETCHIBYTE               /* Performance option        */
#undef  OPTION_CS_USLEEP                /* Sleep if CS collision     */
#define OPTION_FAST_INTCOND             /* Ea CPU has intcond COND   */
#define OPTION_SYNCIO                   /* Synchronous I/O option    */
#undef  OPTION_IODELAY         /*  1000 ** IO delay in uSec          */
#undef  OPTION_FOOTPRINT_BUFFER /* 2048 ** Size must be a power of 2 */
#undef  OPTION_INSTRUCTION_COUNTING     /* First use trace and count */
#define OPTION_CKD_KEY_TRACING          /* Trace CKD search keys     */
#undef  OPTION_CMPSC_DEBUGLVL      /* 3 ** 1=Exp 2=Comp 3=Both debug */
#undef  MODEL_DEPENDENT_STCM            /* STCM, STCMH always store  */
#define OPTION_NOP_MODEL158_DIAGNOSE    /* NOP mod 158 specific diags*/
#define FEATURE_ALD_FORMAT            0


/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
/*
  The following option is currently hard-coded, but only while
  development and testing is still in progress. Once development
  and testing is complete, control of the setting of this option
  should be moved to the configure script. (Either that or it
  should be removed altogether and made a permanent feature.)
*/
#if defined(WIN32)
#define OPTION_W32_CTCI
#endif // defined(WIN32)
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////


/* Allow for compiler command line overrides */
#if defined(OPTION_370_MODE) && defined(NO_370_MODE)
 #undef OPTION_370_MODE
#endif
#if defined(OPTION_390_MODE) && defined(NO_390_MODE)
 #undef OPTION_390_MODE
#endif
#if defined(OPTION_900_MODE) && defined(NO_900_MODE)
 #undef OPTION_900_MODE
#endif


/* OPTION_FISHIO only possible with OPTION_FTHREADS */
#if defined(OPTION_FTHREADS)
  #define OPTION_FISHIO
#else // !defined(OPTION_FTHREADS)
  #undef OPTION_FISHIO
#endif


#undef FEATURE_4K_STORAGE_KEYS
#undef FEATURE_2K_STORAGE_KEYS
#undef FEATURE_ACCESS_REGISTERS
#undef FEATURE_ADDRESS_LIMIT_CHECKING
#undef FEATURE_BASIC_FP_EXTENSIONS
#undef FEATURE_BASIC_STORAGE_KEYS
#undef FEATURE_BCMODE
#undef FEATURE_BIMODAL_ADDRESSING
#undef FEATURE_BINARY_FLOATING_POINT
#undef FEATURE_BRANCH_AND_SET_AUTHORITY
#undef FEATURE_BROADCASTED_PURGING
#undef FEATURE_CALLED_SPACE_IDENTIFICATION
#undef FEATURE_CANCEL_IO_FACILITY
#undef FEATURE_CHANNEL_SUBSYSTEM
#undef FEATURE_CHANNEL_SWITCHING
#undef FEATURE_CHECKSUM_INSTRUCTION
#undef FEATURE_COMPARE_AND_MOVE_EXTENDED
#undef FEATURE_COMPRESSION
#undef FEATURE_CPU_RECONFIG
#undef FEATURE_DUAL_ADDRESS_SPACE
#undef FEATURE_EMULATE_VM
#undef FEATURE_ESAME
#undef FEATURE_ESAME_N3_ESA390
#undef FEATURE_EXPANDED_STORAGE
#undef FEATURE_EXTENDED_STORAGE_KEYS
#undef FEATURE_EXTENDED_TOD_CLOCK
#undef FEATURE_EXTENDED_TRANSLATION
#undef FEATURE_EXTENDED_TRANSLATION_FACILITY_2
#undef FEATURE_FETCH_PROTECTION_OVERRIDE
#undef FEATURE_FPS_EXTENSIONS
#undef FEATURE_HERCULES_DIAGCALLS
#undef FEATURE_HEXADECIMAL_FLOATING_POINT
#undef FEATURE_HFP_EXTENSIONS
#undef FEATURE_HYPERVISOR
#undef FEATURE_IMMEDIATE_AND_RELATIVE
#undef FEATURE_INCORRECT_LENGTH_INDICATION_SUPPRESSION
#undef FEATURE_INTERPRETIVE_EXECUTION
#undef FEATURE_INTERVAL_TIMER
#undef FEATURE_LINKAGE_STACK
#undef FEATURE_LOAD_REVERSED
#undef FEATURE_LOCK_PAGE
#undef FEATURE_MOVE_PAGE_FACILITY_2
#undef FEATURE_MSSF_CALL
#undef FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE
#undef FEATURE_MVS_ASSIST
#undef FEATURE_PAGE_PROTECTION
#undef FEATURE_PERFORM_LOCKED_OPERATION
#undef FEATURE_PER
#undef FEATURE_PER2
#undef FEATURE_PRIVATE_SPACE
#undef FEATURE_RESUME_PROGRAM
#undef FEATURE_S370_CHANNEL
#undef FEATURE_S390_DAT
#undef FEATURE_SERVICE_PROCESSOR
#undef FEATURE_SEGMENT_PROTECTION
#undef FEATURE_CHANNEL_SWITCHING
#undef FEATURE_CHSC
#undef FEATURE_SQUARE_ROOT
#undef FEATURE_STORAGE_KEY_ASSIST
#undef FEATURE_STORAGE_PROTECTION_OVERRIDE
#undef FEATURE_STORE_SYSTEM_INFORMATION
#undef FEATURE_SUBSPACE_GROUP
#undef FEATURE_SUPPRESSION_ON_PROTECTION
#undef FEATURE_SYSTEM_CONSOLE
#undef FEATURE_TRACING
#undef FEATURE_VECTOR_FACILITY

/* end of FEATALL.H */
