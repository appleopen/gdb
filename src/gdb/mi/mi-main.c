/* MI Command Set.
   Copyright 2000, 2001, 2002, 2003 Free Software Foundation, Inc.
   Contributed by Cygnus Solutions (a Red Hat company).

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* Work in progress */

#include <ctype.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <signal.h>            /* for kill() */

#include "defs.h"
#include "target.h"
#include "inferior.h"
#include "gdb_string.h"
#include "top.h"
#include "gdbthread.h"
#include "gdbcmd.h"
#include "mi-cmds.h"
#include "mi-parse.h"
#include "mi-getopt.h"
#include "mi-console.h"
#include "ui-out.h"
#include "mi-out.h"
#include "interpreter.h"
#include "event-loop.h"
#include "event-top.h"
#include "gdbcore.h"		/* for write_memory() */
#include "value.h"		/* for deprecated_write_register_bytes() */
#include "regcache.h"
#include "gdb.h"
#include "frame.h"
#include "wrapper.h"
#include "source.h"

enum
  {
    FROM_TTY = 0
  };

/* Enumerations of the actions that may result from calling
   captured_mi_execute_command */

enum captured_mi_execute_command_actions
  {
    EXECUTE_COMMAND_DISPLAY_PROMPT,
    EXECUTE_COMMAND_SUPRESS_PROMPT,
    EXECUTE_COMMAND_DISPLAY_ERROR
  };

/* This structure is used to pass information from captured_mi_execute_command
   to mi_execute_command. */
struct captured_mi_execute_command_args
{
  /* This return result of the MI command (output) */
  enum mi_cmd_result rc;

  /* What action to perform when the call is finished (output) */
  enum captured_mi_execute_command_actions action;

  /* The command context to be executed (input) */
  struct mi_parse *command;
};


struct mi_continuation_arg
{
  char *token;
  struct mi_timestamp *timestamp;
  struct cleanup *cleanups;
};

int mi_debug_p;

/* These are the various output channels that are used by the mi. */

struct ui_file *raw_stdout;
struct ui_file *mi_stdout;
struct ui_file *mi_stderr;
struct ui_file *mi_stdlog;
struct ui_file *mi_stdtarg;

/* A pointer to the current mi_parse's command token.  This is needed
 because we can't pass the token down to the mi command levels.  This
 will get cleaned up once captured_mi_execute_command finishes, so 
 if you need it for a continuation, dup it.  */
static char *current_command_token;

static char *mi_error_message;
static char *old_regs;

/* This is used to pass the current command timestamp
   down to continuation routines. */
struct mi_timestamp *current_command_ts;

static int do_timings = 0;

/* Points to the current interpreter, used by the mi context callbacks.  */
struct gdb_interpreter *mi_interp;

/* The actual interpreters.  */
struct gdb_interpreter *miunspec_interp;
struct gdb_interpreter *mi0_interp;
struct gdb_interpreter *mi1_interp;
struct gdb_interpreter *mi2_interp;

extern void _initialize_mi_main (void);
static char *mi_input (char *);
static void mi_execute_command (char *cmd, int from_tty);
static enum mi_cmd_result mi_cmd_execute (struct mi_parse *parse);

static void mi_execute_cli_command (const char *cli, char *args);
static enum mi_cmd_result mi_execute_async_cli_command (char *mi, char *args, int from_tty);
static void mi_execute_command_wrapper (char *cmd);

void mi_exec_async_cli_cmd_continuation (struct continuation_arg *arg);

static int register_changed_p (int regnum);
static int get_register (int regnum, int format);
static void mi_load_progress (const char *section_name,
			      unsigned long sent_so_far,
			      unsigned long total_section,
			      unsigned long total_sent,
			      unsigned long grand_total);

/* FIXME: these should go in some .h file, but infcmd.c doesn't have a
   corresponding .h file. These wrappers will be obsolete anyway, once
   we pull the plug on the sanitization. */
extern void interrupt_target_command_wrapper (char *, int);
extern void return_command_wrapper (char *, int);

/* FIXME: these should go in some .h file, but infcmd.c doesn't have a
   corresponding .h file. These wrappers will be obsolete anyway, once
   we pull the plug on the sanitization. */
extern void interrupt_target_command_wrapper (char *, int);
extern void return_command_wrapper (char *, int);

/* These are the interpreter setup, etc. functions for the MI interpreter */

int mi_interpreter_init (void *data);
int mi_interpreter_resume (void *data);
int mi_interpreter_do_one_event (void *data);
int mi_interpreter_suspend (void *data);
int mi_interpreter_delete (void *data); 
int mi_interpreter_prompt(void *data, char *new_prompt);
int mi_interpreter_exec(void *data, char *command);

/* There should be a generic mi .h file where these should go... */
extern void mi_print_frame_more_info (struct ui_out *uiout,
				       struct symtab_and_line *sal,
				       struct frame_info *fi);

/* These are hooks that we put in place while doing interpreter_exec
   so we can report interesting things that happened "behind the mi's 
   back" in this command */

extern void mi_interp_create_breakpoint_hook (struct breakpoint *bpt);
extern void mi_interp_delete_breakpoint_hook (struct breakpoint *bpt);
extern void mi_interp_modify_breakpoint_hook (struct breakpoint *bpt);
extern int mi_interp_query_hook (const char *ctlstr, va_list ap);
extern void mi_interp_stack_changed_hook (void);
extern void mi_interp_frame_changed_hook (int new_frame_number);
extern void mi_interp_context_hook (int thread_id);
extern char * mi_interp_read_one_line_hook (char *prompt, int repeat, char *anno);
extern void mi_interp_stepping_command_hook(void);
extern void mi_interp_continue_command_hook(void);
extern int mi_interp_run_command_hook(void);

void mi_insert_notify_hooks (void);
void mi_remove_notify_hooks (void);

/* Use these calls to route output to the mi interpreter explicitly in
   hook functions that you insert when the console interpreter is running.
   They will ensure that the output doesn't get munged by the console
   interpreter */

static void output_async_notification (char *notification);
static void output_control_change_notification(char *notification);

static int mi_command_completes_while_target_executing (char *command);
static struct mi_continuation_arg *
  setup_continuation_arg (struct cleanup *cleanups);
static void free_continuation_arg (struct mi_continuation_arg *arg);
static void timestamp (struct mi_timestamp *tv);
static void print_diff_now (struct mi_timestamp *start);
static void copy_timestamp (struct mi_timestamp *dst, struct mi_timestamp *src);

static void print_diff (struct mi_timestamp *start, struct mi_timestamp *end);
static long wallclock_diff (struct mi_timestamp *start, struct mi_timestamp *end);
static long user_diff (struct mi_timestamp *start, struct mi_timestamp *end);
static long system_diff (struct mi_timestamp *start, struct mi_timestamp *end);

/* Command implementations. FIXME: Is this libgdb? No.  This is the MI
   layer that calls libgdb.  Any operation used in the below should be
   formalized. */

enum mi_cmd_result
mi_cmd_gdb_exit (char *command, char **argv, int argc)
{
  /* We have to print everything right here because we never return */
  if (current_command_token)
    fputs_unfiltered (current_command_token, raw_stdout);
  fputs_unfiltered ("^exit\n", raw_stdout);
  mi_out_put (uiout, raw_stdout);
  /* FIXME: The function called is not yet a formal libgdb function */
  quit_force (NULL, FROM_TTY);
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_exec_run (char *args, int from_tty)
{
  /* FIXME: Should call a libgdb function, not a cli wrapper */
  return mi_execute_async_cli_command ("run", args, from_tty);
}

enum mi_cmd_result
mi_cmd_exec_next (char *args, int from_tty)
{
  /* FIXME: Should call a libgdb function, not a cli wrapper */
  return mi_execute_async_cli_command ("next", args, from_tty);
}

enum mi_cmd_result
mi_cmd_exec_next_instruction (char *args, int from_tty)
{
  /* FIXME: Should call a libgdb function, not a cli wrapper */
  return mi_execute_async_cli_command ("nexti", args, from_tty);
}

enum mi_cmd_result
mi_cmd_exec_step (char *args, int from_tty)
{
  /* FIXME: Should call a libgdb function, not a cli wrapper */
  return mi_execute_async_cli_command ("step", args, from_tty);
}

enum mi_cmd_result
mi_cmd_exec_step_instruction (char *args, int from_tty)
{
  /* FIXME: Should call a libgdb function, not a cli wrapper */
  return mi_execute_async_cli_command ("stepi", args, from_tty);
}

enum mi_cmd_result
mi_cmd_exec_metrowerks_step (char *args, int from_tty)
{
  /* FIXME: Should call a libgdb function, not a cli wrapper */
  return mi_execute_async_cli_command ("metrowerks-step", args, from_tty);
}

enum mi_cmd_result
mi_cmd_exec_finish (char *args, int from_tty)
{
  /* FIXME: Should call a libgdb function, not a cli wrapper */
  return mi_execute_async_cli_command ("finish", args, from_tty);
}

enum mi_cmd_result
mi_cmd_exec_until (char *args, int from_tty)
{
  /* FIXME: Should call a libgdb function, not a cli wrapper */
  return mi_execute_async_cli_command ("until", args, from_tty);
}

enum mi_cmd_result
mi_cmd_exec_return (char *args, int from_tty)
{
  /* This command doesn't really execute the target, it just pops the
     specified number of frames. */
  if (*args)
    /* Call return_command with from_tty argument equal to 0 so as to
       avoid being queried. */
    return_command (args, 0);
  else
    /* Call return_command with from_tty argument equal to 0 so as to
       avoid being queried. */
    return_command (NULL, 0);

  /* Because we have called return_command with from_tty = 0, we need
     to print the frame here. */
  print_stack_frame (deprecated_selected_frame,
		     frame_relative_level (deprecated_selected_frame),
		     LOC_AND_ADDRESS);

  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_exec_continue (char *args, int from_tty)
{
  /* FIXME: Should call a libgdb function, not a cli wrapper */
  return mi_execute_async_cli_command ("continue", args, from_tty);
}

/* Interrupt the execution of the target.  */

enum mi_cmd_result
mi_cmd_exec_interrupt (char *args, int from_tty)
{
  if (!target_executing)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_exec_interrupt: Inferior not executing.");
      return MI_CMD_ERROR;
    }
    
  if (0)
    {
      interrupt_target_command (args, from_tty);
    }
  else
    {
      int pid = PIDGET (inferior_ptid);
      kill (pid, SIGINT);
    }

  if (current_command_token) {
    fputs_unfiltered (current_command_token, raw_stdout);
  }

  fprintf_unfiltered (raw_stdout, "^done");

  return MI_CMD_QUIET;
}

enum mi_cmd_result
mi_cmd_exec_status (char *command, char **argv, int argc)
{
  char *status;

  if (argc != 0)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_exec_status takes no arguments.");
      return MI_CMD_ERROR;
    }

  if (!target_has_execution)
    status = "not executing";
  else
    if (target_executing)
      /* FIXME: The result reporting needs to be better 
	 centralized.  The "^" for done and the result code
	 should all come from one place, rather than being
	 scattered throughout the code.  But untill this happens,
	 both this command and exec-interrupt will have to play
	 games to get their returns out properly... */
      {
	status = "running";
	fputs_unfiltered ("^done", raw_stdout);
	ui_out_field_string (uiout, "status", status);
	mi_out_put (uiout, raw_stdout);
	mi_out_rewind (uiout);
	fputs_unfiltered ("\n", raw_stdout);
	return MI_CMD_DONE;
      }
    else
      status = "stopped";

  ui_out_field_string (uiout, "status", status);
  
  return MI_CMD_DONE;

}

