/* Variable expansion functions for GNU Make.
Copyright (C) 1988-2020 Free Software Foundation, Inc.
This file is part of GNU Make.

GNU Make is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 3 of the License, or (at your option) any later
version.

GNU Make is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "makeint.h"

#include <assert.h>

#include "filedef.h"
#include "job.h"
#include "commands.h"
#include "variable.h"
#include "rule.h"
#include "pedantic.h"
#include "globals.h"

/* Initially, any errors reported when expanding strings will be reported
   against the file where the error appears.  */
const gmk_floc **expanding_var = &reading_file;

/* The next two describe the variable output buffer.
   This buffer is used to hold the variable-expansion of a line of the
   makefile.  It is made bigger with realloc whenever it is too small.
   variable_buffer_length is the size currently allocated.
   variable_buffer is the address of the buffer.

   For efficiency, it's guaranteed that the buffer will always have
   VARIABLE_BUFFER_ZONE extra bytes allocated.  This allows you to add a few
   extra chars without having to call a function.  Note you should never use
   these bytes unless you're _sure_ you have room (you know when the buffer
   length was last checked.  */

#define VARIABLE_BUFFER_ZONE    5

static size_t variable_buffer_length;
char *variable_buffer;

/* Subroutine of variable_expand and friends:
   The text to add is LENGTH chars starting at STRING to the variable_buffer.
   The text is added to the buffer at PTR, and the updated pointer into
   the buffer is returned as the value.  Thus, the value returned by
   each call to variable_buffer_output should be the first argument to
   the following call.  */

char *
variable_buffer_output (char *ptr, const char *string, size_t length)
{
  size_t newlen = length + (ptr - variable_buffer);

  if ((newlen + VARIABLE_BUFFER_ZONE) > variable_buffer_length)
    {
      size_t offset = ptr - variable_buffer;
      variable_buffer_length = (newlen + 100 > 2 * variable_buffer_length
                                ? newlen + 100
                                : 2 * variable_buffer_length);
      variable_buffer = xrealloc (variable_buffer, variable_buffer_length);
      ptr = variable_buffer + offset;
    }

  memcpy (ptr, string, length);
  return ptr + length;
}

/* Return a pointer to the beginning of the variable buffer.  */

static char *
initialize_variable_output (void)
{
  /* If we don't have a variable output buffer yet, get one.  */

  if (variable_buffer == 0)
    {
      variable_buffer_length = 200;
      variable_buffer = xmalloc (variable_buffer_length);
      variable_buffer[0] = '\0';
    }

  return variable_buffer;
}

/* Recursively expand V.  The returned string is malloc'd.  */

static char *allocated_variable_append (const struct variable *v);

char *
recursively_expand_for_file (struct variable *v, struct file *file)
{
  char *value;
  const gmk_floc *this_var;
  const gmk_floc **saved_varp;
  struct variable_set_list *save = 0;
  int set_reading = 0;

  /* Don't install a new location if this location is empty.
     This can happen for command-line variables, builtin variables, etc.  */
  saved_varp = expanding_var;
  if (v->fileinfo.filenm)
    {
      this_var = &v->fileinfo;
      expanding_var = &this_var;
    }

  /* If we have no other file-reading context, use the variable's context. */
  if (!reading_file)
    {
      set_reading = 1;
      reading_file = &v->fileinfo;
    }

  if (v->expanding)
    {
      if (!v->exp_count)
        /* Expanding V causes infinite recursion.  Lose.  */
        OS (fatal, *expanding_var,
            _("Recursive variable '%s' references itself (eventually)"),
            v->name);
      --v->exp_count;
    }

  if (file)
    {
      save = current_variable_set_list;
      current_variable_set_list = file->variables;
    }

  v->expanding = 1;
  if (v->append)
    value = allocated_variable_append (v);
  else
    value = allocated_variable_expand (v->value);
  v->expanding = 0;

  if (set_reading)
    reading_file = 0;

  if (file)
    current_variable_set_list = save;

  expanding_var = saved_varp;

  return value;
}

/* Expand a simple reference to variable NAME, which is LENGTH chars long.  */

