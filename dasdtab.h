/* DASDTAB.H    (c) Copyright Roger Bowler, 1999-2004                */
/*              DASD table structures                                */

/*-------------------------------------------------------------------*/
/* This header file contains defines the table entries that          */
/* describe all DASD devices supported by Hercules.                  */
/* It also contains function prototypes for the DASD table utilities.*/
/*-------------------------------------------------------------------*/

#if !defined(_DASDTAB_H)
#define _DASDTAB_H

/*-------------------------------------------------------------------*/
/* Definition of a CKD DASD device entry                             */
/*-------------------------------------------------------------------*/
typedef struct _CKDDEV {                /* CKD Device table entry    */
        char   *name;                   /* Device name               */
        U16     devt;                   /* Device type               */
        BYTE    model;                  /* Device model              */
        BYTE    class;                  /* Device class              */
        BYTE    code;                   /* Device code               */
        U16     cyls;                   /* Number primary cylinders  */
        U16     altcyls;                /* Number alternate cylinders*/
        U16     heads;                  /* Number heads (trks/cyl)   */
        U16     r0;                     /* R0 max size               */
        U16     r1;                     /* R1 max size               */
        U16     har0;                   /* HA/R0 overhead size       */
        U16     len;                    /* Max length                */
        U16     sectors;                /* Number sectors            */
        U16     rpscalc;                /* RPS calculation factor    */
        S16     formula;                /* Space calculation formula */
        U16     f1,f2,f3,f4,f5,f6;      /* Space calculation factors */
        char   *cu;                     /* Default control unit name */
      } CKDDEV;
#define CKDDEV_SIZE sizeof(CKDDEV)

/*-------------------------------------------------------------------*/
/* Definition of a CKD DASD control unit entry                       */
/*-------------------------------------------------------------------*/
typedef struct _CKDCU {                 /* CKD Control Unit entry    */
        char   *name;                   /* Control Unit name         */
        U16     devt;                   /* Control Unit type         */
        BYTE    model;                  /* Control Unit model        */
        BYTE    code;                   /* Control Unit code         */
        U32     sctlfeat;               /* Control Unit features     */
        U32     ciw1;                   /* CIW 1                     */
        U32     ciw2;                   /* CIW 2                     */
        U32     ciw3;                   /* CIW 3                     */
        U32     ciw4;                   /* CIW 4                     */
        U32     ciw5;                   /* CIW 5                     */
        U32     ciw6;                   /* CIW 6                     */
        U32     ciw7;                   /* CIW 7                     */
        U32     ciw8;                   /* CIW 8                     */
      } CKDCU;
#define CKDCU_SIZE sizeof(CKDCU)

/*-------------------------------------------------------------------*/
/* Definition of a FBA DASD device entry                             */
/*-------------------------------------------------------------------*/
typedef struct _FBADEV {                /* FBA Device entry          */
        char   *name;                   /* Device name               */
        U16     devt;                   /* Device type               */
        BYTE    class;                  /* Device class              */
        BYTE    type;                   /* Type                      */
        BYTE    model;                  /* Model                     */
        U32     bpg;                    /* Blocks per cyclical group */
        U32     bpp;                    /* Blocks per access position*/
        U32     size;                   /* Block size                */
        U32     blks;                   /* Number of blocks          */
        U16     cu;                     /* Default control unit type */
      } FBADEV; 
#define FBADEV_SIZE sizeof(FBADEV)

/*-------------------------------------------------------------------*/
/* Request types for dasd_lookup                                     */
/*-------------------------------------------------------------------*/
#define DASD_CKDDEV 1                   /* Lookup CKD device         */
#define DASD_CKDCU  2                   /* Lookup CKD control unit   */
#define DASD_FBADEV 3                   /* Lookup FBA device         */

/*-------------------------------------------------------------------*/
/* Dasd table function prototypes                                    */
/*-------------------------------------------------------------------*/
void   *dasd_lookup (int, char *, U32   , U32   );
int     dasd_build_ckd_devid (CKDDEV *, CKDCU *, BYTE *);
int     dasd_build_ckd_devchar (CKDDEV *, CKDCU *, BYTE *, int);
int     dasd_build_fba_devid (FBADEV *, BYTE *);
int     dasd_build_fba_devchar (FBADEV *, BYTE *, int);

#endif /*!defined(_DASDTAB_H)*/