enum mi_cmd_result
mi_cmd_thread_select (char *command, char **argv, int argc)
{
  enum gdb_rc rc;

  if (argc != 1)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_thread_select: USAGE: threadnum.");
      return MI_CMD_ERROR;
    }
  else
    rc = gdb_thread_select (uiout, argv[0], 1);

  /* RC is enum gdb_rc if it is successful (>=0)
     enum return_reason if not (<0). */
  if ((int) rc < 0 && (enum return_reason) rc == RETURN_ERROR)
    return MI_CMD_CAUGHT_ERROR;
  else if ((int) rc >= 0 && rc == GDB_RC_FAIL)
    return MI_CMD_ERROR;
  else
    return MI_CMD_DONE;
}

/* This command sets the pc for the thread given in the first argument
   to the linespec given in the second argument.  It does not
   switch to that thread, however (except temporarily to set the pc).
   The return value is the new bottom stack frame after resetting the
   pc.

   If you specify a linespec that is outside the current function, the
   command will return an error, and not reset the pc.  To force it to
   set the pc anyway, add "-f" before any of the other arguments. */

enum mi_cmd_result
mi_cmd_thread_set_pc (char *command, char **argv, int argc)
{
  enum gdb_rc rc;
  struct symtabs_and_lines sals;
  struct symtab_and_line sal;
  ptid_t current_ptid = inferior_ptid;
  struct cleanup *old_cleanups;
  struct symbol *old_fun, *new_fun;
  int stay_in_function = 1;

  if (argc == 3)
    {
      if (strcmp (argv[0], "-f") == 0)
	{
	  stay_in_function = 0;
	  argc--;
	  argv++;
	}
    }
      
  if (argc != 2)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_thread_select: USAGE: <-f> threadnum pc.");
      return MI_CMD_ERROR;
    }

  old_cleanups = make_cleanup_restore_current_thread (current_ptid, 0);
  rc = gdb_thread_select (uiout, argv[0], 0);

  /* RC is enum gdb_rc if it is successful (>=0)
     enum return_reason if not (<0). */
  if ((int) rc < 0 && (enum return_reason) rc == RETURN_ERROR)
    return MI_CMD_CAUGHT_ERROR;
  else if ((int) rc >= 0 && rc == GDB_RC_FAIL)
    return MI_CMD_ERROR;

  /* Okay, we set the thread, now set the pc: */

  sals = decode_line_spec_1 (argv[1], 1);
  if (sals.nelts == 0)
    error ("Error resolving line spec \"%s\".", argv[1]);

  if (sals.nelts != 1)
    {
      xfree (sals.sals);
      error ("Line spec \"%s\" resolved to more than one location.", argv[1]);
    }

  sal = sals.sals[0];
  xfree (sals.sals);

  if (sal.symtab == 0 && sal.pc == 0)
    error ("No source file has been specified.");

  resolve_sal_pc (&sal);
  
  if (stay_in_function)
    {
      old_fun = get_frame_function (get_current_frame());
      if (old_fun == NULL)
	error ("Can't find the function for old_pc: 0x%lx.",
	       (unsigned long) get_frame_pc (get_current_frame ()));
      new_fun = find_pc_function (sal.pc);
      if (new_fun == NULL)
	error ("Can't find the function for new pc 0x%lx.",
	       (unsigned long) sal.pc);
      if (!SYMBOL_MATCHES_NAME (old_fun, SYMBOL_LINKAGE_NAME (new_fun)))
	error ("New pc: 0x%lx outside of current function", 
	       (unsigned long) sal.pc);
    }

  write_pc (sal.pc);

  /* We have to set the stop_pc to the pc that we moved to as well, so
     that if we are stopped at a breakpoint in the new location, we will
     properly step over it. */

  stop_pc = sal.pc;

  /* Update the current source location as well, so 'list' will do the right
     thing.  */

  set_current_source_symtab_and_line (&sal);

  /* Update the current breakpoint location as well, so break commands will
     do the right thing.  */

  set_default_breakpoint (1, sal.pc, sal.symtab, sal.line);

  /* Is this a Fix and Continue situation, i.e. do we have two
     identically named functions which are different?  We have
     some extra work to do in that case.  */

  if (old_fun != new_fun && 
      SYMBOL_MATCHES_NAME (old_fun, SYMBOL_LINKAGE_NAME (new_fun)))
    {
      update_picbase_register (new_fun);
    }

  /* FIXME - write_pc doesn't actually update the bottom-most frame_info
     structure, so I have to flush_cached_frames.  But that leaves the
     deprecated_selected_frame NULL, which is fatal when you try to print
     a register.  So I have to select the current frame to make everything
     copasetic again.  */

  flush_cached_frames ();
  select_frame (get_current_frame ());
  print_stack_frame (deprecated_selected_frame, 0, LOC_AND_ADDRESS);

  do_cleanups (old_cleanups);
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_thread_list_ids (char *command, char **argv, int argc)
{
  enum gdb_rc rc = MI_CMD_DONE;

  if (argc != 0)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_thread_list_ids: No arguments required.");
      return MI_CMD_ERROR;
    }
  else
    rc = gdb_list_thread_ids (uiout);

  if (rc == GDB_RC_FAIL)
    return MI_CMD_CAUGHT_ERROR;
  else
    return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_data_list_register_names (char *command, char **argv, int argc)
{
  int regnum, numregs;
  int i;
  struct cleanup *cleanup;

  /* Note that the test for a valid register must include checking the
     REGISTER_NAME because NUM_REGS may be allocated for the union of
     the register sets within a family of related processors.  In this
     case, some entries of REGISTER_NAME will change depending upon
     the particular processor being debugged.  */

  numregs = NUM_REGS + NUM_PSEUDO_REGS;

  cleanup = make_cleanup_ui_out_list_begin_end (uiout, "register-names");

  if (argc == 0)		/* No args, just do all the regs */
    {
      for (regnum = 0;
	   regnum < numregs;
	   regnum++)
	{
	  if (REGISTER_NAME (regnum) == NULL
	      || *(REGISTER_NAME (regnum)) == '\0')
	    ui_out_field_string (uiout, NULL, "");
	  else
	    ui_out_field_string (uiout, NULL, REGISTER_NAME (regnum));
	}
    }

  /* Else, list of register #s, just do listed regs */
  for (i = 0; i < argc; i++)
    {
      regnum = atoi (argv[i]);
      if (regnum < 0 || regnum >= numregs)
	{
	  do_cleanups (cleanup);
	  xasprintf (&mi_error_message, "bad register number");
	  return MI_CMD_ERROR;
	}
      if (REGISTER_NAME (regnum) == NULL
	  || *(REGISTER_NAME (regnum)) == '\0')
	ui_out_field_string (uiout, NULL, "");
      else
	ui_out_field_string (uiout, NULL, REGISTER_NAME (regnum));
    }
  do_cleanups (cleanup);
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_data_list_changed_registers (char *command, char **argv, int argc)
{
  int regnum, numregs, changed;
  int i;
  struct cleanup *cleanup;

  /* Note that the test for a valid register must include checking the
     REGISTER_NAME because NUM_REGS may be allocated for the union of
     the register sets within a family of related processors.  In this
     case, some entries of REGISTER_NAME will change depending upon
     the particular processor being debugged.  */

  numregs = NUM_REGS;

  cleanup = make_cleanup_ui_out_list_begin_end (uiout, "changed-registers");

  if (argc == 0)		/* No args, just do all the regs */
    {
      for (regnum = 0;
	   regnum < numregs;
	   regnum++)
	{
	  if (REGISTER_NAME (regnum) == NULL
	      || *(REGISTER_NAME (regnum)) == '\0')
	    continue;
	  changed = register_changed_p (regnum);
	  if (changed < 0)
	    {
	      do_cleanups (cleanup);
	      xasprintf (&mi_error_message,
			 "mi_cmd_data_list_changed_registers: Unable to read register contents.");
	      return MI_CMD_ERROR;
	    }
	  else if (changed)
	    ui_out_field_int (uiout, NULL, regnum);
	}
    }

  /* Else, list of register #s, just do listed regs */
  for (i = 0; i < argc; i++)
    {
      regnum = atoi (argv[i]);

      if (regnum >= 0
	  && regnum < numregs
	  && REGISTER_NAME (regnum) != NULL
	  && *REGISTER_NAME (regnum) != '\000')
	{
	  changed = register_changed_p (regnum);
	  if (changed < 0)
	    {
	      do_cleanups (cleanup);
	      xasprintf (&mi_error_message,
			 "mi_cmd_data_list_register_change: Unable to read register contents.");
	      return MI_CMD_ERROR;
	    }
	  else if (changed)
	    ui_out_field_int (uiout, NULL, regnum);
	}
      else
	{
	  do_cleanups (cleanup);
	  xasprintf (&mi_error_message, "bad register number");
	  return MI_CMD_ERROR;
	}
    }
  do_cleanups (cleanup);
  return MI_CMD_DONE;
}

static int
register_changed_p (int regnum)
{
  char *raw_buffer = alloca (MAX_REGISTER_RAW_SIZE);

  if (! frame_register_read (deprecated_selected_frame, regnum, raw_buffer))
    return -1;

  if (memcmp (&old_regs[REGISTER_BYTE (regnum)], raw_buffer,
	      REGISTER_RAW_SIZE (regnum)) == 0)
    return 0;

  /* Found a changed register. Return 1. */

  memcpy (&old_regs[REGISTER_BYTE (regnum)], raw_buffer,
	  REGISTER_RAW_SIZE (regnum));

  return 1;
}

/* Return a list of register number and value pairs. The valid
   arguments expected are: a letter indicating the format in which to
   display the registers contents. This can be one of: x (hexadecimal), d
   (decimal), N (natural), t (binary), o (octal), r (raw).  After the
   format argumetn there can be a sequence of numbers, indicating which
   registers to fetch the content of. If the format is the only argument,
   a list of all the registers with their values is returned. */
enum mi_cmd_result
mi_cmd_data_list_register_values (char *command, char **argv, int argc)
{
  int regnum, numregs, format, result;
  int i;
  struct cleanup *list_cleanup, *tuple_cleanup;

  /* Note that the test for a valid register must include checking the
     REGISTER_NAME because NUM_REGS may be allocated for the union of
     the register sets within a family of related processors.  In this
     case, some entries of REGISTER_NAME will change depending upon
     the particular processor being debugged.  */

  numregs = NUM_REGS;

  if (argc == 0)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_data_list_register_values: Usage: -data-list-register-values <format> [<regnum1>...<regnumN>]");
      return MI_CMD_ERROR;
    }

  format = (int) argv[0][0];

  if (!target_has_registers)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_data_list_register_values: No registers.");
      return MI_CMD_ERROR;
    }

  list_cleanup = make_cleanup_ui_out_list_begin_end (uiout, "register-values");

  if (argc == 1)		/* No args, beside the format: do all the regs */
    {
      for (regnum = 0;
	   regnum < numregs;
	   regnum++)
	{
	  if (REGISTER_NAME (regnum) == NULL
	      || *(REGISTER_NAME (regnum)) == '\0')
	    continue;
	  tuple_cleanup = make_cleanup_ui_out_tuple_begin_end (uiout, NULL);
	  ui_out_field_int (uiout, "number", regnum);
	  result = get_register (regnum, format);
	  if (result == -1)
	    {
	      do_cleanups (list_cleanup);
	      return MI_CMD_ERROR;
	    }
	  do_cleanups (tuple_cleanup);
	}
    }

  /* Else, list of register #s, just do listed regs */
  for (i = 1; i < argc; i++)
    {
      regnum = atoi (argv[i]);

      if (regnum >= 0
	  && regnum < numregs
	  && REGISTER_NAME (regnum) != NULL
	  && *REGISTER_NAME (regnum) != '\000')
	{
	  tuple_cleanup = make_cleanup_ui_out_tuple_begin_end (uiout, NULL);
	  ui_out_field_int (uiout, "number", regnum);
	  result = get_register (regnum, format);
	  if (result == -1)
	    {
	      do_cleanups (list_cleanup);
	      return MI_CMD_ERROR;
	    }
	  do_cleanups (tuple_cleanup);
	}
      else
	{
	  do_cleanups (list_cleanup);
	  xasprintf (&mi_error_message, "bad register number");
	  return MI_CMD_ERROR;
	}
    }
  do_cleanups (list_cleanup);
  return MI_CMD_DONE;
}

