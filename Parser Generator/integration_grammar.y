%{
#include <iostream>
%}

%token number
%token +
%token *

%start S'

%%

E : E + T { $$ = new BinaryOpNode('+', static_cast<ExprNode*>($1), static_cast<ExprNode*>($3)); }
  | T { $$ = $1; }
  ;

T : T * F { $$ = new BinaryOpNode('*', static_cast<ExprNode*>($1), static_cast<ExprNode*>($3)); }
  | F { $$ = $1; }
  ;

F : number { $$ = $1; }
  ;

S' : E { $$ = $1; }
   ;
