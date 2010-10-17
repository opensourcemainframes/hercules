/* HERROR.H     (c) Copyright Jan Jaeger, 2010                       */
/*              Hercules Specfic Error codes                         */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$


#ifndef _HERROR_H
#define _HERROR_H

/* Generic codes */
#define HNOERROR       (0)                          /* OK / NO ERROR */
#define HERROR        (-1)                          /* Generic Error */
#define HERRINVCMD    (-32767)                      /* Invalid command  KEEP UNIQUE */ 
/* CPU related error codes */
#define HERRCPUOFF    (-2)                          /* CPU Offline */
#define HERRCPUONL    (-3)                          /* CPU Online */

/* Device related error codes */
#define HERRDEVIDA    (-2)                          /* Invalid Device Address */



#endif /* _HERROR_H */
