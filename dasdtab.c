/* DASDTAB.C    (c) Copyright Roger Bowler, 1999-2006                */
/*              Hercules Supported DASD definitions                  */

// $Id$

/*-------------------------------------------------------------------*/
/* This module contains the tables that define the attributes of     */
/* each DASD device and control unit supported by Hercules.          */
/* Routines are also provided to perform table lookup and build the  */
/* device identifier and characteristics areas.                      */
/*                                                                   */
/* Note: source for most CKD/FBA device capacities take from SDI's   */
/* device capacity page at: http://www.sdisw.com/dasd_capacity.html  */
/* (used with permission)                                            */
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.35  2007/06/07 19:15:05  kleonard
// Document circumvention for DSF X'0A' command reject
//
// Revision 1.34  2007/05/04 19:28:38  kleonard
// Circumvent command reject for DSF X'0A' command
//
// Revision 1.33  2007/03/06 22:54:19  gsmith
// Fix ckd RDC response
//
// Revision 1.32  2007/02/15 00:10:04  gsmith
// Fix ckd RCD, SNSS, SNSID responses
//
// Revision 1.31  2006/12/08 09:43:20  jj
// Add CVS message log
//

#include "hstdinc.h"

#define _DASDTAB_C_
#define _HDASD_DLL_

#include "hercules.h"

