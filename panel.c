/* PANEL.C      (c) Copyright Roger Bowler, 1999-2007                */
/*              ESA/390 Control Panel Commands                       */

// $Id$

/*              Modified for New Panel Display =NP=                  */
/*-------------------------------------------------------------------*/
/* This module is the control panel for the ESA/390 emulator.        */
/* It provides a command interface into hercules, and it displays    */
/* messages that are issued by various hercules components.          */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      breakpoint command contributed by Dan Horak                  */
/*      devinit command contributed by Jay Maynard                   */
/*      New Panel Display contributed by Dutch Owen                  */
/*      HMC system console commands contributed by Jan Jaeger        */
/*      Set/reset bad frame indicator command by Jan Jaeger          */
/*      attach/detach/define commands by Jan Jaeger                  */
/*      Panel refresh rate triva by Reed H. Petty                    */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2007      */
/*      64-bit address support by Roger Bowler                       */
/*      Display subchannel command by Nobumichi Kozawa               */
/*      External GUI logic contributed by "Fish" (David B. Trout)    */
/*      Socket Devices originally designed by Malcolm Beattie;       */
/*      actual implementation by "Fish" (David B. Trout).            */
/*-------------------------------------------------------------------*/

// $Log$
// Revision 1.233  2008/08/09 01:44:13  fish
// Put 'msgnum' back for efficiency.
//
// Revision 1.232  2008/08/08 06:25:22  bernard
// Removed all OPTION_MSGHLD stuff
//
// Revision 1.231  2008/08/07 20:47:18  bernard
// Removed variable msgnum. I am getting crazy about this variable.
// It is now less complex to move message arround in the list.
// Still working on the OPTION_MSGHLD
//
// Revision 1.230  2008/08/06 17:03:16  bernard
// message hold scroll_down implemented
//
// Revision 1.229  2008/08/06 15:26:34  bernard
// message hold scroll_up implemented
//
// Revision 1.228  2008/08/06 14:03:29  bernard
// Implemented keep messages when on autoscroll. You will have to delete
// the comment before OPTION_MSGHLD on top to activate. Will work on
// other situations.
//
// Revision 1.227  2008/08/04 19:34:46  fish
// COLOR_DARK_GREY
//
// Revision 1.226  2008/08/03 07:20:06  bernard
// get_color module
//
// Revision 1.225  2008/08/02 09:04:51  bernard
// SCP message colors
//
// Revision 1.224  2008/07/28 15:15:48  bernard
// !scp -> pscp
//
// Revision 1.223  2008/07/24 14:42:36  bernard
// cmdtgt version 2
//
// Revision 1.222  2008/07/12 01:18:07  fish
// .
//
// Revision 1.221  2008/07/12 01:17:40  fish
// no message
//
// Revision 1.220  2008/07/12 01:03:44  fish
// no message
//
// Revision 1.219  2008/07/10 18:38:10  fish
// no message
//
// Revision 1.218  2008/07/10 18:34:55  fish
// Simplifed redesign of panel scrolling logic to support:
// 1) Ctrl+up, Ctrl+down = scroll one line up/down, and
// 2) Ctrl+home, Ctrl+end = top/bottom.
//
// Revision 1.217  2008/07/09 17:35:22  fish
// Revert previous changes until I can resolve
// some remaining issues I forgot about. Sorry.
//
// Revision 1.214  2007/12/29 14:40:51  fish
// fix copyregs function to fallback to using dummyregs whenever regs->hostregs happens to be NULL
//
// Revision 1.213  2007/12/10 23:12:02  gsmith
// Tweaks to OPTION_MIPS_COUNTING processing
//
// Revision 1.212  2007/09/05 00:24:18  gsmith
// Use integer arithmetic calculating cpupct
//
// Revision 1.211  2007/06/23 00:04:15  ivan
// Update copyright notices to include current year (2007)
//
// Revision 1.210  2006/12/20 04:26:20  gsmith
// 19 Dec 2006 ip_all.pat - performance patch - Greg Smith
//
// Revision 1.209  2006/12/08 09:43:29  jj
// Add CVS message log
//

#include "hstdinc.h"

#define _PANEL_C_
#define _HENGINE_DLL_

#include "hercules.h"
#include "devtype.h"
#include "opcode.h"
#include "history.h"
// #include "inline.h"
#include "fillfnam.h"
#include "hconsole.h"

#define  DISPLAY_INSTRUCTION_OPERANDS

#define PANEL_MAX_ROWS 256
#define PANEL_MAX_COLS 256

/*-------------------------------------------------------------------*/
/* Definitions for keyboard input sequences                          */
/*-------------------------------------------------------------------*/

#define xKBD_UP_ARROW           "\x1BOA"
#define xKBD_DOWN_ARROW         "\x1BOB"
#define xKBD_RIGHT_ARROW        "\x1BOC"
#define xKBD_LEFT_ARROW         "\x1BOD"

/*=NP================================================================*/
/* Global data for new panel display                                 */
/*   (Note: all NPD mods are identified by the string =NP=           */
/*===================================================================*/

static int    NPDup = 0;       /* 1 when new panel is up */
static int    NPDinit = 0;     /* 1 when new panel is initialized */
static int    NPhelpup = 0;    /* 1 when displaying help panel */
static int    NPhelppaint = 1; /* 1 when the help panel needs painted */
static int    NPhelpdown = 0;  /* 1 when the help panel is brought down */
static int    NPregdisp = 0;   /* which regs are displayed 0=gpr, 1=cr, 2=ar, 3=fpr */

static int    NPcmd = 0;       /* 1 when command mode for NP is in effect */
static int    NPdataentry = 0; /* 1 when data entry for NP is in progress */
static int    NPdevsel = 0;    /* 1 when device selection is in progress */
static char   NPpending;       /* Command which is pending data entry */
static char   NPentered[256];  /* Data which was entered */
static char   NPprompt1[40];   /* Prompts for left and right bottom of screen */
static char   NPoldprompt1[40];
static char   NPprompt2[40];
static char   NPoldprompt2[40];
static char   NPsel2;          /* Command letter to trigger 2nd phase of dev sel */
static char   NPdevice;        /* Which device selected */
static int    NPasgn;          /* Index to device being reassigned */
static int    NPlastdev;       /* Number of devices */

static char  *NPregnum[]   = {" 0"," 1"," 2"," 3"," 4"," 5"," 6"," 7",
                              " 8"," 9","10","11","12","13","14","15"
                             };
static char  *NPregnum64[] = {"0", "1", "2", "3", "4", "5", "6", "7",
                              "8", "9", "A", "B", "C", "D", "E", "F"
                             };

/* Boolean fields; redraw the corresponding data field if false */
static int    NPcpunum_valid,
              NPcpupct_valid,
              NPpsw_valid,
              NPpswstate_valid,
              NPregs_valid,
              NPaddr_valid,
              NPdata_valid,
              NPmips_valid,
#ifdef OPTION_MIPS_COUNTING
              NPsios_valid,
#endif
              NPdevices_valid,
              NPcpugraph_valid;

/* Current CPU states */
static U16    NPcpunum;
static int    NPcpupct;
static int    NPpswmode;
static int    NPpswzhost;
static QWORD  NPpsw;
static char   NPpswstate[16];
static int    NPregmode;
static int    NPregzhost;
static U64    NPregs64[16];
static U32    NPregs[16];
static U32    NPaddress;
static U32    NPdata;
#ifdef OPTION_MIPS_COUNTING
static U32    NPmips;
static U32    NPsios;
#else
static U64    NPinstcount;
#endif
static int    NPcpugraph;
static int    NPcpugraphpct[MAX_CPU_ENGINES];

/* Current device states */
#define       NP_MAX_DEVICES (PANEL_MAX_ROWS - 3)
static int    NPonline[NP_MAX_DEVICES];
static U16    NPdevnum[NP_MAX_DEVICES];
static int    NPbusy[NP_MAX_DEVICES];
static U16    NPdevtype[NP_MAX_DEVICES];
static int    NPopen[NP_MAX_DEVICES];
static char   NPdevnam[NP_MAX_DEVICES][128];

static short  NPcurrow, NPcurcol;
static int    NPcolorSwitch;
static short  NPcolorFore;
static short  NPcolorBack;
static int    NPdatalen;

static char  *NPhelp[] = {
"All commands consist of one character keypresses.  The various commands are",
"highlighted onscreen by bright white versus the gray of other lettering.",
" ",
"Press the escape key to terminate the control panel and go to command mode.",
" ",
"Display Controls:   G - General purpose regs    C - Control regs",
"                    A - Access registers        F - Floating Point regs",
"                    I - Display main memory at 'ADDRESS'",
"CPU controls:       L - IPL                     S - Start CPU",
"                    E - External interrupt      P - Stop CPU",
"                    W - Exit Hercules           T - Restart interrupt",
"Storage update:     R - Enter ADDRESS to be updated",
"                    D - Enter DATA to be updated at ADDRESS",
"                    O - place DATA value at ADDRESS",
" ",
"Peripherals:        N - enter a new name for the device file assignment",
"                    U - send an I/O attention interrupt",
" ",
"In the display of devices, a green device letter means the device is online,",
"a lighted device address means the device is busy, and a green model number",
"means the attached file is open to the device",
" ",
"                    Press Escape to return to control panel operations",
"" };

////////////////////////////////////////////////////////////////////////

#define MSG_SIZE     PANEL_MAX_COLS      /* Size of one message       */
#define MAX_MSGS     2048                /* Number of slots in buffer */
//efine MAX_MSGS     300                 /* (for testing scrolling)   */
#define NUM_LINES    (cons_rows - 2)     /* Number of scrolling lines */
#define CMD_COLS     (cons_cols - (CMD_PREFIX_LEN + 1));
#define CMD_SIZE     256                 /* cmdline buffer size       */

////////////////////////////////////////////////////////////////////////

static int   cons_rows = 0;             /* console height in lines    */
static int   cons_cols = 0;             /* console width in chars     */
static short cur_cons_row = 0;          /* current console row        */
static short cur_cons_col = 0;          /* current colsole column     */
static char *cons_term = NULL;          /* TERM env value             */

static char  cmdins  = 1;               /* 1==insert mode, 0==overlay */
static char  cmdline[CMD_SIZE+1];       /* Command line buffer        */
static int   cmdlen  = 0;               /* cmdline data len in bytes  */
static int   cmdoff  = 0;               /* cmdline buffer cursor pos  */
static int   cmdcols = 0;               /* visible cmdline width cols */
static int   cmdcol  = 0;               /* cols cmdline scrolled right*/
static FILE *confp   = NULL;            /* Console file pointer       */

#define CMD_PREFIX_STR   "Command ==> "

#ifdef OPTION_CMDTGT
#define CMD_PREFIX_STR1  "SCP ======> " /* Keep same length as above! */
#define CMD_PREFIX_STR2  "PrioSCP ==> " /* Keep same length as above! */
#endif // OPTION_CMDTGT

#define CMD_PREFIX_LEN   strlen(CMD_PREFIX_STR)

