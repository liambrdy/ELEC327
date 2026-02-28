# Language Grammar Specification

This document defines the grammar of the language used by this project.

## Expression Grammar

```bnf
primary-expression ::=
      identifier
    | constant
    | string-literal
    | "(" expression ")"

postfix-expression ::=
      primary-expression
    | postfix-expression "[" expression "]"
    | postfix-expression "(" argument-expression-list ")"
    | postfix-expression "." identifier
    | postfix-expression "->" identifier
    | postfix-expression "++"
    | postfix-expression "--"

argument-expression-list ::=
      assignment-expression
    | argument-expression-list "," assignment-expression


unary-expression ::=
      postfix-expression
    | "++" unary-expression
    | "--" unary-expression
    | unary-operator cast-expression

unary-operator ::=
      "&"
    | "*"
    | "+"
    | "-"
    | "~"
    | "!"


cast-expression ::=
      unary-expression
    | "(" type-name ")" cast-expression


multiplicative-expression ::=
      cast-expression
    | multiplicative-expression "*" cast-expression
    | multiplicative-expression "/" cast-expression
    | multiplicative-expression "%" cast-expression


additive-expression ::=
      multiplicative-expression
    | additive-expression "+" multiplicative-expression
    | additive-expression "-" multiplicative-expression


shift-expression ::=
      additive-expression
    | shift-expression "<<" additive-expression
    | shift-expression ">>" additive-expression


relational-expression ::=
      shift-expression
    | relational-expression "<" shift-expression
    | relational-expression ">" shift-expression
    | relational-expression "<=" shift-expression
    | relational-expression ">=" shift-expression


equality-expression ::=
      relational-expression
    | equality-expression "==" relational-expression
    | equality-expression "!=" relational-expression


and-expression ::=
      equality-expression
    | and-expression "&" equality-expression


exclusive-or-expression ::=
      and-expression
    | exclusive-or-expression "^" and-expression


inclusive-or-expression ::=
      exclusive-or-expression
    | inclusive-or-expression "|" exclusive-or-expression


logical-and-expression ::=
      inclusive-or-expression
    | logical-and-expression "&&" inclusive-or-expression


logical-or-expression ::=
      logical-and-expression
    | logical-or-expression "||" logical-and-expression


conditional-expression ::=
      logical-or-expression
    | logical-or-expression "?" expression ":" conditional-expression


assignment-expression ::=
      conditional-expression
    | unary-expression assignment-operator assignment-expression


assignment-operator ::=
      "="
    | "*="
    | "/="
    | "%="
    | "+="
    | "-="
    | "<<="
    | ">>="
    | "&="
    | "^="
    | "|="


expression ::=
      assignment-expression
    <!-- | expression "," assignment-expression -->


constant-expression ::=
    conditional-expression
```

## Declaration Grammar
```bnp
declaration ::=
    declaration-specifiers [ init-declarator-list ] ";"

declaration-specifiers ::=
    storage-class-specifier [ declaration-specifiers ]
  | type-specifier [ declaration-specifiers ]
  | type-qualifier [ declaration-specifiers ]
  | function-specifier [ declaration-specifiers ]

init-declarator-list ::=
    init-declarator
  | init-declarator-list "," init-declarator

init-declarator ::=
    declarator
  | declarator "=" initializer

storage-class-specifier ::=
    auto
  | extern
  | register
  | static
  | typedef

type-specifier ::=
    void
  | char
  | short
  | int
  | long
  | float
  | double
  | signed
  | unsigned
  | struct-or-union-specifier
  | enum-specifier
  | typedef-name

struct-or-union-specifier ::=
    struct-or-union [ identifier ] "{" struct-declaration-list "}"
  | struct-or-union identifier

struct-or-union ::=
    struct
  | union

struct-declaration-list ::=
    struct-declaration
  | struct-declaration-list struct-declaration

struct-declaration ::=
    specifier-qualifier-list [ struct-declarator-list ] ";"

specifier-qualifier-list ::=
    type-specifier [ specifier-qualifier-list ]
  | type-qualifier [ specifier-qualifier-list ]

struct-declarator-list ::=
    struct-declarator
  | struct-declarator-list "," struct-declarator

struct-declarator ::=
    declarator
  | [ declarator ] ":" constant-expression

enum-specifier ::=
    enum [ identifier ] "{" enumerator-list "}"
  | enum [ identifier ] "{" enumerator-list "," "}"
  | enum identifier

enumerator-list ::=
    enumerator
  | enumerator-list "," enumerator

enumerator ::=
    enumeration-constant
  | enumeration-constant "=" constant-expression

type-qualifier ::=
    const
  | restrict
  | volatile

function-specifier ::=
    inline

declarator ::=
    [ pointer ] direct-declarator

direct-declarator ::=
    identifier
  | "(" declarator ")"
  | direct-declarator "[" [ assignment-expression ] "]"
  | direct-declarator "(" parameter-type-list ")"

pointer ::=
    "*" [ type-qualifier-list ]
  | "*" [ type-qualifier-list ] pointer

type-qualifier-list ::=
    type-qualifier
  | type-qualifier-list type-qualifier

parameter-type-list ::=
    parameter-list
  | parameter-list "," "..."

parameter-list ::=
    parameter-declaration
  | parameter-list "," parameter-declaration

parameter-declaration ::=
    declaration-specifiers declarator
  | declaration-specifiers [ abstract-declarator ]

type-name ::=
    specifier-qualifier-list [ abstract-declarator ]

abstract-declarator ::=
    pointer
  | [ pointer ] direct-abstract-declarator

direct-abstract-declarator ::=
    "(" abstract-declarator ")"
  | direct-abstract-declarator "[" [ assignment-expression ] "]"
  | [ direct-abstract-declarator ] "(" [ parameter-type-list ] ")"

typedef-name ::=
    identifier

initializer ::=
    assignment-expression
  | "{" initializer-list "}"
  | "{" initializer-list "," "}"

initializer-list ::=
    [ designation ] initializer
  | initializer-list "," [ designation ] initializer

designation ::=
    designator-list "="

designator-list ::=
    designator
  | designator-list designator

designator ::=
    "[" constant-expression "]"
  | "." identifier
```

## Statement Grammar
```bnp
statement ::=
  labeled-statement
  | compound-statement
  | expression-statement
  | selection-statement
  | iteration-statement
  | jump-statement

jump-statement ::=
  "goto" identifier ";"
  | "continue" ";"
  | "break" ";"
  | "return" [ expression ] ";"

compound-statement ::=
  "{" [ declaration-list ] [ statement-list ] "}"

declaration-list ::=
  declaration
  | declaration-list declaration

statement-list ::=
  statement
  | statemtn-list statement

expression-statement ::=
  [ expression ] ";"

iteration-statement ::=
  "while" "(" expression ")" statement
  | "do" statement "while" "(" expression ")" ";"
  | "for" "(" [ expression ] ";" [ expression ] ";" [ expression ] ")" statement

selection-statement ::=
  "if" "(" expression ")" statement
  | "if" "(" expression ")" statement "else" statement
  | switch "(" expression ")" statement

labeled-statement ::=
  identifier ":" statement
  | "case" constant-expression ":" statement
  | "default" ":" statement
```

## Translation Unit Parsing
```bnp
translation-unit ::=
  external-declaration
  | translation-unit external-declaration

external-declaration ::=
  function-definition
  | declaration

function-definition ::=
  [ declaration-specifiers ] declarator compound-statement
```