/*-------------------------------------------------------------------*/
/* CKD device definitions                                            */
/*-------------------------------------------------------------------*/
static CKDDEV ckdtab[] = {
/* name         type model clas code prime a hd    r0    r1 har0   len sec    rps  f f1  f2   f3   f4 f5 f6  cu */
 {"2305",      0x2305,0x00,0x20,0x00,   48,0, 8,14568,14136, 432,14568, 90,0x0000,-1,202,432,  0,   0,  0,0,"2835"},
 {"2305-1",    0x2305,0x00,0x20,0x00,   48,0, 8,14568,14136, 432,14568, 90,0x0000,-1,202,432,  0,   0,  0,0,"2835"},
 {"2305-2",    0x2305,0x02,0x20,0x00,   96,0, 8,14858,14660, 198,14858, 90,0x0000,-1, 91,198,  0,   0,  0,0,"2835"},

 {"2311",      0x2311,0x00,0x20,0x00,  200,3,10,    0, 3625,   0, 3625,  0,0x0000,-2,20, 61, 537, 512,  0,0,"2841"},
 {"2311-1",    0x2311,0x00,0x20,0x00,  200,3,10,    0, 3625,   0, 3625,  0,0x0000,-2,20, 61, 537, 512,  0,0,"2841"},

 {"2314",      0x2314,0x00,0x20,0x00,  200,3,20,    0, 7294,   0, 7294,  0,0x0000,-2,45,101,2137,2048,  0,0,"2841"},
 {"2314-1",    0x2314,0x00,0x20,0x00,  200,3,20,    0, 7294,   0, 7294,  0,0x0000,-2,45,101,2137,2048,  0,0,"2841"},

 {"3330",      0x3330,0x01,0x20,0x00,  404,7,19,13165,13030, 135,13165,128,0x0000,-1,56,135,   0,   0,  0,0,"3830"},
 {"3330-1",    0x3330,0x01,0x20,0x00,  404,7,19,13165,13030, 135,13165,128,0x0000,-1,56,135,   0,   0,  0,0,"3830"},
 {"3330-2",    0x3330,0x11,0x20,0x00,  808,7,19,13165,13030, 135,13165,128,0x0000,-1,56,135,   0,   0,  0,0,"3830"},
 {"3330-11",   0x3330,0x11,0x20,0x00,  808,7,19,13165,13030, 135,13165,128,0x0000,-1,56,135,   0,   0,  0,0,"3830"},

 {"3340",      0x3340,0x01,0x20,0x00,  348,1,12, 8535, 8368, 167, 8535, 64,0x0000,-1,75,167,   0,   0,  0,0,"3830"},
 {"3340-1",    0x3340,0x01,0x20,0x00,  348,1,12, 8535, 8368, 167, 8535, 64,0x0000,-1,75,167,   0,   0,  0,0,"3830"},
 {"3340-35",   0x3340,0x01,0x20,0x00,  348,1,12, 8535, 8368, 167, 8535, 64,0x0000,-1,75,167,   0,   0,  0,0,"3830"},
 {"3340-2",    0x3340,0x02,0x20,0x00,  696,2,12, 8535, 8368, 167, 8535, 64,0x0000,-1,75,167,   0,   0,  0,0,"3830"},
 {"3340-70",   0x3340,0x02,0x20,0x00,  696,2,12, 8535, 8368, 167, 8535, 64,0x0000,-1,75,167,   0,   0,  0,0,"3830"},

 {"3350",      0x3350,0x00,0x20,0x00,  555,5,30,19254,19069, 185,19254,128,0x0000,-1,82,185,   0,   0,  0,0,"3830"},
 {"3350-1",    0x3350,0x00,0x20,0x00,  555,5,30,19254,19069, 185,19254,128,0x0000,-1,82,185,   0,   0,  0,0,"3830"},

 {"3375",      0x3375,0x02,0x20,0x0e,  959,3,12,36000,35616, 832,36000,196,0x5007, 1, 0x20,0x01,0x08,0x00,0x0a,0x00,"3880"},
 {"3375-1",    0x3375,0x02,0x20,0x0e,  959,3,12,36000,35616, 832,36000,196,0x5007, 1, 0x20,0x01,0x08,0x00,0x0a,0x00,"3880"},
 
 {"3380",      0x3380,0x16,0x20,0x0e,  885,1,15,47988,47476,1088,47968,222,0x0000, 1, 0x20,0x01,0xec,0x00,0xec,0x00,"3880"},
 {"3380-1",    0x3380,0x16,0x20,0x0e,  885,1,15,47988,47476,1088,47968,222,0x0000, 1, 0x20,0x01,0xec,0x00,0xec,0x00,"3880"},
 {"3380-A",    0x3380,0x16,0x20,0x0e,  885,1,15,47988,47476,1088,47968,222,0x0000, 1, 0x20,0x01,0xec,0x00,0xec,0x00,"3880"},
 {"3380-B",    0x3380,0x16,0x20,0x0e,  885,1,15,47988,47476,1088,47968,222,0x0000, 1, 0x20,0x01,0xec,0x00,0xec,0x00,"3880"},
 {"3380-D",    0x3380,0x16,0x20,0x0e,  885,1,15,47988,47476,1088,47968,222,0x0000, 1, 0x20,0x01,0xec,0x00,0xec,0x00,"3880"},
 {"3380-J",    0x3380,0x16,0x20,0x0e,  885,1,15,47988,47476,1088,47968,222,0x0000, 1, 0x20,0x01,0xec,0x00,0xec,0x00,"3880"},
 {"3380-2",    0x3380,0x0a,0x20,0x0e, 1770,2,15,47988,47476,1088,47968,222,0x0000, 1, 0x20,0x01,0xec,0x00,0xec,0x00,"3880"},
 {"3380-E",    0x3380,0x0a,0x20,0x0e, 1770,2,15,47988,47476,1088,47968,222,0x0000, 1, 0x20,0x01,0xec,0x00,0xec,0x00,"3880"},
 {"3380-3",    0x3380,0x1e,0x20,0x0e, 2655,3,15,47988,47476,1088,47968,222,0x0000, 1, 0x20,0x01,0xec,0x00,0xec,0x00,"3880"},
 {"3380-K",    0x3380,0x1e,0x20,0x0e, 2655,3,15,47988,47476,1088,47968,222,0x0000, 1, 0x20,0x01,0xec,0x00,0xec,0x00,"3880"},
 {"EMC3380K+", 0x3380,0x1e,0x20,0x0e, 3339,3,15,47988,47476,1088,47968,222,0x0000, 1, 0x20,0x01,0xec,0x00,0xec,0x00,"3880"},
 {"EMC3380K++",0x3380,0x1e,0x20,0x0e, 3993,3,15,47988,47476,1088,47968,222,0x0000, 1, 0x20,0x01,0xec,0x00,0xec,0x00,"3880"},

 {"3390",      0x3390,0x02,0x20,0x26, 1113,1,15,57326,56664,1428,58786,224,0x7708, 2, 34,19,   9,   6,116,6,"3990-6"},
 {"3390-1",    0x3390,0x02,0x20,0x26, 1113,1,15,57326,56664,1428,58786,224,0x7708, 2, 34,19,   9,   6,116,6,"3990-6"},
 {"3390-2",    0x3390,0x06,0x20,0x27, 2226,1,15,57326,56664,1428,58786,224,0x7708, 2, 34,19,   9,   6,116,6,"3990-6"},
 {"3390-3",    0x3390,0x0a,0x20,0x24, 3339,1,15,57326,56664,1428,58786,224,0x7708, 2, 34,19,   9,   6,116,6,"3990-6"},
 {"3390-9",    0x3390,0x0c,0x20,0x32,10017,3,15,57326,56664,1428,58786,224,0x7708, 2, 34,19,   9,   6,116,6,"3990-6"},
 {"3390-27",   0x3390,0x0c,0x20,0x32,32760,3,15,57326,56664,1428,58786,224,0x7708, 2, 34,19,   9,   6,116,6,"3990-6"},
 {"3390-J",    0x3390,0x0c,0x20,0x32,32760,3,15,57326,56664,1428,58786,224,0x7708, 2, 34,19,   9,   6,116,6,"3990-6"},
 {"3390-54",   0x3390,0x0c,0x20,0x32,65520,3,15,57326,56664,1428,58786,224,0x7708, 2, 34,19,   9,   6,116,6,"3990-6"},
 {"3390-JJ",   0x3390,0x0c,0x20,0x32,65520,3,15,57326,56664,1428,58786,224,0x7708, 2, 34,19,   9,   6,116,6,"3990-6"},

 {"9345",      0x9345,0x04,0x20,0x04, 1440,0,15,48174,46456,1184,48280,213,0x8b07, 2, 34,18,   7,   6,116,6,"9343"},
 {"9345-1",    0x9345,0x04,0x20,0x04, 1440,0,15,48174,46456,1184,48280,213,0x8b07, 2, 34,18,   7,   6,116,6,"9343"},
 {"9345-2",    0x9345,0x04,0x20,0x04, 2156,0,15,48174,46456,1184,48280,213,0x8b07, 2, 34,18,   7,   6,116,6,"9343"}
} ;
#define CKDDEV_NUM (sizeof(ckdtab)/CKDDEV_SIZE)

