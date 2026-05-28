



typedef enum {
   ROOT,
   ASSIGNMENT,
   EXPANSION,
   ATOM_VALUE
} expr_t;

struct expansion_data{
   int depth;
   char* interm_peda_value;
   size_t interm_peda_len;
   char* final_value;
   size_t final_value_len;
};

struct atom_value_data {
   int depth;
};

typedef struct variable assignment_data;

struct expression
{
   char* body;
   size_t body_len;
   expr_t kind;
   struct expression_list* children;
   struct expression* parent;

   /* metadata, segregated based on the expression's kind */
   union private_data {
      struct expansion_data exp_data;
      struct atom_value_data av_data;
      assignment_data* asig_data;
   } data;
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
   int size;
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
    abc${$xaa${zt}$y} >> abc${AaaCB} (intermediary pedantic value) -> abcD (final value stored normally in variable_buffer)
*/
void build_pedantic_value(struct expression* expr, char* substr, size_t substr_len);

void push_expr(struct expression_list* stack, struct expression* expr);
#define push_dbg_expr(e) push_expr(step_dbg_expr_stack, e);

void pop_expr(struct expression_list* stack);
#define pop_dbg_expr() pop_expr(step_dbg_expr_stack);

struct expression* peek_expr(struct expression_list* ls);
#define peek_dbg_expr() peek_expr(step_dbg_expr_stack)

struct expression_list* init_expr_list(struct expression* e);

void init_pedantic();

struct expression* init_expr(struct expression* parent, char* body, expr_t kind);

#define add_child(p, c) \
   if(p->children == NULL) \
      p->children = init_expr_list(c); \
   else \
      push_expr(p->children, c);



void enter_peda_debug();