#ifdef __GNUC__
__inline
#endif
static char *
reference_variable (
  char *o,
  const char *name, 
  size_t length, 
  expr_t reference_kind
)
{
  struct variable *v;
  char *value;
  char *whole_ref;
  int ref_length;
  struct expandable_data* info;
  struct expression* either_manual_subscope_ref_or_auto;
  struct expression* subscope_ref = NULL;

  v = lookup_variable (name, length);
  /*
  `end` is close delimiter if the enclosing substring does not require any expansion
  `end` is null char otherwise
   end - beg = length means that char at `end` position is not considered during reference_variable
   beg = name
   end = name + length
   name is either a single char or all chars between open and close delimiters without them
  */

  if (v == 0)
    warn_undefined (name, length);

  /* If there's no variable by that name or it has no value, stop now.  */
  if (v == 0 || (*v->value == '\0' && !v->append))
    return o;

  if(makefile_eval_expand){

    switch(reference_kind)
    {
      case SIMPLE_REFERENCE:
        whole_ref = (char*)malloc(1 + 1 + 1);
        whole_ref[0] = '$';
        whole_ref[1] = name[0];
        whole_ref[2] = '\0';
        ref_length = 2;
        break;

      default:
        whole_ref = (char*)malloc(2 + length + 1 + 1);
        memset(whole_ref, 0, 2 + length + 1 + 1);
        whole_ref[0] = '$';
        strncpy(whole_ref + 2, name, length);
        whole_ref[2 + length + 1] = '\0';

        if(reference_kind == BRACE_REFERENCE){
          whole_ref[1] = '{';
          whole_ref[2 + length] = '}';
        }
        else if(reference_kind == PAREN_REFERENCE)
        {
          whole_ref[1] = '(';
          whole_ref[2 + length] = ')';
        }

        ref_length = 2 + length + 1;
        break;
    }

  }

  if(makefile_eval_expand){

    /* reference_variable() is called to solve the final value of
      a multi reference after its nested expression has been previously expanded
    */
    struct expression* unknown_current = peek_dbg_expr();

    /* reference_variable() was called for a reference inside the original expanded string */
    if(unknown_current->scope == AUTOMATIC_SCOPE && unknown_current->kind != MULTI_REFERENCE){
      if(!is_automatic_scope_ref(whole_ref, ref_length)){
        subscope_ref = push_manual_subscope_reference(whole_ref, ref_length, reference_kind, VERTICAL_REF);
        free(whole_ref);
      }
      else /* the whole expanded string is a reference */
        set_real_kind_automatic_scope(reference_kind);
    } /* $($a) but a = b then create an intermediary node $b */
    else if(unknown_current->kind == MULTI_REFERENCE){
      subscope_ref = push_manual_subscope_reference(whole_ref, ref_length, reference_kind, HORIZONTAL_REF);
    }
    else{
      printf("Unkown way to deal with ref: %s for current expression %s of kind %s and scope %s", \
        name, \
        unknown_current->body, \
        to_string_kind(unknown_current->kind),
        to_string_scope(unknown_current->scope)\
      );
      exit(1);
    }
    /* 
    otherwise reference_variable() is called to solve the expanded inner substring
    of a multi reference that is not in automatic scope
    */
    
    either_manual_subscope_ref_or_auto = peek_dbg_expr();
  }
    

  if(v->recursive){
    /* if pedantic mode activated, the recursively_expand() will go to a variable_expand_string() 
    that will mean a new node in DFS traversal */
    value = recursively_expand(v);
  }
  else{
    if(makefile_eval_expand){

      char* dbg_value = strdup(v->value);

      /* Link final value with the current expression but dont push it on stakc since this is a leaf in the tree */
      struct expression* simple_ref_leaf_val = horizontal_next_step_init_expr(
        either_manual_subscope_ref_or_auto, \
        dbg_value, \
        ATOM_VALUE \
      );

      info = (struct expandable_data*)malloc(sizeof(struct expandable_data));

      if(either_manual_subscope_ref_or_auto->kind == ASSIGNMENT)
        info->order = 1;
      else 
        info->order = either_manual_subscope_ref_or_auto->data.exp_data->order + 1;

      simple_ref_leaf_val->data.exp_data = info;
      /* We dont push the reference variable in the stack since its a leaf node, no children to visit further */
    }

    value = v->value;
  }
  
  if(makefile_eval_expand){
    if(subscope_ref){
      pop_subscope_reference(reference_kind);
    }
    
  }

  o = variable_buffer_output (o, value, strlen (value));

  if (v->recursive)
    free (value);

  return o;
}

