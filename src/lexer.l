
%option noyywrap yylineno

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

static char *copy_token(const char *text)
{
    size_t len = strlen(text);
    char *copy = malloc(len + 1);

    if (copy == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    memcpy(copy, text, len + 1);
    return copy;
}
%}

DIGIT       [0-9]
ID_START    [a-zA-Z_]
ID_CHAR     [a-zA-Z0-9_]
IDENTIFIER  {ID_START}{ID_CHAR}*
NUMBER      {DIGIT}+
STRING      \"([^\"\\\n]|\\.)*\"

%%

[ \t\r\n]+              ;


"//"[^\n]*              ;
"/*"([^*]|\*+[^*/])*\*+"/"  ;

"decl"                  return DECL;
"enddecl"               return ENDDECL;
"begin"                 return BEG;
"end"                   return END;

"int"                   return INT;
"str"                   return STR;

"if"                    return IF;
"then"                  return THEN;
"else"                  return ELSE;
"endif"                 return ENDIF;

"while"                 return WHILE;
"do"                    return DO;
"endwhile"              return ENDWHILE;

"read"                  return READ;
"write"                 return WRITE;
"break"                 return BREAK;
"continue"              return CONTINUE;
"return"                return RETURN;

"&&"                    return AND;
"and"                   return AND;
"||"                    return OR;
"or"                    return OR;
"!"                     return NOT;
"not"                   return NOT;

"<="                    return LE;
">="                    return GE;
"=="                    return EQ;
"!="                    return NE;
"<"                     return LT;
">"                     return GT;

"+"                     return '+';
"-"                     return '-';
"*"                     return '*';
"/"                     return '/';
"%"                     return '%';

"="                     return '=';
";"                     return ';';
","                     return ',';
"("                     return '(';
")"                     return ')';

{NUMBER} {
    yylval.ival = strtol(yytext, NULL, 10);
    return NUMBER;
}

{STRING} {
    yylval.sval = copy_token(yytext);
    return STRING_LITERAL;
}


{IDENTIFIER} {
    yylval.sval = copy_token(yytext);
    return ID;
}

. {
    fprintf(stderr,
            "Lexer error at line %d: unexpected character '%s'\n",
            yylineno, yytext);
    return yytext[0];
}

%%