/*-------------------------------------------------------------------*/
/* CKD control unit definitions                                      */
/*-------------------------------------------------------------------*/
static CKDCU ckdcutab[] = {
/*                              func/ type                           */
/* name          type model code feat code features   ciws --------- */
 {"2835",       0x2835,0x00,0x00,0x00,0x00,0x50000103,0,0,0,0,0,0,0,0},
 {"2841",       0x2841,0x00,0x00,0x00,0x00,0x50000103,0,0,0,0,0,0,0,0},
 {"3830",       0x3830,0x02,0x00,0x00,0x00,0x50000103,0,0,0,0,0,0,0,0},
 {"3880",       0x3880,0x05,0x09,0x00,0x00,0x80000000,0,0,0,0,0,0,0,0},
 {"3990",       0x3990,0xc2,0x10,0x00,0x00,0xd0000002,0x40fa0100,0,0,0,0,0,0,0},
 {"3990-3",     0x3990,0xec,0x06,0x00,0x00,0xd000009e,0x40fa0100,0x41270004,0x423e0040,0,0,0,0,0},
 {"3990-6",     0x3990,0xe9,0x15,0x48,0x15,0x50003097,0x40fa0100,0x41270004,0x423e0060,0,0,0,0,0},
 {"9343",       0x9343,0xe0,0x11,0x00,0x00,0x80000000,0,0,0,0,0,0,0,0}
} ;
#define CKDCU_NUM (sizeof(ckdcutab)/CKDCU_SIZE)

