#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "variable.h"
#include "pedantic.h"
#include "gnuremake.h"


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
   l->size = 1;
   return l;
}
struct expression* init_expr(struct expression* parent, char* body, expr_t kind){

   struct expression* e = (struct expression*)malloc(sizeof(struct expression));
   e->body = strdup(body);
   e->body_len = strlen(body);
   e->children = NULL;
   e->kind = kind;
   e->parent = parent;

   if(parent != NULL){
      add_child(parent, e)
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

/* used only for expression of type EXPANSION */
void build_pedantic_value(struct expression* expr, char* substr, size_t substr_len){

   struct expansion_data data = expr->data.exp_data;

   if(substr_len == 0){
      printf("Warning: %s cannot have length 0\n");
      return;
   }

   if(data.interm_peda_value == NULL){

      data.interm_peda_value = (char*)malloc(substr_len + 1);
      data.interm_peda_value[0] = '\0';
      data.interm_peda_len = substr_len;
   }
   else{
      
      data.interm_peda_value = (char*)realloc((void*)data.interm_peda_value, data.interm_peda_len + substr_len + 1);
      data.interm_peda_len += substr_len;
   }

   strncat(data.interm_peda_value, substr, substr_len);

   expr->data.exp_data = data;
}

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

      case ASSIGNMENT:
         printf("%s:%ld\n", v->fileinfo.filenm, v->fileinfo.lineno);
         printf("%s   @%s\n", curr->body, status);
         break;
         
      case EXPANSION:
            
         for(int i = 0; i < curr->data.exp_data.depth; i++)
            printf("\t");
               
         printf("%s -> %s => %s @%s\n", curr->body, \
            curr->data.exp_data.interm_peda_value, \
            curr->data.exp_data.final_value, \
            status);
            
         break;
         
      case ATOM_VALUE:
            
         for(int i = 0; i < curr->data.av_data.depth; i++)
            printf("\t");
            
         printf("%s\n", curr->body);

         break;
         
      default:
         printf("ERROR in visiting foregin node : %s\n", curr->body);
         exit(1);
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
   
   case EXPANSION:
      printf("ERROR cannot enter expand: %s", curr->body);
      exit(2);
      break;
   
   default:
      break;
   }
}