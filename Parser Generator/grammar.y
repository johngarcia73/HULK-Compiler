%token NUMBER
%token IDENTIFIER
%token LET
%token SEMICOLON
%token COMMA
%token L_PAREN
%token R_PAREN
%token L_CURL_BRACK
%token R_CURL_BRACK
%token ASSIGN
%token PLUS
%token MINUS
%token STAR
%token SLASH
%token PERCENTAGE
%token CARET
%token EQ_ARROW

%start program

%%

program : declarations statements

declarations :

declarations : declarations declaration

declaration : var_decl

var_decl : LET IDENTIFIER ASSIGN expr SEMICOLON

statements :

statements : statements statement

statement : expr SEMICOLON
statement : block
statement : var_decl

block : L_CURL_BRACK statements R_CURL_BRACK

expr : additive

additive : multiplicative
additive : additive PLUS multiplicative
additive : additive MINUS multiplicative

multiplicative : power
multiplicative : multiplicative STAR power
multiplicative : multiplicative SLASH power

power : unary
power : unary CARET power

unary : MINUS unary
unary : PLUS unary
unary : primary

primary : NUMBER
primary : IDENTIFIER
primary : L_PAREN expr R_PAREN

%%