#include "hercules.h"
#include "history.h"

#define HISTORY_MAX 10

#define CMD_SIZE 256  /* 32767 is way toooooo much */

typedef struct history {
  int number;
  char *cmdline;
  struct history *prev;
  struct history *next;
} HISTORY;


BYTE history_count;          /* for line numbering */

HISTORY *history_lines;      /* points to the beginning of history list (oldest command) */
HISTORY *history_lines_end;  /* points to the end of history list (most recent command) */
HISTORY *history_ptr;        /* points to last command retrieved by key press */
HISTORY *backup;             /* used for backuping last removed command */

/* these 2 are used in panel.c to see if there was history command requested and
   returns that command */ 
char *historyCmdLine;
int history_requested = 0;

void copy_to_historyCmdLine(char* cmdline)
{
  if (historyCmdLine) free(historyCmdLine);
  historyCmdLine = malloc(strlen(cmdline)+1);
  strcpy(historyCmdLine, cmdline);
}

/* initialize environment */
int history_init() {
  history_lines = NULL;
  history_lines_end = NULL;
  historyCmdLine = (char *) malloc(255);
  history_requested = 0;
  backup = NULL;
  history_count = 0;
  history_ptr = NULL;
  return(0);
}

/* add commandline to history list */
int history_add(char *cmdline) {
  HISTORY *tmp;

  /* if there is some backup line remaining, remove it */
  if (backup != NULL) {
    free(backup->cmdline);
    free(backup);
    backup = NULL;
  }

  /* allocate space and copy string */
  tmp = (HISTORY*) malloc(sizeof(HISTORY));
  tmp->cmdline = (char*) malloc(strlen(cmdline) + 1);
  strcpy(tmp->cmdline, cmdline);
  tmp->next = NULL;
  tmp->prev = NULL;
  tmp->number = ++history_count;

  if (history_lines == NULL) {
    /* first in list */
    history_lines = tmp;
    history_lines_end = tmp;
  }
  else {
    tmp->prev = history_lines_end;
    history_lines_end->next = tmp;
    history_lines_end = tmp;
  }
  history_ptr = NULL;

  if (history_count > HISTORY_MAX) {
    /* if we are over maximum number of lines in history list, oldest one should be deleted
       but we don't know whether history_remove will not be called, so oldest line is backuped and        not removed */
    backup = history_lines;
    history_lines = history_lines->next;
    backup->next = NULL;
    history_lines->prev = NULL;
  }
  return(0);
}

/* show list of lines in history */
int history_show() {
  HISTORY *tmp;
  tmp = history_lines;
  while (tmp != NULL) {
    logmsg("%4d %s\n", tmp->number, tmp->cmdline);
    tmp = tmp->next;
  }
  return(0);
}

/* remove last line from history list (called only when history command was invoked) */
int history_remove() {
  HISTORY *tmp;

  if (history_lines == NULL)
    return(0);
  if (history_lines == history_lines_end) {
    ASSERT(history_lines->next == NULL);
    ASSERT(history_count == 1);
    free(history_lines->cmdline);
    free(history_lines);
    history_lines = NULL;
    history_lines_end = NULL;
    history_count--;
    return(0);
  }
  tmp = history_lines_end->prev;
  tmp->next = NULL;
  free(history_lines_end->cmdline);
  free(history_lines_end);
  history_count--;
  history_lines_end = tmp;
  if (backup != NULL) {
    backup->next = history_lines;
    history_lines->prev = backup;
    history_lines = backup;
    backup = NULL;
  }
  return(0);
}

int history_relative_line(int x) {
  HISTORY *tmp = history_lines_end;

  if (-x > HISTORY_MAX) {
    logmsg("History limited to last %d commands\n", HISTORY_MAX);
    return (-1);
  }

  if (-x > history_count) {
    logmsg("only %d commands in history\n", history_count);
    return (-1);
  }

  while (x<-1) {
    tmp = tmp->prev;
    x++;
  }
  copy_to_historyCmdLine(tmp->cmdline);
  history_ptr = NULL;
  return(0);
}

int history_absolute_line(int x) {
  HISTORY *tmp = history_lines_end;
  int lowlimit;

  if (history_count == 0) {
    logmsg("history empty\n");
    return -1;
  }

  lowlimit = history_count - HISTORY_MAX;

  if (x > history_count || x <= lowlimit) {
    logmsg("only commands %d-%d are in history\n", lowlimit<0? 1 : lowlimit + 1, history_count);
    return (-1);
  }

  while (tmp->number != x)
    tmp = tmp->prev;

  copy_to_historyCmdLine(tmp->cmdline);
  history_ptr = NULL;
  return(0);
}

int history_next() {
  if (history_ptr == NULL) {
    history_ptr = history_lines_end;
    if (history_ptr == NULL)
      return(-1);
    copy_to_historyCmdLine(history_ptr->cmdline);
    return(0);
  }
  if (history_ptr->next == NULL)
    history_ptr = history_lines;
  else 
    history_ptr = history_ptr->next;
  copy_to_historyCmdLine(history_ptr->cmdline);
  return(0);
}

int history_prev() {
  if (history_ptr == NULL) {
    history_ptr = history_lines_end;
    if (history_ptr == NULL)
      return(-1);
    copy_to_historyCmdLine(history_ptr->cmdline);
    return(0);
  }
  if (history_ptr->prev == NULL)
    history_ptr = history_lines_end;
  else 
    history_ptr = history_ptr->prev;
  copy_to_historyCmdLine(history_ptr->cmdline);
  return(0);
}