/* Scan STRING for variable references and expansion-function calls.  Only
   LENGTH bytes of STRING are actually scanned.  If LENGTH is -1, scan until
   a null byte is found.

   Write the results to LINE, which must point into 'variable_buffer'.  If
   LINE is NULL, start at the beginning of the buffer.
   Return a pointer to LINE, or to the beginning of the buffer if LINE is
   NULL.
 */
char *
variable_expand_string (char *line, const char *string, size_t length)
{
  /*
  
  Normally, this function deals with the logic of expanding and referencing variables:
  
  - start from the beginging of string

  while(1){
    1. find first dolar (call it outer dolar, append "normal characters between current position and outer dollar
    in the result string")

    2. find closing delimiter and if not reference case $a

    3. if there is any dolar between outer dolar and closind delimiter 
    then recursively call this function with the substring

    4. result from recursive call is referenced

    5. if not 3 then no inner dolar so reference case ${a}

    6. move curent cursor past the closing delimiter or past variable name referenced at step 
  }

  Capturing all parsing and expansion phases respect a DFS algo.

  Let `scope` be a procedure context for variable_expand_string(). 
  In this scope we define as current visited expression (as in DFS search) the `string` parameter.
  Its children (or neighboring nodes) will be any substring that represents a reference or
  a multi-expansion (the outer dolar substring that encapsulates another 1 or more inner dolars).
  
  A `subscope` is an element within a `scope` in which there are created and pushed in the soon-to-visit stack
  the reference substring and/or multi-expansion substring.

  We can define a 3-way parallel here:
  - `subscope` is within a `scope`
  - the `subscope` deals with a substring of the `scope's` string
  - the expression created and pushed to the stack in the `subscope` is a child of the expression in the `scope`

  The reference_variable() function can also recursively call this function if the string is not immediatelly expandable,
  otherwise create a leaf node that is not put in the stack.

  The parsing can be represented by a pseudo context-free grammar as:

  input = scope (the current expression node - top of the stack has the name of the input)

  scope = <start input>?(<normal chars>${subscope})*<end input>

  subscope = scope (to abstract a recursive call triggered by `reference variable()` or `expand_argument()` )

  */
  struct variable *v;
  const char *p, *p1, *outer_dolar;
  char *save;
  char *o;
  size_t line_offset;
  size_t normal_chars_substr_len;
  char open_substr[3];
  char close_substr[2];
  bool at_least_one_ref = false;

  if (!line)
    line = initialize_variable_output ();
  o = line;
  line_offset = line - variable_buffer;

  if (length == 0)
    {
      variable_buffer_output (o, "", 1);
      return (variable_buffer);
    }

  /* We need a copy of STRING: due to eval, it's possible that it will get
     freed as we process it (it might be the value of a variable that's reset
     for example).  Also having a nil-terminated string is handy.  */
  save = length == SIZE_MAX ? xstrdup (string) : xstrndup (string, length);
  p = save;

  if(makefile_eval_expand)
    push_automatic_scope(string, EITHER_MIXED_ATOM_VALUE_UNKNOWN_REF);

  
  while (1)
    {
      /* Copy all following uninteresting chars all at once to the
         variable output buffer, and skip them.  Uninteresting chars end
         at the next $ or the end of the input.  */

      p1 = strchr (p, '$');
      outer_dolar = p1;
      
      normal_chars_substr_len = p1 != 0 ? (size_t) (p1 - p) : strlen (p) + 1;
      
      o = variable_buffer_output (o, p, normal_chars_substr_len);

      if(makefile_eval_expand){

        // a previous iteration found a ref
        if(at_least_one_ref){

          struct expression* current = peek_automatic_scope();

          /* the whole string is some type of reference, do not poison the type with MIXED */
          if(current->kind != SIMPLE_REFERENCE && \
          current->kind != PAREN_REFERENCE && \
          current->kind != BRACE_REFERENCE && \
          current->kind != MULTI_REFERENCE)
            set_real_kind_automatic_scope(MIXED);
        }
        // none of the previous iterations found a simple/multi ref and no $ in the remaining substring
        else if(p1 == 0){ 
          // automatic_scope_peda_val(p, normal_chars_substr_len - 1);
          set_real_kind_automatic_scope(ATOM_VALUE);
        }
        /* this iteration has found either a simple/multi ref or a $$ so its still either mixed or atom value or ref 
           append the characters from the begining until the first found $
        */
        // if(p1 != 0 && p1 != p) 
        //   automatic_scope_peda_val(p, normal_chars_substr_len);
        
      }

      if (p1 == 0)
        break;
      
      p = p1 + 1;
      
      /* Dispatch on the char that follows the $.  */

      switch (*p)
      {
        case '$':
        case '\0':
          /* $$ or $ at the end of the string means output one $ to the
             variable output buffer.  */
          o = variable_buffer_output (o, p1, 1);
          break;

        case '(':
        case '{':
          /* $(...) or ${...} is the general case of substitution.  */
          {
            char openparen = *p;
            char closeparen = (openparen == '(') ? ')' : '}';
            const char *begp;
            const char *beg = p + 1;
            char *op;
            char *abeg = NULL;
            char* inner_dolar;
            const char *end, *colon;
            struct expression* multi_ref = NULL;
            
            inner_dolar = (char*)outer_dolar; // we dont know if there truly is a inner (second) dolar
            at_least_one_ref = true;

            open_substr[0] = '$';
            open_substr[1] = openparen;
            open_substr[2] = '\0';

            close_substr[0] = closeparen;
            close_substr[1] = '\0';

            op = o;
            begp = p;
            if (handle_function (&op, &begp))
              {
                o = op;
                p = begp;
                break;
              }

            /* Is there a variable reference inside the parens or braces?
               If so, expand it before expanding the entire reference.  */

            end = strchr (beg, closeparen);
            if (end == 0)
              /* Unterminated variable reference.  */
              O (fatal, *expanding_var, _("unterminated variable reference"));
            p1 = lindex (beg, end, '$');
            if (p1 != 0)
              {
                /* BEG now points past the opening paren or brace.
                   Count parens or braces until it is matched.

                   BEGP points to the opening paren or brace
                   
                   p will point to the correct closing paren or brace after this 'for'

                   p1 points to the first $ starting from beg (p1 can also be begp which is interesting case)
                */
                int count = 0;
                inner_dolar = (char*)p1;
                for (p = beg; *p != '\0'; ++p)
                  {
                    if (*p == openparen)
                      ++count;
                    else if (*p == closeparen && --count < 0)
                      break;
                  }
                /* If COUNT is >= 0, there were unmatched opening parens
                   or braces, so we go to the simple case of a variable name
                   such as '$($(a)'.  */
                if (count < 0)
                  {
                     
                    if(makefile_eval_expand){
                      if(!is_automatic_scope_multi_ref(outer_dolar, p)){
                        multi_ref = push_manual_subscope_multi_reference(outer_dolar, p);
                      }
                      else
                        set_real_kind_automatic_scope(MULTI_REFERENCE);
                    }

                    abeg = expand_argument (beg, p); /* Expand the name.  */
                    beg = abeg;
                    end = strchr (beg, '\0');
                    
                  }
              }
            else
              /* Advance P to the end of this reference.  After we are
                 finished expanding this one, P will be incremented to
                 continue the scan.  */
              p = end;

            /* This is not a reference to a built-in function and
               any variable references inside are now expanded.
               Is the resultant text a substitution reference?  */

            colon = lindex (beg, end, ':');
            if (colon)
              {
                /* This looks like a substitution reference: $(FOO:A=B).  */
                const char *subst_beg = colon + 1;
                const char *subst_end = lindex (subst_beg, end, '=');
                if (subst_end == 0)
                  /* There is no = in sight.  Punt on the substitution
                     reference and treat this as a variable name containing
                     a colon, in the code below.  */
                  colon = 0;
                else
                  {
                    const char *replace_beg = subst_end + 1;
                    const char *replace_end = end;

                    /* Extract the variable name before the colon
                       and look up that variable.  */
                    v = lookup_variable (beg, colon - beg);
                    if (v == 0)
                      warn_undefined (beg, colon - beg);

                    /* If the variable is not empty, perform the
                       substitution.  */
                    if (v != 0 && *v->value != '\0')
                      {
                        char *pattern, *replace, *ppercent, *rpercent;
                        char *value = (v->recursive
                                       ? recursively_expand (v)
                                       : v->value);

                        /* Copy the pattern and the replacement.  Add in an
                           extra % at the beginning to use in case there
                           isn't one in the pattern.  */
                        pattern = alloca (subst_end - subst_beg + 2);
                        *(pattern++) = '%';
                        memcpy (pattern, subst_beg, subst_end - subst_beg);
                        pattern[subst_end - subst_beg] = '\0';

                        replace = alloca (replace_end - replace_beg + 2);
                        *(replace++) = '%';
                        memcpy (replace, replace_beg,
                               replace_end - replace_beg);
                        replace[replace_end - replace_beg] = '\0';

                        /* Look for %.  Set the percent pointers properly
                           based on whether we find one or not.  */
                        ppercent = find_percent (pattern);
                        if (ppercent)
                          {
                            ++ppercent;
                            rpercent = find_percent (replace);
                            if (rpercent)
                              ++rpercent;
                          }
                        else
                          {
                            ppercent = pattern;
                            rpercent = replace;
                            --pattern;
                            --replace;
                          }

                        o = patsubst_expand_pat (o, value, pattern, replace,
                                                 ppercent, rpercent);

                        if (v->recursive)
                          free (value);
                      }
                  }
              }

            if (colon == 0)
              /* This is an ordinary variable reference.
                 Look up the value of the variable.
                `end` is close delimiter if the enclosing substring does not require any expansion
                `end` is null char otherwise
                end - beg means that char at `end` position is not considered during reference_variable
              */
              
              o = reference_variable (o, beg, end - beg, (openparen == '(') ? PAREN_REFERENCE : BRACE_REFERENCE);

            if(makefile_eval_expand){
              if(multi_ref)
                pop_subscope_multi_expansion();
            }
                           
            free (abeg);
          }
          break;

        default:
          if (ISSPACE (p[-1]))
            break;
          
          /* A $ followed by a random char is a variable reference:
             $a is equivalent to $(a).  */
          o = reference_variable (o, p, 1, SIMPLE_REFERENCE);

          at_least_one_ref = true;

          break;
      }

      if (*p == '\0'){
        if(makefile_eval_expand){
          struct expression* final_scope = peek_automatic_scope();
          if(final_scope->kind == EITHER_MIXED_ATOM_VALUE_UNKNOWN_REF)
            final_scope->kind = ATOM_VALUE;
        }
        break;
      }

      ++p;
      
    }

  free (save);

  variable_buffer_output (o, "", 1);

  if(makefile_eval_expand){
    struct expression* final_auto = peek_automatic_scope();

    /* generate the final value of the current node after all recursive expansions
    but do a simple string does not have a final value since there were no expansions
    so it is unnecessary to generate another node
     */
    if(final_auto->kind == MIXED)
      horizontal_next_step_init_expr(final_auto, variable_buffer + line_offset, ATOM_VALUE);

    pop_automatic_scope_either_mixed_atom_value_unknown_ref();
  }

  return (variable_buffer + line_offset);
}

