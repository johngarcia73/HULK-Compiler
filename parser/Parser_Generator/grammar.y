%token FUNCTION
%token L_PAREN R_PAREN
%token L_CURL_BRACK R_CURL_BRACK
%token SEMICOLON
%token COMMA
%token LET IN
%token IF ELSE
%token PLUS MINUS STAR SLASH
%token NUMBER
%token NUMBER_TYPE
%token BOOL_TYPE
%token STRING_TYPE
%token IDENTIFIER
%token EQUAL
%token EOF 0
%token COLON

%start program

%left PLUS MINUS
%left STAR SLASH

%%

program : declarations statements

declarations : /* empty */
             | declarations declaration

declaration : function_decl

function_decl : FUNCTION IDENTIFIER L_PAREN param_list_opt R_PAREN block
              | FUNCTION IDENTIFIER L_PAREN param_list_opt R_PAREN COLON type block

param_list_opt : /* empty */
               | param_list

param_list : IDENTIFIER COLON type
           | param_list COMMA IDENTIFIER COLON type




type : NUMBER_TYPE
     | BOOL_TYPE
     | STRING_TYPE


statements : /* empty */
           | statements statement

statement : expr SEMICOLON
          | block

block : L_CURL_BRACK statements R_CURL_BRACK

expr : additive
     | let_expr
     | if_expr
     | call_expr

additive : multiplicative
         | additive PLUS multiplicative
         | additive MINUS multiplicative

multiplicative : primary
               | multiplicative STAR primary
               | multiplicative SLASH primary

primary : NUMBER
        | IDENTIFIER
        | L_PAREN expr R_PAREN

let_expr : LET IDENTIFIER EQUAL expr IN expr

if_expr : IF L_PAREN expr R_PAREN expr ELSE expr

call_expr : IDENTIFIER L_PAREN arg_list_opt R_PAREN

arg_list_opt : /* empty */
             | arg_list

arg_list : expr
         | arg_list COMMA expr

%%