typedef enum {
   ROOT = 1,
   ASSIGNMENT = 2,
   MIXED = 3,
   ATOM_VALUE = 4,
   EITHER_MIXED_ATOM_VALUE_UNKNOWN_REF = 5,
   MULTI_REFERENCE = 6,
   SIMPLE_REFERENCE = 7,
   PAREN_REFERENCE = 8,
   BRACE_REFERENCE = 9
} expr_t;

typedef enum {
   AUTOMATIC_SCOPE,
   MULTI_REF_MANUAL_SUBSCOPE,
   REF_MANUAL_SUBSCOPE
} scope_t;

typedef enum{
   HORIZONTAL_REF,
   VERTICAL_REF
} ref_direction_t;

typedef struct variable assignment_data;

/* this is for expression such as mixed strings, all types of references and atom value */
struct expandable_data {
   int order;
};

struct expression
{
   char* body;
   size_t body_len;
   expr_t kind;
   scope_t scope;
   struct expression* next_step_value;
   struct expression* prev_step_value;
   struct expression_list* children;
   struct expression* parent;

   /* metadata, segregated based on the expression's kind */
   union private_data {
      assignment_data* asig_data;
      struct expandable_data* exp_data;
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
// void build_pedantic_value(struct expression* expr, const char* substr, size_t substr_len);


void push_expr(struct expression_list* stack, struct expression* expr);

/* Create a new expression and link it with the current one, much like pushing on the stack a new neighbor during DFS traversal*/
#define push_dbg_expr(e) push_expr(step_dbg_expr_stack, e);

void pop_expr(struct expression_list* stack);

void pop_dbg_expr(scope_t pop_scope, expr_t exclusive_kind_check);

struct expression* peek_expr(struct expression_list* ls);
#define peek_dbg_expr() peek_expr(step_dbg_expr_stack)

struct expression_list* init_expr_list(struct expression* e);

void init_pedantic();

struct expression* init_expr(struct expression* parent, struct expression* prev_interm_step, const char* body, expr_t kind);

#define horizontal_next_step_init_expr(prev_step_value, body, kind) init_expr(NULL, prev_step_value, body, kind)
#define vertical_child_init_expr(parent, body, kind) init_expr(parent, NULL, body, kind)

#define add_child(p, c) \
   if(p->children == NULL) \
      p->children = init_expr_list(c); \
   else \
      push_expr(p->children, c);



void enter_peda_debug();



/* stages of expressions used by variable_expand_string() and reference_variable() */

void push_automatic_scope(const char* body, expr_t starting_kind);

#define pop_automatic_scope_either_mixed_atom_value_unknown_ref() pop_dbg_expr(AUTOMATIC_SCOPE, (expr_t)NULL)

void set_real_kind_automatic_scope(expr_t real_kind);

// void automatic_scope_peda_val(const char* substr, size_t len);

bool is_automatic_scope_multi_ref(const char* open_delim, const char* close_delim);

bool is_automatic_scope_ref(const char* dolar, int ref_length);

/* Used by an auto mixed or multi reference */
struct expression* push_manual_subscope_multi_reference(const char* open_delim, const char* close_delim);

#define pop_subscope_multi_expansion() pop_dbg_expr(MULTI_REF_MANUAL_SUBSCOPE, MULTI_REFERENCE)

void manual_subscope_peda_val(scope_t check_scope, expr_t check_kind, const char* substr, size_t len);

/* Used by an auto and manual multi reference or an auto reference or mixed */
struct expression* push_manual_subscope_reference(const char* ref_beg, int ref_length, expr_t ref_kind, ref_direction_t ref_direction);

#define pop_subscope_reference(k) pop_dbg_expr(REF_MANUAL_SUBSCOPE, k)

struct expression* peek_automatic_scope();

const char* to_string_kind(expr_t k);

const char* to_string_scope(scope_t scope);