/* Scan LINE for variable references and expansion-function calls.
   Build in 'variable_buffer' the result of expanding the references and calls.
   Return the address of the resulting string, which is null-terminated
   and is valid only until the next time this function is called.  */

char *
variable_expand (const char *line)
{
  return variable_expand_string (NULL, line, SIZE_MAX);
}

/* Expand an argument for an expansion function.
   The text starting at STR and ending at END is variable-expanded
   into a null-terminated string that is returned as the value.
   This is done without clobbering 'variable_buffer' or the current
   variable-expansion that is in progress.  */

char *
expand_argument (const char *str, const char *end)
{
  char *tmp, *alloc = NULL;
  char *r;

  if (str == end)
    return xstrdup ("");

  if (!end || *end == '\0')
    return allocated_variable_expand (str);

  if (end - str + 1 > 1000)
    tmp = alloc = xmalloc (end - str + 1);
  else
    tmp = alloca (end - str + 1);

  memcpy (tmp, str, end - str);
  tmp[end - str] = '\0';

  r = allocated_variable_expand (tmp);

  free (alloc);

  return r;
}

/* Expand LINE for FILE.  Error messages refer to the file and line where
   FILE's commands were found.  Expansion uses FILE's variable set list.  */

char *
variable_expand_for_file (const char *line, struct file *file)
{
  char *result;
  struct variable_set_list *savev;
  const gmk_floc *savef;

  if (file == 0)
    return variable_expand (line);

  savev = current_variable_set_list;
  current_variable_set_list = file->variables;

  savef = reading_file;
  if (file->cmds && file->cmds->fileinfo.filenm)
    reading_file = &file->cmds->fileinfo;
  else
    reading_file = 0;

  result = variable_expand (line);

  current_variable_set_list = savev;
  reading_file = savef;

  return result;
}

