# Atoms:
```ebnf
char: ? any char ?;
digit: "0" .. "9" ;
hex_digit: digit | "A" .. "F" | "a" .. "f" ;
letter: "A" .. "Z" | "a" .. "z" ;
whitespace: " "          (* space *)
           | "\t"         (* tab *)
           | "\n"         (* newline / line feed *)
           | "\r"         (* carriage return *)
           | "\f"         (* form feed *) ;
new_line: "\n";
```

# Literals:
```ebnf 
IDENTIFIER: letter, (letter | digit | "_")* ;
NUMBER: digit+, ('.', digit+)? ;

escape_seq: "\\" ("n" | "t" | "r" | "\\" | '"' | "0" | "x" hex_digit hex_digit) ;
string_char: (char - '"' - "\\") | escape_seq ;
STRING: '"', string_char*, '"' ;

TRUE: "true" ;
FALSE: "false" ;
NULL: "null" ;
```
# SEPARATORS
```ebnf
L_PAREN: "(";
R_PAREN: ")";

L_CURL_BRACK: "{";
R_CURL_BRACK: "}";

L_SQUARE_BRACK:"[";
R_SQUARE_BRACK: "]";
SEMICOLON: ";";
COMMA: ",";
```
# Operators

## Single Char Operators:

```ebnf
PLUS: "+";
CARET: "^";
MINUS: "-"-"->";
SLASH: "\";
PERCENTAGE: "%";
ARROBA: "@"-"@@";
AMPERSAND: "&";
PIPE: "|";
STAR: "*";
EXCLAMATION:"!" - "!=";
LESS: "<"-"<=";
GREAT: ">"-">=";
EQUALS:"="- "=="-"=>";
DOT: ".";
COLON: ":"-":=";
DOLAR_SIGN: "$";
```
## Multichar Operators:
```ebnf
DEST_ASIGN: ":=";
EQ_ARROW: "=>";
ARROW: "->";
DOUBLED_ARROBA: "@@";
LESS_EQ: "<=";
GREAT_EQ: ">=";
DOUBLED_EQ: "==";
NOT_EQ: "!=";

```
# Keywords
```ebnf
FOR:"for";
WHILE: "while";
IF: "if";
ELSE: "else";
ELIF: "elif";
FUNCTION: "function";
LET: "let";
IN: "in";
TYPE:"type";
NEW:"new";
INHERITS: "inherits";
EXTENDS: "extends";
BASE:"base";
PROTOCOL: "protocol";
DEF: "def";
MATCH: "match";
CASE: "case";
DEFAULT: "default";
IS: "is";
AS: "as";
```
# Comments
```ebnf
INL_COMMENT: "//", (char - new_line)* ;
BLOCK_COMMENT: "/*", (char - "*/")*, "*/" ;
```

# Error Handling
```ebnf
ERROR: ? any unrecognized character ? ;
```