/*-------------------------------------------------------------------*/
/* FBA device definitions - courtesy of Tomas Masek                  */
/*-------------------------------------------------------------------*/
static FBADEV fbatab[] = {
/* name          devt class type mdl  bpg bpp size   blks   cu     */
 {"3310",       0x3310,0x21,0x01,0x01, 32,352,512, 125664,0x4331},
 {"3310-1",     0x3310,0x21,0x01,0x01, 32,352,512, 125664,0x4331},
 {"3310-x",     0x3310,0x21,0x01,0x01, 32,352,512,      0,0x4331},

 {"3370",       0x3370,0x21,0x02,0x00, 62,744,512, 558000,0x3880},
 {"3370-1",     0x3370,0x21,0x02,0x00, 62,744,512, 558000,0x3880},
 {"3370-A1",    0x3370,0x21,0x02,0x00, 62,744,512, 558000,0x3880},
 {"3370-B1",    0x3370,0x21,0x02,0x00, 62,744,512, 558000,0x3880},
 {"3370-2",     0x3370,0x21,0x05,0x04, 62,744,512, 712752,0x3880},
 {"3370-A2",    0x3370,0x21,0x05,0x04, 62,744,512, 712752,0x3880},
 {"3370-B2",    0x3370,0x21,0x05,0x04, 62,744,512, 712752,0x3880},
 {"3370-x",     0x3370,0x21,0x05,0x04, 62,744,512,      0,0x3880},

 {"9332",       0x9332,0x21,0x07,0x00, 73,292,512, 360036,0x6310},
 {"9332-400",   0x9332,0x21,0x07,0x00, 73,292,512, 360036,0x6310},
 {"9332-600",   0x9332,0x21,0x07,0x01, 73,292,512, 554800,0x6310},
 {"9332-x",     0x9332,0x21,0x07,0x01, 73,292,512,      0,0x6310},

 {"9335",       0x9335,0x21,0x06,0x01, 71,426,512, 804714,0x6310},
 {"9335-x",     0x9335,0x21,0x06,0x01, 71,426,512,      0,0x6310},

/*"9313",       0x9313,0x21,0x08,0x00, ??,???,512, 246240,0x????}, */
/*"9313-1,      0x9313,0x21,0x08,0x00, ??,???,512, 246240,0x????}, */
/*"9313-14",    0x9313,0x21,0x08,0x14, ??,???,512, 246240,0x????}, */
/* 246240=32*81*5*19 */
 {"9313",       0x9313,0x21,0x08,0x00, 96,480,512, 246240,0x6310},
 {"9313-x",     0x9313,0x21,0x08,0x00, 96,480,512,      0,0x6310},

/* 9336 Junior models 1,2,3 */
/*"9336-J1",    0x9336,0x21,0x11,0x00, 63,315,512, 920115,0x6310}, */
/*"9336-J2",    0x9336,0x21,0x11,0x04, ??,???,512,      ?,0x6310}, */
/*"9336-J3",    0x9336,0x21,0x11,0x08, ??,???,512,      ?,0x6310}, */
/* 9336 Senior models 1,2,3 */
/*"9336-S1",    0x9336,0x21,0x11,0x10,111,777,512,1672881,0x6310}, */
/*"9336-S2",    0x9336,0x21,0x11,0x14,???,???,512,      ?,0x6310}, */
/*"9336-S3",    0x9336,0x21,0x11,0x18,???,???,512,      ?,0x6310}, */
 {"9336",       0x9336,0x21,0x11,0x00, 63,315,512, 920115,0x6310},
 {"9336-10",    0x9336,0x21,0x11,0x00, 63,315,512, 920115,0x6310},
 {"9336-20",    0x9336,0x21,0x11,0x10,111,777,512,1672881,0x6310},
 {"9336-25",    0x9336,0x21,0x11,0x10,111,777,512,1672881,0x6310},
 {"9336-x",     0x9336,0x21,0x11,0x10,111,777,512,      0,0x6310},

 {"0671-08",    0x0671,0x21,0x12,0x08, 63,630,512, 513072,0x6310},
 {"0671",       0x0671,0x21,0x12,0x00, 63,630,512, 574560,0x6310},
 {"0671-04",    0x0671,0x21,0x12,0x04, 63,630,512, 624456,0x6310},
 {"0671-x",     0x0671,0x21,0x12,0x04, 63,630,512,      0,0x6310}
} ;
#define FBADEV_NUM (sizeof(fbatab)/FBADEV_SIZE)