/** Expand PSZ_LINE. Expansion uses P_FILE_SET if it is not NULL. */
char *
variable_expand_set (char *psz_line, variable_set_list_t *p_file_vars)
{
  char *psz_result;
  variable_set_list_t *p_vars_save;

  p_vars_save = current_variable_set_list;
  if (p_file_vars)
    current_variable_set_list = p_file_vars;
  psz_result = variable_expand (psz_line);
  current_variable_set_list = p_vars_save;

  return psz_result;
}

/* Like allocated_variable_expand, but for += target-specific variables.
   First recursively construct the variable value from its appended parts in
   any upper variable sets.  Then expand the resulting value.  */

static char *
variable_append (const char *name, size_t length,
                 const struct variable_set_list *set, int local)
{
  const struct variable *v;
  char *buf = 0;
  int nextlocal;

  /* If there's nothing left to check, return the empty buffer.  */
  if (!set)
    return initialize_variable_output ();

  /* If this set is local and the next is not a parent, then next is local.  */
  nextlocal = local && set->next_is_parent == 0;

  /* Try to find the variable in this variable set.  */
  v = lookup_variable_in_set (name, length, set->set);

  /* If there isn't one, or this one is private, try the set above us.  */
  if (!v || (!local && v->private_var))
    return variable_append (name, length, set->next, nextlocal);

  /* If this variable type is append, first get any upper values.
     If not, initialize the buffer.  */
  if (v->append)
    buf = variable_append (name, length, set->next, nextlocal);
  else
    buf = initialize_variable_output ();

  /* Append this value to the buffer, and return it.
     If we already have a value, first add a space.  */
  if (buf > variable_buffer)
    buf = variable_buffer_output (buf, " ", 1);

  /* Either expand it or copy it, depending.  */
  if (! v->recursive)
    return variable_buffer_output (buf, v->value, strlen (v->value));

  buf = variable_expand_string (buf, v->value, strlen (v->value));
  return (buf + strlen (buf));
}


