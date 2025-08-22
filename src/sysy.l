%option noyywrap
%option nounput
%option noinput
%option yylineno

%{

#include <cstdlib>
#include <string>
#include <iostream>  // 用于错误输出

#include "sysy.tab.hpp"

using namespace std;

// 块注释状态跟踪
int comment_depth = 0;  // 用于处理嵌套注释（如果需要）
%}

/* 空白符和行注释 */
WhiteSpace    [ \t\n\r]*
LineComment   "//".*

/* 块注释 */
BlockCommentStart  "/*"
BlockCommentEnd    "*/"

/* 标识符 */
Identifier    [a-zA-Z_][a-zA-Z0-9_]*

/* 整数字面量 */
Decimal       [1-9][0-9]*
Octal         0[0-7]*
Hexadecimal   0[xX][0-9a-fA-F]+

/* 状态声明（用于处理块注释） */
%x BLOCK_COMMENT

%%

{WhiteSpace}    { /* 忽略空白 */ }
{LineComment}   { /* 忽略行注释 */ }

{BlockCommentStart} { 
    comment_depth = 1;
    BEGIN(BLOCK_COMMENT); 
}

<BLOCK_COMMENT>{
    {BlockCommentStart} { 
        comment_depth++; 
    }
    
    {BlockCommentEnd} {
        comment_depth--;
        if (comment_depth == 0) {
            BEGIN(INITIAL);
        }
    }
    
    [^*\n/]+   { /* 忽略块注释中的普通文本 */ }
    "*"        { }
    "/"        { }
    \n         { }
    <<EOF>>    {
        cerr << "Error: Unclosed block comment at end of file" << endl;
        yyterminate();
    }
}

"<="            { return LE; }
">="            { return GE; }
"=="            { return EQ; }
"!="            { return NE; }
"&&"            { return AND; }
"||"            { return OR; }

"int"           { return INT; }
"const"         { return CONST; }
"return"        { return RETURN; }
"if"            { return IF; }
"else"          { return ELSE; }

{Identifier}    { yylval.str_val = new string(yytext); return IDENT; }

{Decimal}       { yylval.int_val = strtol(yytext, nullptr, 0); return INT_CONST; }
{Octal}         { yylval.int_val = strtol(yytext, nullptr, 0); return INT_CONST; }
{Hexadecimal}   { yylval.int_val = strtol(yytext, nullptr, 0); return INT_CONST; }

.               { return yytext[0]; }

%%