#define ADJ_SCREEN_SIZE() \
    do { \
        int rows, cols; \
        get_dim (&rows, &cols); \
        if (rows != cons_rows || cols != cons_cols) { \
            cons_rows = rows; \
            cons_cols = cols; \
            cmdcols = cons_cols - (CMD_PREFIX_LEN + 1); \
            redraw_msgs = redraw_cmd = redraw_status = 1; \
            NPDinit = 0; \
            clr_screen(); \
        } \
    } while (0)

#define ADJ_CMDCOL() /* (called after modifying cmdoff) */ \
    do { \
        if (cmdoff-cmdcol > cmdcols) { /* past right edge of screen */ \
            cmdcol = cmdoff-cmdcols; \
        } else if (cmdoff < cmdcol) { /* past left edge of screen */ \
            cmdcol = cmdoff; \
        } \
    } while (0)

#define PUTC_CMDLINE() \
    do { \
        ASSERT(cmdcol <= cmdlen); \
        for (i=0; cmdcol+i < cmdlen && i < cmdcols; i++) \
            draw_char (cmdline[cmdcol+i]); \
    } while (0)

///////////////////////////////////////////////////////////////////////

typedef struct _PANMSG
{
    struct _PANMSG*     next;
    struct _PANMSG*     prev;
    int                 msgnum;
    char                msg[MSG_SIZE];
#ifdef OPTION_MSGCLR
    unsigned int        keep:1;
    short               fg;
    short               bg;
#endif
}
PANMSG;

static PANMSG*  msgbuf;         /* Circular message buffer */
static PANMSG*  topmsg;         /* message at top of screen */
static PANMSG*  curmsg;         /* newest message */
static int wrapped = 0;         /* wrapped-around flag */

static char *lmsbuf = NULL;
static int   lmsndx = 0;
static int   lmsnum = -1;
static int   lmscnt = -1;
static int   lmsmax = LOG_DEFSIZE/2;
static int   keybfd = -1;               /* Keyboard file descriptor  */

static REGS  copyregs, copysieregs;     /* Copied regs               */

#ifdef OPTION_MSGCLR
/*-------------------------------------------------------------------*/
/* Get the color name from a string                                  */
/*-------------------------------------------------------------------*/

#define CHECKCOLOR(s, cs, c, cc) if(!strncasecmp(s, cs, sizeof(cs) - 1)) { *c = cc; return(sizeof(cs) - 1); }

int get_color(char *string, short *color)
{
       CHECKCOLOR(string, "black",        color, COLOR_BLACK)
  else CHECKCOLOR(string, "cyan",         color, COLOR_CYAN)
  else CHECKCOLOR(string, "blue",         color, COLOR_BLUE)
  else CHECKCOLOR(string, "darkgrey",     color, COLOR_DARK_GREY)
  else CHECKCOLOR(string, "green",        color, COLOR_GREEN)
  else CHECKCOLOR(string, "lightblue",    color, COLOR_LIGHT_BLUE)
  else CHECKCOLOR(string, "lightcyan",    color, COLOR_LIGHT_CYAN)
  else CHECKCOLOR(string, "lightgreen",   color, COLOR_LIGHT_GREEN)
  else CHECKCOLOR(string, "lightgrey",    color, COLOR_LIGHT_GREY)
  else CHECKCOLOR(string, "lightmagenta", color, COLOR_LIGHT_MAGENTA)
  else CHECKCOLOR(string, "lightred",     color, COLOR_LIGHT_RED)
  else CHECKCOLOR(string, "lightyellow",  color, COLOR_LIGHT_YELLOW)
  else CHECKCOLOR(string, "magenta",      color, COLOR_MAGENTA)
  else CHECKCOLOR(string, "red",          color, COLOR_RED)
  else CHECKCOLOR(string, "white",        color, COLOR_WHITE)
  else CHECKCOLOR(string, "yellow",       color, COLOR_YELLOW)
  else return(0);
}

/*-------------------------------------------------------------------*/
/* Read the panel command prefix                                     */
/*                                                                   */
/* Interpret the possible message command. Valid commands:           */
/*   <pnl,cmd,...>                                                   */
/*     Mandatory prefix "<pnl,"                                      */
/*     followed by one or several commands separated by ","          */
/*     ending with a ">"                                             */
/* Valid commands:                                                   */
/*  color(fg, bg)                                                    */
/*  keep                                                             */
/*  release                                                          */
/*-------------------------------------------------------------------*/
void read_cmd(PANMSG *p)
{
  int  i = 0; // index in message
  int  len;

  if(!strncasecmp(p->msg, "<pnl", 4))
  {
    // examine panel command(s)
    i += 4;
    while(p->msg[i] == ',')
    {
      i += 1; // skip ,
      if(!strncasecmp(&p->msg[i], "color(", 6))
      {
        // inspect color command
        i += 6; // skip color(
        len = get_color(&p->msg[i], &p->fg);
        if(!len)
          break; // no valid color found
        i += len; // skip colorname
        if(p->msg[i] != ',')
          break; // no ,
        i++; // skip ,
        len = get_color(&p->msg[i], &p->bg);
        if(!len)
          break; // no valid color found
        i += len; // skip colorname
        if(p->msg[i] != ')')
          break; // no )
        i++; // skip )
      }
      else if(!strncasecmp(&p->msg[i], "keep", 4))
      {
        p->keep = 1;
        i += 4; // skip keep
      }
      else if(!strncasecmp(&p->msg[i], "release", 7))
      {
        p->keep = 0;
        i += 7; // skip release
      }
      else
        break; // rubbish
    }
    if(p->msg[i] == '>')
    {
      // delete panel command from string
      i += 1;
      memmove(p->msg, &p->msg[i], MSG_SIZE - i); // thanks Fish ;-)
      memset(&p->msg[MSG_SIZE - i], SPACE, i);
      return;
    }
  }

  /* rubbish or no panel command */
  p->fg = COLOR_DEFAULT_FG;
  p->bg = COLOR_DEFAULT_BG;
  p->keep = 0;
}
#endif // OPTION_MSGCLR

/*-------------------------------------------------------------------*/
/* Screen manipulation primitives                                    */
/*-------------------------------------------------------------------*/

static PANMSG* oldest_msg()
{
    return (wrapped) ? curmsg->next : msgbuf;
}

static PANMSG* newest_msg()
{
    return curmsg;
}

static int lines_scrolled()
{
    /* return # of lines 'up' from current line that we're scrolled. */
    if (topmsg->msgnum <= curmsg->msgnum)
        return curmsg->msgnum - topmsg->msgnum;
    return MAX_MSGS - (topmsg->msgnum - curmsg->msgnum);
}

static int visible_lines()
{
    return (lines_scrolled() + 1);
}

static int is_currline_visible()
{
    return (visible_lines() <= NUM_LINES);
}

static int lines_remaining()
{
    return (NUM_LINES - visible_lines());
}

static void scroll_up_lines( int numlines )
{
    int i; for (i=0; i < numlines && topmsg != oldest_msg(); topmsg = topmsg->prev, i++);
}

static void scroll_down_lines( int numlines )
{
    int i; for (i=0; i < numlines && topmsg != newest_msg(); topmsg = topmsg->next, i++);
}

static void page_up        () { scroll_up_lines  ( NUM_LINES - 1 ); }
static void page_down      () { scroll_down_lines( NUM_LINES - 1 ); }
static void full_page_up   () { scroll_up_lines  ( NUM_LINES - 0 ); }
static void full_page_down () { scroll_down_lines( NUM_LINES - 0 ); }

static void scroll_to_top_line ()
{
    topmsg = oldest_msg();
}

static void scroll_to_bottom_line ()
{
    topmsg = newest_msg();
}

static void scroll_to_bottom_screen ()
{
    scroll_to_bottom_line();
    page_up();
}

static void do_panel_command( void* cmdline )
{
    if (!is_currline_visible())
        scroll_to_bottom_screen();
    panel_command( cmdline );
}

static void clr_screen ()
{
    clear_screen (confp);
}

static void get_dim (int *y, int *x)
{
    get_console_dim( confp, y, x);
    if (*y > PANEL_MAX_ROWS)
        *y = PANEL_MAX_ROWS;
    if (*x > PANEL_MAX_COLS)
        *x = PANEL_MAX_COLS;
#if defined(WIN32)
    /* If running from a windows or cygwin command prompt we do
     * better with one less row.
     */
    if (!cons_term || strcmp(cons_term, "xterm"))
        (*y)--;
#endif
}

static void set_color (short fg, short bg)
{
    set_screen_color (confp, fg, bg);
}

static void set_pos (short y, short x)
{
    cur_cons_row = y;
    cur_cons_col = x;
    y = y < 1 ? 1 : y > cons_rows ? cons_rows : y;
    x = x < 1 ? 1 : x > cons_cols ? cons_cols : x;
    set_screen_pos (confp, y, x);
}

static void draw_text (char *text)
{
    int   len;
    char *short_text;

    if (cur_cons_row < 1 || cur_cons_row > cons_rows
     || cur_cons_col < 1 || cur_cons_col > cons_cols)
        return;
    len = strlen(text);
    if (cur_cons_col + len <= cons_cols)
        fprintf (confp, "%s", text);
    else
    {
        len = cons_cols - cur_cons_col + 1;
        if ((short_text = strdup(text)) == NULL)
            return;
        short_text[len] = '\0';
        fprintf (confp, "%s", short_text);
        free (short_text);
    }
    cur_cons_col += len;
}

static void write_text (char *text, int size)
{
    if (cur_cons_row < 1 || cur_cons_row > cons_rows
     || cur_cons_col < 1 || cur_cons_col > cons_cols)
        return;
    if (cons_cols - cur_cons_col + 1 < size)
        size = cons_cols - cur_cons_col + 1;
    fwrite (text, size, 1, confp);
    cur_cons_col += size;
}

static void draw_char (int c)
{
    if (cur_cons_row < 1 || cur_cons_row > cons_rows
     || cur_cons_col < 1 || cur_cons_col > cons_cols)
        return;
    fputc (c, confp);
    cur_cons_col++;
}

static void draw_fw (U32 fw)
{
    char buf[9];
    sprintf (buf, "%8.8X", fw);
    draw_text (buf);
}

static void draw_dw (U64 dw)
{
    char buf[17];
    sprintf (buf, "%16.16"I64_FMT"X", dw);
    draw_text (buf);
}

static void fill_text (char c, short x)
{
    char buf[PANEL_MAX_COLS+1];
    int  len;

    if (x > PANEL_MAX_COLS) x = PANEL_MAX_COLS;
    len = x + 1 - cur_cons_col;
    if (len <= 0) return;
    memset (buf, c, len);
    buf[len] = '\0';
    draw_text (buf);
}

static void draw_button (short bg, short fg, short hfg, char *left, char *mid, char *right)
{
    set_color (fg, bg);
    draw_text (left);
    set_color (hfg, bg);
    draw_text (mid);
    set_color (fg, bg);
    draw_text (right);
}

/*=NP================================================================*/
/*  Initialize the NP data                                           */
/*===================================================================*/

static
void NP_init()
{
    NPdataentry = 0;
    strcpy(NPprompt1, "");
    strcpy(NPprompt2, "");
}

