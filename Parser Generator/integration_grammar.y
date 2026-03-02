%{
#include <iostream>
%}

%token number
%token +
%token *

%start S'

%%

E : E + T
  | T
  ;

T : T * F
  | F
  ;

F : number
  ;

S' : E
   ;
