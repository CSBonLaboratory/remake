#include "types.h"
#include "globals.h"
#include <string.h>

int env_overrides = 0;

/* Nonzero means ignore status codes returned by commands
   executed to remake files.  Just treat them all as successful (-i).  */

int ignore_errors_flag = 0;

/* Nonzero means don't remake anything, just print the data base
   that results from reading the makefile (-p).  */

int print_data_base_flag = 0;

/* Nonzero means don't remake anything; just return a nonzero status
   if the specified targets are not up to date (-q).  */

int question_flag = 0;

/* Nonzero means do not use any of the builtin rules (-r) / variables (-R).  */

int no_builtin_rules_flag = 0;
int no_builtin_variables_flag = 0;

/* Nonzero means check symlink mtimes.  */
int check_symlink_flag = 0;

/* Nonzero means print directory before starting and when done (-w).  */
int print_directory = 0;

/* Nonzero means print version information.  */
int print_version_flag = 0;

/*! Nonzero means --trace and shell trace with input.  */
int shell_trace = 0;

/* Nonzero means profiling is enabled with specific output requested. (option --profile=callgrind|json  */
int profile_flag = 0;

/* Path to directory to dump profiling data */
const char *profile_directory;

/* Nonzero means look in parent directories for a Makefile if one isn't found
   in the current directory (option --search-parent).  */
int search_parent_flag = 0;

/* Nonzero means do extra verification (that may slow things down).  */
int verify_flag;

/* Nonzero means do not print commands to be executed (-s).  */
int silent_flag;

/* Nonzero means just touch the files
   that would appear to need remaking (-t)  */
int touch_flag;

/* Nonzero means just print what commands would need to be executed,
   don't actually execute them (-n).  */
int just_print_flag;

/*! If 1, we don't give additional error reporting information. */
int no_extended_errors = 0;

int db_level = 0;

/*! Value of the MAKELEVEL variable at startup (or 0).  */
unsigned int makelevel;

/*! Value of the MAKEPARENT variable at startup (or 0). */
pid_t makeparent_pid;

/*! Value of the MAKEPARENT_TARGET variable at startup (or NULL). */
char *makeparent_target;

/*! Nonzero gives a list of explicit target names and exits. Set by option
  --targets
 */
int show_targets_flag = 0;

/*! Nonzero gives a list of explicit target names that have commands
  associated with them and exits. Set by option --tasks
 */
int show_tasks_flag = 0;

/*! If 1, same as --debugger=preaction */
int debugger_flag;

/* If nonzero, we should print a warning message
   for each reference to an undefined variable.  */
int warn_undefined_variables_flag;

/** True if we are inside the debugger, false otherwise. */
int in_debugger = false;

/*! Nonzero if we have seen the magic '.POSIX' target.
   This turns on pedantic compliance with POSIX.2.  */
int posix_pedantic;

/*! If nonzero, we are debugging after each "step" for that many times.
  When we have a value 1, then we actually run the debugger read loop.
  Otherwise we decrement the step count.

*/
unsigned int i_debugger_stepping = 0;

/*! If nonzero, we are debugging after each "next" for that many times.
  When we have a value 1, then we actually run the debugger read loop.
  Otherwise we decrement the step count.

*/
unsigned int i_debugger_nexting = 0;

/*! If nonzero, enter the debugger if we hit a fatal error.
*/
unsigned int debugger_on_error = 0;

/*! If nonzero, we have requested some sort of debugging.
*/
unsigned int debugger_enabled;

/*! If true, enter the debugger before reading any makefiles. */
bool b_debugger_preread = false;

/* If true, enter the debugger at every variable assignment, variable definition and conditional structure*/
bool b_debugger_pedantic = false;

/* This character introduces a command: it's the first char on the line.  */
char cmd_prefix = '\t';

/*! Our current directory after processing all -C options.  */
char *starting_directory;

/* One of OUTPUT_SYNC_* if the "--output-sync" option was given.  This
   attempts to synchronize the output of parallel jobs such that the results
   of each job stay together.  */
int output_sync = OUTPUT_SYNC_NONE;

/*! This is the path to the shell used run Makefile commands.
The value is set in job.c.
*/
extern const char *default_shell;

char *remote_description = 0;

/* Remember the original value of the SHELL variable, from the environment.  */
struct variable shell_var;

/* The filename and pointer to line number of the
   makefile currently being read in.  */
const gmk_floc *reading_file = 0;

struct expression_list* step_dbg_expr_stack = NULL;
struct expression root_expr_tree;

struct expression_list* init_expr_list(struct expression* e){
   
   struct expression_list* l = (struct expression_list*)malloc(sizeof(struct expression_list));
   struct expression_node* head = (struct expression_node*)malloc(sizeof(struct expression_node));

   l->first = head;
   l->first->prev = NULL;
   l->first->next = NULL;
   l->first->expr = e;
   l->last = l->first;
}
struct expression* init_expr(struct expression* parent, char* body){

   struct expression* e = (struct expression*)malloc(sizeof(struct expression));
   e->body = strdup(body);
   e->body_len = strlen(body);
   e->int_peda_value = NULL;
   e->int_peda_len = 0;
   e->children = NULL;
   e->parent = parent;

   if(parent != NULL){
      if(parent->children == NULL)
         parent->children = init_expr_list(e);
      else
         push_expr(parent->children, e);
   }

   return e;
}
void init_pedantic(){

   step_dbg_expr_stack = init_expr_list((struct expression*)NULL);

   root_expr_tree.children = NULL;
   root_expr_tree.body = NULL;
   root_expr_tree.parent = NULL;
}

void push_expr(struct expression_list* ls, struct expression* expr){

   struct expression_node* new = (struct expression_node*)malloc(sizeof(struct expression_node));
   new->expr = expr;
   new->next = NULL;
   new->prev = ls->last;

   // link last node with the new node
   ls->last->next = new;

   // cache the new node as the last in the list
   ls->last = new;
}


void pop_expr(struct expression_list* ls){

   struct expression_node* old = ls->last;
   ls->last = old->prev;
   ls->last->next = NULL;
   
   // do not free the expression pointer since it is also shared in the root_expr_tree
   // this function just pops the node from the stack
   free(old);
}

struct expression* peek_expr(){

   return step_dbg_expr_stack->last->expr;
}

void build_pedantic_value(struct expression* expr, char* substr, size_t substr_len){

   if(substr_len == 0)
      substr_len = strlen(substr);

   if(expr->int_peda_value == NULL){

      expr->int_peda_value = (char*)malloc(substr_len + 1);
      expr->int_peda_value[0] = '\0';
      expr->int_peda_len = substr_len;
   }
   else{
      
      expr->int_peda_value = (char*)realloc((void*)expr->int_peda_value, expr->int_peda_len + substr_len + 1);
      expr->int_peda_len += substr_len;
   }
   strncat(expr->int_peda_value, substr, substr_len);
}