/*=NP================================================================*/
/*  This draws the initial screen template                           */
/*===================================================================*/

static void NP_screen_redraw (REGS *regs)
{
    int  i, line;
    char buf[1024];

    /* Force all data to be redrawn */
    NPcpunum_valid   = NPcpupct_valid   = NPpsw_valid  =
    NPpswstate_valid = NPregs_valid     = NPaddr_valid =
    NPdata_valid     = NPmips_valid     = NPsios_valid =
    NPdevices_valid  = NPcpugraph_valid = 0;

#if defined(_FEATURE_SIE)
    if(regs->sie_active)
        regs = regs->guestregs;
#endif /*defined(_FEATURE_SIE)*/

    /*
     * Draw the static parts of the NP screen
     */
    set_color (COLOR_LIGHT_GREY, COLOR_BLACK );
    clr_screen ();

    /* Line 1 - title line */
    set_color (COLOR_WHITE, COLOR_BLUE );
    set_pos   (1, 1);
    draw_text ("  Hercules  CPU    :    %");
    fill_text (' ', 30);
    draw_text ((char *)get_arch_mode_string(NULL));
    fill_text (' ', 38);
    set_color (COLOR_LIGHT_GREY, COLOR_BLUE );
    draw_text ("| ");
    set_color (COLOR_WHITE, COLOR_BLUE );

    /* Center "Peripherals" on the right-hand-side */
    if (cons_cols > 52)
        fill_text (' ', 40 + (cons_cols - 52) / 2);
    draw_text ("Peripherals");
    fill_text (' ', (short)cons_cols);

    /* Line 2 - peripheral headings */
    set_pos (2, 41);
    set_color (COLOR_WHITE, COLOR_BLACK);
    draw_char ('U');
    set_color (COLOR_LIGHT_GREY, COLOR_BLACK);
    draw_text(" Addr Modl Type Assig");
    set_color (COLOR_WHITE, COLOR_BLACK);
    draw_char ('n');
    set_color (COLOR_LIGHT_GREY, COLOR_BLACK);
    draw_text("ment");

    /* 4th line - PSW */
    NPpswmode = (regs->arch_mode == ARCH_900);
    NPpswzhost =
#if defined(_FEATURE_SIE)
                 !NPpswmode && SIE_MODE(regs) && regs->hostregs->arch_mode == ARCH_900;
#else
                 0;
#endif /*defined(_FEATURE_SIE)*/
    set_pos (4, NPpswmode || NPpswzhost ? 19 : 10);
    draw_text ("PSW");

    /* Lines 6 .. 13 : register area */
    set_color (COLOR_LIGHT_GREY, COLOR_BLACK);
    NPregmode = (regs->arch_mode == ARCH_900 && (NPregdisp == 0 || NPregdisp == 1));
    NPregzhost =
#if defined(_FEATURE_SIE)
                 (regs->arch_mode != ARCH_900
               && SIE_MODE(regs) && regs->hostregs->arch_mode == ARCH_900
               && (NPregdisp == 0 || NPregdisp == 1));
#else
                 0;
#endif /*defined(_FEATURE_SIE)*/
    if (NPregmode == 1 || NPregzhost)
    {
        for (i = 0; i < 8; i++)
        {
            set_pos (i+6, 1);
            draw_text (NPregnum64[i*2]);
            set_pos (i+6, 20);
            draw_text (NPregnum64[i*2+1]);
        }
    }
    else
    {
        for (i = 0; i < 4; i++)
        {
            set_pos (i*2+7,9);
            draw_text (NPregnum[i*4]);
            set_pos (i*2+7,18);
            draw_text (NPregnum[i*4+1]);
            set_pos (i*2+7,27);
            draw_text (NPregnum[i*4+2]);
            set_pos (i*2+7,36);
            draw_text (NPregnum[i*4+3]);
        }
    }

    /* Line 14 : register selection */
    set_color (COLOR_LIGHT_GREY, COLOR_BLACK);
    set_pos (14, 6);
    draw_text ("GPR");
    set_pos (14, 14);
    draw_text ("CR");
    set_pos (14, 22);
    draw_text ("AR");
    set_pos (14, 30);
    draw_text ("FPR");

    /* Line 16 .. 17 : Address and data */
    set_pos (16, 2);
    draw_text ("ADD");
    set_color (COLOR_WHITE, COLOR_BLACK);
    draw_char ('R');
    set_color (COLOR_LIGHT_GREY, COLOR_BLACK);
    draw_text ("ESS:");
    set_pos (16, 22);
    set_color (COLOR_WHITE, COLOR_BLACK);
    draw_char ('D');
    set_color (COLOR_LIGHT_GREY, COLOR_BLACK);
    draw_text ("ATA:");

    /* Line 18 : separator */
    set_pos (18, 1);
    fill_text ('-', 38);

    /* Lines 19 .. 23 : buttons */

    set_pos (19, 16);
    draw_button(COLOR_BLUE,  COLOR_LIGHT_GREY, COLOR_WHITE,  " ST", "O", " "  );
    set_pos (19, 24);
    draw_button(COLOR_BLUE,  COLOR_LIGHT_GREY, COLOR_WHITE,  " D",  "I", "S " );
    set_pos (19, 32);
    draw_button(COLOR_BLUE,  COLOR_LIGHT_GREY, COLOR_WHITE,  " RS", "T", " "  );

    set_pos (20, 3);
    set_color (COLOR_LIGHT_GREY, COLOR_BLACK);
    draw_text ("MIPS");
    set_pos (20, 9);
    draw_text ("SIO/s");

    set_pos (22, 2);
    draw_button(COLOR_GREEN, COLOR_LIGHT_GREY, COLOR_WHITE,  " ",   "S", "TR ");
    set_pos (22, 9);
    draw_button(COLOR_RED,   COLOR_LIGHT_GREY, COLOR_WHITE,  " ST", "P", " "  );
    set_pos (22, 16);
    draw_button(COLOR_BLUE,  COLOR_LIGHT_GREY, COLOR_WHITE,  " ",   "E", "XT ");
    set_pos (22, 24);
    draw_button(COLOR_BLUE,  COLOR_LIGHT_GREY, COLOR_WHITE,  " IP", "L", " "  );
    set_pos (22, 32);
    draw_button(COLOR_RED,   COLOR_LIGHT_GREY, COLOR_WHITE,  " P",  "W", "R " );

    set_color (COLOR_LIGHT_GREY, COLOR_BLACK);

    /* CPU busy graph if we have enough rows */
    line = 24;
    if (cons_rows - line >= sysblk.numcpu + 2)
    {
        NPcpugraph = 1;
        NPcpugraph_valid = 0;
        set_pos (line++, 1);
        fill_text ('-', 38);
        set_pos (line++, 1);
        draw_text ("CPU");
        for (i = 0; i < HI_CPU; i++)
        {
            sprintf (buf, "%2d  ", i);
            set_pos (line++, 1);
            draw_text (buf);
        }
    }
    else
        NPcpugraph = 0;

    /* Vertical separators */
    for (i = 2; i <= cons_rows; i++)
    {
        set_pos (i , 39);
        draw_char ('|');
    }

    /* Last line : horizontal separator */
    if (cons_rows >= 24)
    {
        set_pos (cons_rows, 1);
        fill_text ('-', 38);
        draw_char ('|');
        fill_text ('-', cons_cols);
    }

    /* positions the cursor */
    set_pos (cons_rows, cons_cols);
}

/*=NP================================================================*/
/*  This refreshes the screen with new data every cycle              */
/*===================================================================*/

