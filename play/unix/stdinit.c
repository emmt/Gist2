/*
 * $Id: stdinit.c,v 1.1 2005-09-18 22:05:39 dhmunro Exp $
 * UNIX version of play terminal I/O
 */
/* Copyright (c) 2005, The Regents of the University of California.
 * All rights reserved.
 * This file is part of yorick (http://yorick.sourceforge.net).
 * Read the accompanying LICENSE file for details.
 */

#include "config.h"
#include "play2.h"
#include "play/std.h"
#include "play/io.h"
#include "playu.h"

#include <stdio.h>
#include <string.h>

static void u_fd0_ready(void *);

static char *u_input_line = 0;
static int u_input_max = 0;
static int u_term_errs = 0;

static void (*u_stdin)(char *input_line);

void
pl_stdinit(void (*on_stdin)(char *input_line))
{
  u_stdin = on_stdin;
  _pl_u_event_src(0, &u_fd0_ready, (void *)0);
}

void
pl_stdout(char *output_line)
{
  if (pl_signalling == PL_SIG_NONE) {
    fputs(output_line, stdout);
    fflush(stdout);
  }
}

void
pl_stderr(char *output_line)
{
  if (pl_signalling == PL_SIG_NONE) {
    fputs(output_line, stderr);
    fflush(stderr);
  }
}

/* ARGSUSED */
static void
u_fd0_ready(void *c)
{
  char *line = u_input_line;
  int n;

  /* before calling fgets, check to be sure that this process is
   * not in the background (via UNIX shell job control) */
  if (_pl_u_in_background()) return;

  do {
    if (u_input_max < (line-u_input_line)+1024) {
      if (u_input_max > 16000) break;
      n = line - u_input_line;
      u_input_line = pl_realloc(u_input_line, u_input_max+1024);
      u_input_max += 1024;
      line = u_input_line + n;
    }
    if (!fgets(line, 1024, stdin)) {
      int at_eof = feof(stdin);
      clearerr(stdin);
      if (++u_term_errs>3 || at_eof) {
        /* cannot read stdin -- maybe serious error, remove it */
        _pl_u_event_src(0, (void (*)(void *))0, (void *)0);
        return;
      }
    } else {
      u_term_errs = 0;  /* reset error counter on each successful read */
    }
    n = strlen(line);
    line += n;
  } while (n==1023 && line[-1]!='\n');
  if (line[-1]=='\n') line[-1] = '\0';

  if (u_stdin) u_stdin(u_input_line);
  else pl_stderr("\a");  /* beep to indicate rejection */

  if (u_input_max>1024) {
    u_input_line = pl_realloc(u_input_line, 1024);
    u_input_max = 1024;
  }
}