/*-------------------------------------------------------------------*/
/* Lookup a table entry either by name or type                       */
/*-------------------------------------------------------------------*/
DLL_EXPORT void *dasd_lookup (int dtype, char *name, U32 devt, U32 size)
{
U32 i;                                  /* Loop Index                */

    switch (dtype) {

    case DASD_CKDDEV:
        for (i = 0; i < (int)CKDDEV_NUM; i++)
        {
            if ((name && !strcmp(name, ckdtab[i].name))
             || (((U32)devt == (U32)ckdtab[i].devt || (U32)devt == (U32)(ckdtab[i].devt & 0xff))
              && (U32)size <= (U32)(ckdtab[i].cyls + ckdtab[i].altcyls)))
                return &ckdtab[i];
        }
        return NULL;

    case DASD_CKDCU:
        for (i = 0; i < (int)CKDCU_NUM; i++)
        {
            if ((name != NULL && strcmp(name, ckdcutab[i].name) == 0)
             || devt == ckdcutab[i].devt)
                return &ckdcutab[i];
        }
        return NULL;

    case DASD_FBADEV:
        for (i = 0; i < (int)FBADEV_NUM; i++)
        {
            if ((name && !strcmp(name, fbatab[i].name))
             || (((U32)devt == (U32)fbatab[i].devt || (U32)devt == (U32)(fbatab[i].devt & 0xff))
              && ((size <= fbatab[i].blks) || (fbatab[i].blks == 0))))
                return &fbatab[i];
        }
        return NULL;

    default:
        return NULL;
    }
    return NULL;
}

/*-------------------------------------------------------------------*/
/* Build CKD devid field                                             */
/*-------------------------------------------------------------------*/
int dasd_build_ckd_devid (CKDDEV *ckd, CKDCU *cu, BYTE *devid)
{
int len;

    memset (devid, 0, 256);

    store_fw (devid + 0, 0xFF000000 | (cu->devt << 8) | cu->model);
    store_fw (devid + 4, (ckd->devt << 16) | (ckd->model << 8) | 0x00);
    store_fw (devid + 8, cu->ciw1);
    store_fw (devid +12, cu->ciw2);
    store_fw (devid +16, cu->ciw3);
    store_fw (devid +20, cu->ciw4);
    store_fw (devid +24, cu->ciw5);
    store_fw (devid +28, cu->ciw6);
    store_fw (devid +32, cu->ciw7);
    store_fw (devid +36, cu->ciw8);

    /* Calculate length */
    for (len = 40; fetch_fw(devid + len-4) == 0; len -= 4);
    len = len < 12 ? 12 : len;

    return len;
}


/*-------------------------------------------------------------------*/
/* Build CKD devchar field                                           */
/*-------------------------------------------------------------------*/
int dasd_build_ckd_devchar (CKDDEV *ckd, CKDCU *cu, BYTE *devchar,
                            int cyls)
{
int altcyls;                            /* Number alternate cyls     */

    if (cyls > ckd->cyls) altcyls = cyls - ckd->cyls;
    else altcyls = 0;

    memset (devchar, 0, 64);
    store_hw(devchar+0, cu->devt);              // Storage control type
    devchar[2]  = cu->model;                    // CU model
    store_hw(devchar+3, ckd->devt);             // Device type
    devchar[5]  = ckd->model;                   // Device model
    store_fw(devchar+6, cu->sctlfeat);          // Device and SD facilities
    devchar[10] = ckd->class;                   // Device class code
    devchar[11] = ckd->code;                    // Device type code
    store_hw(devchar+12, cyls - altcyls);       // Primary cylinders
    store_hw(devchar+14, ckd->heads);           // Tracks per cylinder
    devchar[16] = (BYTE)(ckd->sectors);         // Number of sectors
    store_hw(devchar+18, ckd->len);             // Track length
    store_hw(devchar+20, ckd->har0);            // Length of HA and R0
    if (ckd->formula > 0)
    {
        devchar[22] = (BYTE)(ckd->formula);     // Track capacity formula
        devchar[23] = (BYTE)(ckd->f1);          // Factor F1
        devchar[24] = (BYTE)(ckd->f2);          // Factor F2
        devchar[25] = (BYTE)(ckd->f3);          // (F2F3)
        devchar[26] = (BYTE)(ckd->f4);          // Factor F3
        devchar[27] = (BYTE)(ckd->f5);          // (F4F5)
    }
    if (altcyls > 0)
    {
        store_hw(devchar+28, cyls - altcyls);   // Alternate cylinder & tracks
        store_hw(devchar+30, altcyls * ckd->heads);
    }
    devchar[40] = ckd->code;                    // MDR record ID
    devchar[41] = ckd->code;                    // OBR record ID
    devchar[42] = cu->code;                     // CU Type Code
    devchar[43] = 0x02;                         // Parameter length
    store_hw(devchar+44, ckd->r0);              // Record 0 length
    devchar[47] = 0x01;                         // Track set
    devchar[48] = (BYTE)(ckd->f6);              // F6
    store_hw(devchar+49, ckd->rpscalc);         // RPS factor
    devchar[51] = MODEL6(cu) ? 0x0f : 0x00;     // reserved byte 51
    devchar[54] = cu->funcfeat;                 // device/CU functions/features
    devchar[56] = cu->typecode;                 // Real CU type code

    /*---------------------------------------------------------------*/
    /* 2007/05/04 @kl                                                */
    /* The following line to set devchar[57] to 0xff was restored    */
    /* to circumvent a command reject when ICKDSF issues a Read      */
    /* Special Home Address (0x0a) to an alternate track.            */
    /* According to the IBM 3880 Storage Control Reference,          */
    /* GA16-1661-09, and the 3990/9330 Reference, GA32-0274-05,      */
    /* it should be 0x00 for real 3380 and 3390 devices.  Setting    */
    /* it to 0xff makes the underlying real DASD look like a         */
    /* disk array (whose virtual 3380/3390 disks have no alternate   */
    /* tracks).  This causes DSF to skip issuing the 0x0a channel    */
    /* command, which Hercules does not currently support, for       */
    /* alternate tracks.                                             */
    /*---------------------------------------------------------------*/
    devchar[57] = 0xff;                         // real device type code

    return 64;
}