/* Output one register's contents in the desired format. */
static int
get_register (int regnum, int format)
{
  char *raw_buffer = alloca (MAX_REGISTER_RAW_SIZE);
  char *virtual_buffer = alloca (MAX_REGISTER_VIRTUAL_SIZE);
  int optim;
  static struct ui_stream *stb = NULL;

  stb = ui_out_stream_new (uiout);

  if (format == 'N')
    format = 0;

  get_saved_register (raw_buffer, &optim, (CORE_ADDR *) NULL,
		      deprecated_selected_frame,
		      regnum, (enum lval_type *) NULL);
  if (optim)
    {
      xasprintf (&mi_error_message, "Optimized out");
      return -1;
    }

  /* Convert raw data to virtual format if necessary.  */

  if (REGISTER_CONVERTIBLE (regnum))
    {
      REGISTER_CONVERT_TO_VIRTUAL (regnum, REGISTER_VIRTUAL_TYPE (regnum),
				   raw_buffer, virtual_buffer);
    }
  else
    memcpy (virtual_buffer, raw_buffer, REGISTER_VIRTUAL_SIZE (regnum));

  if (format == 'r')
    {
      int j;
      char *ptr, buf[1024];

      strcpy (buf, "0x");
      ptr = buf + 2;
      for (j = 0; j < REGISTER_RAW_SIZE (regnum); j++)
	{
	  register int idx = TARGET_BYTE_ORDER == BFD_ENDIAN_BIG ? j
	  : REGISTER_RAW_SIZE (regnum) - 1 - j;
	  sprintf (ptr, "%02x", (unsigned char) raw_buffer[idx]);
	  ptr += 2;
	}
      ui_out_field_string (uiout, "value", buf);
      /*fputs_filtered (buf, gdb_stdout); */
    }
  else
    {
      val_print (REGISTER_VIRTUAL_TYPE (regnum), virtual_buffer, 0, 0,
		 stb->stream, format, 1, 0, Val_pretty_default);
      ui_out_field_stream (uiout, "value", stb);
      ui_out_stream_delete (stb);
    }
  return 1;
}

/* Write given values into registers. The registers and values are
   given as pairs. The corresponding MI command is 
   -data-write-register-values <format> [<regnum1> <value1>...<regnumN> <valueN>]*/
enum mi_cmd_result
mi_cmd_data_write_register_values (char *command, char **argv, int argc)
{
  int regnum;
  int i;
  int numregs;
  LONGEST value;
  char format;

  /* Note that the test for a valid register must include checking the
     REGISTER_NAME because NUM_REGS may be allocated for the union of
     the register sets within a family of related processors.  In this
     case, some entries of REGISTER_NAME will change depending upon
     the particular processor being debugged.  */

  numregs = NUM_REGS;

  if (argc == 0)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_data_write_register_values: Usage: -data-write-register-values <format> [<regnum1> <value1>...<regnumN> <valueN>]");
      return MI_CMD_ERROR;
    }

  format = (int) argv[0][0];

  if (!target_has_registers)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_data_write_register_values: No registers.");
      return MI_CMD_ERROR;
    }

  if (!(argc - 1))
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_data_write_register_values: No regs and values specified.");
      return MI_CMD_ERROR;
    }

  if ((argc - 1) % 2)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_data_write_register_values: Regs and vals are not in pairs.");
      return MI_CMD_ERROR;
    }

  for (i = 1; i < argc; i = i + 2)
    {
      regnum = atoi (argv[i]);

      if (regnum >= 0
	  && regnum < numregs
	  && REGISTER_NAME (regnum) != NULL
	  && *REGISTER_NAME (regnum) != '\000')
	{
	  void *buffer;
	  struct cleanup *old_chain;

	  /* Get the value as a number */
	  value = parse_and_eval_address (argv[i + 1]);
	  /* Get the value into an array */
	  buffer = xmalloc (REGISTER_SIZE);
	  old_chain = make_cleanup (xfree, buffer);
	  store_signed_integer (buffer, REGISTER_SIZE, value);
	  /* Write it down */
	  deprecated_write_register_bytes (REGISTER_BYTE (regnum), buffer, REGISTER_RAW_SIZE (regnum));
	  /* Free the buffer.  */
	  do_cleanups (old_chain);
	}
      else
	{
	  xasprintf (&mi_error_message, "bad register number");
	  return MI_CMD_ERROR;
	}
    }
  return MI_CMD_DONE;
}

#if 0
/*This is commented out because we decided it was not useful. I leave
   it, just in case. ezannoni:1999-12-08 */

/* Assign a value to a variable. The expression argument must be in
   the form A=2 or "A = 2" (I.e. if there are spaces it needs to be
   quoted. */
enum mi_cmd_result
mi_cmd_data_assign (char *command, char **argv, int argc)
{
  struct expression *expr;
  struct cleanup *old_chain;

  if (argc != 1)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_data_assign: Usage: -data-assign expression");
      return MI_CMD_ERROR;
    }

  /* NOTE what follows is a clone of set_command(). FIXME: ezannoni
     01-12-1999: Need to decide what to do with this for libgdb purposes. */

  old_chain = make_cleanup_set_restore_scheduler_locking_mode (scheduler_locking_on);
  expr = parse_expression (argv[0]);
  make_cleanup (free_current_contents, &expr);
  evaluate_expression (expr);
  do_cleanups (old_chain);
  return MI_CMD_DONE;
}
#endif

/* Evaluate the value of the argument. The argument is an
   expression. If the expression contains spaces it needs to be
   included in double quotes. */
enum mi_cmd_result
mi_cmd_data_evaluate_expression (char *command, char **argv, int argc)
{
  struct expression *expr;
  struct cleanup *old_chain = NULL;
  struct value *val;
  struct ui_stream *stb = NULL;
  int unwinding_was_requested = 0;
  char *expr_string;

  stb = ui_out_stream_new (uiout);

  if (argc == 1)
    {
      expr_string = argv[0];
    }
  else if (argc == 2)
    {
      if (strcmp (argv[0], "-u") != 0)
	{
	  xasprintf (&mi_error_message,
		     "mi_cmd_data_evaluate_expression: Usage: "
		     "-data-evaluate-expression [-u] expression");
	  return MI_CMD_ERROR;
	}
      else
	{
          unwinding_was_requested = 1;
	  expr_string = argv[1];
	}
    }
  else
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_data_evaluate_expression: Usage: "
		 "-data-evaluate-expression [-u] expression");
      return MI_CMD_ERROR;
    }
  
  old_chain = make_cleanup_set_restore_scheduler_locking_mode (scheduler_locking_on);
  if (unwinding_was_requested)
    make_cleanup (set_unwind_on_signal, set_unwind_on_signal (1));

  expr = parse_expression (expr_string);

  make_cleanup (free_current_contents, &expr);

  val = evaluate_expression (expr);

  /* Print the result of the expression evaluation. */
  val_print (VALUE_TYPE (val), VALUE_CONTENTS (val),
	     VALUE_EMBEDDED_OFFSET (val), VALUE_ADDRESS (val),
	     stb->stream, 0, 0, 0, 0);

  ui_out_field_stream (uiout, "value", stb);
  ui_out_stream_delete (stb);

  do_cleanups (old_chain);

  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_target_download (char *args, int from_tty)
{
  char *run;
  struct cleanup *old_cleanups = NULL;

  xasprintf (&run, "load %s", args);
  old_cleanups = make_cleanup (xfree, run);
  execute_command (run, from_tty);

  do_cleanups (old_cleanups);
  return MI_CMD_DONE;
}

