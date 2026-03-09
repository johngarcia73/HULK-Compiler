# Hulk Program
```ebnf
hulk_program: (declaration)* stament;

statment: (block (SEMICOLON)?| not_block_expr SEMICOLON);
```
# Declaration:
```ebnf 
declaration: (function_declaration|type_declaration|protocol_declaration)

function_declaration: FUNCTION,
                    L_PAREN,
                    (IDENTIFIER (COMMA IDENTIFIER)*)?
                    R_PAREN
                    (
                        (EQ_ARROW non_block_expr SEMICOLON) | (block (SEMICOLON)?)
                    );

type_declaration: TYPE IDENTIFIER 
        L_PAREN                     
        (IDENTIFIER (COMMA IDENTIFIER)*)?
        R_PAREN
        INHERITS
        IDENTIFIER 
        L_PAREN
        ( expr (COMMA expr)*)?
        R_PAREN
    L_CURL_BRACK
    (attribute_declaration|function_declaration)*;
    R_CURL_BRACK

 protocol_declaration: PROTOCOL IDENTIFIER
            EXTENDS
            IDENTIDIER
            L_CURL_BRACK
            (function_declaration)*
            R_CURL_BRACK;

attribute_declaration: IDENTIFIER EQUALS expr SEMICOLON;
```
# Block
```ebnf
block:L_CURL_BRACK
            (statement)* 
    R_CURL_BRACK;
```
# Command Expressions
```ebnf
command_expr: (function_call|variable_declaration|if|for|while);

function_call: IDENTIFIER,
            L_PAREN
            (expr (COMMA expr)*)?
            R_PAREN;

variable_declaration: LET 
                    (IDENTIFIER EQUALS expr)
                    (COMMA IDENTIFIER EQUALS expr)*
                    IN
                    expr;



if: IF L_PAREN expr R_PAREN
                    expr
                   (ELIF LPAREN expr R_PAREN
                    expr
                   )*
                   (ELSE expr)?;

while: WHILE L_PAREN expr R_PAREN
                    expr;

for: FOR L_PAREN IDENTIFIER IN expr R_PAREN
                expr;

```

# Expressions:
```ebnf
expr: (block | not_block_expr);

not_block_expr: assignment;

assignment: IDENTIFIER := logic_or;

logic_or: logic_and (PIPE logic_and)*;

logic_and: equality (AMPERSAND equality)*; 

equality: relational ((EQUAL|NOT_EQ) relational)?

relational: concatenation ( (LESS|GREAT|GREAT_EQ|LESS_EQ) concatenation )?;

concatenation: additive ( (ARROBA|DOUBLED_ARROBA) additive)*

additive : multiplicative ( (PLUS|MINUS) multiplicative)*;

multiplicative: unary ( (STAR|SLASH|PERCENTAGE) unary)*; 

unary: (MINUS|EXCLAMATION) unary |  
        power; 

power: primary (CARET power)?;

member_acess: primary ( DOT primary)*;

primary: literal|group|command_expression|variable|inicialization;

literal: NUMBER|STRING|TRUE|FALSE;

group: L_PAREN expr RPAREN;

variable: IDENTIFIER;

inicialization: NEW IDENTIFIER 
                    L_PAREN 
                    (exp (COMMA expr)*)?
                    R_PAREN;
```



