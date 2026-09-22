#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "variable.h"
#include "pedantic.h"
#include "gnuremake.h"


/* a stack used for explaining the expansion logic of variables and function calls */
struct expression_list* step_dbg_expr_stack = NULL;

/* the global AST of the current makefile */
struct expression root_expr_tree;

struct expression_list* init_expr_list(struct expression* e){
   
   struct expression_list* l = (struct expression_list*)malloc(sizeof(struct expression_list));
   struct expression_node* head = (struct expression_node*)malloc(sizeof(struct expression_node));

   l->first = head;
   l->first->prev = NULL;
   l->first->next = NULL;
   l->first->expr = e;
   l->last = l->first;
   l->size = 1;
   return l;
}
struct expression* init_expr(struct expression* parent, struct expression* prev_interm_step, const char* body, expr_t kind){

   struct expression* e = (struct expression*)malloc(sizeof(struct expression));
   e->body = strdup(body);
   e->body_len = strlen(body);
   e->children = NULL;
   e->prev_step_value = prev_interm_step;
   e->next_step_value = NULL;
   e->kind = kind;
   e->parent = parent;

   memset(&e->data, 0, sizeof(union private_data));
   
   if(parent != NULL){
      add_child(parent, e)
   }
   else{
      prev_interm_step->next_step_value = e;
   }

   return e;
}
void init_pedantic(){

   step_dbg_expr_stack = init_expr_list((struct expression*)NULL);

   root_expr_tree.children = NULL;
   root_expr_tree.body = NULL;
   root_expr_tree.parent = NULL;
   root_expr_tree.kind = ROOT;
}

void push_expr(struct expression_list* ls, struct expression* expr){

   struct expression_node* new = (struct expression_node*)malloc(sizeof(struct expression_node));
   new->expr = expr;
   new->next = NULL;

   if(ls->size == 0)
   {
      ls->first = new;
      ls->last = new;
      new->prev = NULL;
     
   }
   else{
      new->prev = ls->last;
      
      ls->last->next = new;

      ls->last = new;
   }

   ls->size++;  
}


void pop_expr(struct expression_list* ls){
   struct expression_node* old;

   if(ls->size == 0){
      printf("cannot pop empty list");
      exit(1);
   }

   old = ls->last;

   if(ls->last == ls->first && ls->size == 1){
      ls->first = NULL;
      ls->last = NULL;
   }
   else{
   
      ls->last = old->prev;

   }
   
   // do not free the expression pointer since it is also shared in the root_expr_tree
   // this function just pops the node from the stack
   free(old);

   ls->size--;
}

struct expression* peek_expr(struct expression_list* ls){

   return ls->last->expr;
}


// void build_pedantic_value(struct expression* expr, const char* substr, size_t substr_len){

   
//    if(expr->interm_step_value == NULL){

//       struct expression* step = init_expr(expr, substr, )
//       data.interm_peda_value = (char*)malloc(substr_len + 1);
//       data.interm_peda_value[0] = '\0';
//       data.interm_peda_len = substr_len;
//    }
//    else{
      
//       data.interm_peda_value = (char*)realloc((void*)data.interm_peda_value, data.interm_peda_len + substr_len + 1);
//       data.interm_peda_len += substr_len;
//    }

//    strncat(data.interm_peda_value, substr, substr_len);

//    expr->data.exp_data = data;
// }

/* Print the tree of variable expansions starting from the assignment as root 
   Just a DFS traversal of the expression tree
*/
void visit_assignment(){

   char imm[] = "IMMEDIATE";
   char lazy[] = "LAZY";
   char* status = imm; /* in the begining we are on the left side of the assignment where everything is marked as immediatelly expanded */
   struct expression* curr;

   struct expression* assig = peek_dbg_expr();

   struct variable* v = assig->data.asig_data;
      
   struct expression_list* visited = init_expr_list(assig);

   /* an assignment has 2 children: left side which is the first and right side which is the next*/
   struct expression_node* right_expr = assig->children->first->next;

   while(visited->size > 0){

      curr = peek_expr(visited);

      pop_expr(visited);

      if(curr == right_expr->expr){
         if(v->flavor == f_simple)
            status = imm;
         else
            status = lazy;
      }
      
      if(curr->children != NULL)
         for(struct expression_node* child = curr->children->last; child != NULL; child = child->prev)
            push_expr(visited, child->expr);

      switch(curr->kind){

      case ROOT:
         printf("Cannot visit ROOT");
         exit(1);
         break;

      case EITHER_MIXED_ATOM_VALUE_UNKNOWN_REF:
         printf("Cannot visit incomplete node %s\n", curr->body);
         exit(1);
         break;

      case ASSIGNMENT:
         printf("%s:%lu\n", v->fileinfo.filenm, v->fileinfo.lineno);
         printf("%s\n", curr->body);
         break;
      
      case MULTI_REFERENCE:
      case MIXED:
         printf("%d.  %s", curr->data.exp_data->order, curr->body);

         for(struct expression* val = curr->next_step_value; val != NULL; val = val->next_step_value)
            printf("  ---> %s", val->body);
         printf("\n");
         break;
         
      case ATOM_VALUE:
         printf("%d. %s\n", curr->data.exp_data->order, curr->body);
         break;
         
      case SIMPLE_REFERENCE:
      case PAREN_REFERENCE:
      case BRACE_REFERENCE:
         printf("%d.  %s",curr->data.exp_data->order, curr->body);
         for(struct expression* val = curr->next_step_value; val != NULL; val = val->next_step_value)
            printf("  ---> %s", val->body);
         printf("\n");
      }
         
   }
   printf("\n");
}