/* Connect to the remote target. */
enum mi_cmd_result
mi_cmd_target_select (char *args, int from_tty)
{
  char *run;
  struct cleanup *old_cleanups = NULL;

  xasprintf (&run, "target %s", args);
  old_cleanups = make_cleanup (xfree, run);

  /* target-select is always synchronous.  once the call has returned
     we know that we are connected. */
  /* NOTE: At present all targets that are connected are also
     (implicitly) talking to a halted target.  In the future this may
     change. */
  execute_command (run, from_tty);

  do_cleanups (old_cleanups);

  /* Issue the completion message here. */
  if (current_command_token)
    fputs_unfiltered (current_command_token, raw_stdout);
  fputs_unfiltered ("^connected", raw_stdout);
  do_exec_cleanups (ALL_CLEANUPS);
  return MI_CMD_QUIET;
}

/* DATA-MEMORY-READ:

   ADDR: start address of data to be dumped.
   WORD-FORMAT: a char indicating format for the ``word''. See 
   the ``x'' command.
   WORD-SIZE: size of each ``word''; 1,2,4, or 8 bytes
   NR_ROW: Number of rows.
   NR_COL: The number of colums (words per row).
   ASCHAR: (OPTIONAL) Append an ascii character dump to each row.  Use
   ASCHAR for unprintable characters.

   Reads SIZE*NR_ROW*NR_COL bytes starting at ADDR from memory and
   displayes them.  Returns:

   {addr="...",rowN={wordN="..." ,... [,ascii="..."]}, ...}

   Returns: 
   The number of bytes read is SIZE*ROW*COL. */

enum mi_cmd_result
mi_cmd_data_read_memory (char *command, char **argv, int argc)
{
  struct cleanup *cleanups = make_cleanup (null_cleanup, NULL);
  CORE_ADDR addr;
  long total_bytes;
  long nr_cols;
  long nr_rows;
  char word_format;
  struct type *word_type;
  long word_size;
  char word_asize;
  char aschar;
  char *mbuf;
  int nr_bytes;
  long offset = 0;
  int optind = 0;
  char *optarg;
  enum opt
    {
      OFFSET_OPT
    };
  static struct mi_opt opts[] =
  {
    {"o", OFFSET_OPT, 1},
    0
  };

  while (1)
    {
      int opt = mi_getopt ("mi_cmd_data_read_memory", argc, argv, opts,
			   &optind, &optarg);
      if (opt < 0)
	break;
      switch ((enum opt) opt)
	{
	case OFFSET_OPT:
	  offset = atol (optarg);
	  break;
	}
    }
  argv += optind;
  argc -= optind;

  if (argc < 5 || argc > 6)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_data_read_memory: Usage: ADDR WORD-FORMAT WORD-SIZE NR-ROWS NR-COLS [ASCHAR].");
      return MI_CMD_ERROR;
    }

  /* Extract all the arguments. */

  /* Start address of the memory dump. */
  addr = parse_and_eval_address (argv[0]) + offset;
  /* The format character to use when displaying a memory word. See
     the ``x'' command. */
  word_format = argv[1][0];
  /* The size of the memory word. */
  word_size = atol (argv[2]);
  switch (word_size)
    {
    case 1:
      word_type = builtin_type_int8;
      word_asize = 'b';
      break;
    case 2:
      word_type = builtin_type_int16;
      word_asize = 'h';
      break;
    case 4:
      word_type = builtin_type_int32;
      word_asize = 'w';
      break;
    case 8:
      word_type = builtin_type_int64;
      word_asize = 'g';
      break;
    default:
      word_type = builtin_type_int8;
      word_asize = 'b';
    }
  /* The number of rows */
  nr_rows = atol (argv[3]);
  if (nr_rows <= 0)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_data_read_memory: invalid number of rows.");
      return MI_CMD_ERROR;
    }
  /* number of bytes per row. */
  nr_cols = atol (argv[4]);
  if (nr_cols <= 0)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_data_read_memory: invalid number of columns.");
    }
  /* The un-printable character when printing ascii. */
  if (argc == 6)
    aschar = *argv[5];
  else
    aschar = 0;

  /* create a buffer and read it in. */
  total_bytes = word_size * nr_rows * nr_cols;
  mbuf = xcalloc (total_bytes, 1);
  make_cleanup (xfree, mbuf);
  if (mbuf == NULL)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_data_read_memory: out of memory.");
      return MI_CMD_ERROR;
    }
  nr_bytes = 0;
  while (nr_bytes < total_bytes)
    {
      int error;
      long num = target_read_memory_partial (addr + nr_bytes, mbuf + nr_bytes,
					     total_bytes - nr_bytes,
					     &error);
      if (num <= 0)
	break;
      nr_bytes += num;
    }

  /* output the header information. */
  ui_out_field_core_addr (uiout, "addr", addr);
  ui_out_field_int (uiout, "nr-bytes", nr_bytes);
  ui_out_field_int (uiout, "total-bytes", total_bytes);
  ui_out_field_core_addr (uiout, "next-row", addr + word_size * nr_cols);
  ui_out_field_core_addr (uiout, "prev-row", addr - word_size * nr_cols);
  ui_out_field_core_addr (uiout, "next-page", addr + total_bytes);
  ui_out_field_core_addr (uiout, "prev-page", addr - total_bytes);

  /* Build the result as a two dimentional table. */
  {
    struct ui_stream *stream = ui_out_stream_new (uiout);
    struct cleanup *cleanup_list_memory;
    int row;
    int row_byte;
    cleanup_list_memory = make_cleanup_ui_out_list_begin_end (uiout, "memory");
    for (row = 0, row_byte = 0;
	 row < nr_rows;
	 row++, row_byte += nr_cols * word_size)
      {
	int col;
	int col_byte;
	struct cleanup *cleanup_tuple;
	struct cleanup *cleanup_list_data;
	cleanup_tuple = make_cleanup_ui_out_tuple_begin_end (uiout, NULL);
	ui_out_field_core_addr (uiout, "addr", addr + row_byte);
	/* ui_out_field_core_addr_symbolic (uiout, "saddr", addr + row_byte); */
	cleanup_list_data = make_cleanup_ui_out_list_begin_end (uiout, "data");
	for (col = 0, col_byte = row_byte;
	     col < nr_cols;
	     col++, col_byte += word_size)
	  {
	    if (col_byte + word_size > nr_bytes)
	      {
		ui_out_field_string (uiout, NULL, "N/A");
	      }
	    else
	      {
		ui_file_rewind (stream->stream);
		print_scalar_formatted (mbuf + col_byte, word_type, word_format,
					word_asize, stream->stream);
		ui_out_field_stream (uiout, NULL, stream);
	      }
	  }
	do_cleanups (cleanup_list_data);
	if (aschar)
	  {
	    int byte;
	    ui_file_rewind (stream->stream);
	    for (byte = row_byte; byte < row_byte + word_size * nr_cols; byte++)
	      {
		if (byte >= nr_bytes)
		  {
		    fputc_unfiltered ('X', stream->stream);
		  }
		else if (mbuf[byte] < 32 || mbuf[byte] > 126)
		  {
		    fputc_unfiltered (aschar, stream->stream);
		  }
		else
		  fputc_unfiltered (mbuf[byte], stream->stream);
	      }
	    ui_out_field_stream (uiout, "ascii", stream);
	  }
	do_cleanups (cleanup_tuple);
      }
    ui_out_stream_delete (stream);
    do_cleanups (cleanup_list_memory);
  }
  do_cleanups (cleanups);
  return MI_CMD_DONE;
}

/* DATA-MEMORY-WRITE:

   COLUMN_OFFSET: optional argument. Must be preceeded by '-o'. The
   offset from the beginning of the memory grid row where the cell to
   be written is.
   ADDR: start address of the row in the memory grid where the memory
   cell is, if OFFSET_COLUMN is specified. Otherwise, the address of
   the location to write to.
   FORMAT: a char indicating format for the ``word''. See 
   the ``x'' command.
   WORD_SIZE: size of each ``word''; 1,2,4, or 8 bytes
   VALUE: value to be written into the memory address.

   Writes VALUE into ADDR + (COLUMN_OFFSET * WORD_SIZE).

   Prints nothing. */
enum mi_cmd_result
mi_cmd_data_write_memory (char *command, char **argv, int argc)
{
  CORE_ADDR addr;
  char word_format;
  long word_size;
  /* FIXME: ezannoni 2000-02-17 LONGEST could possibly not be big
     enough when using a compiler other than GCC. */
  LONGEST value;
  void *buffer;
  struct cleanup *old_chain;
  long offset = 0;
  int optind = 0;
  char *optarg;
  enum opt
    {
      OFFSET_OPT
    };
  static struct mi_opt opts[] =
  {
    {"o", OFFSET_OPT, 1},
    0
  };

  while (1)
    {
      int opt = mi_getopt ("mi_cmd_data_write_memory", argc, argv, opts,
			   &optind, &optarg);
      if (opt < 0)
	break;
      switch ((enum opt) opt)
	{
	case OFFSET_OPT:
	  offset = atol (optarg);
	  break;
	}
    }
  argv += optind;
  argc -= optind;

  if (argc != 4)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_data_write_memory: Usage: [-o COLUMN_OFFSET] ADDR FORMAT WORD-SIZE VALUE.");
      return MI_CMD_ERROR;
    }

  /* Extract all the arguments. */
  /* Start address of the memory dump. */
  addr = parse_and_eval_address (argv[0]);
  /* The format character to use when displaying a memory word. See
     the ``x'' command. */
  word_format = argv[1][0];
  /* The size of the memory word. */
  word_size = atol (argv[2]);

  /* Calculate the real address of the write destination. */
  addr += (offset * word_size);

  /* Get the value as a number */
  value = parse_and_eval_address (argv[3]);
  /* Get the value into an array */
  buffer = xmalloc (word_size);
  old_chain = make_cleanup (xfree, buffer);
  store_signed_integer (buffer, word_size, value);
  /* Write it down to memory */
  write_memory (addr, buffer, word_size);
  /* Free the buffer.  */
  do_cleanups (old_chain);

  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_enable_timings (char *command, char **argv, int argc)
{
  if (argc == 0)
    do_timings = 1;
  else if (argc == 1)
    {
      if (strcmp (argv[0], "yes") == 0)
	do_timings = 1;
      else if (strcmp (argv[0], "no") == 0)
	do_timings = 0;
      else
	goto usage_error;
    }
  else
    goto usage_error;
    
  return MI_CMD_DONE;

 usage_error:
  error ("mi_cmd_enable_timings: Usage: %s {yes|no}", command);
  return MI_CMD_ERROR;
  
}

