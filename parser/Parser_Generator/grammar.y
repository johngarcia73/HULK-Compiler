%token FUNCTION TYPE PROTOCOL INHERITS EXTENDS NEW IS AS
%token L_PAREN R_PAREN
%token L_SQUARE_BRACK R_SQUARE_BRACK
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
%token DOT
%token ARROW "=>"
%token TYPE_ARROW "->"
%token RETURN

%start program

%left OR
%left AND
%nonassoc EQUALITY NOT_EQUAL LESS_THAN GREATER_THAN LESS_EQUALS GREATER_EQUALS IS AS
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
            | type_decl
            | protocol_decl

function_decl : FUNCTION IDENTIFIER L_PAREN param_list_opt R_PAREN block
              | FUNCTION IDENTIFIER L_PAREN param_list_opt R_PAREN COLON type block
              | FUNCTION IDENTIFIER L_PAREN param_list_opt R_PAREN ARROW expr SEMICOLON
              | FUNCTION IDENTIFIER L_PAREN param_list_opt R_PAREN COLON type ARROW expr SEMICOLON

type_decl : TYPE IDENTIFIER ctor_param_clause_opt inheritance_clause type_body

protocol_decl : PROTOCOL IDENTIFIER protocol_extension_clause protocol_body

ctor_param_clause_opt : /* empty */
                      | L_PAREN param_list_opt R_PAREN

inheritance_clause : /* empty */
                   | INHERITS IDENTIFIER
                   | INHERITS IDENTIFIER L_PAREN arg_list_opt R_PAREN

protocol_extension_clause : /* empty */
                          | EXTENDS IDENTIFIER

type_body : L_CURL_BRACK type_members R_CURL_BRACK

protocol_body : L_CURL_BRACK protocol_methods R_CURL_BRACK

type_members : /* empty */
             | type_members type_member

protocol_methods : /* empty */
                 | protocol_methods protocol_method_decl

type_member : attribute_decl
            | method_decl

protocol_method_decl : IDENTIFIER L_PAREN param_list_opt R_PAREN COLON type SEMICOLON

attribute_decl : IDENTIFIER opt_type_annotation EQUAL expr SEMICOLON

method_decl : IDENTIFIER L_PAREN param_list_opt R_PAREN block
            | IDENTIFIER L_PAREN param_list_opt R_PAREN COLON type block
            | IDENTIFIER L_PAREN param_list_opt R_PAREN ARROW expr SEMICOLON
            | IDENTIFIER L_PAREN param_list_opt R_PAREN COLON type ARROW expr SEMICOLON

param_list_opt : /* empty */
               | param_list

param_list : param_decl
           | param_list COMMA param_decl

param_decl : IDENTIFIER opt_type_annotation

opt_type_annotation : /* empty */
                    | COLON type

type : type_atom
     | type STAR
     | type L_SQUARE_BRACK R_SQUARE_BRACK
     | L_PAREN type_list_opt R_PAREN TYPE_ARROW type

type_atom : NUMBER_TYPE
          | BOOL_TYPE
          | STRING_TYPE
          | IDENTIFIER

type_list_opt : /* empty */
              | type_list

type_list : type
          | type_list COMMA type

statement : assignment SEMICOLON
          | let_expr SEMICOLON
          | block
          | return_stmt
          | if_stmt
          | while_stmt SEMICOLON
          | for_stmt SEMICOLON

return_stmt : RETURN expr SEMICOLON

return_inline : RETURN expr

block : L_CURL_BRACK statements R_CURL_BRACK

statements : /* empty */
           | statements statement

if_stmt : IF L_PAREN expr R_PAREN conditional_stmt_body conditional_stmt_tail
        | IF L_PAREN expr R_PAREN conditional_inline_stmt_body conditional_stmt_tail   

conditional_stmt_tail : /* empty */
                      | ELSE conditional_stmt_body
                      | ELIF L_PAREN expr R_PAREN conditional_stmt_body conditional_stmt_tail
                      | ELSE conditional_inline_stmt_body
                      | ELIF L_PAREN expr R_PAREN conditional_inline_stmt_body conditional_stmt_tail

conditional_inline_stmt_body : block
                      | assignment
                      | let_expr
                      | return_stmt
                      | if_stmt
                      | while_stmt
                      | for_stmt


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
               | while_stmt SEMICOLON
               | for_stmt SEMICOLON

expr : assignment
     | let_expr
     | if_expr
     | while_expr
     | for_expr

assignment : logical_or
           | assignable COLON EQUAL expr

assignable : IDENTIFIER
           | member_access

logical_or : logical_and
           | logical_or OR logical_and

logical_and : equality
            | logical_and AND equality

equality : type_relation
         | type_relation EQUALITY type_relation
         | type_relation NOT_EQUAL type_relation

type_relation : relational
              | relational IS type
              | relational AS type

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
        | lambda_expr
        | vector_expr
        | if_expr
        | global_call
        | member_call
        | member_access
        | index_access
        | new_expr
        | L_PAREN expr R_PAREN

global_call : IDENTIFIER L_PAREN arg_list_opt R_PAREN

member_access : access_base DOT IDENTIFIER
              | member_access DOT IDENTIFIER

access_base : IDENTIFIER
            | new_expr
            | L_PAREN expr R_PAREN

member_call : access_base DOT IDENTIFIER L_PAREN arg_list_opt R_PAREN
            | member_access DOT IDENTIFIER L_PAREN arg_list_opt R_PAREN

index_access : access_base L_SQUARE_BRACK expr R_SQUARE_BRACK
             | member_access L_SQUARE_BRACK expr R_SQUARE_BRACK
             | global_call L_SQUARE_BRACK expr R_SQUARE_BRACK

new_expr : NEW IDENTIFIER L_PAREN arg_list_opt R_PAREN

lambda_expr : L_PAREN param_list_opt R_PAREN ARROW expr
            | L_PAREN param_list_opt R_PAREN COLON type ARROW expr

vector_expr : L_SQUARE_BRACK R_SQUARE_BRACK
            | L_SQUARE_BRACK vector_items R_SQUARE_BRACK
            | L_SQUARE_BRACK additive OR IDENTIFIER IN expr R_SQUARE_BRACK

vector_items : expr
             | vector_items COMMA expr

let_expr : LET let_bindings IN loop_expr_body

let_bindings : let_binding
             | let_bindings COMMA let_binding

let_binding : IDENTIFIER opt_type_annotation EQUAL expr

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

arg_list_opt : /* empty */
             | arg_list

arg_list : expr
         | arg_list COMMA expr

%%