void enter_peda_debug(){

   struct expression* curr = peek_dbg_expr();

   switch (curr->kind)
   {
   case ASSIGNMENT:
      
      visit_assignment();
      break;
   
   case MIXED:
      printf("ERROR cannot enter expand: %s", curr->body);
      exit(2);
      break;
   
   default:
      break;
   }
}


void push_automatic_scope(const char* body, expr_t starting_kind){

   struct expression* e;
   struct expandable_data* info;
   struct expression* parent_or_prev_step = peek_dbg_expr();

   info = (struct expandable_data*)malloc(sizeof(struct expandable_data));
   /* references do not have any children (substrings that can be expanded) so from now on we recursively expand until an atom value
      building a chain of all intermediary steps until the final value
   */
   if(parent_or_prev_step->kind == SIMPLE_REFERENCE || \
      parent_or_prev_step->kind == PAREN_REFERENCE || \
      parent_or_prev_step->kind == BRACE_REFERENCE \
   )
      e = horizontal_next_step_init_expr(parent_or_prev_step, body, starting_kind);
   else /* we still have substring to expand */
      e = vertical_child_init_expr(parent_or_prev_step, body, starting_kind);

   e->scope = AUTOMATIC_SCOPE;

   if(parent_or_prev_step->kind == ASSIGNMENT)
      info->order = 1;
   else
      info->order = parent_or_prev_step->data.exp_data->order + 1;

   e->data.exp_data = info;
   push_dbg_expr(e);
}

void set_real_kind_automatic_scope(expr_t real_kind){

   struct expression* back_expr;
   struct expression* e = peek_automatic_scope();
   

   if(real_kind == MIXED || \
      real_kind == SIMPLE_REFERENCE || \
      real_kind == BRACE_REFERENCE || \
      real_kind == PAREN_REFERENCE || \
      real_kind == MULTI_REFERENCE || \
      real_kind == ATOM_VALUE){

         e->kind = real_kind;
         
         back_expr = e->parent != NULL ? e->parent : e->prev_step_value;

         e->data.exp_data = (struct expandable_data*)malloc(sizeof(struct expandable_data));

         /* expansions store in their private data the depth which will be used when printing the full expression tree */
         if(back_expr->kind == ASSIGNMENT)
            e->data.exp_data->order = 1;
         else /* we are 2 or more levels down the expansion tree so the parent is also an expansion */
            e->data.exp_data->order = back_expr->data.exp_data->order + 1;
   }
   else{
      printf("Cannot bind kind %s for expr %s", to_string_kind(real_kind), e->body);
      exit(1);
   }
   
}

bool is_automatic_scope_multi_ref(const char* outer_dolar, const char* close_delim){

   struct expression* parent_scope_expr = peek_automatic_scope();

   /* do not consider the open and close paren or bracket since these will be left out when going deeper in expand_argument()
   and can repeat in the future scope that goes 1 level deeper
   */
   int size_future_scope_expr_body = close_delim - outer_dolar + 1;

   if(strncmp(parent_scope_expr->body, outer_dolar, size_future_scope_expr_body) == 0)
      return true;
   
   return false;
}

struct expression* push_manual_subscope_multi_reference(const char* outer_dolar, const char* close_delim){
   char* multi_exp_body;
   struct expression* multi_exp;
   struct expandable_data* info;

   info = (struct expandable_data*)malloc(sizeof(struct expandable_data));

   multi_exp_body = (char*)malloc((close_delim - outer_dolar + 1) + 1);

   memset(multi_exp_body, 0, (close_delim - outer_dolar + 1) + 1);

   strncpy(multi_exp_body, outer_dolar, close_delim - outer_dolar + 1);
                    
   multi_exp = vertical_child_init_expr(peek_dbg_expr(), multi_exp_body, MULTI_REFERENCE);

   multi_exp->scope = MULTI_REF_MANUAL_SUBSCOPE;

   info->order = peek_automatic_scope()->data.exp_data->order + 1;
                        
   multi_exp->data.exp_data = info;

   push_dbg_expr(multi_exp);