enum mi_cmd_result
mi_cmd_mi_verify_command (char *command, char **argv, int argc)
{
  char 		*command_name = argv[0];
  struct mi_cmd *cmd;
  
  if (argc != 1)
    {
      error ("mi_cmd_mi_verify_command: Usage: MI_COMMAND_NAME.");
    }

  cmd = mi_lookup (command_name);

  ui_out_field_string (uiout, "name", command_name);
  if (cmd != NULL) 
    {
       ui_out_field_string (uiout, "defined", "true");
       ui_out_field_string (uiout, "implemented",
            ((cmd->cli != NULL) ||
             (cmd->argv_func != NULL) ||
             (cmd->args_func != NULL)) ? "true" : "false");
    }
  else 
    {
       ui_out_field_string (uiout, "defined", "false");
    }
  
  return MI_CMD_DONE;
}


enum mi_cmd_result
mi_cmd_mi_no_op (char *command, char **argv, int argc)
{
  /* how does one know when a bunch of MI commands have finished being processed?
     just send a no-op as the last command and look for that...
   */
  return MI_CMD_DONE;
}

/* Execute a command within a safe environment.  Return >0 for
   ok. Return <0 for supress prompt.  Return 0 to have the error
   extracted from error_last_message(). */

static int
captured_mi_execute_command (struct ui_out *uiout, void *data)
{
  struct captured_mi_execute_command_args *args =
    (struct captured_mi_execute_command_args *) data;
  struct mi_parse *parse = args->command;
  struct ui_out *saved_uiout = uiout;
  enum mi_cmd_result rc = MI_CMD_DONE;
  struct mi_timestamp cmd_finished;

  switch (parse->op)
    {

    case MI_COMMAND:
      /* A MI command was read from the input stream */
      if (mi_debug_p)
	/* FIXME: gdb_???? */
	fprintf_unfiltered (raw_stdout, " token=`%s' command=`%s' args=`%s'\n",
			    parse->token, parse->command, parse->args);
      /* FIXME: cagney/1999-09-25: Rather than this convoluted
         condition expression, each function should return an
         indication of what action is required and then switch on
         that. */
      args->action = EXECUTE_COMMAND_DISPLAY_PROMPT;

      /* This is a bit of a hack.  We need to pass the cmd_start down to
	 the mi command so that it can be copied into continuations if
	 needs be.  But we don't pass the parse but just a few bits
	 instead.  So we need to route it through this instead... */

      
      current_command_token = parse->token;

      if (do_timings)
	current_command_ts = parse->cmd_start;

      args->rc = mi_cmd_execute (parse);

      if (do_timings)
          timestamp (&cmd_finished);
      
      if (!target_can_async_p () || !target_executing
	  || mi_command_completes_while_target_executing (parse->command))
	{
	  /* print the result if there were no errors 
	   
	     Remember that on the way out of executing a command, you have
	     to directly use the mi_interp's uiout, since the command could 
	     have reset the interpreter, in which case the current uiout 
	     will most likely crash in the mi_out_* routines. 
	  */
			    
	  if (args->rc == MI_CMD_DONE)
	    {
	      fputs_unfiltered (parse->token, raw_stdout);
	      fputs_unfiltered ("^done", raw_stdout);
	      mi_out_put (saved_uiout, raw_stdout);
	      mi_out_rewind (saved_uiout);
	      /* Have to check cmd_start, since the command could be
		 "mi-enable-timings". */
	      if (do_timings && parse->cmd_start)
		  print_diff (parse->cmd_start, &cmd_finished);
	      fputs_unfiltered ("\n", raw_stdout);
	    }
	  else if (args->rc == MI_CMD_QUIET)
	    {
	      /* Just need to flush and print the timings here. */
	      
	      mi_out_put (saved_uiout, raw_stdout);
	      mi_out_rewind (saved_uiout);
	      if (do_timings && parse->cmd_start)
		  print_diff (parse->cmd_start, &cmd_finished);
	      fputs_unfiltered ("\n", raw_stdout);
	    }
	  else if (args->rc == MI_CMD_ERROR)
	    {
	      if (mi_error_message)
		{
		  fputs_unfiltered (parse->token, raw_stdout);
		  fputs_unfiltered ("^error,msg=\"", raw_stdout);
		  fputstr_unfiltered (mi_error_message, '"', raw_stdout);
		  xfree (mi_error_message);
		  fputs_unfiltered ("\"\n", raw_stdout);
		}
	      mi_out_rewind (saved_uiout);
	    }
	  else if (args->rc == MI_CMD_CAUGHT_ERROR)
	    {
	      mi_out_rewind (saved_uiout);
	      args->action = EXECUTE_COMMAND_DISPLAY_ERROR;
	      return 1;
	    }
	  else
	    mi_out_rewind (saved_uiout);
	}
      else if (sync_execution)
	{
	  /* Don't print the prompt. We are executing the target in
	     synchronous mode. */
	  args->action = EXECUTE_COMMAND_SUPRESS_PROMPT;
	  return 1;
	}
      break;

    case CLI_COMMAND:
      /* A CLI command was read from the input stream */
      /* This will be removed as soon as we have a complete set of
         mi commands */
      /* echo the command on the console. */
      fprintf_unfiltered (gdb_stdlog, "%s\n", parse->command);
      /* FIXME: If the command string has something that looks like 
         a format spec (e.g. %s) we will get a core dump */
      mi_execute_cli_command ("%s", parse->command);
      /* print the result */
      /* FIXME: Check for errors here. */
      fputs_unfiltered (parse->token, raw_stdout);
      fputs_unfiltered ("^done", raw_stdout);

      /* Be careful to route this through the real mi uiout, since
         the command could be "set interpreter console", and so we 
	 might not have the uiout around any more... */

      mi_out_put (saved_uiout, raw_stdout);
      mi_out_rewind (saved_uiout);
      fputs_unfiltered ("\n", raw_stdout);
      args->action = EXECUTE_COMMAND_DISPLAY_PROMPT;
      args->rc = MI_CMD_DONE;
      break;

    }
  return rc;
}


void
mi_execute_command (char *cmd, int from_tty)
{
  struct mi_parse *command;
  struct ui_out *saved_uiout = uiout;
  struct captured_mi_execute_command_args args;
  int result;

  args.rc = MI_CMD_DONE;

  /* This is to handle EOF (^D). We just quit gdb. */
  /* FIXME: we should call some API function here. */
  if (cmd == 0)
    quit_force (NULL, from_tty);

  command = mi_parse (cmd);

  if (command != NULL)
    {
      /* FIXME: cagney/1999-11-04: Can this use of catch_exceptions either
         be pushed even further down or even eliminated? */
      if (do_timings)
	{
	  command->cmd_start = (struct mi_timestamp *)
	    xmalloc (sizeof (struct mi_timestamp));
	  timestamp (command->cmd_start);
	}

      args.command = command;
      result = catch_exceptions (uiout, captured_mi_execute_command, &args, "",
				 RETURN_MASK_ALL);

      if (args.action == EXECUTE_COMMAND_SUPRESS_PROMPT)
	{
	  /* The command is executing synchronously.  Bail out early
	     suppressing the finished prompt. */
	  mi_parse_free (command);
	  return;
	}
      if (args.action == EXECUTE_COMMAND_DISPLAY_ERROR || result < 0)
	{
	  char *msg = error_last_message ();

	  make_cleanup (xfree, msg);

	  /* The command execution failed and error() was called
	     somewhere. Try to dump the accumulated output from the command. */
          ui_out_cleanup_after_error (saved_uiout);
	  fputs_unfiltered (command->token, raw_stdout);
	  fputs_unfiltered ("^error,msg=\"", raw_stdout);
	  fputstr_unfiltered (msg, '"', raw_stdout);
	  fputs_unfiltered ("\"", raw_stdout);
          mi_out_put (saved_uiout, raw_stdout);
          mi_out_rewind (saved_uiout);
	  fputs_unfiltered ("\n", raw_stdout);
	}
      mi_parse_free (command);
    }

  if (args.rc != MI_CMD_QUIET)
    {
      fputs_unfiltered ("(gdb) \n", raw_stdout);
      /* print any buffered hook code */
      /* ..... */
    }

  gdb_flush (raw_stdout);
}

static int 
mi_command_completes_while_target_executing (char *command)
{
  if (strcmp (command, "exec-interrupt")
      && strcmp (command, "exec-status"))
    return 0;
  else
    return 1;
}

static enum mi_cmd_result
mi_cmd_execute (struct mi_parse *parse)
{

  if (parse->cmd->argv_func != NULL
      || parse->cmd->args_func != NULL)
    {
      
      if (target_executing)
	{
	  if (!mi_command_completes_while_target_executing(parse->command))
	    {
	      fputs_unfiltered (parse->token, raw_stdout);
	      fputs_unfiltered ("^error,msg=\"", raw_stdout);
	      fputs_unfiltered ("Cannot execute command ", raw_stdout);
	      fputstr_unfiltered (parse->command, '"', raw_stdout);
	      fputs_unfiltered (" while target running", raw_stdout);
	      fputs_unfiltered ("\"\n", raw_stdout);
	      return MI_CMD_ERROR;
	    }
	}

      /* FIXME: DELETE THIS! */
      if (parse->cmd->args_func != NULL)
	return parse->cmd->args_func (parse->args, 0 /*from_tty */ );
      return parse->cmd->argv_func (parse->command, parse->argv, parse->argc);
    }
  else if (parse->cmd->cli != 0)
    {
      /* FIXME: DELETE THIS. */
      /* The operation is still implemented by a cli command */
      /* Must be a synchronous one */
      mi_execute_cli_command (parse->cmd->cli, parse->args);
      return MI_CMD_DONE;
    }
  else
    {
      /* FIXME: DELETE THIS. */
      fputs_unfiltered (parse->token, raw_stdout);
      fputs_unfiltered ("^error,msg=\"", raw_stdout);
      fputs_unfiltered ("Undefined mi command: ", raw_stdout);
      fputstr_unfiltered (parse->command, '"', raw_stdout);
      fputs_unfiltered (" (missing implementation)", raw_stdout);
      fputs_unfiltered ("\"\n", raw_stdout);
      return MI_CMD_ERROR;
    }
}

static void
mi_execute_command_wrapper (char *cmd)
{
  mi_execute_command (cmd, stdin == instream);
}

/* FIXME: This is just a hack so we can get some extra commands going.
   We don't want to channel things through the CLI, but call libgdb directly */
/* Use only for synchronous commands */