static
void NP_update(REGS *regs)
{
    int     i, n;
    int     mode, zhost;
    QWORD   curpsw;
    U32     addr, aaddr;
    DEVBLK *dev;
    int     online, busy, open;
    char   *devclass;
    char    devnam[128];
    char    buf[1024];

    if (NPhelpup == 1)
    {
        if (NPhelpdown == 1)
        {
             NP_init();
             NP_screen_redraw(regs);
             NPhelpup = 0;
             NPhelpdown = 0;
             NPhelppaint = 1;
        }
        else
        {
            if (NPhelppaint)
            {
                set_color (COLOR_LIGHT_GREY, COLOR_BLACK);
                clr_screen ();
                for (i = 0; strcmp(NPhelp[i], ""); i++)
                {
                    set_pos (i+1, 1);
                    draw_text (NPhelp[i]);
                }
                NPhelppaint = 0;
            }
            return;
        }
    }

#if defined(_FEATURE_SIE)
    if(SIE_MODE(regs))
        regs = regs->hostregs;
#endif /*defined(_FEATURE_SIE)*/

    /* line 1 : cpu number and percent busy */
    if (!NPcpunum_valid || NPcpunum != regs->cpuad)
    {
        set_color (COLOR_WHITE, COLOR_BLUE);
        set_pos (1, 16);
        sprintf (buf, "%4.4X:",regs->cpuad);
        draw_text (buf);
        NPcpunum_valid = 1;
        NPcpunum = regs->cpuad;
    }

#if defined(OPTION_MIPS_COUNTING)
    if (!NPcpupct_valid || NPcpupct != regs->cpupct)
    {
        set_color (COLOR_WHITE, COLOR_BLUE);
        set_pos (1, 22);
        sprintf(buf, "%3d", regs->cpupct);
        draw_text (buf);
        NPcpupct_valid = 1;
        NPcpupct = regs->cpupct;
    }
#else
    if (!NPcpupct_valid)
    {
        set_color (COLOR_WHITE, COLOR_BLUE);
        set_pos (1, 21);
        draw_text ("     ");
        NPcpupct_valid = 1;
    }
#endif /*defined(OPTION_MIPS_COUNTING)*/

#if defined(_FEATURE_SIE)
    if(regs->sie_active)
        regs = regs->guestregs;
#endif /*defined(_FEATURE_SIE)*/

    mode = (regs->arch_mode == ARCH_900);
    zhost =
#if defined(_FEATURE_SIE)
            !mode && SIE_MODE(regs) && regs->hostregs->arch_mode == ARCH_900;
#else
            0;
#endif

    /* Redraw the psw template if the mode changed */
    if (NPpswmode != mode || NPpswzhost != zhost)
    {
        NPpswmode = mode;
        NPpswzhost = zhost;
        NPpsw_valid = NPpswstate_valid = 0;
        set_color (COLOR_LIGHT_GREY, COLOR_BLACK);
        set_pos (3, 1);
        fill_text (' ',38);
        set_pos (4, 1);
        fill_text (' ', 38);
        set_pos (4, NPpswmode || NPpswzhost ? 19 : 10);
        draw_text ("PSW");
    }

    /* Display the psw */
    memset (curpsw, 0, sizeof(QWORD));
    copy_psw (regs, curpsw);
    if (!NPpsw_valid || memcmp(NPpsw, curpsw, sizeof(QWORD)))
    {
        set_color (COLOR_LIGHT_YELLOW, COLOR_BLACK);
        set_pos (3, 3);
        if (mode)
        {
            draw_dw (fetch_dw(curpsw));
            set_pos (3, 22);
            draw_dw (fetch_dw(curpsw+8));
        }
        else if (zhost)
        {
            draw_fw (fetch_fw(curpsw));
//          draw_fw (0);
            draw_fw (fetch_fw(curpsw+4)); /* *JJ */
            set_pos (3, 22);
//          draw_fw (fetch_fw(curpsw+4) & 0x80000000 ? 0x80000000 : 0);
//          draw_fw (fetch_fw(curpsw+4) & 0x7fffffff);
            draw_text("----------------"); /* *JJ */
        }
        else
        {
            draw_fw (fetch_fw(curpsw));
            set_pos (3, 12);
            draw_fw (fetch_fw(curpsw+4));
        }
        NPpsw_valid = 1;
        memcpy (NPpsw, curpsw, sizeof(QWORD));
    }

    /* Display psw state */
    sprintf (buf, "%2d%c%c%c%c%c%c%c%c",
                  regs->psw.amode64                  ? 64  : 
                  regs->psw.amode                    ? 31  : 24, 
                  regs->cpustate == CPUSTATE_STOPPED ? 'M' : '.',
                  sysblk.inststep                    ? 'T' : '.',
                  WAITSTATE (&regs->psw)             ? 'W' : '.',
                  regs->loadstate                    ? 'L' : '.',
                  regs->checkstop                    ? 'C' : '.',
                  PROBSTATE(&regs->psw)              ? 'P' : '.',
                  SIE_MODE(regs)                     ? 'S' : '.',
                  mode                               ? 'Z' : '.');
    if (!NPpswstate_valid || strcmp(NPpswstate, buf))
    {
        set_color (COLOR_LIGHT_YELLOW, COLOR_BLACK );
        set_pos (mode || zhost ? 4 : 3, 28);
        draw_text (buf);
        NPpswstate_valid = 1;
        strcpy (NPpswstate, buf);
    }

    /* Redraw the register template if the regmode switched */
    mode = (regs->arch_mode == ARCH_900 && (NPregdisp == 0 || NPregdisp == 1));
    zhost =
#if defined(_FEATURE_SIE)
            (regs->arch_mode != ARCH_900
          && SIE_MODE(regs) && regs->hostregs->arch_mode == ARCH_900
          && (NPregdisp == 0 || NPregdisp == 1));
#else
                 0;
#endif /*defined(_FEATURE_SIE)*/
    if (NPregmode != mode || NPregzhost != zhost)
    {
        NPregmode = mode;
        NPregzhost = zhost;
        NPregs_valid = 0;
        set_color (COLOR_LIGHT_GREY, COLOR_BLACK);
        if (NPregmode || NPregzhost)
        {
            /* 64 bit registers */
            for (i = 0; i < 8; i++)
            {
                set_pos (i+6, 1);
                fill_text (' ', 38);
                set_pos (i+6, 1);
                draw_text (NPregnum64[i*2]);
                set_pos (i+6, 20);
                draw_text (NPregnum64[i*2+1]);
            }
        }
        else
        {
            /* 32 bit registers */
            for (i = 0; i < 4; i++)
            {
                set_pos (i*2+6,1);
                fill_text (' ', 38);
                set_pos (i*2+7,1);
                fill_text (' ', 38);
                set_pos (i*2+7,9);
                draw_text (NPregnum[i*4]);
                set_pos (i*2+7,18);
                draw_text (NPregnum[i*4+1]);
                set_pos (i*2+7,27);
                draw_text (NPregnum[i*4+2]);
                set_pos (i*2+7,36);
                draw_text (NPregnum[i*4+3]);
            }
        }
    }

    /* Display register values */
    set_color (COLOR_LIGHT_YELLOW, COLOR_BLACK );
    if (NPregmode)
    {
        /* 64 bit registers */
        for (i = 0; i < 16; i++)
        {
            switch (NPregdisp) {
            case 0:
                if (!NPregs_valid || NPregs64[i] != regs->GR_G(i))
                {
                    set_pos (6 + i/2, 3 + (i%2)*19);
                    draw_dw (regs->GR_G(i));
                    NPregs64[i] = regs->GR_G(i);
                }
                break;
            case 1:
                if (!NPregs_valid || NPregs64[i] != regs->CR_G(i))
                {
                    set_pos (6 + i/2, 3 + (i%2)*19);
                    draw_dw (regs->CR_G(i));
                    NPregs64[i] = regs->CR_G(i);
                }
                break;
            }
        }
    }
    else if (NPregzhost)
    {
        /* 32 bit registers on 64 bit template */
        for (i = 0; i < 16; i++)
        {
            switch (NPregdisp) {
            case 0:
                if (!NPregs_valid || NPregs[i] != regs->GR_L(i))
                {
                    set_pos (6 + i/2, 3 + (i%2)*19);
//                  draw_fw (0);
                    draw_text("--------");
                    draw_fw (regs->GR_L(i));
                    NPregs[i] = regs->GR_L(i);
                }
                break;
            case 1:
                if (!NPregs_valid || NPregs[i] != regs->CR_L(i))
                {
                    set_pos (6 + i/2, 3 + (i%2)*19);
//                  draw_fw (0);
                    draw_text("--------");
                    draw_fw (regs->CR_L(i));
                    NPregs[i] = regs->CR_L(i);
                }
                break;
            }
        }
    }
    else
    {
        /* 32 bit registers */
        addr = NPaddress;
        for (i = 0; i < 16; i++)
        {
            switch (NPregdisp) {
            default:
            case 0:
                if (!NPregs_valid || NPregs[i] != regs->GR_L(i))
                {
                    set_pos (6 + (i/4)*2, 3 + (i%4)*9);
                    draw_fw (regs->GR_L(i));
                    NPregs[i] = regs->GR_L(i);
                }
                break;
            case 1:
                if (!NPregs_valid || NPregs[i] != regs->CR_L(i))
                {
                    set_pos (6 + (i/4)*2, 3 + (i%4)*9);
                    draw_fw (regs->CR_L(i));
                    NPregs[i] = regs->CR_L(i);
                }
                break;
            case 2:
                if (!NPregs_valid || NPregs[i] != regs->AR(i))
                {
                    set_pos (6 + (i/4)*2, 3 + (i%4)*9);
                    draw_fw (regs->AR(i));
                    NPregs[i] = regs->AR(i);
                }
                break;
            case 3:
                if (!NPregs_valid || NPregs[i] != regs->fpr[i])
                {
                    set_pos (6 + (i/4)*2, 3 + (i%4)*9);
                    draw_fw (regs->fpr[i]);
                    NPregs[i] = regs->fpr[i];
                }
                break;
            case 4:
                aaddr = APPLY_PREFIXING (addr, regs->PX);
                addr += 4;
                if (aaddr + 3 > regs->mainlim)
                    break;
                if (!NPregs_valid || NPregs[i] != fetch_fw(regs->mainstor + aaddr))
                {
                    set_pos (6 + (i/4)*2, 3 + (i%4)*9);
                    draw_fw (fetch_fw(regs->mainstor + aaddr));
                    NPregs[i] = fetch_fw(regs->mainstor + aaddr);
                }
                break;
            }
        }
    }

    /* Update register selection indicator */
    if (!NPregs_valid)
    {
        set_pos (14, 6);
        set_color (NPregdisp == 0 ? COLOR_LIGHT_YELLOW : COLOR_WHITE, COLOR_BLACK);
        draw_char ('G');

        set_pos (14, 14);
        set_color (NPregdisp == 1 ? COLOR_LIGHT_YELLOW : COLOR_WHITE, COLOR_BLACK);
        draw_char ('C');

        set_pos (14, 22);
        set_color (NPregdisp == 2 ? COLOR_LIGHT_YELLOW : COLOR_WHITE, COLOR_BLACK);
        draw_char ('A');

        set_pos (14, 30);
        set_color (NPregdisp == 3 ? COLOR_LIGHT_YELLOW : COLOR_WHITE, COLOR_BLACK);
        draw_char ('F');
    }

    NPregs_valid = 1;

    /* Address & Data */
    if (!NPaddr_valid)
    {
        set_color (COLOR_LIGHT_YELLOW, COLOR_BLACK);
        set_pos (16, 12);
        draw_fw (NPaddress);
        NPaddr_valid = 1;
    }
    if (!NPdata_valid)
    {
        set_color (COLOR_LIGHT_YELLOW, COLOR_BLACK);
        set_pos (16, 30);
        draw_fw (NPdata);
        NPdata_valid = 1;
    }

    /* Rates */
#ifdef OPTION_MIPS_COUNTING
    if (!NPmips_valid || sysblk.mipsrate != NPmips)
    {
        set_color (COLOR_LIGHT_YELLOW, COLOR_BLACK);
        set_pos (19, 1);
        sprintf(buf, "%3.1d.%2.2d",
            sysblk.mipsrate / 1000000, (sysblk.mipsrate % 1000000) / 10000);
        draw_text (buf);
        NPmips = sysblk.mipsrate;
        NPmips_valid = 1;
    }
    if (!NPsios_valid || NPsios != sysblk.siosrate)
    {
        set_color (COLOR_LIGHT_YELLOW, COLOR_BLACK);
        set_pos (19, 7);
        sprintf(buf, "%7d", sysblk.siosrate);
        draw_text (buf);
        NPsios = sysblk.siosrate;
        NPsios_valid = 1;
    }
#else
    instcount = INSTCOUNT(regs);
    if (!NPmips_valid || NPinstcount != instcount)
        set_color (COLOR_LIGHT_YELLOW, COLOR_BLACK);
        set_pos (19, 1);
        sprintf(buf, "%12.12u", instcount);
        draw_text (buf);
        NPinstcount = instcount;
        NPmips_valid = 1;
    }
