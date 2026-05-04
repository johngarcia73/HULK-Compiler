%token FUNCTION
%token L_PAREN R_PAREN
%token L_CURL_BRACK R_CURL_BRACK
%token SEMICOLON
%token COMMA
%token LET IN
%token IF ELIF ELSE
%token WHILE FOR
%token TRUE FALSE
%token PLUS MINUS STAR SLASH MODULE
%token NOT AND OR
%token CONCAT
%token LESS_THAN GREATER_THAN LESS_EQUALS GREATER_EQUALS EQUALITY NOT_EQUAL
%token NUMBER
%token NUMBER_TYPE
%token BOOL_TYPE
%token STRING_TYPE
%token STRING
%token IDENTIFIER
%token EQUAL
%token EOF 0
%token COLON
%token ARROW "=>"
%token RETURN

%start program

%left OR
%left AND
%nonassoc LESS_THAN GREATER_THAN LESS_EQUALS GREATER_EQUALS EQUALITY NOT_EQUAL
%left CONCAT
%left PLUS MINUS
%left STAR SLASH MODULE
%right NOT
%nonassoc ELSE ELIF

%%

program : top_level_items

top_level_items : /* empty */
               | top_level_items top_level_item

top_level_item : declaration
               | statement

declaration : function_decl

function_decl : FUNCTION IDENTIFIER L_PAREN param_list_opt R_PAREN block
              | FUNCTION IDENTIFIER L_PAREN param_list_opt R_PAREN COLON type block
              | FUNCTION IDENTIFIER L_PAREN param_list_opt R_PAREN ARROW expr SEMICOLON
              | FUNCTION IDENTIFIER L_PAREN param_list_opt R_PAREN COLON type ARROW expr SEMICOLON

param_list_opt : /* empty */
               | param_list

param_list : IDENTIFIER COLON type
           | param_list COMMA IDENTIFIER COLON type

type : NUMBER_TYPE
     | BOOL_TYPE
     | STRING_TYPE

statement : assignment SEMICOLON
          | let_expr SEMICOLON
          | block
          | return_stmt
          | if_stmt
          | while_stmt
          | for_stmt

return_stmt : RETURN expr SEMICOLON

block : L_CURL_BRACK statements R_CURL_BRACK

statements : /* empty */
           | statements statement

if_stmt : IF L_PAREN expr R_PAREN conditional_stmt_body conditional_stmt_tail

conditional_stmt_tail : /* empty */
                      | ELSE conditional_stmt_body
                      | ELIF L_PAREN expr R_PAREN conditional_stmt_body conditional_stmt_tail

conditional_stmt_body : block
                      | assignment SEMICOLON
                      | let_expr SEMICOLON
                      | return_stmt
                      | if_stmt
                      | while_stmt
                      | for_stmt

while_stmt : WHILE L_PAREN expr R_PAREN loop_stmt_body

for_stmt : FOR L_PAREN IDENTIFIER IN expr R_PAREN loop_stmt_body

loop_stmt_body : block
               | assignment SEMICOLON
               | let_expr SEMICOLON
               | return_stmt
               | if_stmt
               | while_stmt
               | for_stmt

expr : assignment
     | let_expr
     | if_expr
     | while_expr
     | for_expr
     /* call_expr eliminado de aquí */

assignment : logical_or
           | IDENTIFIER COLON EQUAL expr

logical_or : logical_and
           | logical_or OR logical_and

logical_and : equality
            | logical_and AND equality

equality : relational
         | relational EQUALITY relational
         | relational NOT_EQUAL relational

relational : concatenation
           | concatenation LESS_THAN concatenation
           | concatenation GREATER_THAN concatenation
           | concatenation LESS_EQUALS concatenation
           | concatenation GREATER_EQUALS concatenation

concatenation : additive
              | concatenation CONCAT additive

additive : multiplicative
         | additive PLUS multiplicative
         | additive MINUS multiplicative

multiplicative : unary
               | multiplicative STAR unary
               | multiplicative SLASH unary
               | multiplicative MODULE unary

unary : MINUS unary
      | PLUS unary
      | NOT unary
      | primary

primary : NUMBER
        | STRING
        | TRUE
        | FALSE
        | IDENTIFIER
        | IDENTIFIER L_PAREN arg_list_opt R_PAREN   /* NUEVA: llamada a función */
        | L_PAREN expr R_PAREN

let_expr : LET let_bindings IN loop_expr_body

let_bindings : let_binding
             | let_bindings COMMA let_binding

let_binding : IDENTIFIER EQUAL expr
            | LET IDENTIFIER EQUAL expr

if_expr : IF L_PAREN expr R_PAREN conditional_expr_body conditional_expr_tail

conditional_expr_tail : /* empty */
                      | ELSE conditional_expr_body
                      | ELIF L_PAREN expr R_PAREN conditional_expr_body conditional_expr_tail

conditional_expr_body : block
                      | expr

while_expr : WHILE L_PAREN expr R_PAREN loop_expr_body

for_expr : FOR L_PAREN IDENTIFIER IN expr R_PAREN loop_expr_body

loop_expr_body : block
               | expr

/* call_expr se mantiene por compatibilidad con el AST builder,
   pero ya no es alcanzable desde expr. */
call_expr : IDENTIFIER L_PAREN arg_list_opt R_PAREN

arg_list_opt : /* empty */
             | arg_list

arg_list : expr
         | arg_list COMMA expr

%%
