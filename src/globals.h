#ifndef GLOBALS_H
#define GLOBALS_H

#include "types.h"
#include "variable.h"

#define OUTPUT_SYNC_NONE    0
#define OUTPUT_SYNC_LINE    1
#define OUTPUT_SYNC_TARGET  2
#define OUTPUT_SYNC_RECURSE 3

extern int env_overrides;

/* Nonzero means ignore status codes returned by commands
   executed to remake files.  Just treat them all as successful (-i).  */
extern int ignore_errors_flag;

/* Nonzero means don't remake anything, just print the data base
   that results from reading the makefile (-p).  */
extern int print_data_base_flag;

/* Nonzero means don't remake anything; just return a nonzero status
   if the specified targets are not up to date (-q).  */
extern int question_flag;

/* Nonzero means do not use any of the builtin rules (-r) / variables (-R).  */

extern int no_builtin_rules_flag;
extern int no_builtin_variables_flag;

/* Nonzero means check symlink mtimes.  */
extern int check_symlink_flag;

/* Nonzero means print directory before starting and when done (-w).  */
extern int print_directory;

/* Nonzero means print version information.  */
extern int print_version_flag;

/*! Nonzero means --trace and shell trace with input.  */
extern int shell_trace;

/*! Nonzero means profiling is enabled with specific output requested. (option --profile=callgrind|json)  */
extern int profile_flag;

/* Path to directory to dump profiling data */
extern const char *profile_directory;

/*! Nonzero means look in parent directories for a Makefile if one isn't found
   in the current directory (option --search-parent).  */
extern int search_parent_flag;

/* Nonzero means do extra verification (that may slow things down).  */
extern int verify_flag;

/* Nonzero means do not print commands to be executed (-s).  */
extern int silent_flag;

/* Nonzero means just touch the files
   that would appear to need remaking (-t)  */
extern int touch_flag;

/* Nonzero means just print what commands would need to be executed,
   don't actually execute them (-n).  */
extern int just_print_flag;

/*! If 1, we don't give additional error reporting information. */
extern int no_extended_errors;

extern int db_level;

/*! Value of the MAKELEVEL variable at startup (or 0).  */
extern unsigned int makelevel;

/*! Value of the MAKEPARENT_PID variable at startup (or 0). */
extern pid_t makeparent_pid;

/*! Value of the MAKEPARENT_TARGET variable at startup (or 0). */
extern char *makeparent_target;

/*! Nonzero gives a list of explicit target names and exits. Set by option
  --targets
 */
extern int show_targets_flag;

/*! Nonzero gives a list of explicit target names that have commands
  associated with them and exits. Set by option --tasks
 */
extern int show_tasks_flag;

/*! If 1, same as --debugger=preaction */
extern int debugger_flag;

/** True if we are inside the debugger, false otherwise. */
extern int in_debugger;

/*! If true, enter the debugger before reading any makefiles. */
extern bool b_debugger_preread;

/* If true, enter the debugger at every variable assignment, variable definition and conditional structure*/
extern bool b_debugger_pedantic;

struct expression
{
   char* body;
   size_t body_len;
   char* int_peda_value;
   size_t int_peda_len;
   struct expression_list* children;
   struct expression* parent;
};

struct expression_node
{
   struct expression_node *next;
   struct expression_node *prev;
   struct expression* expr;
};

struct expression_list
{
   struct expression_node* first;
   struct expression_node* last;
};

extern struct expression root_expr_tree;
extern struct expression_list* step_dbg_expr_stack;


/* During the pedantic mode, substrings such as $() or ${} that
   contain other nested expandable expressions are not transformed into
   the final value, in order to be easily understood.
   Example:
    x=A
    y=B
    zt=C
    AaaCB=D
      then:
    abc${$xaa${zt}$y} -> abc${AaaCB} (intermediary pedantic value) -> abcD (final value stored normally in variable_buffer)
  

*/
void build_pedantic_value(struct expression* expr, char* substr, size_t substr_len);

void push_expr(struct expression_list* stack, struct expression* expr);
#define push_dbg_expr(e) push_expr(step_dbg_expr_stack, e);

void pop_expr(struct expression_list* stack);
#define pop_dbg_expr() pop_expr(step_dbg_expr_stack);

struct expression* peek_expr();

struct expression_list* init_expr_list(struct expression* e);

void init_pedantic();

struct expression* init_expr(struct expression* parent, char* body);

#define add_child(p, c) \
   if(p->children == NULL) \
      p->children = init_expr_list(c); \
   else \
      push_expr(p->children, c)



/* Remember the original value of the SHELL variable, from the environment.  */
extern struct variable shell_var;

#endif /*GLOBALS_H*/