#endif /* OPTION_MIPS_COUNTING */

    /* Optional cpu graph */
    if (NPcpugraph)
    {
        for (i = 0; i < HI_CPU; i++)
        {
            if (!IS_CPU_ONLINE(i))
            {
                if (!NPcpugraph_valid || NPcpugraphpct[i] != -2.0)
                {
                    set_color (COLOR_RED, COLOR_BLACK);
                    set_pos (26+i, 4);
                    draw_text ("OFFLINE");
                    fill_text (' ', 38);
                    NPcpugraphpct[i] = -2.0;
                }
            }
            else if (sysblk.regs[i]->cpustate != CPUSTATE_STARTED)
            {
                if (!NPcpugraph_valid || NPcpugraphpct[i] != -1.0)
                {
                    set_color (COLOR_LIGHT_YELLOW, COLOR_BLACK);
                    set_pos (26+i, 4);
                    draw_text ("STOPPED");
                    fill_text (' ', 38);
                    NPcpugraphpct[i] = -1.0;
                }
            }
            else if (!NPcpugraph_valid || NPcpugraphpct[i] != sysblk.regs[i]->cpupct)
            {
                n = (34 * sysblk.regs[i]->cpupct) / 100;
                if (n == 0 && sysblk.regs[i]->cpupct > 0)
                    n = 1;
                else if (n > 34)
                    n = 34;
                set_color (n > 17 ? COLOR_WHITE : COLOR_LIGHT_GREY, COLOR_BLACK);
                set_pos (26+i, 4);
                fill_text ('*', n+3);
                fill_text (' ', 38);
                NPcpugraphpct[i] = sysblk.regs[i]->cpupct;
            }
            set_color (COLOR_LIGHT_GREY, COLOR_BLACK);
        }
        NPcpugraph_valid = 1;
    }

    /* Process devices */
    for (i = 0, dev = sysblk.firstdev; dev != NULL; i++, dev = dev->nextdev)
    {
        if (i >= cons_rows - 3) break;
        if (!(dev->pmcw.flag5 & PMCW5_V)) continue;

        online = (dev->console && dev->connected) || strlen(dev->filename) > 0;
        busy   = dev->busy != 0 || IOPENDING(dev) != 0;
        open   = dev->fd > 2;

        /* device identifier */
        if (!NPdevices_valid || online != NPonline[i])
        {
            set_pos (i+3, 41);
            set_color (online ? COLOR_LIGHT_GREEN : COLOR_LIGHT_GREY, COLOR_BLACK);
            draw_char (i < 26 ? 'A' + i : '.');
            NPonline[i] = online;
        }

        /* device number */
        if (!NPdevices_valid || dev->devnum != NPdevnum[i] || NPbusy[i] != busy)
        {
            set_pos (i+3, 43);
            set_color (busy ? COLOR_LIGHT_YELLOW : COLOR_LIGHT_GREY, COLOR_BLACK);
            sprintf (buf, "%4.4X", dev->devnum);
            draw_text (buf);
            NPdevnum[i] = dev->devnum;
            NPbusy[i] = busy;
        }

        /* device type */
        if (!NPdevices_valid || dev->devtype != NPdevtype[i] || open != NPopen[i])
        {
            set_pos (i+3, 48);
            set_color (open ? COLOR_LIGHT_GREEN : COLOR_LIGHT_GREY, COLOR_BLACK);
            sprintf (buf, "%4.4X", dev->devtype);
            draw_text (buf);
            NPdevtype[i] = dev->devtype;
            NPopen[i] = open;
        }

        /* device class and name */
        (dev->hnd->query)(dev, &devclass, sizeof(devnam), devnam);
        if (!NPdevices_valid || strcmp(NPdevnam[i], devnam))
        {
            set_color (COLOR_LIGHT_GREY, COLOR_BLACK);
            set_pos (i+3, 53);
            sprintf (buf, "%-4.4s", devclass);
            draw_text (buf);
            set_pos (i+3, 58);
            draw_text (devnam);
            fill_text (' ', PANEL_MAX_COLS);
        }
    }

    /* Complete the device state table */
    if (!NPdevices_valid)
    {
        NPlastdev = i > 26 ? 26 : i - 1;
        for ( ; i < NP_MAX_DEVICES; i++)
        {
            NPonline[i] = NPdevnum[i] = NPbusy[i] = NPdevtype[i] = NPopen[i] = 0;
            strcpy (NPdevnam[i], "");
        }
        NPdevices_valid = 1;
    }

    /* Prompt 1 */
    if (strcmp(NPprompt1, NPoldprompt1))
    {
        strcpy(NPoldprompt1, NPprompt1);
        if (strlen(NPprompt1) > 0)
        {
            set_color (COLOR_WHITE, COLOR_BLUE);
            set_pos (cons_rows, (40 - strlen(NPprompt1)) / 2);
            draw_text (NPprompt1);
        }
        else if (cons_rows >= 24)
        {
            set_color (COLOR_LIGHT_GREY, COLOR_BLACK);
            set_pos (cons_rows, 1);
            fill_text ('-', 38);
        }
    }

    /* Prompt 2 */
    if (strcmp(NPprompt2, NPoldprompt2))
    {
        strcpy(NPoldprompt2, NPprompt2);
        if (strlen(NPprompt2) > 0)
        {
            set_color (COLOR_WHITE, COLOR_BLUE);
            set_pos (cons_rows, 41);
            draw_text (NPprompt2);
        }
        else if (cons_rows >= 24)
        {
            set_color (COLOR_LIGHT_GREY, COLOR_BLACK);
            set_pos (cons_rows, 41);
            fill_text ('-', cons_cols);
        }
    }

    /* Data entry field */
    if (NPdataentry)
    {
        set_pos (NPcurrow, NPcurcol);
        if (NPcolorSwitch)
            set_color (NPcolorFore, NPcolorBack);
        fill_text (' ', NPcurcol + NPdatalen - 1);
        set_pos (NPcurrow, NPcurcol);
        PUTC_CMDLINE();
    }
    else
        /* Position the cursor to the bottom right */
        set_pos(cons_rows, cons_cols);
}

/* ==============   End of the main NP block of code    =============*/

static void panel_cleanup(void *unused);    // (forward reference)

#ifdef OPTION_MIPS_COUNTING

///////////////////////////////////////////////////////////////////////
// "maxrates" command support...

#define  DEF_MAXRATES_RPT_INTVL   (  1440  )

DLL_EXPORT U32    curr_high_mips_rate = 0;  // (high water mark for current interval)
DLL_EXPORT U32    curr_high_sios_rate = 0;  // (high water mark for current interval)

DLL_EXPORT U32    prev_high_mips_rate = 0;  // (saved high water mark for previous interval)
DLL_EXPORT U32    prev_high_sios_rate = 0;  // (saved high water mark for previous interval)

DLL_EXPORT time_t curr_int_start_time = 0;  // (start time of current interval)
DLL_EXPORT time_t prev_int_start_time = 0;  // (start time of previous interval)

DLL_EXPORT U32    maxrates_rpt_intvl  = DEF_MAXRATES_RPT_INTVL;

DLL_EXPORT void update_maxrates_hwm()       // (update high-water-mark values)
{
    time_t  current_time = 0;
    U32     elapsed_secs = 0;

    if (curr_high_mips_rate < sysblk.mipsrate)
        curr_high_mips_rate = sysblk.mipsrate;

    if (curr_high_sios_rate < sysblk.siosrate)
        curr_high_sios_rate = sysblk.siosrate;

    // Save high water marks for current interval...

    time( &current_time );

    elapsed_secs = current_time - curr_int_start_time;

    if ( elapsed_secs >= ( maxrates_rpt_intvl * 60 ) )
    {
        prev_high_mips_rate = curr_high_mips_rate;
        prev_high_sios_rate = curr_high_sios_rate;

        curr_high_mips_rate = 0;
        curr_high_sios_rate = 0;

        prev_int_start_time = curr_int_start_time;
        curr_int_start_time = current_time;
    }
}
#endif // OPTION_MIPS_COUNTING
///////////////////////////////////////////////////////////////////////

REGS *copy_regs(int cpu)
{
    REGS *regs;

    if (cpu < 0 || cpu >= MAX_CPU_ENGINES)
        cpu = 0;

    obtain_lock (&sysblk.cpulock[cpu]);

    if ((regs = sysblk.regs[cpu]) == NULL)
    {
        release_lock(&sysblk.cpulock[cpu]);
        return &sysblk.dummyregs;
    }

    memcpy (&copyregs, regs, sysblk.regs_copy_len);

    if (copyregs.hostregs == NULL)
    {
        release_lock(&sysblk.cpulock[cpu]);
        return &sysblk.dummyregs;
    }

#if defined(_FEATURE_SIE)
    if (regs->sie_active)
    {
        memcpy (&copysieregs, regs->guestregs, sysblk.regs_copy_len);
        copyregs.guestregs = &copysieregs;
        copysieregs.hostregs = &copyregs;
        regs = &copysieregs;
    }
    else
#endif
        regs = &copyregs;

    SET_PSW_IA(regs);

    release_lock(&sysblk.cpulock[cpu]);
    return regs;
}

/*-------------------------------------------------------------------*/
/* Panel display thread                                              */
/*                                                                   */
/* This function runs on the main thread.  It receives messages      */
/* from the log task and displays them on the screen.  It accepts    */
/* panel commands from the keyboard and executes them.  It samples   */
/* the PSW periodically and displays it on the screen status line.   */
/*-------------------------------------------------------------------*/