   return multi_exp;
}


struct expression* push_manual_subscope_reference(
const char* ref_body,
int ref_length,
expr_t ref_kind,
ref_direction_t ref_direction)
{
   struct expression* reference;
   char *ref = malloc(ref_length + 1);
   struct expandable_data* info;
   
   info = (struct expandable_data*)malloc(sizeof(struct expandable_data));

   memset(ref, 0, ref_length + 1);
   strncpy(ref, ref_body, ref_length);
   ref[ref_length + 1] = '\0';

   if(ref_direction == VERTICAL_REF)
      reference = vertical_child_init_expr(peek_dbg_expr(), ref, ref_kind);
   else
      reference = horizontal_next_step_init_expr(peek_dbg_expr(), ref, ref_kind);

   reference->scope = REF_MANUAL_SUBSCOPE;
   free(ref);

   info->order = peek_dbg_expr()->data.exp_data->order + 1;
   reference->data.exp_data = info;
   push_dbg_expr(reference);
   return reference;
}

bool is_automatic_scope_ref(const char* dolar, int ref_length){

   struct expression* parent = peek_automatic_scope();

   if(parent->body_len != ref_length)
      return false;

   for(int i = 0; i < parent->body_len; i++){

      if(dolar[i] != parent->body[i])
         return false;
   }

   return true;
}

const char* to_string_scope(scope_t scope){
   switch(scope){
      case AUTOMATIC_SCOPE:
         return "AUTOMATIC_CURRENT_SCOPE";
         break;
      case MULTI_REF_MANUAL_SUBSCOPE:
         return "MULTI_REF_MANUAL_SUBSCOPE";
         break;
      case REF_MANUAL_SUBSCOPE:
         return "REF_MANUAL_SUBSCOPE";
         break;
      default:
         break;
   }
   return "UNKNOWN";
}

const char* to_string_kind(expr_t k){
   switch(k){
      case ROOT:
         return "ROOT";
         break;
      case ASSIGNMENT:
         return "ASSIGNMENT";
         break;
      case MIXED:
         return "MIXED";
         break;
      case ATOM_VALUE:
         return "ATOM_VALUE";
         break;
      case EITHER_MIXED_ATOM_VALUE_UNKNOWN_REF:
         return "EITHER_MIXED_ATOM_VALUE_UNKNOWN_REF";
         break;
      case MULTI_REFERENCE:
         return "MULTI_REFERENCE";
         break;
      case SIMPLE_REFERENCE:
         return "SIMPLE_REFERENCE";
         break;
      case PAREN_REFERENCE:
         return "PAREN_REFERENCE";
         break;
      case BRACE_REFERENCE:
         return "BRACE_REFERENCE";
         break;
   }

   return "UNKNOWN";
}

void pop_dbg_expr(scope_t pop_scope, expr_t exclusive_kind_check){
   struct expression* e = step_dbg_expr_stack->last->expr;
   scope_t push_scope = e->scope;
   if(push_scope != pop_scope){
      printf("Push scope %s for token %s VS pop scope (%s)\n", to_string_scope(push_scope), e->body, to_string_scope(pop_scope));
      exit(1);
   }

   if(exclusive_kind_check == (expr_t)NULL){
      if(e->kind == EITHER_MIXED_ATOM_VALUE_UNKNOWN_REF){
         printf("Push scope %s does not have final kind assigned\n", e->body);
         exit(1);
      }
   }
   else if(e->kind != exclusive_kind_check){
      printf("Token %s kind %s VS expected kind %s\n", e->body, to_string_kind(e->kind), to_string_kind(exclusive_kind_check));
      exit(1);
   }

   pop_expr(step_dbg_expr_stack);
}

struct expression* peek_automatic_scope(){

   struct expression* e = peek_dbg_expr();

   if(e->scope != AUTOMATIC_SCOPE){
      printf("Token %s is not in the current scope, it is in %s", e->body, to_string_scope(e->scope));
      exit(1);
   }

   return e;
}

// void automatic_scope_peda_val(const char* substr, size_t len){
//    build_pedantic_value(peek_automatic_scope(), substr, len);
// }

/*
void manual_subscope_peda_val(scope_t check_scope, expr_t check_kind, const char* substr, size_t len){
   struct expression* e = peek_dbg_expr();

   if(e->scope != check_scope){
      printf("Cannot append peda value %s for token %s since scope %s diff than expected %s\n",\
         substr,\
         e->body,\
         to_string_scope(e->scope),\
         to_string_scope(check_scope)\
      );
      exit(1);
   }

   if(e->kind != check_kind){
      printf("Cannot append peda value %s for token %s since kind %s diff than expected %s\n",\
         substr,\
         e->body,\
         to_string_kind(e->kind),\
         to_string_kind(check_kind)\
      );
      exit(1);
   }

   build_pedantic_value(e, substr, len);
}
   */
