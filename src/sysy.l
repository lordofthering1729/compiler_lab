%option noyywrap
%option nounput
%option noinput
%option yylineno

%{
#include <cstdlib>
#include <string>
#include <iostream>
#include "sysy.tab.hpp"
using namespace std;
int comment_depth = 0;
%}

/* 空白/行注释等 */
WhiteSpace    [ \t\n\r]*
LineComment   "//".*
BlockCommentStart  "/*"
BlockCommentEnd    "*/"
Identifier    [a-zA-Z_][a-zA-Z0-9_]*
Decimal       [1-9][0-9]*
Octal         0[0-7]*
Hexadecimal   0[xX][0-9a-fA-F]+
%x BLOCK_COMMENT
%%
{WhiteSpace}    { }
{LineComment}   { }

{BlockCommentStart} { comment_depth = 1; BEGIN(BLOCK_COMMENT); }
<BLOCK_COMMENT>{
    {BlockCommentStart} { comment_depth++; }
    {BlockCommentEnd} { comment_depth--; if (comment_depth == 0) BEGIN(INITIAL); }
    [^*\n/]+   { }
    "*"        { }
    "/"        { }
    \n         { }
    <<EOF>>    { cerr << "Error: Unclosed block comment at end of file" << endl; yyterminate(); }
}

"<="            { return LE; }
">="            { return GE; }
"=="            { return EQ; }
"!="            { return NE; }
"&&"            { return AND; }
"||"            { return OR; }
"int"           { return INT; }
"void"          { return VOID; }
"const"         { return CONST; }
"return"        { return RETURN; }
"if"            { return IF; }
"else"          { return ELSE; }
"while"         { return WHILE; }
"break"         { return BREAK; }
"continue"      { return CONTINUE; }

{Identifier}    { yylval.str_val = new string(yytext); return IDENT; }

{Decimal}       { yylval.int_val = strtol(yytext, nullptr, 0); return INT_CONST; }
{Octal}         { yylval.int_val = strtol(yytext, nullptr, 0); return INT_CONST; }
{Hexadecimal}   { yylval.int_val = strtol(yytext, nullptr, 0); return INT_CONST; }

.               { return yytext[0]; }
%%