void
mi_execute_cli_command (const char *cli, char *args)
{
  if (cli != 0)
    {
      struct cleanup *old_cleanups;
      char *run;
      xasprintf (&run, cli, args);
      if (mi_debug_p)
	/* FIXME: gdb_???? */
	fprintf_unfiltered (gdb_stdout, "cli=%s run=%s\n",
			    cli, run);
      old_cleanups = make_cleanup (xfree, run);
      execute_command ( /*ui */ run, 0 /*from_tty */ );
      do_cleanups (old_cleanups);
      return;
    }
}

enum mi_cmd_result
mi_execute_async_cli_command (char *mi, char *args, int from_tty)
{
  char *run;
  char *async_args;

  if (!target_can_async_p ())
    {
      struct cleanup *old_cleanups;
      xasprintf (&run, "%s %s", mi, args);
      old_cleanups = make_cleanup (xfree, run);

      /* NOTE: For synchronous targets asynchronous behavour is faked by
         printing out the GDB prompt before we even try to execute the
         command. */
      if (current_command_token)
	fputs_unfiltered (current_command_token, raw_stdout);
      fputs_unfiltered ("^running\n", raw_stdout);
      fputs_unfiltered ("(gdb) \n", raw_stdout);
      gdb_flush (raw_stdout);
      
      execute_command ( /*ui */ run, 0 /*from_tty */ );

      /* Do this before doing any printing.  It would appear that some
         print code leaves garbage around in the buffer. */
      do_cleanups (old_cleanups);
      /* If the target was doing the operation synchronously we fake
         the stopped message. */
      if (current_command_token)
        {
	  fputs_unfiltered (current_command_token, raw_stdout);
        }
      fputs_unfiltered ("*stopped", raw_stdout);
      if (current_command_ts)
	print_diff_now (current_command_ts);
      return MI_CMD_QUIET;
    }
  else
    {
      struct mi_continuation_arg *arg = NULL; 
      struct cleanup *old_cleanups = NULL;
      int retval;

      async_args = (char *) xmalloc (strlen (args) + 2);
      old_cleanups = make_exec_cleanup (free, async_args);
      strcpy (async_args, args);
      strcat (async_args, "&");
      xasprintf (&run, "%s %s", mi, async_args);
      make_exec_cleanup (free, run);

      /* Transfer the command token to the continuation.  That
	 will now print the results associated with this command. 
         Tricky point: have to add the continuation BEFORE running
         execute_command, or it will get run before any continuations
         that might get added by execute_command, in which case the
         cleanups will be out of order. */
      
      arg = setup_continuation_arg (old_cleanups);
      add_continuation (mi_exec_async_cli_cmd_continuation, 
			(struct continuation_arg *) arg);

      retval = safe_execute_command ( /*ui */ run, 0 /*from_tty */ );

      if (target_executing)
	{
	  if (current_command_token)
	    fputs_unfiltered (current_command_token, raw_stdout);
	  fputs_unfiltered ("^running\n", raw_stdout);
	  
	}
      else
	{
	  /* If we didn't manage to set the inferior going, that's
	     most likely an error... */
	  discard_all_continuations ();
	  free_continuation_arg (arg);
	  mi_error_message = error_last_message ();
	  return MI_CMD_ERROR;
	}

    }

  return MI_CMD_DONE;
}

void
mi_exec_async_cli_cmd_continuation (struct continuation_arg *in_arg)
{
  struct mi_continuation_arg *arg =
    (struct mi_continuation_arg *) in_arg;

  if (!target_executing)
    {
      if (arg && arg->token)
	{
	  fputs_unfiltered (arg->token, raw_stdout);
	}

      /* Do the cleanups.  Remember to set this to NULL, 
	 since we are passing the arg to the next continuation
	 if the target restarts, and we don't want to do these
	 cleanups again. */
      if (arg->cleanups)
	{
	  do_exec_cleanups (arg->cleanups);
	  arg->cleanups = NULL;
	}
      
      fputs_unfiltered ("*stopped", raw_stdout);
      if (do_timings && arg && arg->timestamp)
	print_diff_now (arg->timestamp);
      mi_out_put (uiout, raw_stdout);
      fputs_unfiltered ("\n", raw_stdout);
      
      /* Now run the actions for this breakpoint.  This may start
	 the target going again, but we shouldn't have to do
	 anything special about that, since the continuation
	 hooks for the commands will take care of that. */

      /* Tricky bit: we have to register the continuation before
	 calling bpstat_do_actions, or the continuations will
	 be out of order.  If we don't start, then we have to 
         discard the continuations.  N.B. We are assuming here
         that nothing that CALLED us has registered a continuation,
         if that becomes a problem, we will need to add the ability
         to discard continuations up to (and including) a given 
         continuation. */

      if (target_can_async_p ())
	{
	  /* Reset the timer, we will just accumulate times for this
	     command. */
	  if (do_timings && arg && arg->timestamp)
	    timestamp (arg->timestamp);
	  add_continuation (mi_exec_async_cli_cmd_continuation, 
			    (struct continuation_arg *) arg);
	}

      bpstat_do_actions (&stop_bpstat);
      
      if (!target_executing)
	{
	  /* Okay, we didn't need to use the continuation,
	     so discard it now. */
	  if (target_can_async_p ())
	    {
	      discard_all_continuations ();
	      free_continuation_arg (arg);
	    }
	  fputs_unfiltered ("(gdb) \n", raw_stdout);
	  gdb_flush (raw_stdout);
	}
      else
	{
	  if (arg && arg->token)
	    fputs_unfiltered (arg->token, raw_stdout);
	  
	  ui_out_field_string (uiout, "reason", "breakpoint-command");
	  fputs_unfiltered ("*started", raw_stdout);
	  mi_out_put (uiout, raw_stdout);
	  fputs_unfiltered ("\n", raw_stdout);
	  gdb_flush (raw_stdout);
	}	  
    }
  else if (target_can_async_p ())
    {
      add_continuation (mi_exec_async_cli_cmd_continuation, in_arg);
    }
}

static char *
mi_input (char *buf)
{
  return gdb_readline (NULL);
}

static void
mi_load_progress (const char *section_name,
		  unsigned long sent_so_far,
		  unsigned long total_section,
		  unsigned long total_sent,
		  unsigned long grand_total)
{
  struct timeval time_now, delta, update_threshold;
  static struct timeval last_update;
  static char *previous_sect_name = NULL;
  int new_section;

  if (!interpreter_p || strncmp (interpreter_p, "mi", 2) != 0)
    return;

  update_threshold.tv_sec = 0;
  update_threshold.tv_usec = 500000;
  gettimeofday (&time_now, NULL);

  delta.tv_usec = time_now.tv_usec - last_update.tv_usec;
  delta.tv_sec = time_now.tv_sec - last_update.tv_sec;

  if (delta.tv_usec < 0)
    {
      delta.tv_sec -= 1;
      delta.tv_usec += 1000000;
    }

  new_section = (previous_sect_name ?
		 strcmp (previous_sect_name, section_name) : 1);
  if (new_section)
    {
      struct cleanup *cleanup_tuple;
      xfree (previous_sect_name);
      previous_sect_name = xstrdup (section_name);

      if (current_command_token)
	fputs_unfiltered (current_command_token, raw_stdout);
      fputs_unfiltered ("+download", raw_stdout);
      cleanup_tuple = make_cleanup_ui_out_tuple_begin_end (uiout, NULL);
      ui_out_field_string (uiout, "section", section_name);
      ui_out_field_int (uiout, "section-size", total_section);
      ui_out_field_int (uiout, "total-size", grand_total);
      do_cleanups (cleanup_tuple);
      mi_out_put (uiout, raw_stdout);
      fputs_unfiltered ("\n", raw_stdout);
      gdb_flush (raw_stdout);
    }

  if (delta.tv_sec >= update_threshold.tv_sec &&
      delta.tv_usec >= update_threshold.tv_usec)
    {
      struct cleanup *cleanup_tuple;
      last_update.tv_sec = time_now.tv_sec;
      last_update.tv_usec = time_now.tv_usec;
      if (current_command_token)
	fputs_unfiltered (current_command_token, raw_stdout);
      fputs_unfiltered ("+download", raw_stdout);
      cleanup_tuple = make_cleanup_ui_out_tuple_begin_end (uiout, NULL);
      ui_out_field_string (uiout, "section", section_name);
      ui_out_field_int (uiout, "section-sent", sent_so_far);
      ui_out_field_int (uiout, "section-size", total_section);
      ui_out_field_int (uiout, "total-sent", total_sent);
      ui_out_field_int (uiout, "total-size", grand_total);
      do_cleanups (cleanup_tuple);
      mi_out_put (uiout, raw_stdout);
      fputs_unfiltered ("\n", raw_stdout);
      gdb_flush (raw_stdout);
    }
}

static void
mi_command_loop (int mi_version)
{
  if (mi_version <= 1)
    {
      /* HACK: Force stdout/stderr to point at the console.  This avoids
         any potential side effects caused by legacy code that is still
         using the TUI / fputs_unfiltered_hook */
      raw_stdout = stdio_fileopen (stdout);
      /* Route normal output through the MIx */
      gdb_stdout = mi_console_file_new (raw_stdout, "~");
    }

  /* Route error and log output through the MI */

  gdb_stderr = mi_console_file_new (raw_stdout, "&");
  gdb_stdlog = gdb_stderr;
  /* Route target output through the MI. */
  gdb_stdtarg = mi_console_file_new (raw_stdout, "@");

  /* HACK: Poke the ui_out table directly.  Should we be creating a
     mi_out object wired up to the above gdb_stdout / gdb_stderr? */
  uiout = gdb_interpreter_ui_out (gdb_current_interpreter ());

  /* HACK: Override any other interpreter hooks.  We need to create a
     real event table and pass in that. */
  init_ui_hook = 0;
  /* command_loop_hook = 0; */
  print_frame_info_listing_hook = 0;
  query_hook = 0;
  warning_hook = 0;
  create_breakpoint_hook = 0;
  delete_breakpoint_hook = 0;
  modify_breakpoint_hook = 0;
  interactive_hook = 0;
  registers_changed_hook = 0;
  readline_begin_hook = 0;
  readline_hook = 0;
  readline_end_hook = 0;
  register_changed_hook = 0;
  memory_changed_hook = 0;
  context_hook = 0;
  target_wait_hook = 0;
  call_command_hook = 0;
  error_hook = 0;
  error_begin_hook = 0;
  show_load_progress = mi_load_progress;

  /* Turn off 8 bit strings in quoted output.  Any character with the
     high bit set is printed using C's octal format. */
  sevenbit_strings = 1;

  /* Tell the world that we're alive */
  fputs_unfiltered ("(gdb) \n", raw_stdout);
  gdb_flush (raw_stdout);

  if (!event_loop_p)
    simplified_command_loop (mi_input, mi_execute_command);
  else
    start_event_loop ();
}