static char *
allocated_variable_append (const struct variable *v)
{
  char *val;

  /* Construct the appended variable value.  */

  char *obuf = variable_buffer;
  size_t olen = variable_buffer_length;

  variable_buffer = 0;

  val = variable_append (v->name, strlen (v->name),
                         current_variable_set_list, 1);
  variable_buffer_output (val, "", 1);
  val = variable_buffer;

  variable_buffer = obuf;
  variable_buffer_length = olen;

  return val;
}

/* Like variable_expand_for_file, but the returned string is malloc'd.
   This function is called a lot.  It wants to be efficient.  */

char *
allocated_variable_expand_for_file (const char *line, struct file *file)
{
  char *value;

  char *obuf = variable_buffer;
  size_t olen = variable_buffer_length;

  variable_buffer = 0;

  value = variable_expand_for_file (line, file);

  variable_buffer = obuf;
  variable_buffer_length = olen;

  return value;
}

/* Install a new variable_buffer context, returning the current one for
   safe-keeping.  */

void
install_variable_buffer (char **bufp, size_t *lenp)
{
  *bufp = variable_buffer;
  *lenp = variable_buffer_length;

  variable_buffer = 0;
  initialize_variable_output ();
}

/* Restore a previously-saved variable_buffer setting (free the current one).
 */

void
restore_variable_buffer (char *buf, size_t len)
{
  free (variable_buffer);

  variable_buffer = buf;
  variable_buffer_length = len;
}
