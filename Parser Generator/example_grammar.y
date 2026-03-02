%{
// User C++ code section
#include <iostream>
%}

%token NUMBER PLUS TIMES LPAREN RPAREN

%start expr

%%

expr : expr PLUS term { $$ = $1 + $3; }
     | term { $$ = $1; }
     ;

term : term TIMES factor { $$ = $1 * $3; }
     | factor { $$ = $1; }
     ;

factor : NUMBER { $$ = $1; }
       | LPAREN expr RPAREN { $$ = $2; }
       ;

%%

// User C++ code section (implementation)