static void
mi0_command_loop (void)
{
  mi_command_loop (0);
}

static void
mi1_command_loop (void)
{
  mi_command_loop (1);
}

static void
mi2_command_loop (void)
{
  mi_command_loop (2);
}

static void
setup_architecture_data (void)
{
  /* don't trust REGISTER_BYTES to be zero. */
  old_regs = xmalloc (REGISTER_BYTES + 1);
  memset (old_regs, 0, REGISTER_BYTES + 1);
}

static void
mi_init_ui (char *arg0)
{
  if (strlen (interpreter_p) <= 2 ||
      interpreter_p[2] > '1')
    {
      /* HACK: Force stdout/stderr to point at the console.  This avoids
         any potential side effects caused by legacy code that is still
         using the TUI / fputs_unfiltered_hook */
      raw_stdout = stdio_fileopen (stdout);
      /* Route normal output through the MIx */
      gdb_stdout = mi_console_file_new (raw_stdout, "~");
    }
}

static struct gdb_interpreter *
mi_create_interpreter (char *name, int mi_version)
{
  struct gdb_interpreter *interp;

  interp = gdb_new_interpreter (name, (void *) mi_version,
				mi_out_new (mi_version), 
				mi_interpreter_init,
				mi_interpreter_resume,
				NULL /* do one event proc */,
				mi_interpreter_suspend,
				mi_interpreter_delete,
				mi_interpreter_exec,
				mi_interpreter_prompt,
				NULL);
  if (interp == NULL)
    error ("Couldn't allocate a new interpreter for the mi interpreter\n");
  if (gdb_add_interpreter (interp) != 1)
    error ("Couldn't add the mi interpreter to gdb.\n");

  return interp;
}

void
_initialize_mi_main (void)
{
  static int init = 0;
  struct cmd_list_element *cmd;

  if (init)
    return;
  init = 1;

  miunspec_interp = mi_create_interpreter ("mi", 0);
  mi0_interp = mi_create_interpreter ("mi0", 0);
  mi1_interp = mi_create_interpreter ("mi1", 1);
  mi2_interp = mi_create_interpreter ("mi2", 2);

  /* Lets create a gdb "set" variable to control
     mi timings.  This seems gross, but it will allow
     control from the .gdbinit. */
  cmd = add_set_cmd ("mi-timings-enabled", class_obscure, var_boolean, 
		     (char *) &do_timings,
		     "Set whether timing information is displayed for mi commands.",
		     &setlist);
  add_show_from_set (cmd, &showlist);
}

int 
mi_interpreter_init (void *data)
{
  /* Why is this a part of the mi architecture? */
  
  setup_architecture_data ();
  
  /* HACK: We need to force stdout/stderr to point at the console.  This avoids
     any potential side effects caused by legacy code that is still
     using the TUI / fputs_unfiltered_hook.  So we set up output channels for
     this now, and swap them in when we are run. */
     
  mi_init_ui (NULL);

  raw_stdout = stdio_fileopen (stdout);
  /* Route normal output through the MIx */
  mi_stdout = mi_console_file_new (raw_stdout, "~");
  /* Route error and log output through the MI */
  mi_stderr = mi_console_file_new (raw_stdout, "&");
  mi_stdlog = mi_stderr;
  /* Route target output through the MI. */
  mi_stdtarg = mi_console_file_new (raw_stdout, "@");

  return 1;
}

int 
mi_interpreter_resume (void *data)
{

  /* As per hack note in mi_interpreter_init, swap in the output channels... */
  
  gdb_setup_readline ();
  register_gdbarch_swap (&old_regs, sizeof (old_regs), NULL);
  register_gdbarch_swap (NULL, 0, setup_architecture_data);
  if (event_loop_p)
    {
      /* These overwrite some of the initialization done in
        _intialize_event_loop. */
      call_readline = gdb_readline2;
      input_handler = mi_execute_command_wrapper;
      add_file_handler (input_fd, stdin_event_handler, 0);
      async_command_editing_p = 0;
      /* FIXME: This is a total hack for now.  PB's use of the MI implicitly
	 relies on a bug in the async support which allows asynchronous
	 commands to leak through the commmand loop.  The bug involves 
	 (but is not limited to) the fact that sync_execution was
	 erroneously initialized to 0.  Duplicate by initializing it
	 thus here... */
      sync_execution = 0;
    }    

  gdb_stdout = mi_stdout;
  /* Route error and log output through the MI */
  gdb_stderr = mi_stderr;
  gdb_stdlog = mi_stdlog;
  /* Route target output through the MI. */
  gdb_stdtarg = mi_stdtarg;
  
  /* Replace all the hooks that we know about.  There really needs to be a better way
     of doing this... */

  clear_interpreter_hooks ();

  if ((int) data == 0)
    command_loop_hook = mi0_command_loop;
  else if ((int) data == 1)
    command_loop_hook = mi1_command_loop;
  else
    command_loop_hook = mi2_command_loop;

  show_load_progress = mi_load_progress;
  print_frame_more_info_hook = mi_print_frame_more_info;

  /* Turn off 8 bit strings in quoted output.  Any character with the
     high bit set is printed using C's octal format. */
  sevenbit_strings = 1;

  return 1;
}

int 
mi_interpreter_suspend (void *data)
{
  gdb_disable_readline ();
  return 1;
}

int
mi_interpreter_delete (void *data) 
{
  return 1;
}

int 
mi_interpreter_prompt(void *data, char *new_prompt)
{
  return 1;
}

int 
mi_interpreter_exec(void *data, char *command)
{
  mi_execute_command (command, 0);

  return 1;
}

int 
mi_do_one_event (void *data)
{
  return 1;
}

void
mi_interpreter_exec_continuation (struct continuation_arg *in_arg)
{
  struct mi_continuation_arg *arg 
    = (struct mi_continuation_arg *) in_arg;

  if (!target_executing) 
    {
      /* This is a little tricky because bpstat_do_actions can
       restart the inferior.  So first say we have stopped,
      and flush the output so we get the reason aligned correctly,
      then run the breakpoint actions, and if they have restarted
      the inferior, suppress the prompt. */

      if (arg->cleanups != NULL)
	do_exec_cleanups (arg->cleanups);

      if (arg && arg->token)
	fputs_unfiltered (arg->token, raw_stdout);

      fputs_unfiltered ("*stopped", raw_stdout);
      if (do_timings && arg && arg->timestamp)
	print_diff_now (arg->timestamp);
      mi_out_put (uiout, raw_stdout);
      fputs_unfiltered ("\n", raw_stdout);
      
      /* Tricky point - we need to add this continuation 
	 before we run the actions, since one of the breakpoint commands
	 could have added a continuation, and ours would bet in
	 front of theirs, and then the cleanups would be out of order. */

      if (target_can_async_p()) 
	{
	  if (arg && arg->timestamp)
	    timestamp (arg->timestamp);
	  
	  add_continuation (mi_interpreter_exec_continuation, 
			  (struct continuation_arg *) arg);
	}

      bpstat_do_actions (&stop_bpstat);
      
      if (!target_executing)
	{
	  if (target_can_async_p ())
	    {
	      discard_all_continuations ();
	      free_continuation_arg (arg);
	    }
	  fputs_unfiltered ("(gdb) \n", raw_stdout);
	}
      else
	{
	  ui_out_field_string (uiout, "reason", "breakpoint-command");
	  if (arg && arg->token)
	    fputs_unfiltered (arg->token, raw_stdout);
	  fputs_unfiltered ("*started", raw_stdout);
	  if (do_timings && arg && arg->timestamp)
	    print_diff_now (arg->timestamp);
	  mi_out_put (uiout, raw_stdout);
	  fputs_unfiltered ("\n", raw_stdout);
	}
      
      gdb_flush (raw_stdout);
      
    }
  else if (target_can_async_p()) 
    {
      add_continuation (mi_interpreter_exec_continuation, in_arg);
    }
}

/* This is kind of a hack.  When we run a breakpoint command in the mi,
   execute_control_command is going to do mi_cmd_interpreter_exec so that
   it will be treated as a cli command.  But if that command sets the 
   target running, then it will register a continuation.  But the real
   command that set it going will already be doing that.  So we just
   flag mi_cmd_interpreter_exec not to register the continuation in
   this case. */

static int mi_dont_register_continuation = 0;

void
mi_interpreter_exec_bp_cmd (char *command, char **argv, int argc)
{
  mi_dont_register_continuation = 1;
  mi_cmd_interpreter_exec (command, argv, argc);
  mi_dont_register_continuation = 0;
}

enum mi_cmd_result 
mi_cmd_interpreter_exec (char *command, char **argv, int argc)
{
  struct gdb_interpreter *old_interp, *interp_to_use;
  enum mi_cmd_result result = MI_CMD_DONE;
  int i, old_quiet;
  
  if (argc < 2)
    {
      asprintf (&mi_error_message, 
		"Wrong # or arguments, should be \"%s interp cmd <cmd ...>\".",
		command);
      return MI_CMD_ERROR;
    }

  old_interp = gdb_current_interpreter ();
  mi_interp = gdb_current_interpreter ();
  
  interp_to_use = gdb_lookup_interpreter (argv[0]);
  if (interp_to_use == NULL)
    {
      asprintf (&mi_error_message,
		"Could not find interpreter \"%s\".", argv[0]);
      return MI_CMD_ERROR;
    }

  if (!interp_to_use->exec_proc)
    {
      asprintf (&mi_error_message, "Interpreter \"%s\" does not support command execution.",
		argv[0]);
      return MI_CMD_ERROR;
    }

  old_quiet = gdb_interpreter_set_quiet (interp_to_use, 1);

  if (!gdb_set_interpreter (interp_to_use))
    {
      asprintf (&mi_error_message,
		"Could not switch to interpreter \"%s\".", argv[0]);
      return MI_CMD_ERROR;
    }

  /* Insert the MI out hooks, making sure to also call the interpreter's hooks
     if it has any. */

  mi_insert_notify_hooks ();

  /* Now run the code... */

  for (i = 1; i < argc; i++) {
    char *buff = NULL;
    /* Do this in a cleaner way...  We want to force execution to be
       asynchronous for commands that run the target.  */
    if (target_can_async_p () && (strncmp (argv[0], "console", 7) == 0))
      {
	int len = strlen (argv[i]);
	buff = xmalloc (len + 2);
	memcpy (buff, argv[i], len);
	buff[len] = '&';
	buff[len + 1] = '\0';
      }
    
    /* We had to set sync_execution = 0 for the mi (well really for Project
       Builder's use of the mi - particularly so interrupting would work.
       But for console commands to work, we need to initialize it to 1 -
       since that is what the cli expects - before running the command,
       and then set it back to 0 when we are done. */
    sync_execution = 1;
    if (!interp_to_use->exec_proc (interp_to_use->data, argv[i]))
      {
	asprintf (&mi_error_message,
		  "mi_interpreter_execute: error in command: \"%s\".",
		  argv[i]);
	
	result = MI_CMD_ERROR;
	break;
      }
    xfree (buff);
    do_exec_error_cleanups (ALL_CLEANUPS);
    sync_execution = 0;

  }
  
  /* Now do the switch... */

  gdb_set_interpreter (old_interp);
  mi_interp = NULL;

  mi_remove_notify_hooks ();
  gdb_interpreter_set_quiet (interp_to_use, old_quiet);

  /* Okay, now let's see if the command set the inferior going...
     Tricky point - have to do this AFTER resetting the interpreter, since
     changing the interpreter will clear out all the continuations for
     that interpreter... */
  
  if (target_can_async_p () && target_executing
      && !mi_dont_register_continuation)
    {
      struct mi_continuation_arg *cont_args = 
	setup_continuation_arg (NULL);

      if (current_command_token)
	fputs_unfiltered (current_command_token, raw_stdout);

      fputs_unfiltered ("^running\n", raw_stdout);
      add_continuation (mi_interpreter_exec_continuation, 
			(void *) cont_args);
    }

  return result;
}