#if defined(OPTION_DYNAMIC_LOAD)
void panel_display_r (void)
#else
void panel_display (void)
#endif
{
#ifndef _MSVC_
  int     rc;                           /* Return code               */
  int     maxfd;                        /* Highest file descriptor   */
  fd_set  readset;                      /* Select file descriptors   */
  struct  timeval tv;                   /* Select timeout structure  */
#endif
int     i;                              /* Array subscripts          */
int     len;                            /* Length                    */
REGS   *regs;                           /* -> CPU register context   */
QWORD   curpsw;                         /* Current PSW               */
QWORD   prvpsw;                         /* Previous PSW              */
BYTE    prvstate = 0xFF;                /* Previous stopped state    */
U64     prvicount = 0;                  /* Previous instruction count*/
U64     prvtcount = 0;                  /* Previous total count      */
int     prvcpupct = 0;                  /* Previous cpu percentage   */
#if defined(OPTION_SHARED_DEVICES)
U32     prvscount = 0;                  /* Previous shrdcount        */
#endif
int     redraw_msgs;                    /* 1=Redraw message area     */
int     redraw_cmd;                     /* 1=Redraw command line     */
int     redraw_status;                  /* 1=Redraw status line      */
char    readbuf[MSG_SIZE];              /* Message read buffer       */
int     readoff = 0;                    /* Number of bytes in readbuf*/
BYTE    c;                              /* Character work area       */
size_t  kbbufsize = CMD_SIZE;           /* Size of keyboard buffer   */
char   *kbbuf = NULL;                   /* Keyboard input buffer     */
int     kblen;                          /* Number of chars in kbbuf  */
U32     aaddr;                          /* Absolute address for STO  */
char    buf[1024];                      /* Buffer workarea           */

    SET_THREAD_NAME("panel_display");

    /* Display thread started message on control panel */
    logmsg (_("HHCPN001I Control panel thread started: "
            "tid="TIDPAT", pid=%d\n"),
            thread_id(), getpid());

    /* Notify logger_thread we're in control */
    sysblk.panel_init = 1;

    hdl_adsc("panel_cleanup",panel_cleanup, NULL);
    history_init();

    /* Set up the input file descriptors */
    confp = stderr;
    keybfd = STDIN_FILENO;

    /* Initialize screen dimensions */
    cons_term = getenv ("TERM");
    get_dim (&cons_rows, &cons_cols);

    /* Clear the command-line buffer */
    memset (cmdline, 0, sizeof(cmdline));
    cmdcols = cons_cols - (CMD_PREFIX_LEN + 1);

    /* Obtain storage for the keyboard buffer */
    if (!(kbbuf = malloc (kbbufsize)))
    {
        logmsg(_("HHCPN002S Cannot obtain keyboard buffer: %s\n"),
                strerror(errno));
        return;
    }

    /* Obtain storage for the circular message buffer */
    msgbuf = malloc (MAX_MSGS * sizeof(PANMSG));
    if (msgbuf == NULL)
    {
        fprintf (stderr,
                _("HHCPN003S Cannot obtain message buffer: %s\n"),
                strerror(errno));
        return;
    }

    /* Initialize circular message buffer */
    for (curmsg = msgbuf, i=0; i < MAX_MSGS; curmsg++, i++)
    {
        curmsg->next = curmsg + 1;
        curmsg->prev = curmsg - 1;
        curmsg->msgnum = i;
        memset(curmsg->msg,SPACE,MSG_SIZE);
    }

    /* Complete the circle */
    msgbuf->prev = msgbuf + MAX_MSGS - 1;
    msgbuf->prev->next = msgbuf;

    /* Indicate "first-time" state */
    curmsg = topmsg = NULL;
    wrapped = 0;

    /* Set screen output stream to NON-buffered */
    setvbuf (confp, NULL, _IONBF, 0);

    /* Put the terminal into cbreak mode */
    set_or_reset_console_mode( keybfd, 1 );

    /* Set console title */
#if defined( _MSVC_ )
    if (sysblk.pantitle)
        w32_set_console_title(sysblk.pantitle);
#endif /* defined( _MSVC_ ) */

    /* Clear the screen */
    set_color (COLOR_DEFAULT_FG, COLOR_DEFAULT_BG);
    clr_screen ();
    redraw_msgs = redraw_cmd = redraw_status = 1;

    /* Process messages and commands */
    while ( 1 )
    {
#if defined(OPTION_MIPS_COUNTING)
        update_maxrates_hwm(); // (update high-water-mark values)
#endif // defined(OPTION_MIPS_COUNTING)

#if defined( _MSVC_ )
        /* Wait for keyboard input */
#define WAIT_FOR_KEYBOARD_INPUT_SLEEP_MILLISECS  (20)
        for (i=sysblk.panrate/WAIT_FOR_KEYBOARD_INPUT_SLEEP_MILLISECS; i && !kbhit(); i--)
            Sleep(WAIT_FOR_KEYBOARD_INPUT_SLEEP_MILLISECS);

        ADJ_SCREEN_SIZE();

        /* If keyboard input has [finally] arrived, then process it */
        if ( kbhit() )
        {
            /* Read character(s) from the keyboard */
            kbbuf[0] = getch();
            kbbuf[kblen=1] = '\0';
            translate_keystroke( kbbuf, &kblen );

#else // !defined( _MSVC_ )

        /* Set the file descriptors for select */
        FD_ZERO (&readset);
        FD_SET (keybfd, &readset);
        maxfd = keybfd;

        /* Wait for a message to arrive, a key to be pressed,
           or the inactivity interval to expire */
        tv.tv_sec = sysblk.panrate / 1000;
        tv.tv_usec = (sysblk.panrate * 1000) % 1000000;
        rc = select (maxfd + 1, &readset, NULL, NULL, &tv);
        if (rc < 0 )
        {
            if (errno == EINTR) continue;
            fprintf (stderr,
                    _("HHCPN004E select: %s\n"),
                    strerror(errno));
            break;
        }

        ADJ_SCREEN_SIZE();

        /* If keyboard input has arrived then process it */
        if (FD_ISSET(keybfd, &readset))
        {
            /* Read character(s) from the keyboard */
            kblen = read (keybfd, kbbuf, kbbufsize-1);

            if (kblen < 0)
            {
                fprintf (stderr,
                        _("HHCPN005E keyboard read: %s\n"),
                        strerror(errno));
                break;
            }

            kbbuf[kblen] = '\0';

#endif // defined( _MSVC_ )

            /* =NP= : Intercept NP commands & process */
            if (NPDup == 1)
            {
                if (NPdevsel == 1)
                {
                    NPdevsel = 0;
                    NPdevice = kbbuf[0];  /* save the device selected */
                    kbbuf[0] = NPsel2;    /* setup for 2nd part of rtn */
                }
                if (NPdataentry == 0 && kblen == 1)
                {   /* We are in command mode */
                    if (NPhelpup == 1)
                    {
                        if (kbbuf[0] == 0x1b)
                            NPhelpdown = 1;
                        kbbuf[0] = '\0';
                        redraw_status = 1;
                    }
                    cmdline[0] = '\0';
                    cmdlen = 0;
                    cmdoff = 0;
                    ADJ_CMDCOL();
                    switch(kbbuf[0]) {
                        case 0x1b:                  /* ESC */
                            NPDup = 0;
                            cmdline[0] = '\0';
                            cmdoff = 0;
                            cmdlen = 0;
                            ADJ_CMDCOL();
                            break;
                        case '?':
                            NPhelpup = 1;
                            redraw_status = 1;
                            break;
                        case 'S':                   /* START */
                        case 's':
                            do_panel_command("startall");
                            break;
                        case 'P':                   /* STOP */
                        case 'p':
                            do_panel_command("stopall");
                            break;
                        case 'O':                   /* Store */
                        case 'o':
                            regs = copy_regs(sysblk.pcpu);
                            aaddr = APPLY_PREFIXING (NPaddress, regs->PX);
                            if (aaddr > regs->mainlim)
                                break;
                            store_fw (regs->mainstor + aaddr, NPdata);
                            redraw_status = 1;
                            break;
                        case 'I':                   /* Display */
                        case 'i':
                            NPregdisp = 4;
                            NPregs_valid = 0;
                            redraw_status = 1;
                            break;
                        case 'g':                   /* display GPR */
                        case 'G':
                            NPregdisp = 0;
                            NPregs_valid = 0;
                            redraw_status = 1;
                            break;
                        case 'a':                   /* Display AR */
                        case 'A':
                            NPregdisp = 2;
                            NPregs_valid = 0;
                            redraw_status = 1;
                            break;
                        case 'c':
                        case 'C':                   /* Case CR */
                            NPregdisp = 1;
                            NPregs_valid = 0;
                            redraw_status = 1;
                            break;
                        case 'f':                   /* Case FPR */
                        case 'F':
                            NPregdisp = 3;
                            NPregs_valid = 0;
                            redraw_status = 1;
                            break;
                        case 'r':                   /* Enter address */
                        case 'R':
                            NPdataentry = 1;
                            NPpending = 'r';
                            NPcurrow = 16;
                            NPcurcol = 12;
                            NPdatalen = 8;
                            NPcolorSwitch = 1;
                            NPcolorFore = COLOR_WHITE;
                            NPcolorBack = COLOR_BLUE;
                            strcpy(NPentered, "");
                            strcpy(NPprompt1, "Enter Address");
                            redraw_status = 1;
                            break;
                        case 'd':                   /* Enter data */
                        case 'D':
                            NPdataentry = 1;
                            NPpending = 'd';
                            NPcurrow = 16;
                            NPcurcol = 30;
                            NPdatalen = 8;
                            NPcolorSwitch = 1;
                            NPcolorFore = COLOR_WHITE;
                            NPcolorBack = COLOR_BLUE;
                            strcpy(NPentered, "");
                            strcpy(NPprompt1, "Enter Data Value");
                            redraw_status = 1;
                            break;
                        case 'l':                   /* IPL */
                        case 'L':
                            NPdevsel = 1;
                            NPsel2 = 1;
                            strcpy(NPprompt2, "Select Device for IPL");
                            redraw_status = 1;
                            break;
                        case 1:                     /* IPL - 2nd part */
                            i = toupper(NPdevice) - 'A';
                            if (i < 0 || i > NPlastdev) {
                                strcpy(NPprompt2, "");
                                redraw_status = 1;
                                break;
                            }
                            sprintf (cmdline, "ipl %4.4x", NPdevnum[i]);
                            do_panel_command(cmdline);
                            strcpy(NPprompt2, "");
                            redraw_status = 1;
                            break;
                        case 'u':                   /* Device interrupt */
                        case 'U':
                            NPdevsel = 1;
                            NPsel2 = 2;
                            strcpy(NPprompt2, "Select Device for Interrupt");
                            redraw_status = 1;
                            break;
                        case 2:                     /* Device int: part 2 */
                            i = toupper(NPdevice) - 'A';
                            if (i < 0 || i > NPlastdev) {
                                strcpy(NPprompt2, "");
                                redraw_status = 1;
                                break;
                            }
                            sprintf (cmdline, "i %4.4x", NPdevnum[i]);
                            do_panel_command(cmdline);
                            strcpy(NPprompt2, "");
                            redraw_status = 1;
                            break;
                        case 'n':                   /* Device Assignment */
                        case 'N':
                            NPdevsel = 1;
                            NPsel2 = 3;
                            strcpy(NPprompt2, "Select Device to Reassign");
                            redraw_status = 1;
                            break;
                        case 3:                     /* Device asgn: part 2 */
                            i = toupper(NPdevice) - 'A';
                            if (i < 0 || i > NPlastdev) {
                                strcpy(NPprompt2, "");
                                redraw_status = 1;
                                break;
                            }
                            NPdataentry = 1;
                            NPpending = 'n';
                            NPasgn = i;
                            NPcurrow = 3 + i;
                            NPcurcol = 58;
                            NPdatalen = cons_cols - 57;
                            NPcolorSwitch = 1;
                            NPcolorFore = COLOR_DEFAULT_LIGHT;
                            NPcolorBack = COLOR_BLUE;
                            strcpy(NPentered, "");
                            strcpy(NPprompt2, "New Name, or [enter] to Reload");
                            redraw_status = 1;
                            break;
                        case 'W':                   /* POWER */
                        case 'w':
                            NPdevsel = 1;
                            NPsel2 = 4;
                            strcpy(NPprompt1, "Confirm Powerdown Y or N");
                            redraw_status = 1;
                            break;
                        case 4:                     /* POWER - 2nd part */
                            if (NPdevice == 'y' || NPdevice == 'Y')
                                do_panel_command("quit");
                            strcpy(NPprompt1, "Powering down");
                            redraw_status = 1;
                            break;
                        case 'T':                   /* Restart */
                        case 't':
                            NPdevsel = 1;
                            NPsel2 = 5;
                            strcpy(NPprompt1, "Confirm Restart Y or N");
                            redraw_status = 1;
                            break;
                        case 5:                    /* Restart - part 2 */
                            if (NPdevice == 'y' || NPdevice == 'Y')
                                do_panel_command("restart");
                            strcpy(NPprompt1, "");
                            redraw_status = 1;
                            break;
                        case 'E':                   /* Ext int */
                        case 'e':
                            NPdevsel = 1;
                            NPsel2 = 6;
                            strcpy(NPprompt1, "Confirm External Interrupt Y or N");
                            redraw_status = 1;
                            break;
                        case 6:                    /* External - part 2 */
                            if (NPdevice == 'y' || NPdevice == 'Y')
                                do_panel_command("ext");
                            strcpy(NPprompt1, "");
                            redraw_status = 1;
                            break;
                        default:
                            break;
                    }
                    NPcmd = 1;
                } else {  /* We are in data entry mode */
                    NPcmd = 0;
                }
                if (NPcmd == 1)
                    kblen = 0;                  /* don't process as command */
            }
            /* =NP END= */

            /* Process characters in the keyboard buffer */
            for (i = 0; i < kblen; )
            {
                /* Test for home command... */
                if (strcmp(kbbuf+i, KBD_HOME) == 0) {
                    if (cmdlen) {
                        cmdoff = 0;
                        ADJ_CMDCOL();
                        redraw_cmd = 1;
                    } else {
                        scroll_to_top_line();
                        redraw_msgs = 1;
                    }
                    break;
                }

                /* Test for end command... */
                if (strcmp(kbbuf+i, KBD_END) == 0) {
                    if (cmdlen) {
                        cmdoff = cmdlen;
                        ADJ_CMDCOL();
                        redraw_cmd = 1;
                    } else {
                        scroll_to_bottom_screen();
                        redraw_msgs = 1;
                    }
                    break;
                }

                /* Test for ctrl+home scroll top command */
                if (strcmp(kbbuf+i, KBD_CTRL_HOME) == 0) {
                    scroll_to_top_line();
                    redraw_msgs = 1;
                    break;
                }

                /* Test for ctrl+end scroll bottom command */
                if (strcmp(kbbuf+i, KBD_CTRL_END) == 0) {
                    scroll_to_bottom_line();
                    redraw_msgs = 1;
                    break;
                }

                /* Test for up arrow prev history command */
                if (strcmp(kbbuf+i,  KBD_UP_ARROW) == 0 ||
                    strcmp(kbbuf+i, xKBD_UP_ARROW) == 0) {
                    if (history_prev() != -1) {
                        strcpy(cmdline, historyCmdLine);
                        cmdlen = strlen(cmdline);
                        cmdoff = cmdlen < cmdcols ? cmdlen : 0;
                        ADJ_CMDCOL();
                        NPDup = 0;
                        NPDinit = 1;
                        redraw_cmd = 1;
                    }
                    break;
                }

                /* Test for down arrow next history command */
                if (strcmp(kbbuf+i,  KBD_DOWN_ARROW) == 0 ||
                    strcmp(kbbuf+i, xKBD_DOWN_ARROW) == 0) {
                    if (history_next() != -1) {
                        strcpy(cmdline, historyCmdLine);
                        cmdlen = strlen(cmdline);
                        cmdoff = cmdlen < cmdcols ? cmdlen : 0;
                        ADJ_CMDCOL();
                        NPDup = 0;
                        NPDinit = 1;
                        redraw_cmd = 1;
                    }
                    break;
                }

                /* Test for page scroll up command */
                if (strcmp(kbbuf+i, KBD_PAGE_UP) == 0) {
                    page_up();
                    redraw_msgs = 1;
                    break;
                }

                /* Test for page scroll down command */
                if (strcmp(kbbuf+i, KBD_PAGE_DOWN) == 0) {
                    page_down();
                    redraw_msgs = 1;
                    break;
                }

                /* Test line scroll up command */
                if (strcmp(kbbuf+i, KBD_CTRL_UP_ARROW) == 0) {
                    scroll_up_lines(1);
                    redraw_msgs = 1;
                    break;
                }

                /* Test line scroll down command */
                if (strcmp(kbbuf+i, KBD_CTRL_DOWN_ARROW) == 0) {
                    scroll_down_lines(1);
                    redraw_msgs = 1;
                    break;
                }

                /* Process backspace character  */
                if (kbbuf[i] == '\b' || kbbuf[i] == '\x7F') {
                    if (cmdoff > 0) {
                        int j;
                        for (j = cmdoff-1; j<cmdlen; j++)
                            cmdline[j] = cmdline[j+1];
                        cmdoff--;
                        cmdlen--;
                        ADJ_CMDCOL();
                    }
                    i++;
                    redraw_cmd = 1;
                    break;
                }

                /* Process DEL character */
                if (strcmp(kbbuf+i, KBD_DELETE) == 0) {
                    if (cmdoff < cmdlen) {
                        int j;
                        for (j = cmdoff; j<cmdlen; j++)
                            cmdline[j] = cmdline[j+1];
                        cmdlen--;
                    }
                    i++;
                    redraw_cmd = 1;
                    break;
                }

                /* Process LEFT_ARROW character */
                if (strcmp(kbbuf+i,  KBD_LEFT_ARROW) == 0 ||
                    strcmp(kbbuf+i, xKBD_LEFT_ARROW) == 0) {
                    if (cmdoff > 0) cmdoff--;
                    ADJ_CMDCOL();
                    i++;
                    redraw_cmd = 1;
                    break;
                }

                /* Process RIGHT_ARROW character */
                if (strcmp(kbbuf+i,  KBD_RIGHT_ARROW) == 0 ||
                    strcmp(kbbuf+i, xKBD_RIGHT_ARROW) == 0) {
                    if (cmdoff < cmdlen) cmdoff++;
                    ADJ_CMDCOL();
                    i++;
                    redraw_cmd = 1;
                    break;
                }

                /* Process INSERT character */
                if (strcmp(kbbuf+i, KBD_INSERT) == 0 ) {
                    cmdins = !cmdins;
                    set_console_cursor_shape( confp, cmdins );
                    i++;
                    break;
                }

                /* Process escape key */
                if (kbbuf[i] == '\x1B') {
                    if (cmdlen) {
                        cmdline[0] = '\0';
                        cmdlen = 0;
                        cmdoff = 0;
                        ADJ_CMDCOL();
                        redraw_cmd = 1;
                    } else {
                        /* =NP= : Switch to new panel display */
                        NP_init();
                        NPDup = 1;
                        cmdline[0] = '\0';
                        cmdlen = 0;
                        cmdoff = 0;
                        ADJ_CMDCOL();
                        /* =END= */
                    }
                    break;
                }

                /* Process TAB character */
                if (kbbuf[i] == '\t' || kbbuf[i] == '\x7F') {
                    tab_pressed(cmdline, &cmdoff);
                    cmdlen = strlen(cmdline);
                    ADJ_CMDCOL();
                    i++;
                    redraw_cmd = 1;
                    break;
                }

                /* Process the command if newline was read */
                if (kbbuf[i] == '\n') {
                    if (cmdlen == 0 && NPDup == 0 && !sysblk.inststep) {
                        history_show();
                    } else {
                        cmdline[cmdlen] = '\0';
                        /* =NP= create_thread replaced with: */
                        if (NPDup == 0) {
                            if ('#' == cmdline[0] || '*' == cmdline[0]) {
                                if (!is_currline_visible())
                                    scroll_to_bottom_screen();
                                logmsg("%s\n", cmdline);
                                for (;cmdlen >=0; cmdlen--)
                                    cmdline[cmdlen] = '\0';
                                cmdlen = 0;
                                cmdoff = 0;
                                ADJ_CMDCOL();
                            } else {
                                history_requested = 0;
                                do_panel_command(cmdline);
                                redraw_msgs = 1;
                                redraw_cmd = 1;
                                redraw_status = 1;
                                for (;cmdlen >=0; cmdlen--)
                                    cmdline[cmdlen] = '\0';
                                cmdlen = 0;
                                cmdoff = 0;
                                ADJ_CMDCOL();
                                if (history_requested == 1) {
                                    strcpy(cmdline, historyCmdLine);
                                    cmdlen = strlen(cmdline);
                                    cmdoff = cmdlen;
                                    ADJ_CMDCOL();
                                    NPDup = 0;
                                    NPDinit = 1;
                                }
                            }
                        } else {
                            NPdataentry = 0;
                            NPcurrow = cons_rows;
                            NPcurcol = cons_cols;
                            NPcolorSwitch = 0;
                            switch (NPpending) {
                                case 'r':
                                    sscanf(cmdline, "%x", &NPaddress);
                                    NPaddr_valid = 0;
                                    strcpy(NPprompt1, "");
                                    break;
                                case 'd':
                                    sscanf(cmdline, "%x", &NPdata);
                                    NPdata_valid = 0;
                                    strcpy(NPprompt1, "");
                                    break;
                                case 'n':
                                    if (strlen(cmdline) < 1) {
                                        strcpy(cmdline, NPdevnam[NPasgn]);
                                    }
                                    strcpy(NPdevnam[NPasgn], "");
                                    sprintf (NPentered, "devinit %4.4x %s",
                                             NPdevnum[NPasgn], cmdline);
                                    do_panel_command(NPentered);
                                    strcpy(NPprompt2, "");
                                    break;
                                default:
                                    break;
                            }
                            redraw_status = 1;
                            cmdline[0] = '\0';
                            cmdlen = 0;
                            cmdoff = 0;
                            ADJ_CMDCOL();
                        }
                        /* =END= */
                        redraw_cmd = 1;
                    }
                    break;
                }

                /* Ignore non-printable characters */
                if (!isprint(kbbuf[i])) {
#if 0 /* do we REALLY need to be doing this?! */
                    logmsg ("%2.2X\n", kbbuf[i]);
#endif
                    console_beep( confp );
                    i++;
                    continue;
                }

                /* Append the character to the command buffer */
                ASSERT(cmdlen <= CMD_SIZE-1 && cmdoff <= cmdlen);
                if (0
                    || (cmdoff >= CMD_SIZE-1)
                    || (cmdins && cmdlen >= CMD_SIZE-1)
                )
                {
                    /* (no more room!) */
                    console_beep( confp );
                }
                else /* (there's still room) */
                {
                    ASSERT(cmdlen < CMD_SIZE-1 || (!cmdins && cmdoff < cmdlen));
                    if (cmdoff >= cmdlen)
                    {
                        /* Append to end of buffer */
                        ASSERT(!(cmdoff > cmdlen)); // (sanity check)
                        cmdline[cmdoff++] = kbbuf[i];
                        cmdline[cmdoff] = '\0';
                        cmdlen++;
                    }
                    else
                    {
                        ASSERT(cmdoff < cmdlen);
                        if (cmdins)
                        {
                            /* Insert: make room by sliding all
                               following chars right one position */
                            int j;
                            for (j=cmdlen-1; j>=cmdoff; j--)
                                cmdline[j+1] = cmdline[j];
                            cmdline[cmdoff++] = kbbuf[i];
                            cmdlen++;
                        }
                        else
                        {
                            /* Overlay: replace current position */
                            cmdline[cmdoff++] = kbbuf[i];
                        }
                    }
                    ADJ_CMDCOL();
                    redraw_cmd = 1;
                }
                i++;
            } /* end for(i) */
        } /* end if keystroke */

FinishShutdown:

        // If we finished processing all of the message data
        // the last time we were here, then get some more...

        if ( lmsndx >= lmscnt )  // (all previous data processed?)
        {
            lmscnt = log_read( &lmsbuf, &lmsnum, LOG_NOBLOCK );
            lmsndx = 0;
        }
        else if ( lmsndx >= lmsmax )
        {
            lmsbuf += lmsndx;   // pick up where we left off at...
            lmscnt -= lmsndx;   // pick up where we left off at...
            lmsndx = 0;
        }

        // Process all message data or until limit reached...

        // (limiting the amount of data we process a console message flood
        // from preventing keyboard from being read since we need to break
        // out of the below message data processing loop to loop back up
        // to read the keyboard again...)

        /* Read message bytes until newline... */
        while ( lmsndx < lmscnt && lmsndx < lmsmax )
        {
            /* Initialize the read buffer */
            if (!readoff || readoff >= MSG_SIZE) {
                memset (readbuf, SPACE, MSG_SIZE);
                readoff = 0;
            }

            /* Read message bytes and copy into read buffer
               until we either encounter a newline character
               or our buffer is completely filled with data. */
            while ( lmsndx < lmscnt && lmsndx < lmsmax )
            {
                /* Read a byte from the message pipe */
                c = *(lmsbuf + lmsndx); lmsndx++;

                /* Break to process received message
                   whenever a newline is encountered */
                if (c == '\n' || c == '\r') {
                    readoff = 0;    /* (for next time) */
                    break;
                }

                /* Handle tab character */
                if (c == '\t') {
                    readoff += 8;
                    readoff &= 0xFFFFFFF8;
                    /* Messages longer than one screen line will
                       be continued on the very next screen line */
                    if (readoff >= MSG_SIZE)
                        break;
                    else continue;
                }

                /* Eliminate non-displayable characters */
                if (!isgraph(c)) c = SPACE;

                /* Stuff byte into message processing buffer */
                readbuf[readoff++] = c;

                /* Messages longer than one screen line will
                   be continued on the very next screen line */
                if (readoff >= MSG_SIZE)
                    break;

            } // end while ( lmsndx < lmscnt && lmsndx < lmsmax )

            /* If we have a message to be displayed (or a complete
               part of one), then copy it to the circular buffer. */
            if (!readoff || readoff >= MSG_SIZE) {

                /* First-time here? */
                if (curmsg == NULL) {
                    curmsg = topmsg = msgbuf;
                } else {
                    /* Perform autoscroll if needed */
                    if (is_currline_visible()) {
                        if (lines_remaining() < 1)
                            topmsg = topmsg->next;
                        /* Set the display update indicator */
                        redraw_msgs = 1;
                    }

                    /* Go on to next available msg buffer */
                    curmsg = curmsg->next;

                    /* Updated wrapped indicator */
                    if (curmsg == msgbuf)
                        wrapped = 1;
                }

                /* Copy message into next available PANMSG slot */
                memcpy( curmsg->msg, readbuf, MSG_SIZE );
#ifdef OPTION_MSGCLR
                read_cmd(curmsg);
#endif
            }

        } // end while ( lmsndx < lmscnt && lmsndx < lmsmax )

        /* Don't read or otherwise process any input
           once system shutdown has been initiated
        */
        if ( sysblk.shutdown )
        {
            if ( sysblk.shutfini ) break;
            /* wait for system to finish shutting down */
            usleep(10000);
            lmsmax = INT_MAX;
            goto FinishShutdown;
        }

        /* =NP= : Reinit traditional panel if NP is down */
        if (NPDup == 0 && NPDinit == 1) {
            NPDinit = 0;
            redraw_msgs = redraw_status = redraw_cmd = 1;
            set_color (COLOR_DEFAULT_FG, COLOR_DEFAULT_BG);
            clr_screen ();
        }
        /* =END= */

        /* Obtain the PSW for target CPU */
        regs = copy_regs(sysblk.pcpu);
        memset (curpsw, 0x00, sizeof(curpsw));
        copy_psw (regs, curpsw);

        /* Set the display update indicator if the PSW has changed
           or if the instruction counter has changed, or if
           the CPU stopped state has changed */
        if (memcmp(curpsw, prvpsw, sizeof(curpsw)) != 0
         || prvicount != INSTCOUNT(regs)
         || prvcpupct != regs->cpupct
#if defined(OPTION_SHARED_DEVICES)
         || prvscount != sysblk.shrdcount
#endif
         || prvstate != regs->cpustate
         || (NPDup && NPcpugraph && prvtcount != sysblk.instcount)
           )
        {
            redraw_status = 1;
            memcpy (prvpsw, curpsw, sizeof(prvpsw));
            prvicount = INSTCOUNT(regs);
            prvcpupct = regs->cpupct;
            prvstate  = regs->cpustate;
#if defined(OPTION_SHARED_DEVICES)
            prvscount = sysblk.shrdcount;
#endif
            prvtcount = sysblk.instcount;
        }

        /* =NP= : Display the screen - traditional or NP */
        /*        Note: this is the only code block modified rather */
        /*        than inserted.  It makes the block of 3 ifs in the */
        /*        original code dependent on NPDup == 0, and inserts */
        /*        the NP display as an else after those ifs */

        if (NPDup == 0) {
            /* Rewrite the screen if display update indicators are set */
            if (redraw_msgs && !sysblk.npquiet)
            {
                /* Display messages in scrolling area */
                PANMSG* p = topmsg;
                for (i=0; i < NUM_LINES && (p != curmsg->next || p == topmsg); i++, p = p->next)
                {
                    set_pos (i+1, 1);
#ifdef OPTION_MSGCLR
                    set_color (p->fg, p->bg);
#else
                    set_color (COLOR_DEFAULT_FG, COLOR_DEFAULT_BG);
#endif
                    write_text (p->msg, MSG_SIZE);
                }

                /* Pad remainder of screen with blank lines */
                for (; i < NUM_LINES; i++)
                {
                    set_pos (i+1, 1);
                    set_color (COLOR_DEFAULT_FG, COLOR_DEFAULT_BG);
                    erase_to_eol( confp );
                }

                /* Display the scroll indicators */
                if (topmsg != oldest_msg())
                {
                    /* More messages precede top line */
                    set_pos (1, cons_cols);
                    set_color (COLOR_DEFAULT_LIGHT, COLOR_DEFAULT_BG);
                    draw_text ("+" );
                }
                if (!is_currline_visible())
                {
                    /* More messages follow bottom line */
                    set_pos (cons_rows-2, cons_cols);
                    set_color (COLOR_DEFAULT_LIGHT, COLOR_DEFAULT_BG);
                    draw_text ("V");
                }
            } /* end if(redraw_msgs) */

            if (redraw_cmd)
            {
                /* Display the command line */
                set_pos (cons_rows-1, 1);
                set_color (COLOR_DEFAULT_LIGHT, COLOR_DEFAULT_BG);

#ifdef OPTION_CMDTGT
                switch(sysblk.cmdtgt)
                {
                  case 0: // cmdtgt herc 
                  {
                    draw_text(CMD_PREFIX_STR);
                    break;
                  }
                  case 1: // cmdtgt scp
                  {
                    draw_text(CMD_PREFIX_STR1);
                    break;
                  }
                  case 2: // cmdtgt pscp
                  {
                    draw_text(CMD_PREFIX_STR2);
                    break;
                  }
                }
#elif
                draw_text (CMD_PREFIX_STR);
#endif // OPTION_CMDTGT

                set_color (COLOR_DEFAULT_FG, COLOR_DEFAULT_BG);
                PUTC_CMDLINE ();
                fill_text (' ',cons_cols);
            } /* end if(redraw_cmd) */

            if (redraw_status && !sysblk.npquiet)
            {
                memset (buf, ' ', cons_cols);
                len = sprintf (buf, "CPU%4.4X ", sysblk.pcpu);
                if (IS_CPU_ONLINE(sysblk.pcpu))
                {
                    char ibuf[32];
                    len += sprintf(buf+len, "PSW=%8.8X %8.8X ",
                                   fetch_fw(curpsw), fetch_fw(curpsw+4));
                    if (regs->arch_mode == ARCH_900)
                        len += sprintf (buf+len, "%16.16"I64_FMT"X ",
                                        fetch_dw (curpsw+8));
#if defined(_FEATURE_SIE)
                    else
            if( SIE_MODE(regs) )
                            for(i = 0;i < 17;i++)
                                buf[len++] = ' ';
#endif /*defined(_FEATURE_SIE)*/
                    len += sprintf (buf+len, "%2d%c%c%c%c%c%c%c%c",
                           regs->psw.amode64                  ? 64 :
                           regs->psw.amode                    ? 31 : 24, 
                           regs->cpustate == CPUSTATE_STOPPED ? 'M' : '.',
                           sysblk.inststep                    ? 'T' : '.',
                           WAITSTATE(&regs->psw)              ? 'W' : '.',
                           regs->loadstate                    ? 'L' : '.',
                           regs->checkstop                    ? 'C' : '.',
                           PROBSTATE(&regs->psw)              ? 'P' : '.',
                           SIE_MODE(regs)                     ? 'S' : '.',
                           regs->arch_mode == ARCH_900        ? 'Z' : '.');
                    buf[len++] = ' ';
                    sprintf (ibuf, "instcount=%" I64_FMT "u", INSTCOUNT(regs));
                    if (len + (int)strlen(ibuf) < cons_cols)
                        len = cons_cols - strlen(ibuf);
                    strcpy (buf + len, ibuf);
                }
                else
                {
                    len += sprintf (buf+len,"%s", "Offline");
                    buf[len++] = ' ';
                }
                buf[cons_cols] = '\0';
                set_pos (cons_rows, 1);
                set_color (COLOR_LIGHT_YELLOW, COLOR_RED);
                draw_text (buf);
            } /* end if(redraw_status) */

            /* Flush screen buffer and reset display update indicators */
            if (redraw_msgs || redraw_cmd || redraw_status)
            {
                set_color (COLOR_DEFAULT_FG, COLOR_DEFAULT_BG);
                set_pos (cons_rows-1, CMD_PREFIX_LEN+1+cmdoff-cmdcol);
                fflush (confp);
                redraw_msgs = redraw_cmd = redraw_status = 0;
            }

        } else {

            if (redraw_status || (NPDinit == 0 && NPDup == 1)
                   || (redraw_cmd && NPdataentry == 1)) {
                if (NPDinit == 0) {
                    NPDinit = 1;
                    NP_screen_redraw(regs);
                }
            }
            NP_update(regs);
            fflush (confp);
            redraw_msgs = redraw_cmd = redraw_status = 0;
        }

    /* =END= */

    } /* end while */

    ASSERT( sysblk.shutdown );  // (why else would we be here?!)

} /* end function panel_display */

static
void panel_cleanup(void *unused)
{
int i;
PANMSG* p;

    UNREFERENCED(unused);

    log_wakeup(NULL);

    set_screen_color( stderr, COLOR_DEFAULT_FG, COLOR_DEFAULT_BG );
    clear_screen( stderr );

    /* Scroll to last full screen's worth of messages */
    scroll_to_bottom_screen();

    /* Display messages in scrolling area */
    for (i=0, p = topmsg; i < NUM_LINES && p != curmsg->next; i++, p = p->next)
    {
        set_pos (i+1, 1);
#ifdef OPTION_MSGCLR
        set_color (p->fg, p->bg);
#else
        set_color (COLOR_DEFAULT_FG, COLOR_DEFAULT_BG);
#endif
        write_text (p->msg, MSG_SIZE);
    }

    /* Restore the terminal mode */
    set_or_reset_console_mode( keybfd, 0 );

    /* Position to next line */
    fwrite("\n",1,1,stderr);

    /* Read and display any msgs still remaining in the system log */
    while((lmscnt = log_read(&lmsbuf, &lmsnum, LOG_NOBLOCK)))
        fwrite(lmsbuf,lmscnt,1,stderr);

    fflush(stderr);
}
