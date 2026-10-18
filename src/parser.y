
%{
#include <stdio.h>
#include <stdlib.h>
#include "ast.h"
#include "symtab.h"

Program *parse_result = NULL;

extern int yylineno;
int yylex(void);
void yyerror(const char *message);
%}

%code requires {
    #include "ast.h"
}

%code provides {
    extern Program *parse_result;
}

%union {
    long ival;
    char *sval;
    Expr *expr;
    Stmt *stmt;
    Decl *decl;
    DataType dtype;
}

%token DECL ENDDECL BEG END
%token INT STR
%token IF THEN ELSE ENDIF
%token WHILE DO ENDWHILE
%token READ WRITE BREAK CONTINUE RETURN
%token AND OR NOT
%token LT GT LE GE EQ NE
%token <ival> NUMBER
%token <sval> ID STRING_LITERAL

%type <dtype> type
%type <decl> declarations declaration identifier_list
%type <stmt> statements statement
%type <expr> expression

%left OR
%left AND
%nonassoc EQ NE LT GT LE GE
%left '+' '-'
%left '*' '/' '%'
%precedence NOT UMINUS

%start program

%initial-action {
    symtab_init();
    parse_result = NULL;
}

%%

program
    : DECL declarations ENDDECL BEG statements END
      {
          parse_result = make_program($2, $5);
      }
    ;

declarations
    : %empty
      {
          $$ = NULL;
      }
    | declarations declaration
      {
          $$ = append_decl($1, $2);
      }
    ;

declaration
    : type identifier_list ';'
      {
          Decl *d = $2;

          for (; d != NULL; d = d->next) {
              d->type = $1;

              if (!symtab_add(d->name, d->type)) {
                  yyerror("duplicate variable declaration");
                  YYERROR;
              }
          }

          $$ = $2;
      }
    ;

type
    : INT
      {
          $$ = TYPE_INT;
      }
    | STR
      {
          $$ = TYPE_STR;
      }
    ;

identifier_list
    : ID
      {
          $$ = make_decl($1, TYPE_INT);
          free($1);
      }
    | identifier_list ',' ID
      {
          $$ = append_decl($1, make_decl($3, TYPE_INT));
          free($3);
      }
    ;

statements
    : %empty
      {
          $$ = NULL;
      }
    | statements statement
      {
          $$ = append_stmt($1, $2);
      }
    ;

statement
    : ID '=' expression ';'
      {
          $$ = make_assign($1, $3);
          free($1);
      }
    | READ '(' ID ')' ';'
      {
          $$ = make_read($3);
          free($3);
      }
    | WRITE '(' expression ')' ';'
      {
          $$ = make_write($3);
      }
    | IF '(' expression ')' THEN statements ENDIF
      {
          $$ = make_if($3, $6, NULL);
      }
    | IF '(' expression ')' THEN statements ELSE statements ENDIF
      {
          $$ = make_if($3, $6, $8);
      }
    | WHILE '(' expression ')' DO statements ENDWHILE
      {
          $$ = make_while($3, $6);
      }
    | BREAK ';'
      {
          $$ = make_break();
      }
    | CONTINUE ';'
      {
          $$ = make_continue();
      }
    | RETURN ';'
      {
          $$ = make_return(NULL);
      }
    | RETURN expression ';'
      {
          $$ = make_return($2);
      }
    ;

expression
    : NUMBER
      {
          $$ = make_int($1);
      }
    | STRING_LITERAL
      {
          $$ = make_string($1);
          free($1);
      }
    | ID
      {
          $$ = make_var($1);
          free($1);
      }
    | '(' expression ')'
      {
          $$ = $2;
      }
    | expression '+' expression
      {
          $$ = make_binary(AST_ADD, $1, $3);
      }
    | expression '-' expression
      {
          $$ = make_binary(AST_SUB, $1, $3);
      }
    | expression '*' expression
      {
          $$ = make_binary(AST_MUL, $1, $3);
      }
    | expression '/' expression
      {
          $$ = make_binary(AST_DIV, $1, $3);
      }
    | expression '%' expression
      {
          $$ = make_binary(AST_MOD, $1, $3);
      }
    | '-' expression %prec UMINUS
      {
          $$ = make_unary(AST_NEG, $2);
      }
    | NOT expression
      {
          $$ = make_unary(AST_NOT, $2);
      }
    | expression LT expression
      {
          $$ = make_binary(AST_LT, $1, $3);
      }
    | expression GT expression
      {
          $$ = make_binary(AST_GT, $1, $3);
      }
    | expression LE expression
      {
          $$ = make_binary(AST_LE, $1, $3);
      }
    | expression GE expression
      {
          $$ = make_binary(AST_GE, $1, $3);
      }
    | expression EQ expression
      {
          $$ = make_binary(AST_EQ, $1, $3);
      }
    | expression NE expression
      {
          $$ = make_binary(AST_NE, $1, $3);
      }
    | expression AND expression
      {
          $$ = make_binary(AST_AND, $1, $3);
      }
    | expression OR expression
      {
          $$ = make_binary(AST_OR, $1, $3);
      }
    ;

%%

void yyerror(const char *message)
{
    fprintf(stderr, "Parser error at line %d: %s\n",
            yylineno, message);
}