enum mi_cmd_result 
mi_cmd_interpreter_set (char *command, char **argv, int argc)
{
  struct gdb_interpreter *interp;
  int result;
  
  if (argc != 1) 
    {  
      asprintf (&mi_error_message, "mi_cmd_interpreter_set: wrong #of args, should be 1");
      return MI_CMD_ERROR;
    }
  interp = gdb_lookup_interpreter (argv[0]);
  if (interp == NULL)
    {
      asprintf (&mi_error_message, "mi_cmd_interpreter_set: could not find interpreter %s", argv[0]);
      return MI_CMD_ERROR;
    }

  result = gdb_set_interpreter (interp);	
  if (result != 1) 
    {
      asprintf (&mi_error_message, "mi_cmd_interpreter_set: error setting interpreter %s", argv[0]);
      return MI_CMD_ERROR;
    }
			    
  /* We don't want to put up the "done" and whatnot here, since we
   * are going over to another interpreter.
   */
  return MI_CMD_QUIET;
}
			    
/* This implements the "interpreter complete command" which takes an
   interpreter, a command string, and optionally a cursor position 
   within the command, and completes the string based on that interpreter's
   completion function.  */

enum mi_cmd_result 
mi_cmd_interpreter_complete (char *command, char **argv, int argc)
{
  struct gdb_interpreter *interp_to_use;
  enum mi_cmd_result result = MI_CMD_DONE;
  int cursor;
  
  if (argc < 2 || argc > 3)
    {
      asprintf (&mi_error_message, 
		"Wrong # or arguments, should be \"%s interp command <cursor>\".",
		command);
      return MI_CMD_ERROR;
    }

  interp_to_use = gdb_lookup_interpreter (argv[0]);
  if (interp_to_use == NULL)
    {
      asprintf (&mi_error_message,
		"Could not find interpreter \"%s\".", argv[0]);
      return MI_CMD_ERROR;
    }
  
  if (argc == 3)
    {
      cursor = atoi (argv[2]);
    }
  else
    {
      cursor = strlen (argv[1]);
    }

  if (gdb_interpreter_complete (interp_to_use, argv[1], argv[1], cursor) == 0)
      return MI_CMD_ERROR;
  else
    return MI_CMD_DONE;

}

/*
 * mi_insert_notify_hooks - This inserts a number of hooks that are meant to produce
 * async-notify ("=") MI messages while running commands in another interpreter
 * using mi_interpreter_exec.  The canonical use for this is to allow access to
 * the gdb CLI interpreter from within the MI, while still producing MI style output
 * when actions in the CLI command change gdb's state. 
*/

void
mi_insert_notify_hooks (void)
{
  create_breakpoint_hook = mi_interp_create_breakpoint_hook;
  delete_breakpoint_hook = mi_interp_delete_breakpoint_hook;
  modify_breakpoint_hook = mi_interp_modify_breakpoint_hook;

  frame_changed_hook = mi_interp_frame_changed_hook;
  stack_changed_hook = mi_interp_stack_changed_hook;
  context_hook = mi_interp_context_hook;

  /* command_line_input_hook = mi_interp_command_line_input; */
  query_hook = mi_interp_query_hook;
  command_line_input_hook = mi_interp_read_one_line_hook;

  stepping_command_hook = mi_interp_stepping_command_hook;
  continue_command_hook = mi_interp_continue_command_hook;
  run_command_hook = mi_interp_run_command_hook;
}

void
mi_remove_notify_hooks ()
{
  create_breakpoint_hook = NULL;
  delete_breakpoint_hook = NULL;
  modify_breakpoint_hook = NULL;

  frame_changed_hook = NULL;
  stack_changed_hook = NULL;
  context_hook = NULL;

  /* command_line_input_hook = NULL; */
  query_hook = NULL;
  command_line_input_hook = NULL;

  stepping_command_hook = NULL;
  continue_command_hook = NULL;
  run_command_hook = NULL;

}

int 
mi_interp_query_hook (const char * ctlstr, va_list ap)
{
  return 1;
}

/* Use this (or rather the *_notification functions that call it) for 
   notifications in all the hooks that get installed when another 
   interpreter is executing.  That will ensure that the notifications
   come out raw, and don't end up getting wrapped (as will happen, for
   instance, with console-quoted). */

static void
route_output_through_mi (char *prefix, char *notification)
{
  static struct ui_file *rerouting_ui_file = NULL;

  if (rerouting_ui_file == NULL)
    {
      rerouting_ui_file =  stdio_fileopen (stdout);
    }

  fprintf_unfiltered (rerouting_ui_file, "%s%s\n", prefix, notification);
  gdb_flush (rerouting_ui_file);

}

static void
output_async_notification (char *notification)
{
  route_output_through_mi ("=", notification);
}

static void
output_control_change_notification(char *notification)
{
  route_output_through_mi ("^", notification);
}

char *
mi_interp_read_one_line_hook (char *prompt, int repeat, char *anno)
{
  static char buff[256];
  
  if (strlen (prompt) > 200)
    internal_error (__FILE__, __LINE__,
		    "Prompt \"%s\" ridiculously long.", prompt);

  sprintf (buff, "read-one-line,prompt=\"%s\"", prompt);
  output_async_notification (buff);
  
  (void) fgets(buff, sizeof(buff), stdin);
  buff[(strlen(buff) - 1)] = 0;
  
  return buff;
  
}

void
mi_interp_stepping_command_hook ()
{
  output_control_change_notification("stepping");
}

void
mi_interp_continue_command_hook ()
{
  output_control_change_notification("continuing");
}

int
mi_interp_run_command_hook ()
{
  /* request that the ide initiate a restart of the target */
  output_async_notification ("rerun");
  return 0;
}

/* setup_continuation_arg - sets up a continuation structure
   with the timer info and the command token, for use with
   an asyncronous mi command.  Will only cleanup the exec_cleanup
   chain back to CLEANUPS, or not at all if CLEANUPS is NULL. */

static struct mi_continuation_arg *
setup_continuation_arg (struct cleanup *cleanups)
{
  struct mi_continuation_arg *arg
    = (struct mi_continuation_arg *) 
    xmalloc (sizeof (struct mi_continuation_arg));

  if (current_command_token)
    {
      arg->token = xstrdup (current_command_token);
    }
  else
    arg->token = NULL;

  if (do_timings && current_command_ts)
    {
      arg->timestamp = (struct mi_timestamp *) 
	xmalloc (sizeof (struct mi_timestamp));
      copy_timestamp (arg->timestamp, current_command_ts);
      current_command_ts = NULL;
    }
  else
    arg->timestamp = NULL;

  arg->cleanups = cleanups;

  return arg;
}

static void
free_continuation_arg (struct mi_continuation_arg *arg)
{
  if (arg)
    {
      if (arg->token)
	xfree (arg->token);
      if (arg->timestamp)
	xfree (arg->timestamp);
      xfree (arg);
    }
}

/* The only three called from other parts of mi-main.c will probably be
   timestamp(), print_diff_now() and copy_timestamp() */

static void 
timestamp (struct mi_timestamp *tv)
  {
    gettimeofday (&tv->wallclock, NULL);
    getrusage (RUSAGE_SELF, &tv->rusage);
  }

static void 
print_diff_now (struct mi_timestamp *start)
  {
    struct mi_timestamp now;
    timestamp (&now);
    print_diff (start, &now);
  }

static void
copy_timestamp (struct mi_timestamp *dst, struct mi_timestamp *src)
  {
    memcpy (dst, src, sizeof (struct mi_timestamp));
  }

static void 
print_diff (struct mi_timestamp *start, struct mi_timestamp *end)
  {
    fprintf_unfiltered (raw_stdout,
       ",time={wallclock=\"%0.5f\",user=\"%0.5f\",system=\"%0.5f\",start=\"%d.%06d\",end=\"%d.%06d\"}", 
       wallclock_diff (start, end) / 1000000.0, 
       user_diff (start, end) / 1000000.0, 
       system_diff (start, end) / 1000000.0,
       start->wallclock.tv_sec, start->wallclock.tv_usec,
       end->wallclock.tv_sec, end->wallclock.tv_usec);
  }

static long 
wallclock_diff (struct mi_timestamp *start, struct mi_timestamp *end)
  {
    return ((end->wallclock.tv_sec - start->wallclock.tv_sec) * 1000000) +
           (end->wallclock.tv_usec - start->wallclock.tv_usec);
  }

static long 
user_diff (struct mi_timestamp *start, struct mi_timestamp *end)
  {
    return 
     ((end->rusage.ru_utime.tv_sec - start->rusage.ru_utime.tv_sec) * 1000000) +
      (end->rusage.ru_utime.tv_usec - start->rusage.ru_utime.tv_usec);
  }

static long 
system_diff (struct mi_timestamp *start, struct mi_timestamp *end)
  {
    return 
     ((end->rusage.ru_stime.tv_sec - start->rusage.ru_stime.tv_sec) * 1000000) +
      (end->rusage.ru_stime.tv_usec - start->rusage.ru_stime.tv_usec);
  }