/*-------------------------------------------------------------------*/
/* Build CKD configuration data                                      */
/*-------------------------------------------------------------------*/
DLL_EXPORT int dasd_build_ckd_config_data (DEVBLK *dev, BYTE *iobuf, int count)
{
int  i;
BYTE buf[256];

    /* Clear the configuration data area */
    memset (buf, 0, 256);

    /* Bytes 0-31: NED 1  Node element descriptor for the device */
    store_fw (buf, 0xc4010100);
    sprintf ((char *)&buf[4], "  %4.4X0%2.2XHRCZZ000000000001",
                        dev->ckdtab->devt, dev->ckdtab->model);
    for (i = 4; i < 30; i++)
        buf[i] = host_to_guest(buf[i]);
    store_hw(buf + 30, 0x0300);

    /* Bytes 32-63: NED 2  Node element descriptor for the string */
    store_fw (buf + 32, 0xc4000000);
    sprintf ((char *)&buf[36], "  %4.4X0%2.2XHRCZZ000000000001",
                        dev->ckdtab->devt, dev->ckdtab->model);
    for (i = 36; i < 62; i++)
        buf[i] = host_to_guest(buf[i]);
    store_hw (buf + 62, 0x0300);

    /* Bytes 64-95: NED 3  Node element descriptor for the storage director */
    store_fw (buf + 64, 0xd4020000);
    sprintf ((char *)&buf[68], "  %4.4X0%2.2XHRCZZ000000000001",
                        dev->ckdcu->devt, dev->ckdcu->model);
    for (i = 68; i < 94; i++)
        buf[i] = host_to_guest(buf[i]);
    store_hw (buf + 94, 0x0300);

    /* Bytes 96-127: NED 4  Node element descriptor for the subsystem */
    store_fw (buf + 96, 0xF0000001);
    sprintf ((char *)&buf[100], "  %4.4X   HRCZZ000000000001",
                        dev->ckdcu->devt);
    for (i = 100; i < 126; i++)
        buf[i] = host_to_guest(buf[i]);
    store_hw (buf + 126, 0x0300);

    /* Bytes 128-223: zeroes */

    /* Bytes 224-255: NEQ  Node Element Qualifier */
    buf[224] = 0x80;                  // flags (general NEQ)
    buf[225] = 0;                     // record selector
    store_hw (buf + 226, IFID(dev));  // interface id
    store_hw (buf + 228, 0);          // must be zero
    buf[230] = 0x1E;                  // primary missing interrupt timer interval
    buf[231] = 0x00;                  // secondary missing interrupt timer interval
    store_hw (buf + 232, SSID(dev));  // subsystem id
    buf[234] = 0x80;                  // path/cluster id
    buf[235] = (dev->devnum & 0xFF);  // unit address
    buf[236] = (dev->devnum & 0xFF);  // physical device id
    buf[237] = (dev->devnum & 0xFF);  // physical device address
    buf[238] = buf[227];              // SA ID (same as interface ID, byte 227)
    store_hw (buf + 239, 0);          // escon link address
    buf[241] = 0x80;                  // interface protocol type (parallel)
//  buf[241] = 0x40;                  // interface protocol type (escon)
    buf[242] = 0x80;                  // NEQ format flags
    buf[243] = (dev->devnum & 0xFF);  // logical device address (LDA)
                                      // bytes 244-255 must be zero

    /* Copy data characteristics to the I/O buf */
    count = count > 256 ? 256 : count;
    memcpy (iobuf, buf, count);

    return 256;
}


/*-------------------------------------------------------------------*/
/* Build CKD subsystem status                                        */
/*-------------------------------------------------------------------*/
DLL_EXPORT int dasd_build_ckd_subsys_status (DEVBLK *dev, BYTE *iobuf, int count)
{
int  num;
BYTE buf[44];

    /* Build the basic subsystem status data in the I/O area */
    memset (buf, 0, 44);
    buf[1] = dev->devnum & 0xFF;
    buf[2] = DEVICES_PER_SUBSYS - 1;
    store_hw (buf + 38, SSID(dev));
    num = 40;

    /* Build an additional 4 bytes of data for the 3990-6 */
    if (MODEL6(dev->ckdcu)) 
    {
        buf[0] = 0x01;            /* Set 3990-6 enhanced flag */
        num = 44;                   
    } /* end if(3990-6) */

    /* Copy subsystem status to the I/O buf */
    count = count > num ? num : count;
    memcpy (iobuf, buf, count);

    return num;
}


/*-------------------------------------------------------------------*/
/* Build FBA devid field                                             */
/*-------------------------------------------------------------------*/
int dasd_build_fba_devid (FBADEV *fba, BYTE *devid)
{

    memset (devid, 0, 256);

    devid[0] = 0xff;
    devid[1] = (fba->cu >> 8) & 0xff; 
    devid[2] = fba->cu & 0xff;
    devid[3] = 0x01;                  /* assume model is 1 */
    devid[4] = (fba->devt >> 8) & 0xff;
    devid[5] = fba->devt & 0xff;
    devid[6] = fba->model;

    return 7;
}

/*-------------------------------------------------------------------*/
/* Build FBA devchar field                                           */
/*-------------------------------------------------------------------*/
int dasd_build_fba_devchar (FBADEV *fba, BYTE *devchar, int blks)
{

    memset (devchar, 0, 64);

    devchar[0]  = 0x30;                     // operation modes
    devchar[1]  = 0x08;                     // features
    devchar[2]  = fba->class;               // device class
    devchar[3]  = fba->type;                // unit type
    devchar[4]  = (fba->size >> 8) & 0xff;  // block size
    devchar[5]  = fba->size & 0xff;
    devchar[6]  = (fba->bpg >> 24) & 0xff;  // blks per cyclical group
    devchar[7]  = (fba->bpg >> 16) & 0xff;
    devchar[8]  = (fba->bpg >> 8) & 0xff;
    devchar[9]  = fba->bpg & 0xff;
    devchar[10] = (fba->bpp >> 24) & 0xff;  // blks per access position
    devchar[11] = (fba->bpp >> 16) & 0xff;
    devchar[12] = (fba->bpp >> 8) & 0xff;
    devchar[13] = fba->bpp & 0xff;
    devchar[14] = (blks >> 24) & 0xff;      // blks under movable heads
    devchar[15] = (blks >> 16) & 0xff;
    devchar[16] = (blks >> 8) & 0xff;
    devchar[17] = blks & 0xff;
    devchar[18] = 0;                        // blks under fixed heads
    devchar[19] = 0;
    devchar[20] = 0;
    devchar[21] = 0;
    devchar[22] = 0;                        // blks in alternate area
    devchar[23] = 0;
    devchar[24] = 0;                        // blks in CE+SA areas
    devchar[25] = 0;
    devchar[26] = 0;                        // cyclic period in ms
    devchar[27] = 0;
    devchar[28] = 0;                        // min time to change access
    devchar[29] = 0;                        //   position in ms
    devchar[30] = 0;                        // max to change access
    devchar[31] = 0;                        //   position in ms

    return 32;
}
